// Master_Display.ino
#include "DisplayManager.h"
#include "CommunicationManager.h"
#include "CommandHandler.h"

DisplayManager displayMgr;
CommunicationManager commMgr;
CommandHandler cmdHandler;

void setup() {
  Serial.begin(115200);
  Serial.println("=== Master Display Controller ===");
  
  // Initialize display
  if (!displayMgr.begin()) {
    Serial.println("Display init failed!");
    while(1) delay(1000);
  }
  
  // Initialize ESP-NOW communication
  if (!commMgr.begin()) {
    Serial.println("Communication init failed!");
    while(1) delay(1000);
  }
  
  // Link modules
  cmdHandler.setDisplayManager(&displayMgr);
  cmdHandler.setCommunicationManager(&commMgr);
  
  Serial.println("\n=== Commands Available ===");
  Serial.println("CAPTURE       - Request single frame");
  Serial.println("START_STREAM  - Start continuous streaming (2 FPS)");
  Serial.println("STOP_STREAM   - Stop streaming");
  Serial.println("STATUS        - Show system status");
  Serial.println("MSG: <text>   - Send text message to slave");
  Serial.println("==========================\n");
  
  displayMgr.showReady();
}

void loop() {
  // Process incoming serial commands
  cmdHandler.processSerialCommands();
  
  // Process received image data
  commMgr.processIncomingData();
  
  // Check for complete images and display them
  if (commMgr.hasCompleteImage()) {
    CompleteImage img = commMgr.getCompleteImage();
    displayMgr.displayImage(img);
    commMgr.freeImage(img);
  }
  
  // Periodic status update
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 30000) {
    cmdHandler.printStatus();
    lastStatus = millis();
  }
  
  delay(10);
}