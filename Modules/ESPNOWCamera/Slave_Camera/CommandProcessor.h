// CommandProcessor.h
#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <Arduino.h>
#include <esp_now.h>
#include "CameraModule.h"
#include "TransmissionManager.h"
#include "DataStructures.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class CommandProcessor {
private:
  CameraModule* camera;
  TransmissionManager* transmit;
  
  QueueHandle_t commandQueue;
  
  bool streamingMode;
  unsigned long lastStreamTime;
  unsigned long streamInterval;  // 500ms = 2 FPS
  
  static CommandProcessor* instance;
  static void onCommandReceived(const uint8_t* mac, const uint8_t* data, int len);
  
  void executeCommand(const CommandPacket& cmd);
  void captureAndSend();
  void processSerialInput();
  
public:
  CommandProcessor();
  void setCameraModule(CameraModule* cam) { camera = cam; }
  void setTransmissionManager(TransmissionManager* trans) { transmit = trans; }
  bool begin();
  void processCommands();
  void handleStreaming();
  void printStatus();
  bool isStreaming() { return streamingMode; }
};

#endif