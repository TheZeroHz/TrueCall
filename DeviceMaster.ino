// ================= Device A: ESP32 (regular) Rotary Master → ESP-NOW TX =================
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <cstring>
#include "AiEsp32RotaryEncoder.h"

// --------- Rotary Pins (ESP32 regular) ----------
#define ROTARY_ENCODER_A_PIN 27
#define ROTARY_ENCODER_B_PIN 26
#define ROTARY_ENCODER_BUTTON_PIN 25
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4

// --------- ESP-NOW ----------
const uint8_t WIFI_CHANNEL = 1;   // Keep both devices on same channel

// Device B (Display)
uint8_t DISPLAY_PEER_MAC[] = {0x24, 0xEC, 0x4A, 0xE8, 0x6C, 0xB4};
// Device C (Ringer/LED/Matrix)
uint8_t RINGER_PEER_MAC[]  = {0x3C, 0x8A, 0x1F, 0xD4, 0x53, 0xBC};

// --------- Item Types ----------
enum ItemType {
  TYPE_PERSON = 0,
  TYPE_BEVERAGE_FOOD = 1
};

// --------- Items (A's order preserved for B) ----------
const char* ITEMS[] = {
  "Pa(Eshan)",   // 0
  "Ps(Saiful)",  // 1
  "Pion(Altaf)", // 2
  "Tea",         // 3 -> beverage
  "Coffee",      // 4 -> beverage
  "Snacks"       // 5 -> beverage
};
const int TOTAL_ITEMS = sizeof(ITEMS) / sizeof(ITEMS[0]);

// Helper function to determine item type
static inline ItemType getItemType(int idx) { 
  return (idx < 3) ? TYPE_PERSON : TYPE_BEVERAGE_FOOD; 
}

// If your Device C expects Tea, Snacks, Coffee order, map Coffee<->Snacks when sending to C.
static inline int mapIndexForC(int idxA) {
  if (idxA == 4) return 5; // Coffee(A) -> Coffee(C)=5
  if (idxA == 5) return 4; // Snacks(A) -> Snacks(C)=4
  return idxA;
}

// --------- Rotary config ----------
const int   STEPS_PER_ITEM = 3;  // 3 detents per item
const bool  ENABLE_CYCLING = true;
const bool  ENABLE_DEBUG   = true;  // Temporarily enable for debugging

// --------- Button timing ----------
const unsigned long LONG_PRESS_MS          = 600;
const unsigned long DOUBLE_CLICK_TIME_MS   = 300;  // Reduced from 400ms for more responsive double-click
const unsigned long EMERGENCY_HOLD_TIME_MS = 1500;

// --------- Encoder state ----------
AiEsp32RotaryEncoder rotaryEncoder(
  ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN,
  ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

const int MAX_ENCODER_VALUE = (TOTAL_ITEMS * STEPS_PER_ITEM) - 1;

int  lastDisplayedValue = -1;
bool quantityMode       = false;
int  qtyItemIndex       = -1;
int  qtyCurrent         = 1;

unsigned long buttonStart   = 0;
unsigned long lastClickTs   = 0;
bool waitingDouble          = false;
bool doubleDetected         = false;
bool isLongHandled          = false;

// --------- Actions / payloads ----------
enum Action : uint8_t {
  ACT_SELECT = 1,
  ACT_SHORT_PRESS = 2,
  ACT_RESET = 3,
  ACT_DOUBLE_CLICK = 4,
  ACT_EMERGENCY = 5,
  ACT_QUANTITY_SELECT = 6
};

typedef struct {
  uint8_t  action;
  int16_t  index;
  char     name[32];
  int16_t  quantity;
} __attribute__((packed)) Payload;

typedef struct {
  char command[32];  // "RING","STOP","LED_ON","LED_OFF"
  int  value;
} __attribute__((packed)) RingerMessage;

// --------- Helpers ----------
inline int  idxFromEnc(int enc) { return enc / STEPS_PER_ITEM; }
inline bool validIndex(int i)   { return (i >= 0 && i < TOTAL_ITEMS); }

String displayNameFor(int idx, int qty = 0) {
  if (!validIndex(idx)) return "Unknown";
  String s = String(ITEMS[idx]);
  if (getItemType(idx) == TYPE_BEVERAGE_FOOD && qty > 0) s += " x" + String(qty);
  return s;
}

// --------- Send to B (Display) ---------
bool sendToDisplay(uint8_t action, int index, const char* name, int quantity = 0) {
  Payload p{};
  p.action   = action;
  p.index    = index;      // B uses A's indexing/order
  p.quantity = quantity;
  strlcpy(p.name, name, sizeof(p.name));

  esp_err_t r = esp_now_send(DISPLAY_PEER_MAC, (uint8_t*)&p, sizeof(p));
  if (r != ESP_OK) {
    Serial.printf("[A→B] send err=%d\n", r);
    return false;
  }
  return true;
}

// NEW: robust preview helper so B updates even if it only listens to ACT_SELECT
void sendQuantityPreviewToB(int index, int quantity) {
  String label = displayNameFor(index, quantity);
  // 1) Preferred: QUANTITY_SELECT (if B supports it)
  sendToDisplay(ACT_QUANTITY_SELECT, index, label.c_str(), quantity);
  // 2) Compatibility: also send as SELECT so B certainly updates its live label
  sendToDisplay(ACT_SELECT, index, label.c_str(), quantity);
}

// --------- Send to C (mapped index if needed) ---------
bool sendDisplayToC(uint8_t action, int index, const char* name, int quantity = 0) {
  Payload p{};
  p.action   = action;
  p.index    = mapIndexForC(index);
  p.quantity = quantity;
  strlcpy(p.name, name, sizeof(p.name));
  esp_err_t r = esp_now_send(RINGER_PEER_MAC, (uint8_t*)&p, sizeof(p));
  if (r != ESP_OK) {
    Serial.printf("[A→C display] send err=%d\n", r);
    return false;
  }
  return true;
}

// --------- Simple commands to C (ringer/LED) ---------
bool sendToRinger(const char* cmd, int value = 0) {
  RingerMessage m{};
  strlcpy(m.command, cmd, sizeof(m.command));
  m.value = value;
  esp_err_t r = esp_now_send(RINGER_PEER_MAC, (uint8_t*)&m, sizeof(m));
  if (r != ESP_OK) {
    Serial.printf("[A→C cmd] send err=%d\n", r);
    return false;
  }
  if (ENABLE_DEBUG) Serial.printf("[A→C cmd] %s (%d)\n", cmd, value);
  return true;
}
void c_ring()     { sendToRinger("RING");    sendToRinger("LED_ON");  }
void c_stopAll()  { sendToRinger("STOP");    sendToRinger("LED_OFF"); }

// --------- ESPNOW callback ----------
void onDataSent(const uint8_t*, esp_now_send_status_t status) {
  if (ENABLE_DEBUG) {
    Serial.printf("[ESP-NOW] Delivery: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
  }
}

void IRAM_ATTR readEncoderISR() { rotaryEncoder.readEncoder_ISR(); }

// --------- Quantity Mode ----------
void exitQuantityMode() {
  quantityMode = false;
  qtyItemIndex = -1;
  qtyCurrent   = 1;
  rotaryEncoder.setBoundaries(0, MAX_ENCODER_VALUE, ENABLE_CYCLING);
  Serial.println("Exited quantity selection mode");
}

void enterQuantityMode(int itemIndex) {
  quantityMode  = true;
  qtyItemIndex  = itemIndex;
  qtyCurrent    = 1;
  rotaryEncoder.setBoundaries(1, 10, true);
  rotaryEncoder.reset(qtyCurrent);

  Serial.printf("Entered quantity mode for: %s\n", displayNameFor(itemIndex, qtyCurrent).c_str());

  // LIVE preview to B (send both QUANTITY_SELECT + SELECT for compatibility)
  sendQuantityPreviewToB(itemIndex, qtyCurrent);

  // optional cue on C
  c_ring();
  
  // Clear any pending button state
  waitingDouble = false;
  doubleDetected = false;
  buttonStart = 0;
  isLongHandled = false;
}

// --------- Actions ----------
void handleLongPress() {
  Serial.println(F("=== RESET ==="));

  if (quantityMode) exitQuantityMode();

  // Tell C to fall back to clock + stop sounds
  sendDisplayToC(ACT_RESET, 0, ITEMS[0], 0);
  c_stopAll();

  // Also tell B to reset its UI (so it exits any message/preview state)
  sendToDisplay(ACT_RESET, 0, ITEMS[0], 0);

  rotaryEncoder.reset(0);
  lastDisplayedValue = -1;
  Serial.printf("Selected: %s\n", ITEMS[0]);
}

void handleEmergency(int itemIndex) {
  const char* name = validIndex(itemIndex) ? ITEMS[itemIndex] : "Unknown";
  Serial.printf("[EMERGENCY] %s\n", name);
  sendDisplayToC(ACT_EMERGENCY, itemIndex, name, 0);
  c_ring();
}

void rotary_onButtonClick() {
  static unsigned long lastTimePressed = 0;
  if (millis() - lastTimePressed < 200) return; // debounce
  lastTimePressed = millis();

  if (quantityMode) {
    // Confirm beverage quantity → BOTH B and C
    String s = displayNameFor(qtyItemIndex, qtyCurrent);
    Serial.printf("Confirmed: %s\n", s.c_str());

    sendToDisplay(ACT_SHORT_PRESS,  qtyItemIndex, s.c_str(), qtyCurrent);
    sendDisplayToC(ACT_SHORT_PRESS, qtyItemIndex, s.c_str(), qtyCurrent);

    c_ring();
    exitQuantityMode();
    return;
  }

  // Double-click detection window
  unsigned long now = millis();
  if (waitingDouble && (now - lastClickTs) < DOUBLE_CLICK_TIME_MS) {
    // Double click
    doubleDetected   = true;
    waitingDouble    = false;

    int enc = rotaryEncoder.readEncoder();
    int idx = idxFromEnc(enc);
    const char* nm = validIndex(idx) ? ITEMS[idx] : "Unknown";

    Serial.printf("Double click on %s (idx=%d, type=%s)\n",
                  nm, idx, (getItemType(idx) == TYPE_BEVERAGE_FOOD) ? "BEV" : "PERSON");

    if (getItemType(idx) == TYPE_BEVERAGE_FOOD) {
      // Enter quantity mode (B already gets live previews inside enterQuantityMode)
      enterQuantityMode(idx);
    } else {
      // Attention/action for people → send to BOTH
      sendToDisplay(ACT_DOUBLE_CLICK,  idx, nm, 0);
      sendDisplayToC(ACT_DOUBLE_CLICK, idx, nm, 0);
      c_ring();
    }
    return;
  }

  // First click → start waiting for possible second click
  lastClickTs   = now;
  waitingDouble = true;
  Serial.println("First click detected, waiting for potential double click...");
}



void handleSingleClick() {
  int enc = rotaryEncoder.readEncoder();
  int idx = idxFromEnc(enc);
  const char* nm = validIndex(idx) ? ITEMS[idx] : "Unknown";

  Serial.printf("[%s] Single Click (index: %d)\n", nm, idx);

  // Send to BOTH: B (so it shows the committed selection) and C (to ring/LED/etc)
  sendToDisplay(ACT_SHORT_PRESS, idx, nm, 0);
  sendDisplayToC(ACT_SHORT_PRESS, idx, nm, 0);

  c_ring();
}

// --------- Main rotary loop (LIVE to B; actions to C) ----------
void rotary_loop() {
  if (rotaryEncoder.encoderChanged()) {
    int enc = rotaryEncoder.readEncoder();

    if (quantityMode) {
      // LIVE quantity preview to B
      qtyCurrent = enc;
      Serial.printf("Quantity (live→B): %s\n", displayNameFor(qtyItemIndex, qtyCurrent).c_str());
      sendQuantityPreviewToB(qtyItemIndex, qtyCurrent);
      lastDisplayedValue = enc;
    } else {
      // LIVE selection to B (only when item actually changes)
      int curIdx  = idxFromEnc(enc);
      int lastIdx = (lastDisplayedValue < 0) ? -1 : idxFromEnc(lastDisplayedValue);
      if (enc != lastDisplayedValue && curIdx != lastIdx) {
        String s = displayNameFor(curIdx);
        Serial.printf("Selected (live→B): %s\n", s.c_str());
        if (ENABLE_DEBUG) {
          Serial.printf("  [enc=%d, idx=%d]\n", enc, curIdx);
        }
        sendToDisplay(ACT_SELECT, curIdx, s.c_str());
      }
      lastDisplayedValue = enc;
    }
  }

  // Button timings
  if (rotaryEncoder.isEncoderButtonDown()) {
    if (buttonStart == 0) {
      buttonStart = millis();
      isLongHandled = false;
    } else if (!isLongHandled) {
      unsigned long held = millis() - buttonStart;
      if (doubleDetected && held >= EMERGENCY_HOLD_TIME_MS) {
        int enc = rotaryEncoder.readEncoder();
        int idx = quantityMode ? qtyItemIndex : idxFromEnc(enc);
        handleEmergency(idx);
        isLongHandled = true;
      } else if (!doubleDetected && held >= LONG_PRESS_MS) {
        handleLongPress();
        isLongHandled = true;
      }
    }
  } else if (buttonStart > 0) {
    if (!isLongHandled) {
      rotary_onButtonClick();
    }
    buttonStart    = 0;
    isLongHandled  = false;
    doubleDetected = false;
  }

  // Single vs double resolve
  if (waitingDouble && (millis() - lastClickTs) > DOUBLE_CLICK_TIME_MS) {
    waitingDouble = false;
    if (!doubleDetected) {
      Serial.println("Double click timeout - executing single click");
      handleSingleClick();
    }
  }
}

// --------- ESP-NOW setup ----------
void setupESPNow() {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] init failed");
    ESP.restart();
  }
  esp_now_register_send_cb(onDataSent);

  // Peer B
  esp_now_peer_info_t peerB{};
  memcpy(peerB.peer_addr, DISPLAY_PEER_MAC, 6);
  peerB.channel = WIFI_CHANNEL;
  peerB.encrypt = false;
  if (esp_now_add_peer(&peerB) != ESP_OK) {
    Serial.println("[ESP-NOW] add peer B failed");
  }

  // Peer C
  esp_now_peer_info_t peerC{};
  memcpy(peerC.peer_addr, RINGER_PEER_MAC, 6);
  peerC.channel = WIFI_CHANNEL;
  peerC.encrypt = false;
  if (esp_now_add_peer(&peerC) != ESP_OK) {
    Serial.println("[ESP-NOW] add peer C failed");
  }
}

void printSystemInfo() {
  Serial.println(F("=== Device A: Rotary Master → Display (B) + Ringer (C) ==="));
  Serial.print(F("My MAC: ")); Serial.println(WiFi.macAddress());
  Serial.print(F("Channel: ")); Serial.println(WIFI_CHANNEL);
  Serial.print(F("Items: ")); Serial.println(TOTAL_ITEMS);
  Serial.println(F("Controls:"));
  Serial.println(F("  Rotate → LIVE to B (ACT_SELECT / ACT_QUANTITY_SELECT)"));
  Serial.println(F("  Single Click → C action (beep/LED)"));
  Serial.println(F("  Double Click → enter quantity (beverages) / attention for person"));
  Serial.println(F("  Long Press → Reset (C: clock + stop)"));
  Serial.println(F("  Double Click + Hold → Emergency → C"));
}

void setup() {
  Serial.begin(115200);
  delay(200);

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(0, MAX_ENCODER_VALUE, ENABLE_CYCLING);
  rotaryEncoder.setAcceleration(2);

  setupESPNow();
  printSystemInfo();

  lastDisplayedValue = 0;
  Serial.printf("Selected: %s\n", ITEMS[0]);

  // Optional: initial hint to B
  sendToDisplay(ACT_SELECT, 0, ITEMS[0], 0);
}

void loop() {
  rotary_loop();
  delay(10);
}