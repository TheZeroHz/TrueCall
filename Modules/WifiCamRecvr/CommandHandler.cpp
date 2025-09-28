// CommandHandler.cpp
#include "CommandHandler.h"

CommandHandler::CommandHandler() {}

String CommandHandler::checkForCommand() {
    if (Serial.available() > 0) {
        String cmd = Serial.readString();
        cmd.trim();
        return cmd;
    }
    return "";
}

void CommandHandler::printHelp() {
    Serial.println("\n╔════════════════════════════════════╗");
    Serial.println("║   SignalMaster Command Reference   ║");
    Serial.println("╚════════════════════════════════════╝");
    Serial.println("\n┌─ Display Modes ─────────────────┐");
    Serial.println("│ watch      Switch to watch face  │");
    Serial.println("│ menu       Open menu system      │");
    Serial.println("│ camera     Open camera view      │");
    Serial.println("└──────────────────────────────────┘");
    Serial.println("\n┌─ Menu Navigation ───────────────┐");
    Serial.println("│ next       Next menu item        │");
    Serial.println("│ prev       Previous menu item    │");
    Serial.println("│ select     Select current item   │");
    Serial.println("└──────────────────────────────────┘");
    Serial.println("\n┌─ System Commands ───────────────┐");
    Serial.println("│ weather    Update weather data   │");
    Serial.println("│ status     Show system status    │");
    Serial.println("│ help       Show this help        │");
    Serial.println("└──────────────────────────────────┘\n");
}