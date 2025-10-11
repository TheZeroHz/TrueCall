// CommandHandler.cpp
#include "CommandHandler.h"

CommandHandler::CommandHandler() {
  display = nullptr;
  comm = nullptr;
  streamingActive = false;
  lastCommandTime = 0;
  inputBuffer = "";
}

void CommandHandler::processSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        inputBuffer.trim();
        inputBuffer.toUpperCase();
        executeCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
}

void CommandHandler::executeCommand(const String& cmd) {
  Serial.printf("\n>>> Command: %s\n", cmd.c_str());
  lastCommandTime = millis();
  
  // Check if it's a text message (starts with MSG: or M:)
  if (cmd.startsWith("MSG:") || cmd.startsWith("M:")) {
    String message = cmd;
    if (cmd.startsWith("MSG:")) {
      message = cmd.substring(4);
    } else {
      message = cmd.substring(2);
    }
    message.trim();
    
    if (message.length() > 0) {
      comm->sendTextMessage(message.c_str());
    } else {
      Serial.println("Error: Empty message!");
    }
    return;
  }
  
  if (cmd == "CAPTURE" || cmd == "C") {
    Serial.println("Requesting single frame...");
    
    // Clear any old images from queue before requesting new capture
    if (comm) {
      CompleteImage oldImg;
      while (comm->hasCompleteImage()) {
        oldImg = comm->getCompleteImage();
        comm->freeImage(oldImg);
        Serial.println("[Master] Cleared old image from queue");
      }
    }
    
    if (comm->sendCommand("CAPTURE")) {
      display->showWaiting();
      streamingActive = false;
    }
  }
  else if (cmd == "START_STREAM" || cmd == "START" || cmd == "S") {
    Serial.println("Starting continuous stream (2 FPS)...");
    if (comm->sendCommand("START_STREAM")) {
      streamingActive = true;
      display->showWaiting();
    }
  }
  else if (cmd == "STOP_STREAM" || cmd == "STOP" || cmd == "X") {
    Serial.println("Stopping stream...");
    if (comm->sendCommand("STOP_STREAM")) {
      streamingActive = false;
      display->showReady();
    }
  }
  else if (cmd == "STATUS" || cmd == "?") {
    printStatus();
  }
  else if (cmd == "HELP" || cmd == "H") {
    showHelp();
  }
  else {
    Serial.println("Unknown command! Type HELP for command list.");
  }
  
  Serial.println();
}

void CommandHandler::showHelp() {
  Serial.println("\n=== COMMAND LIST ===");
  Serial.println("CAPTURE (C)       - Request single frame from camera");
  Serial.println("START_STREAM (S)  - Start continuous 2 FPS streaming");
  Serial.println("STOP_STREAM (X)   - Stop streaming");
  Serial.println("STATUS (?)        - Show system status");
  Serial.println("HELP (H)          - Show this help");
  Serial.println("MSG: <text>       - Send text message to slave");
  Serial.println("M: <text>         - Send text message (short form)");
  Serial.println("\nMessage Examples:");
  Serial.println("  MSG: Hello Slave!");
  Serial.println("  M: Temperature check");
  Serial.println("====================\n");
}

void CommandHandler::printStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.printf("Streaming: %s\n", streamingActive ? "ACTIVE" : "STOPPED");
  Serial.printf("Display FPS: %.1f\n", display->getFPS());
  Serial.printf("Frames Displayed: %d\n", display->getFramesDisplayed());
  Serial.printf("Total Received: %d\n", comm->getReceivedCount());
  Serial.printf("Total Lost: %d\n", comm->getLostCount());
  Serial.printf("Free Heap: %d bytes\n", esp_get_free_heap_size());
  Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
  Serial.println("=====================\n");
}