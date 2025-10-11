// CommandProcessor.cpp
#include "CommandProcessor.h"

CommandProcessor* CommandProcessor::instance = nullptr;

CommandProcessor::CommandProcessor() {
  instance = this;
  camera = nullptr;
  transmit = nullptr;
  streamingMode = false;
  lastStreamTime = 0;
  streamInterval = 500;  // 500ms = 2 FPS
  commandQueue = nullptr;
}

String serialInputBuffer = "";  // For serial text message input

bool CommandProcessor::begin() {
  Serial.println("Initializing command processor...");
  
  // Create command queue
  commandQueue = xQueueCreate(5, sizeof(CommandPacket));
  if (!commandQueue) {
    Serial.println("Failed to create command queue!");
    return false;
  }
  
  // Register ESP-NOW receive callback
  esp_now_register_recv_cb(onCommandReceived);
  
  Serial.println("Command processor ready");
  return true;
}

void CommandProcessor::onCommandReceived(const uint8_t* mac, const uint8_t* data, int len) {
  if (!instance || !instance->commandQueue) return;
  
  if (len == sizeof(CommandPacket)) {
    CommandPacket cmd = *(CommandPacket*)data;
    xQueueSendFromISR(instance->commandQueue, &cmd, NULL);
    Serial.printf("Command received: %s\n", cmd.command);
    return;
  }
  
  // Check for text message
  if (len == sizeof(TextMessagePacket)) {
    TextMessagePacket* msg = (TextMessagePacket*)data;
    if (msg->fromMaster == 1) {  // Message from master
      Serial.printf("\n📨 [Message from Master]: %s\n\n", msg->message);
    }
    return;
  }
}

void CommandProcessor::processCommands() {
  CommandPacket cmd;
  
  // Process ESP-NOW commands
  while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
    executeCommand(cmd);
  }
  
  // Process serial input for text messages
  processSerialInput();
}

void CommandProcessor::processSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (serialInputBuffer.length() > 0) {
        serialInputBuffer.trim();
        
        // Check if it's a message command
        if (serialInputBuffer.startsWith("MSG:") || serialInputBuffer.startsWith("M:")) {
          String message = serialInputBuffer;
          if (serialInputBuffer.startsWith("MSG:")) {
            message = serialInputBuffer.substring(4);
          } else {
            message = serialInputBuffer.substring(2);
          }
          message.trim();
          
          if (message.length() > 0 && transmit) {
            transmit->sendTextMessage(message.c_str());
          } else {
            Serial.println("Error: Empty message or transmit not ready!");
          }
        } else if (serialInputBuffer == "HELP" || serialInputBuffer == "H" || serialInputBuffer == "?") {
          Serial.println("\n=== SLAVE COMMANDS ===");
          Serial.println("MSG: <text>  - Send text message to master");
          Serial.println("M: <text>    - Send text message (short form)");
          Serial.println("HELP or ?    - Show this help");
          Serial.println("\nMessage Examples:");
          Serial.println("  MSG: Camera ready!");
          Serial.println("  M: Temperature: 45C");
          Serial.println("======================\n");
        } else {
          Serial.printf("Unknown command: %s (Type HELP for commands)\n", serialInputBuffer.c_str());
        }
        
        serialInputBuffer = "";
      }
    } else {
      serialInputBuffer += c;
      serialInputBuffer.toUpperCase();  // Make it case-insensitive
    }
  }
}

void CommandProcessor::executeCommand(const CommandPacket& cmd) {
  String command = String(cmd.command);
  command.trim();
  
  Serial.printf("\n>>> Executing: %s\n", command.c_str());
  
  if (command == "CAPTURE") {
    Serial.println("[Slave] Single frame capture requested");
    streamingMode = false;
    
    // Clear any old frames from camera buffer first
    if (camera) {
      Serial.println("[Slave] Clearing old frames from buffer...");
      camera_fb_t* old_fb = esp_camera_fb_get();
      if (old_fb) {
        esp_camera_fb_return(old_fb);
      }
      delay(50); // Small delay to ensure fresh capture
    }
    
    captureAndSend();
  }
  else if (command == "START_STREAM") {
    Serial.println("[Slave] Starting continuous stream at 2 FPS");
    streamingMode = true;
    lastStreamTime = millis();
  }
  else if (command == "STOP_STREAM") {
    Serial.println("[Slave] Stopping stream");
    streamingMode = false;
    
    // Clear any pending frames
    if (camera) {
      camera_fb_t* old_fb = esp_camera_fb_get();
      if (old_fb) {
        esp_camera_fb_return(old_fb);
      }
    }
  }
  else {
    Serial.printf("[Slave] Unknown command: %s\n", command.c_str());
  }
  
  Serial.println();
}

void CommandProcessor::handleStreaming() {
  if (!streamingMode) return;
  
  unsigned long currentTime = millis();
  
  if (currentTime - lastStreamTime >= streamInterval) {
    captureAndSend();
    lastStreamTime = currentTime;
  }
}

void CommandProcessor::captureAndSend() {
  if (!camera || !transmit) {
    Serial.println("[Slave] Camera or transmission not initialized!");
    return;
  }
  
  unsigned long startTime = millis();
  
  Serial.println("[Slave] Capturing fresh frame...");
  
  // Capture frame
  camera_fb_t* fb = camera->captureFrame();
  if (!fb) {
    Serial.println("[Slave] Frame capture failed!");
    return;
  }
  
  Serial.printf("[Slave] Frame captured: %d bytes\n", fb->len);
  
  // Send frame
  bool success = transmit->sendFrame(fb);
  
  // Return frame buffer
  camera->returnFrame(fb);
  
  unsigned long totalTime = millis() - startTime;
  
  if (success) {
    Serial.printf("[Slave] ✓ Frame processed successfully in %lu ms\n\n", totalTime);
  } else {
    Serial.println("[Slave] ✗ Frame transmission failed!\n");
  }
}

void CommandProcessor::printStatus() {
  Serial.println("\n=== SLAVE STATUS ===");
  Serial.printf("Streaming: %s\n", streamingMode ? "ACTIVE (2 FPS)" : "STOPPED");
  Serial.printf("Frames Captured: %d\n", camera ? camera->getFramesCaptured() : 0);
  Serial.printf("Frames Sent: %d\n", transmit ? transmit->getFramesSent() : 0);
  Serial.printf("Send Failures: %d\n", transmit ? transmit->getFailures() : 0);
  
  if (transmit && transmit->getFramesSent() > 0) {
    Serial.printf("Avg Transmit Time: %lu ms\n", transmit->getAvgTransmitTime());
  }
  
  Serial.printf("Free Heap: %d bytes\n", esp_get_free_heap_size());
  Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
  Serial.println("====================\n");
}