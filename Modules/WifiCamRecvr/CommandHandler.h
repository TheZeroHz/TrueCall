
// CommandHandler.h
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>

class CommandHandler {
public:
    CommandHandler();
    String checkForCommand();
    void printHelp();
};

#endif