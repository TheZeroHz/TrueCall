// ================= Device B: ESP32-C3 + ST7789 Display (ESP-NOW RX, v2) =================
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <esp_wifi.h>  // for esp_wifi_set_channel

// --------- TFT Pins (ESP32-C3) ----------
#define TFT_CS    2
#define TFT_DC    3
#define TFT_MOSI  7
#define TFT_SCLK  4
#define TFT_RST   8
#define TFT_W     240
#define TFT_H     240

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// --------- ESP-NOW ----------
const uint8_t WIFI_CHANNEL = 1;  // must match the master

// --------- Actions ----------
enum Action : uint8_t {
  ACT_SELECT        = 1,
  ACT_SHORT_PRESS   = 2,
  ACT_RESET         = 3, // will be ignored (per your request)
  ACT_DOUBLE_CLICK  = 4, // new: double click gesture
  ACT_EMERGENCY     = 5, // new: double click + hold
  ACT_QUANTITY_SELECT = 6 // new: quantity selection mode
};

typedef struct {
  uint8_t  action;
  int16_t  index;
  char     name[32];
  int16_t  quantity;  // New field for quantity
} __attribute__((packed)) Payload;

// --------- Item Types ----------
enum ItemType {
  TYPE_PERSON = 0,
  TYPE_BEVERAGE_FOOD = 1
};

ItemType getItemType(int index) {
  // First 3 are people, rest are beverages/food
  return (index < 3) ? TYPE_PERSON : TYPE_BEVERAGE_FOOD;
}

// --------- UI Config ----------
const int HEADER_H            = 40;
const int BORDER_THICKNESS    = 6;    // thicker border
const int BORDER_BLINKS       = 3;    // blink count for normal short-press
const int BORDER_BLINKS_EMERG = 8;    // more blinks for emergency
const int BLINK_DELAY_MS      = 120;

// --------- State ----------
char lastName[32] = "—";
bool haveSelection = false;
bool emergencyActive = false;
bool quantityMode = false;
int currentQuantity = 0;

// --------- Helpers ----------
uint16_t roleColor(const char* name, int index = -1) {
  // Use index if available for more accurate color determination
  if (index >= 0) {
    if (index < 3) {
      // People colors based on role prefix
      if (strncmp(name, "Ps(", 3)   == 0) return ST77XX_GREEN; // Personal Secretary
      if (strncmp(name, "Pa(", 3)   == 0) return ST77XX_BLUE;  // Personal Assistant
      if (strncmp(name, "Pion(", 5) == 0) return ST77XX_RED;   // Pion
    } else {
      // Beverage/Food colors
      if (strstr(name, "Tea") != NULL) return ST77XX_GREEN;
      if (strstr(name, "Coffee") != NULL) return 0x8410;  // Brown-ish
      if (strstr(name, "Snacks") != NULL) return ST77XX_YELLOW;
    }
  }
  
  // Fallback to original logic
  if (strncmp(name, "Ps(", 3)   == 0) return ST77XX_GREEN; // Personal Secretary
  if (strncmp(name, "Pa(", 3)   == 0) return ST77XX_BLUE;  // Personal Assistant
  if (strncmp(name, "Pion(", 5) == 0) return ST77XX_RED;   // Pion
  if (strstr(name, "Tea") != NULL) return ST77XX_GREEN;
  if (strstr(name, "Coffee") != NULL) return 0x8410;  // Brown-ish
  if (strstr(name, "Snacks") != NULL) return ST77XX_YELLOW;
  
  return ST77XX_WHITE;
}

void drawThickBorder(uint16_t color, int thickness) {
  // Draw a thick border by filling rectangles on all four edges
  tft.fillRect(0, 0, TFT_W, thickness, color);                      // top
  tft.fillRect(0, TFT_H - thickness, TFT_W, thickness, color);      // bottom
  tft.fillRect(0, 0, thickness, TFT_H, color);                      // left
  tft.fillRect(TFT_W - thickness, 0, thickness, TFT_H, color);      // right
}

void clearThickBorder(int thickness) {
  drawThickBorder(ST77XX_BLACK, thickness);
}

void blinkThickBorder(uint16_t color, int thickness, int times, int dly) {
  for (int i = 0; i < times; i++) {
    drawThickBorder(color, thickness);
    delay(dly);
    clearThickBorder(thickness);
    delay(dly);
  }
}

void drawHeader(const char* name, int index = -1, bool isQuantity = false) {
  uint16_t c = roleColor(name, index);
  tft.fillRect(0, 0, TFT_W, HEADER_H, c);
  tft.setCursor(8, 10);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  
  if (isQuantity) {
    tft.print("QUANTITY");
  } else {
    tft.print("Selected");
  }
}

void drawName(const char* name, int index = -1, int quantity = 0) {
  tft.fillRect(0, HEADER_H, TFT_W, TFT_H - HEADER_H, ST77XX_BLACK);
  tft.setTextWrap(true);
  tft.setTextColor(ST77XX_WHITE);

  // Extract base name (remove quantity suffix if present)
  String baseName = String(name);
  int xPos = baseName.indexOf(" x");
  if (xPos != -1) {
    baseName = baseName.substring(0, xPos);
  }

  // Draw main name
  tft.setTextSize(3);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(baseName.c_str(), 0, 0, &x1, &y1, &w, &h);
  int x = (TFT_W - (int)w) / 2; 
  if (x < 8) x = 8;
  int y = quantityMode ? 80 : 100;
  tft.setCursor(x, y);
  tft.print(baseName);

  // Draw quantity if in quantity mode
  if (quantityMode && quantity > 0) {
    String qtyText = "x" + String(quantity);
    tft.setTextSize(4);
    tft.getTextBounds(qtyText.c_str(), 0, 0, &x1, &y1, &w, &h);
    x = (TFT_W - (int)w) / 2;
    if (x < 8) x = 8;
    tft.setCursor(x, 130);
    tft.setTextColor(ST77XX_CYAN);
    tft.print(qtyText);
  }

  // Role/Type chip
  uint16_t c = roleColor(name, index);
  tft.fillRoundRect(40, 190, 160, 32, 8, c);
  tft.setCursor(60, 198);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_BLACK);
  
  if (index >= 0 && getItemType(index) == TYPE_BEVERAGE_FOOD) {
    if (quantityMode) {
      tft.print("QTY MODE");
    } else {
      tft.print("F&B");
    }
  } else {
    tft.print("ROLE");
  }
}

void showReady() {
  // NOTE: We keep this for power-on; ACT_RESET will be ignored at runtime.
  quantityMode = false;
  currentQuantity = 0;
  
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRect(0, 0, TFT_W, HEADER_H, ST77XX_WHITE);
  tft.setCursor(8, 10);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.print("READY");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(40, 110);
  tft.print("Waiting...");
}

void showEmergencyOverlay(const char* name, int index = -1) {
  quantityMode = false; // Exit quantity mode during emergency
  
  // Red header + keep the selected name visible + bold pulsing border + EMERGENCY label
  uint16_t rc = ST77XX_RED;
  tft.fillRect(0, 0, TFT_W, HEADER_H, rc);
  tft.setCursor(8, 10);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.print("EMERGENCY");

  // Keep name area (don't wipe selection)
  tft.fillRect(0, HEADER_H, TFT_W, TFT_H - HEADER_H, ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  
  // Extract base name (remove quantity if present)
  String baseName = String(name);
  int xPos = baseName.indexOf(" x");
  if (xPos != -1) {
    baseName = baseName.substring(0, xPos);
  }
  
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(baseName.c_str(), 0, 0, &x1, &y1, &w, &h);
  int x = (TFT_W - (int)w) / 2; if (x < 8) x = 8;
  int y = 100;
  tft.setCursor(x, y);
  tft.print(baseName);

  // EMERGENCY tag pill
  tft.fillRoundRect(20, 190, 200, 32, 8, rc);
  tft.setCursor(45, 198);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_BLACK);
  tft.print("EMERGENCY");

  // Start with one blink cycle (more will be done by caller if desired)
  blinkThickBorder(rc, BORDER_THICKNESS, 1, BLINK_DELAY_MS);
}

// --------- ESP-NOW RX ----------
void onDataRecv(const uint8_t* /*mac*/, const uint8_t* incomingData, int len) {
  if (len != sizeof(Payload)) return;
  Payload p; memcpy(&p, incomingData, sizeof(Payload));

  switch (p.action) {
    case ACT_SELECT: {
      emergencyActive = false;                   // selecting clears emergency mode visually
      quantityMode = false;                      // exit quantity mode when selecting new item
      currentQuantity = 0;
      
      strncpy(lastName, p.name, sizeof(lastName));
      haveSelection = true;
      drawHeader(p.name, p.index);
      drawName(p.name, p.index);
      break;
    }

    case ACT_QUANTITY_SELECT: {
      quantityMode = true;
      currentQuantity = p.quantity;
      strncpy(lastName, p.name, sizeof(lastName));
      haveSelection = true;
      drawHeader(p.name, p.index, true);  // Show "QUANTITY" header
      drawName(p.name, p.index, p.quantity);
      break;
    }

    case ACT_SHORT_PRESS: {
      // Thicker border blink in the role color of current selection if available,
      // otherwise use white.
      const char* ref = haveSelection ? lastName : " ";
      uint16_t c = roleColor(ref, haveSelection ? p.index : -1);
      blinkThickBorder(c, BORDER_THICKNESS, BORDER_BLINKS, BLINK_DELAY_MS);
      
      // If quantity was confirmed, exit quantity mode
      if (quantityMode && p.quantity > 0) {
        quantityMode = false;
        currentQuantity = 0;
        // Update display to show confirmed selection
        drawHeader(p.name, p.index, false);
        drawName(p.name, p.index, 0);
      }
      break;
    }

    case ACT_DOUBLE_CLICK: {
      // Distinct blink pattern (faster double-blink bursts)
      const char* ref = haveSelection ? lastName : " ";
      uint16_t c = roleColor(ref, haveSelection ? p.index : -1);
      blinkThickBorder(c, BORDER_THICKNESS, BORDER_BLINKS + 1, BLINK_DELAY_MS - 30);
      break;
    }

    case ACT_EMERGENCY: {
      emergencyActive = true;
      quantityMode = false; // Exit quantity mode during emergency
      // Show emergency overlay and keep pulsing a few times
      const char* ref = haveSelection ? lastName : p.name;
      strncpy(lastName, ref, sizeof(lastName)); // ensure name stays
      showEmergencyOverlay(ref, p.index);
      blinkThickBorder(ST77XX_RED, BORDER_THICKNESS, BORDER_BLINKS_EMERG, BLINK_DELAY_MS);
      break;
    }

    case ACT_RESET:
      // Per your request: DO NOTHING on hold/long-press (ignore reset).
      // If you ever want a subtle hint, you can add a small border flash here.
      quantityMode = false;
      currentQuantity = 0;
      showReady(); // Reset display
      break;
  }
}

void setupESPNow() {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] init failed!");
    ESP.restart();
  }
  esp_now_register_recv_cb(onDataRecv);
}

void setupTFT() {
  SPI.begin(TFT_SCLK, -1 /*MISO*/, TFT_MOSI, TFT_CS);
  tft.init(240, 240);
  tft.setRotation(0);
  showReady();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.print("Display (ESP32-C3) MAC: ");
  Serial.println(WiFi.macAddress()); // copy this into Device A

  setupTFT();
  setupESPNow();
}

void loop() {
  // event-driven
}