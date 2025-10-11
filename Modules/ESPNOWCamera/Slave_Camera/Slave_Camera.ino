// Slave_Camera.ino
#include "CameraModule.h"
#include "TransmissionManager.h"
#include "CommandProcessor.h"

CameraModule camera;
TransmissionManager transmit;
CommandProcessor cmdProc;

void setup() {
  Serial.begin(115200);
  Serial.println("=== Slave Camera Module ===");
  
  // Turn off LED to reduce power/heat
  pinMode(48, OUTPUT);
  digitalWrite(48, LOW);
  
  // Initialize camera
  if (!camera.begin()) {
    Serial.println("Camera init failed!");
    ESP.restart();
  }
  
  // Initialize transmission
  if (!transmit.begin()) {
    Serial.println("Transmission init failed!");
    ESP.restart();
  }
  
  // Link modules
  cmdProc.setCameraModule(&camera);
  cmdProc.setTransmissionManager(&transmit);
  
  // Initialize command processor (sets up ESP-NOW receive callback)
  if (!cmdProc.begin()) {
    Serial.println("Command processor init failed!");
    ESP.restart();
  }
  
  Serial.println("Slave camera ready!");
  Serial.println("Waiting for commands from master...");
  Serial.println("Type 'HELP' for messaging commands\n");
}

void loop() {
  // Process incoming commands from master
  cmdProc.processCommands();
  
  // Handle streaming if active
  cmdProc.handleStreaming();
  
  // Periodic status
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    cmdProc.printStatus();
    lastStatus = millis();
  }
  
  delay(10);
}