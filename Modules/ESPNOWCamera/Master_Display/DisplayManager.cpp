// DisplayManager.cpp
#include "DisplayManager.h"

DisplayManager* DisplayManager::instance = nullptr;

DisplayManager::DisplayManager() : sprite(&tft) {
  instance = this;
  lastDisplayTime = 0;
  framesDisplayed = 0;
  currentFPS = 0.0;
}

bool DisplayManager::jpegCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (instance && instance->sprite.created()) {
    instance->sprite.pushImage(x, y, w, h, bitmap);
    return true;
  }
  return false;
}

bool DisplayManager::begin() {
  Serial.println("Initializing display...");
  
  tft.init();
  tft.setRotation(3);  // Landscape 320x240
  tft.fillScreen(TFT_BLACK);
  
  // Create sprite buffer
  if (!sprite.createSprite(320, 240)) {
    Serial.println("Failed to create sprite!");
    return false;
  }
  
  sprite.fillSprite(TFT_BLACK);
  
  // Setup JPEG decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(jpegCallback);
  
  Serial.println("Display initialized successfully");
  return true;
}

void DisplayManager::showReady() {
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprite.setTextDatum(MC_DATUM);
  sprite.setTextSize(2);
  sprite.drawString("CAMERA READY", 160, 100);
  sprite.setTextSize(1);
  sprite.drawString("Send commands via Serial", 160, 140);
  sprite.pushSprite(0, 0);
}

void DisplayManager::showWaiting() {
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
  sprite.setTextDatum(MC_DATUM);
  sprite.setTextSize(2);
  sprite.drawString("WAITING...", 160, 120);
  sprite.pushSprite(0, 0);
}

void DisplayManager::showError(const char* message) {
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_RED, TFT_BLACK);
  sprite.setTextDatum(MC_DATUM);
  sprite.setTextSize(1);
  sprite.drawString("ERROR:", 160, 100);
  sprite.drawString(message, 160, 120);
  sprite.pushSprite(0, 0);
  Serial.printf("Display Error: %s\n", message);
}

bool DisplayManager::displayImage(const CompleteImage& img) {
  unsigned long startTime = millis();
  
  sprite.fillSprite(TFT_BLACK);
  
  // Decode and render JPEG
  if (TJpgDec.drawJpg(0, 0, img.imageData, img.imageSize) != JDR_OK) {
    showError("JPEG Decode Failed");
    return false;
  }
  
  // Push to screen
  sprite.pushSprite(0, 0);
  
  framesDisplayed++;
  unsigned long displayTime = millis() - startTime;
  
  // Calculate FPS
  if (millis() - lastDisplayTime > 1000) {
    currentFPS = framesDisplayed / ((millis() - lastDisplayTime) / 1000.0);
    lastDisplayTime = millis();
    framesDisplayed = 0;
  }
  
  Serial.printf("Image displayed: %dx%d, %d bytes, %lu ms\n", 
                img.width, img.height, img.imageSize, displayTime);
  
  return true;
}

void DisplayManager::displayStats(int received, int displayed, float fps) {
  Serial.printf("Stats - Received: %d, Displayed: %d, FPS: %.1f\n", 
                received, displayed, fps);
}