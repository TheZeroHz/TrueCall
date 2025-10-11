// CameraModule.h
#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include "esp_camera.h"
#include <Arduino.h>

class CameraModule {
private:
  // Freenove ESP32-S3 camera pins
  static constexpr int PWDN_GPIO_NUM = -1;
  static constexpr int RESET_GPIO_NUM = -1;
  static constexpr int XCLK_GPIO_NUM = 15;
  static constexpr int SIOD_GPIO_NUM = 4;
  static constexpr int SIOC_GPIO_NUM = 5;
  static constexpr int Y9_GPIO_NUM = 16;
  static constexpr int Y8_GPIO_NUM = 17;
  static constexpr int Y7_GPIO_NUM = 18;
  static constexpr int Y6_GPIO_NUM = 12;
  static constexpr int Y5_GPIO_NUM = 10;
  static constexpr int Y4_GPIO_NUM = 8;
  static constexpr int Y3_GPIO_NUM = 9;
  static constexpr int Y2_GPIO_NUM = 11;
  static constexpr int VSYNC_GPIO_NUM = 6;
  static constexpr int HREF_GPIO_NUM = 7;
  static constexpr int PCLK_GPIO_NUM = 13;
  
  int framesCaptured;
  unsigned long lastCaptureTime;
  
public:
  CameraModule();
  bool begin();
  camera_fb_t* captureFrame();
  void returnFrame(camera_fb_t* fb);
  int getFramesCaptured() { return framesCaptured; }
  void resetStats();
};

#endif