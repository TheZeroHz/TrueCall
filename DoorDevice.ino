// ================= Device C: ESP32 Display + Ringer + On-demand WiFiManager =================
// - Clock by default (12h, blinking colon)
// - ESP-NOW receiver: RingerMessage (RING/STOP/LED) + DisplayPayload (name/qty)
// - Hold BUTTON (GPIO16) for 5s -> open WiFiManager AP "Calling Bell"
// - Matrix scrolls "WiFi Setup: Connect to 'Calling Bell'" while portal is open
// - After save/connect -> NTP sync -> disconnect -> back to ESP-NOW ch=1
// - Single polling routine handles short press (STOP) and long hold (portal)

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <cstring>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <SPI.h>
#include <time.h>
#include <WiFiManager.h>   // tzapu/WiFiManager

#include "protocol.h"      // shared: RingerMessage, DisplayPayload, Action

// ===================== CONFIG =========================
const uint8_t ESPNOW_CHANNEL = 1;        // fixed ESP-NOW channel
const long    TZ_OFFSET_SEC  = 6 * 3600; // Dhaka UTC+6
const int     DST_OFFSET_SEC = 0;

// ===================== PINS & HW =====================
#define FLASH_LED_PIN     4
#define BUZZER_PIN        14
#define BUTTON_STOP_PIN   16   // button to GND (INPUT_PULLUP)

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   4
#define CLK_PIN       15
#define DATA_PIN      12
#define CS_PIN        13

MD_Parola matrix(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
const uint8_t MATRIX_DEFAULT_INTENSITY = 5; // 0..15

// ===================== LEDC (PWM) ====================
const int led_freq   = 5000;
const int buz_freq   = 2000;
const int resolution = 8;    // 0..255 duty
const int led_ch     = 0;
const int buz_ch     = 1;

// ===================== TASKS =========================
TaskHandle_t ledFadeTaskHandle = NULL;
TaskHandle_t beepTaskHandle    = NULL;

// ===================== STATE (ringer/LED) ============
volatile bool g_fadeLED     = false;
volatile bool g_triggerBeep = false;
volatile bool g_stopAll     = false;

// ===================== BUTTON / PORTAL ===============
bool          g_inPortal         = false;   // true while portal is open
uint32_t      g_btnDownMs        = 0;       // 0 = not pressed
const uint32_t PORTAL_HOLD_MS    = 5000;    // hold 5s to enter portal
const uint32_t SHORT_PRESS_MS    = 50;      // >50ms counts as press
const uint32_t DEBOUNCE_MS       = 25;

// ===================== UI STATE ======================
enum UIMode { MODE_CLOCK = 0, MODE_MESSAGE = 1, MODE_PORTAL = 2 };
volatile UIMode g_mode = MODE_CLOCK;
String g_messageText;
bool   g_parolaRunning = false;

// Clock blink
int  prevSecond = -1;
bool colonOn    = true;
unsigned long lastClockMs = 0;
const unsigned long CLOCK_UPDATE_MS = 200;

// ===================== NAMES (MATCH A's ORDER) =======
static const int NUM_ITEMS = 6;
// A order: 0:Eshan, 1:Saiful, 2:Altaf, 3:Tea, 4:Coffee, 5:Snacks
const char* ITEM_NAMES[NUM_ITEMS] = {
  "Eshan", "Saiful", "Altaf", "Tea", "Snacks", "Coffee"
};
inline bool isBeverageIdx(int idx) { return idx >= 3; }

// ===================== ESP-NOW STATE =================
bool g_espnowInitialized = false;

// ===================== PROTOTYPES ====================
void onDataReceived(const esp_now_recv_info *info, const uint8_t *incoming, int len);
void onDataReceivedOld(const uint8_t *mac, const uint8_t *incoming, int len);
bool initEspNow();
void deinitEspNowSafely();

// ===================== HELPERS =======================
void ledWrite(int duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0)
  ledcWrite(FLASH_LED_PIN, duty);
#else
  ledcWrite(led_ch, duty);
#endif
}
void buzWrite(int duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0)
  ledcWrite(BUZZER_PIN, duty);
#else
  ledcWrite(buz_ch, duty);
#endif
}
void buzTone(int freq) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0)
  ledcWriteTone(BUZZER_PIN, freq);
#else
  ledcWriteTone(buz_ch, freq);
#endif
}
void buzStop() { buzWrite(0); }

bool delayAbortableMs(int ms) {
  const int slice = 10;
  int elapsed = 0;
  while (elapsed < ms) {
    if (g_stopAll) return false;
    vTaskDelay(pdMS_TO_TICKS(slice));
    elapsed += slice;
  }
  return true;
}

void matrixResetClockPaint() {
  matrix.displaySuspend(false);
  matrix.displayReset();
  matrix.setIntensity(MATRIX_DEFAULT_INTENSITY);
  matrix.setInvert(false);
  matrix.displayClear();
  // force refresh soon
  prevSecond = -1;
  lastClockMs = millis() - CLOCK_UPDATE_MS - 1;
}

void enterClockModeNow() {
  g_mode = MODE_CLOCK;
  g_parolaRunning = false;
  matrixResetClockPaint();
}

void enterPortalModeNow() {
  g_mode = MODE_PORTAL;
  g_parolaRunning = false;
  matrix.displaySuspend(false);
  matrix.displayClear();
  matrix.setIntensity(MATRIX_DEFAULT_INTENSITY);
  matrix.displayText("WiFi Setup: Connect to 'Calling Bell'",
                     PA_LEFT, 50, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  g_parolaRunning = true;
}

void enterMessageMode(const String& text) {
  g_mode = MODE_MESSAGE;
  g_messageText = text;
  g_parolaRunning = false;
}

void stopAllNow(const char* reason = nullptr) {
  g_stopAll     = true;
  g_triggerBeep = false;
  g_fadeLED     = false;
  buzStop();
  ledWrite(0);
  enterClockModeNow();
  if (reason) Serial.printf("STOP (%s)\n", reason);
  else        Serial.println("STOP");
}

// ===================== BEEP ==========================
void playBeepBeepOnce() {
  const int on_ms  = 180;
  const int gap_ms = 120;

  if (g_stopAll) return;
  buzTone(buz_freq); buzWrite(128);
  if (!delayAbortableMs(on_ms)) { buzStop(); return; }

  buzStop();
  if (!delayAbortableMs(gap_ms)) return;

  buzTone(buz_freq); buzWrite(128);
  if (!delayAbortableMs(on_ms)) { buzStop(); return; }

  buzStop();
}

// ===================== TASKS =========================
void ledFadeTask(void* p) {
  int duty = 0; bool up = true;
  for (;;) {
    if (!g_stopAll && g_fadeLED) {
      duty += (up ? 5 : -5);
      if (duty >= 255) { duty = 255; up = false; }
      if (duty <= 0)   { duty = 0;   up = true;  }
      ledWrite(duty);
      vTaskDelay(pdMS_TO_TICKS(20));
    } else {
      ledWrite(0);
      vTaskDelay(pdMS_TO_TICKS(60));
    }
  }
}

void beepTask(void* p) {
  for (;;) {
    if (!g_stopAll && g_triggerBeep) {
      g_triggerBeep = false;
      for (int i = 0; i < 4 && !g_stopAll; ++i) {
        playBeepBeepOnce();
        if (g_stopAll) break;
        if (!delayAbortableMs(1000)) break;
      }
      delayAbortableMs(120);
    } else {
      vTaskDelay(pdMS_TO_TICKS(15));
    }
  }
}

// ===================== ESP-NOW RX ====================
void onDataReceived(const esp_now_recv_info *info, const uint8_t *incoming, int len) {
  if (!g_espnowInitialized || g_inPortal) {
    return; // Ignore if ESP-NOW not properly initialized or portal is active
  }

  const uint8_t* mac = info->src_addr;

  if (len == (int)sizeof(RingerMessage)) {
    RingerMessage m{}; memcpy(&m, incoming, sizeof(m));
    Serial.printf("CMD %02X:%02X:%02X:%02X:%02X:%02X | %s %d\n",
                  mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], m.command, m.value);

    if (!strcmp(m.command, "RING") || !strcmp(m.command, "RING1")
     || !strcmp(m.command, "RING2") || !strcmp(m.command, "RING3")) {
      g_stopAll      = false;
      g_fadeLED      = true;
      g_triggerBeep  = true;
    } else if (!strcmp(m.command, "LED_ON")) {
      g_stopAll = false; g_fadeLED = true;
    } else if (!strcmp(m.command, "LED_OFF")) {
      g_fadeLED = false; ledWrite(0);
    } else if (!strcmp(m.command, "STOP")) {
      stopAllNow("remote");
    } else {
      Serial.println("Unknown command");
    }
    return;
  }

  if (len == (int)sizeof(DisplayPayload)) {
    DisplayPayload p{}; memcpy(&p, incoming, sizeof(p));
    Serial.printf("DISP %02X:%02X:%02X:%02X:%02X:%02X | act=%u idx=%d name=%s qty=%d\n",
                  mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], p.action, p.index, p.name, p.quantity);

    if (p.action == ACT_RESET) {
      enterClockModeNow();
      return;
    }

    // Build message text (index order matches Device A)
    String msg;
    if (p.index >= 0 && p.index < NUM_ITEMS) {
      if (isBeverageIdx(p.index)) {
        int q = (p.quantity > 0) ? p.quantity : 1;
        msg = String(ITEM_NAMES[p.index]) + " x" + String(q);
      } else {
        msg = ITEM_NAMES[p.index];
      }
    } else {
      msg = String(p.name);
    }
    enterMessageMode(msg);
    return;
  }

  // Heuristic ASCII-ish command
  if (len >= (int)sizeof(((RingerMessage*)0)->command) && incoming[0] != 0 && incoming[0] < 0x80) {
    RingerMessage m{}; memcpy(&m, incoming, min((int)sizeof(m), len));
    Serial.println("Heuristic CMD packet");
    if (!strcmp(m.command, "STOP")) stopAllNow("remote");
    else { g_stopAll=false; g_fadeLED=true; g_triggerBeep=true; }
  } else {
    Serial.printf("Unknown packet len=%d\n", len);
  }
}

void onDataReceivedOld(const uint8_t *mac, const uint8_t *incoming, int len) {
  if (!g_espnowInitialized || g_inPortal) {
    return; // Safety check
  }
  esp_now_recv_info shim{}; memcpy((void*)shim.src_addr, mac, 6);
  onDataReceived(&shim, incoming, len);
}

// ===================== ESP-NOW helpers =================
bool initEspNow() {
  if (g_espnowInitialized) {
    Serial.println("ESP-NOW already initialized");
    return true;
  }

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  
  esp_err_t result = esp_now_init();
  if (result != ESP_OK) {
    Serial.printf("ESP-NOW init failed: %d\n", result);
    return false;
  }

  // Try new callback first, fallback to old
  esp_err_t rc = esp_now_register_recv_cb(onDataReceived);
  if (rc != ESP_OK) {
    Serial.println("Fallback to old RX cb");
    rc = esp_now_register_recv_cb((esp_now_recv_cb_t)onDataReceivedOld);
  }
  
  if (rc == ESP_OK) {
    g_espnowInitialized = true;
    Serial.println("ESP-NOW initialized successfully");
    return true;
  } else {
    Serial.printf("ESP-NOW callback registration failed: %d\n", rc);
    esp_now_deinit();
    return false;
  }
}

void deinitEspNowSafely() {
  if (!g_espnowInitialized) {
    return;
  }
  
  Serial.println("Deinitializing ESP-NOW...");
  
  // Set flag first to prevent new callbacks
  g_espnowInitialized = false;
  delay(100); // Allow any pending operations to complete
  
  // Unregister callback
  esp_now_register_recv_cb(nullptr);
  delay(200); // Longer delay to ensure callback is fully unregistered
  
  // Deinitialize ESP-NOW
  esp_err_t result = esp_now_deinit();
  if (result == ESP_OK) {
    Serial.println("ESP-NOW deinitialized successfully");
  } else {
    Serial.printf("ESP-NOW deinit warning: %d\n", result);
  }
  
  delay(100); // Additional delay after deinit
}

// ===================== WIFI MANAGER ===================
void runWiFiManagerPortal() {
  Serial.println("Entering WiFiManager portal...");
  g_inPortal = true;

  // Stop alarms/LEDs but keep display active
  g_stopAll = true; 
  g_triggerBeep = false; 
  g_fadeLED = false; 
  buzStop(); 
  ledWrite(0);

  // DON'T suspend tasks - let them run but in safe mode
  delay(200); // Allow current operations to complete

  // Safely deinitialize ESP-NOW
  deinitEspNowSafely();
  delay(200);

  // Show portal message BEFORE switching WiFi modes
  enterPortalModeNow();
  delay(100); // Let display settle

  // Switch to AP+STA mode for portal
  Serial.println("Switching to AP+STA mode...");
  WiFi.mode(WIFI_AP_STA);
  delay(500); // Longer delay for mode switch

  // Configure WiFiManager with stability improvements
  WiFiManager wm;
  
  // Reduce memory usage and improve stability
  wm.setConfigPortalBlocking(true);
  wm.setTimeout(300); // 5 minutes timeout (longer)
  wm.setDebugOutput(false); // Reduce serial spam that can cause issues
  
  // Minimal callbacks to reduce crash risk
  wm.setAPCallback([](WiFiManager *wmp) {
    Serial.println("[Portal] AP 'Calling Bell' is active");
  });

  // Set custom AP configuration for stability
  wm.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), 
                         IPAddress(192, 168, 4, 1), 
                         IPAddress(255, 255, 255, 0));

  Serial.println("Starting WiFiManager portal...");
  Serial.println("Connect to 'Calling Bell' network and go to 192.168.4.1");
  
  bool connected = false;
  
  // Start portal with error handling
  try {
    connected = wm.startConfigPortal("Calling Bell");
  } catch (...) {
    Serial.println("[Portal] Exception in WiFiManager, continuing cleanup...");
    connected = false;
  }

  if (connected && WiFi.status() == WL_CONNECTED) {
    Serial.println("[Portal] Successfully connected to WiFi!");
    Serial.print("[Portal] IP: "); Serial.println(WiFi.localIP());
    
    // Configure NTP with error handling
    Serial.println("[Portal] Syncing time...");
    configTime(TZ_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
    
    // Wait for NTP sync with shorter timeout to prevent hangs
    time_t t0 = 0; 
    uint32_t tstart = millis();
    while (t0 < 100000 && (millis() - tstart) < 5000) {
      delay(100); 
      t0 = time(nullptr);
      
      // Keep display active during sync
      if (g_mode == MODE_PORTAL && matrix.displayAnimate()) {
        matrix.displayReset();
      }
    }
    
    Serial.printf("[Portal] NTP sync: %s\n", (t0 >= 100000) ? "SUCCESS" : "TIMEOUT");
  } else {
    Serial.println("[Portal] No connection or portal timed out");
  }

  // Graceful cleanup to prevent restart
  Serial.println("[Portal] Starting cleanup sequence...");
  
  // Stop the portal gracefully
  WiFi.softAPdisconnect(true);
  delay(300);
  
  // If connected to station, disconnect gracefully  
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(false, false); // Don't erase credentials, don't clear
    delay(300);
  }
  
  // Turn off WiFi completely and wait
  WiFi.mode(WIFI_OFF);
  delay(1000); // Critical delay to let WiFi stack reset

  Serial.println("[Portal] WiFi off, reinitializing ESP-NOW...");

  // Reinitialize ESP-NOW with retry logic
  int retries = 3;
  while (retries > 0 && !initEspNow()) {
    Serial.printf("[Portal] ESP-NOW init attempt %d failed, retrying...\n", 4-retries);
    delay(500);
    retries--;
  }
  
  if (!g_espnowInitialized) {
    Serial.println("[Portal] ESP-NOW reinit failed completely, restarting in 3 seconds...");
    
    // Show error on display
    matrix.displaySuspend(false);
    matrix.displayClear();
    matrix.setIntensity(MATRIX_DEFAULT_INTENSITY);
    matrix.displayText("ESP-NOW Error - Restarting...", PA_LEFT, 50, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    
    delay(3000);
    ESP.restart();
  }

  // Return to normal operation
  g_inPortal = false;
  g_stopAll = false; // Re-enable normal operation
  enterClockModeNow();
  
  Serial.println("[Portal] Successfully returned to ESP-NOW mode");
  Serial.printf("[Portal] Free heap: %d bytes\n", ESP.getFreeHeap());
}

// ===================== BUTTON POLLING =================
void pollButton() {
  // Always allow button polling, but be smart about what we do
  static uint32_t lastReadMs = 0;
  uint32_t now = millis();
  if (now - lastReadMs < DEBOUNCE_MS) return;
  lastReadMs = now;

  int level = digitalRead(BUTTON_STOP_PIN); // INPUT_PULLUP, active LOW

  if (g_btnDownMs == 0) {
    if (level == LOW) g_btnDownMs = now;     // press start
  } else {
    if (level == LOW) {
      // Only allow portal entry if not already in portal and ESP-NOW is ready
      if (!g_inPortal && g_espnowInitialized && (now - g_btnDownMs >= PORTAL_HOLD_MS)) {
        runWiFiManagerPortal();               // long hold -> portal
        g_btnDownMs = 0;
      }
    } else {
      uint32_t held = now - g_btnDownMs;     // released
      g_btnDownMs = 0;
      // Only allow stop if not in portal
      if (!g_inPortal && held >= SHORT_PRESS_MS && held < PORTAL_HOLD_MS) {
        stopAllNow("button");                 // short press -> STOP
      }
    }
  }
}

// ===================== UI: CLOCK ======================
void showClockIfDue() {
  if (g_mode != MODE_CLOCK) return;

  unsigned long nowMs = millis();
  if (nowMs - lastClockMs < CLOCK_UPDATE_MS) return;
  lastClockMs = nowMs;

  time_t t = time(nullptr);
  if (t == 0) return;
  struct tm* tm_info = localtime(&t);

  if (tm_info->tm_sec != prevSecond) {
    prevSecond = tm_info->tm_sec;
    colonOn = !colonOn;

    int hour = tm_info->tm_hour % 12;
    if (hour == 0) hour = 12;
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d%c%02d",
             hour, (colonOn ? ':' : ' '), tm_info->tm_min);

    matrix.displaySuspend(false);
    matrix.setTextAlignment(PA_CENTER);
    matrix.print(buf);
  }
}

// ===================== UI: MESSAGE ====================
void runMessageMode() {
  if (g_mode != MODE_MESSAGE) return;

  if (!g_parolaRunning) {
    matrix.displaySuspend(false);
    matrix.displayClear();
    matrix.setIntensity(MATRIX_DEFAULT_INTENSITY);
    matrix.displayText(g_messageText.c_str(),
                       PA_LEFT, 50, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    g_parolaRunning = true;
  }
  if (matrix.displayAnimate()) matrix.displayReset();
}

// ===================== SETUP/LOOP ====================
void setup() {
  Serial.begin(115200);
  delay(500); // Increased delay for stability

  pinMode(FLASH_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_STOP_PIN, INPUT_PULLUP);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0)
  ledcAttach(FLASH_LED_PIN, led_freq,  resolution);
  ledcAttach(BUZZER_PIN,     buz_freq, resolution);
#else
  ledcSetup(led_ch, led_freq, resolution);
  ledcAttachPin(FLASH_LED_PIN, led_ch);
  ledcSetup(buz_ch, buz_freq, resolution);
  ledcAttachPin(BUZZER_PIN, buz_ch);
#endif

  matrix.begin();
  matrix.setIntensity(MATRIX_DEFAULT_INTENSITY);
  matrix.setInvert(false);
  matrix.displayClear();

  // Initialize ESP-NOW
  if (!initEspNow()) {
    Serial.println("ESP-NOW initialization failed, restarting...");
    delay(2000);
    ESP.restart();
  }

  // Create tasks
  xTaskCreatePinnedToCore(ledFadeTask, "LED Fade", 2048, NULL, 1, &ledFadeTaskHandle, 0);
  xTaskCreatePinnedToCore(beepTask,    "Beep",     2048, NULL, 2, &beepTaskHandle,    1);

  enterClockModeNow();

  Serial.println("=== Device C ready (Clock; hold button 5s for WiFiManager 'Calling Bell') ===");
  Serial.print("My MAC: "); Serial.println(WiFi.macAddress());
  Serial.print("ESPNOW Channel: "); Serial.println(ESPNOW_CHANNEL);
}

void loop() {
  // Always allow basic operations
  pollButton();

  // Drive UI based on mode
  if (g_mode == MODE_CLOCK && !g_inPortal) {
    showClockIfDue();
  } else if (g_mode == MODE_MESSAGE && !g_inPortal) {
    runMessageMode();
  } else if (g_mode == MODE_PORTAL) {
    // Keep portal message scrolling even during WiFi operations
    if (matrix.displayAnimate()) {
      matrix.displayReset();
    }
  }

  // Shorter delay for more responsive display during portal
  vTaskDelay(pdMS_TO_TICKS(g_inPortal ? 5 : 10));
}