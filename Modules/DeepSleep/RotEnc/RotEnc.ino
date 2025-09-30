#include <TFT_eSPI.h>
#include "esp_sleep.h"

// ================== Rotary Encoder Pins ==================
#define ENC_CLK 19   // Rotation A (wake source)
#define ENC_DT  20   // Rotation B (for direction, not wake)
#define ENC_SW  21   // Button press (wake source)

// ================== TFT Controller Commands ==================
#define TFT_SLPIN   0x10
#define TFT_SLPOUT  0x11
#define TFT_DISPOFF 0x28
#define TFT_DISPON  0x29

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);

  // --------- Initialize TFT ---------
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);

  // --------- Setup encoder pins ---------
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  // --------- Check wake-up reason ---------
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    tft.println("Woke by Encoder!");
  } else {
    tft.println("Fresh Boot!");
  }

  delay(2000);

  // --------- Enable wake-up source ---------
  // Wake on encoder rotation (CLK) or button press (SW)
  esp_sleep_enable_ext1_wakeup(
    (1ULL << ENC_CLK) | (1ULL << ENC_SW),
    ESP_EXT1_WAKEUP_ANY_LOW
  );

  // --------- Prepare for sleep ---------
  tft.println("Going to sleep...");
  delay(2000);

  // Put TFT into sleep mode
  tft.writecommand(TFT_DISPOFF);
  tft.writecommand(TFT_SLPIN);

  // --------- Enter deep sleep ---------
  esp_deep_sleep_start();
}

void loop() {
  // never reached
}
