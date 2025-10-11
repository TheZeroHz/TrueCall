// DisplayManager.h
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "DataStructures.h"

class DisplayManager {
private:
  TFT_eSPI tft;
  TFT_eSprite sprite;
  
  unsigned long lastDisplayTime;
  int framesDisplayed;
  float currentFPS;
  
  static DisplayManager* instance;
  static bool jpegCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
  
public:
  DisplayManager();
  bool begin();
  void showReady();
  void showWaiting();
  void showError(const char* message);
  bool displayImage(const CompleteImage& img);
  void displayStats(int received, int displayed, float fps);
  float getFPS() { return currentFPS; }
  int getFramesDisplayed() { return framesDisplayed; }
};

#endif