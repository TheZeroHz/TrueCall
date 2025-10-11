// TransmissionManager.h
#ifndef TRANSMISSION_MANAGER_H
#define TRANSMISSION_MANAGER_H

#include <WiFi.h>
#include <esp_now.h>
#include "esp_camera.h"
#include "DataStructures.h"

class TransmissionManager {
private:
  // Master device MAC address - UPDATE THIS!
  uint8_t masterMac[6] = {0xE8, 0x06, 0x90, 0x97, 0x92, 0xC4};
  
  int framesSent;
  int sendFailures;
  unsigned long totalTransmitTime;
  
  static TransmissionManager* instance;
  static void onDataSent(const uint8_t* mac, esp_now_send_status_t status);
  
public:
  TransmissionManager();
  bool begin();
  bool sendFrame(camera_fb_t* fb);
  bool sendTextMessage(const char* message);
  void setMasterMac(uint8_t* mac);
  int getFramesSent() { return framesSent; }
  int getFailures() { return sendFailures; }
  unsigned long getAvgTransmitTime();
  void resetStats();
};

#endif