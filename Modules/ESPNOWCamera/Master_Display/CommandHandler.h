// CommandHandler.h
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include "DisplayManager.h"
#include "CommunicationManager.h"

class CommandHandler {
private:
  DisplayManager* display;
  CommunicationManager* comm;
  
  String inputBuffer;
  bool streamingActive;
  unsigned long lastCommandTime;
  
  void executeCommand(const String& cmd);
  void showHelp();
  
public:
  CommandHandler();
  void setDisplayManager(DisplayManager* mgr) { display = mgr; }
  void setCommunicationManager(CommunicationManager* mgr) { comm = mgr; }
  void processSerialCommands();
  void printStatus();
};

#endif