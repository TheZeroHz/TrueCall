// CommunicationManager.h
#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <WiFi.h>
#include <esp_now.h>
#include "DataStructures.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

class CommunicationManager {
private:
  // Slave device MAC address - UPDATE THIS!
  uint8_t slaveMac[6] = {0x3C, 0x84, 0x27, 0xC0, 0x2B, 0x90};
  
  // Image reception state
  uint8_t* imageBuffer;
  uint32_t expectedImageSize;
  uint16_t expectedTotalPackets;
  bool* packetReceived;
  uint32_t packetsReceivedCount;
  bool receivingImage;
  ImageHeader currentHeader;
  unsigned long lastPacketTime;
  
  // RTOS components
  QueueHandle_t imageQueue;
  QueueHandle_t packetQueue;
  SemaphoreHandle_t rxMutex;
  TaskHandle_t packetTaskHandle;
  
  // Statistics
  int totalReceived;
  int totalLost;
  
  static CommunicationManager* instance;
  static void onDataReceived(const uint8_t *mac, const uint8_t *data, int len);
  static void packetProcessingTask(void* param);
  
  void resetReception();
  void processPacket(const ImagePacket& packet);
  
public:
  CommunicationManager();
  bool begin();
  bool sendCommand(const char* command);
  bool sendTextMessage(const char* message);
  void processIncomingData();
  bool hasCompleteImage();
  CompleteImage getCompleteImage();
  void freeImage(CompleteImage& img);
  void setSlaveMac(uint8_t* mac);
  int getReceivedCount() { return totalReceived; }
  int getLostCount() { return totalLost; }
};

#endif