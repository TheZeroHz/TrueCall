// ================= Device A: ESP32 (regular) Rotary Master → ESP-NOW TX =================
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "AiEsp32RotaryEncoder.h"
#include <esp_wifi.h>

// --------- Rotary Pins (ESP32 regular) ----------
#define ROTARY_ENCODER_A_PIN 27
#define ROTARY_ENCODER_B_PIN 26
#define ROTARY_ENCODER_BUTTON_PIN 25
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4

// --------- ESP-NOW ----------
const uint8_t WIFI_CHANNEL = 1;   // Keep both devices on same channel
uint8_t DISPLAY_PEER_MAC[] = {0x24, 0xEC, 0x4A, 0xE8, 0x6C, 0xB4}; // <--- REPLACE (from Device B)

// --------- People / Config ----------
const char* PEOPLE[] = {
  "Pa(Eshan)",
  "Ps(Saiful)",
  "Pion(Altaf)",
  "Tea",
  "Coffee",
  "Snacks"
};

// --------- New: Item Types ----------
enum ItemType {
  TYPE_PERSON = 0,
  TYPE_BEVERAGE_FOOD = 1
};

ItemType getItemType(int personIndex) {
  // First 3 are people, rest are beverages/food
  return (personIndex < 3) ? TYPE_PERSON : TYPE_BEVERAGE_FOOD;
}

const int STEPS_PER_PERSON = 3;
const unsigned long LONG_PRESS_MS = 600;
const unsigned long DOUBLE_CLICK_TIME_MS = 400;  // Time window for double click
const unsigned long EMERGENCY_HOLD_TIME_MS = 1500; // Hold time after double click for emergency
const bool ENABLE_CYCLING = true;
const bool ENABLE_DEBUG = false;

// --------- Auto ----------
const int TOTAL_PEOPLE = sizeof(PEOPLE) / sizeof(PEOPLE[0]);
const int MAX_ENCODER_VALUE = (TOTAL_PEOPLE * STEPS_PER_PERSON) - 1;

AiEsp32RotaryEncoder rotaryEncoder(
  ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN,
  ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

int lastDisplayedValue = -1;
unsigned long buttonPressStart = 0;
unsigned long lastClickTime = 0;
bool isLongPressHandled = false;
bool waitingForDoubleClick = false;
bool doubleClickDetected = false;

// --------- Quantity Selection State ----------
bool quantityMode = false;
int selectedItemIndex = -1;
int currentQuantity = 1;
const int MIN_QUANTITY = 1;
const int MAX_QUANTITY = 10;

// --------- Payload ----------
enum Action : uint8_t { 
  ACT_SELECT = 1, 
  ACT_SHORT_PRESS = 2, 
  ACT_RESET = 3, 
  ACT_DOUBLE_CLICK = 4, 
  ACT_EMERGENCY = 5,
  ACT_QUANTITY_SELECT = 6  // New action for quantity selection
};

typedef struct {
  uint8_t  action;
  int16_t  index;
  char     name[32];
  int16_t  quantity;  // New field for quantity
} __attribute__((packed)) Payload;

int getPersonIndex(int encoderValue) { return encoderValue / STEPS_PER_PERSON; }

const char* getPersonName(int encoderValue) {
  int idx = getPersonIndex(encoderValue);
  return (idx >= 0 && idx < TOTAL_PEOPLE) ? PEOPLE[idx] : "Unknown";
}

String getDisplayName(int personIndex, int quantity = 0) {
  if (personIndex < 0 || personIndex >= TOTAL_PEOPLE) return "Unknown";
  
  String name = String(PEOPLE[personIndex]);
  if (getItemType(personIndex) == TYPE_BEVERAGE_FOOD && quantity > 0) {
    name += " x" + String(quantity);
  }
  return name;
}

bool sendPayload(uint8_t action, int index, const char* name, int quantity = 0) {
  Payload p{};
  p.action = action;
  p.index = index;
  p.quantity = quantity;
  strlcpy(p.name, name, sizeof(p.name));
  
  esp_err_t r = esp_now_send(DISPLAY_PEER_MAC, (uint8_t*)&p, sizeof(p));
  if (r != ESP_OK) {
    Serial.printf("[ESP-NOW] send err=%d\n", r);
    return false;
  }
  return true;
}

void onDataSent(const uint8_t*, esp_now_send_status_t status) {
  if (ENABLE_DEBUG) {
    Serial.printf("[ESP-NOW] Delivery: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
  }
}

void IRAM_ATTR readEncoderISR() { 
  rotaryEncoder.readEncoder_ISR(); 
}

void exitQuantityMode() {
  quantityMode = false;
  selectedItemIndex = -1;
  currentQuantity = 1;
  
  // Restore normal encoder bounds
  rotaryEncoder.setBoundaries(0, MAX_ENCODER_VALUE, ENABLE_CYCLING);
  
  Serial.println("Exited quantity selection mode");
}

void enterQuantityMode(int itemIndex) {
  quantityMode = true;
  selectedItemIndex = itemIndex;
  currentQuantity = 1;
  
  // Set encoder bounds for quantity selection
  rotaryEncoder.setBoundaries(MIN_QUANTITY, MAX_QUANTITY, true);
  rotaryEncoder.reset(currentQuantity);
  
  String displayName = getDisplayName(itemIndex, currentQuantity);
  Serial.printf("Entered quantity mode for: %s\n", displayName.c_str());
  sendPayload(ACT_QUANTITY_SELECT, itemIndex, displayName.c_str(), currentQuantity);
}

void handleLongPress() {
  Serial.println(F("=== RESET ==="));
  
  if (quantityMode) {
    exitQuantityMode();
  }
  
  rotaryEncoder.reset(0);
  lastDisplayedValue = -1;
  Serial.printf("Selected: %s\n", PEOPLE[0]);
  sendPayload(ACT_RESET, 0, PEOPLE[0]);
}

void handleEmergency(int personIndex) {
  const char* name = PEOPLE[personIndex];
  Serial.printf("[EMERGENCY] %s\n", name);
  sendPayload(ACT_EMERGENCY, personIndex, name);
}

void rotary_onButtonClick() {
  static unsigned long lastTimePressed = 0;
  if (millis() - lastTimePressed < 200) return; // debounce
  lastTimePressed = millis();

  if (quantityMode) {
    // In quantity mode, single click confirms the quantity
    String displayName = getDisplayName(selectedItemIndex, currentQuantity);
    Serial.printf("Confirmed: %s\n", displayName.c_str());
    sendPayload(ACT_SHORT_PRESS, selectedItemIndex, displayName.c_str(), currentQuantity);
    exitQuantityMode();
    return;
  }

  // Normal mode - check for double click
  unsigned long now = millis();
  if (waitingForDoubleClick && (now - lastClickTime) < DOUBLE_CLICK_TIME_MS) {
    // Double click detected
    doubleClickDetected = true;
    waitingForDoubleClick = false;
    
    int val = rotaryEncoder.readEncoder();
    int personIndex = getPersonIndex(val);
    const char* name = getPersonName(val);
    
    // Check if it's a beverage/food item
    if (getItemType(personIndex) == TYPE_BEVERAGE_FOOD) {
      // Enter quantity selection mode
      enterQuantityMode(personIndex);
    } else {
      // For people, double click is just a different action
      Serial.printf("[%s] Double Click\n", name);
      sendPayload(ACT_DOUBLE_CLICK, personIndex, name);
    }
    return;
  }
  
  // First click or not within double click time
  lastClickTime = now;
  waitingForDoubleClick = true;
  
  // Set a timer to handle single click if no double click comes
  // This will be handled in the main loop
}

void handleSingleClick() {
  int val = rotaryEncoder.readEncoder();
  const char* name = getPersonName(val);
  int personIndex = getPersonIndex(val);
  
  Serial.printf("[%s] Single Click\n", name);
  sendPayload(ACT_SHORT_PRESS, personIndex, name);
}

void rotary_loop() {
  // Handle encoder changes
  if (rotaryEncoder.encoderChanged()) {
    int val = rotaryEncoder.readEncoder();
    
    if (quantityMode) {
      // In quantity mode, encoder changes quantity
      currentQuantity = val;
      String displayName = getDisplayName(selectedItemIndex, currentQuantity);
      Serial.printf("Quantity: %s\n", displayName.c_str());
      sendPayload(ACT_QUANTITY_SELECT, selectedItemIndex, displayName.c_str(), currentQuantity);
      lastDisplayedValue = val;
    } else {
      // Normal mode - selecting items
      int currentPerson = getPersonIndex(val);
      int lastPerson = getPersonIndex(lastDisplayedValue);

      if (val != lastDisplayedValue && currentPerson != lastPerson) {
        String displayName = getDisplayName(currentPerson);
        Serial.printf("Selected: %s\n", displayName.c_str());
        
        if (ENABLE_DEBUG) {
          Serial.printf("  [enc=%d, person=%d]\n", val, currentPerson);
        }
        sendPayload(ACT_SELECT, currentPerson, displayName.c_str());
      }
      lastDisplayedValue = val;
    }
  }

  // Handle button press logic
  if (rotaryEncoder.isEncoderButtonDown()) {
    if (buttonPressStart == 0) {
      buttonPressStart = millis();
      isLongPressHandled = false;
    } else if (!isLongPressHandled) {
      unsigned long pressTime = millis() - buttonPressStart;
      
      if (doubleClickDetected && pressTime >= EMERGENCY_HOLD_TIME_MS) {
        // Emergency: double click + hold
        int val = quantityMode ? selectedItemIndex : getPersonIndex(rotaryEncoder.readEncoder());
        handleEmergency(val);
        isLongPressHandled = true;
      } else if (!doubleClickDetected && pressTime >= LONG_PRESS_MS) {
        // Long press (reset)
        handleLongPress();
        isLongPressHandled = true;
      }
    }
  } else if (buttonPressStart > 0) {
    // Button released
    if (!isLongPressHandled) {
      rotary_onButtonClick();
    }
    buttonPressStart = 0;
    isLongPressHandled = false;
    doubleClickDetected = false;
  }

  // Handle single click timeout
  if (waitingForDoubleClick && (millis() - lastClickTime) > DOUBLE_CLICK_TIME_MS) {
    waitingForDoubleClick = false;
    if (!doubleClickDetected) {
      handleSingleClick();
    }
  }
}

void setupESPNow() {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] init failed");
    ESP.restart();
  }
  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, DISPLAY_PEER_MAC, 6);
  peer.channel = WIFI_CHANNEL;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("[ESP-NOW] add peer failed");
  }
}

void printSystemInfo() {
  Serial.println(F("=== Rotary Master (ESP32) → Display (ESP32-C3) ==="));
  Serial.print(F("My MAC: ")); Serial.println(WiFi.macAddress());
  Serial.print(F("Channel: ")); Serial.println(WIFI_CHANNEL);
  Serial.print(F("Items: ")); Serial.println(TOTAL_PEOPLE);
  Serial.println(F("Usage:"));
  Serial.println(F("  Rotate: Navigate items"));
  Serial.println(F("  Single Click: Select/Confirm"));
  Serial.println(F("  Double Click: Enter quantity mode (Tea/Coffee/Snacks)"));
  Serial.println(F("  Long Press: Reset"));
  Serial.println(F("  Double Click + Hold: Emergency"));
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

  Serial.printf("Selected: %s\n", PEOPLE[0]);
  lastDisplayedValue = 0;
  sendPayload(ACT_SELECT, 0, PEOPLE[0]); // initial sync
}

void loop() {
  rotary_loop();
  delay(10);
}