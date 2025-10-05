// BootScreen.h - Linux-style boot sequence
#ifndef BOOTSCREEN_H
#define BOOTSCREEN_H

#include <TFT_eSPI.h>
#include <FFat.h>
#include "Colors.h"

class BootScreen {
private:
    TFT_eSPI* tft;
    int currentY;
    
    void printLine(const char* msg, bool ok = true, int delayMs = 100) {
        tft->setTextColor(TFT_GREEN, TFT_BLACK);
        tft->print("[");
        
        if (ok) {
            tft->setTextColor(TFT_GREEN, TFT_BLACK);
            tft->print("OK");
        } else {
            tft->setTextColor(TFT_YELLOW, TFT_BLACK);
            tft->print("WARN");
        }
        tft->print("] ");
        tft->setTextColor(TFT_WHITE, TFT_BLACK);
        tft->println(msg);
        
        delay(delayMs);
    }
    
    void printHeader(const char* msg) {
        tft->setTextColor(TFT_CYAN, TFT_BLACK);
        tft->println(msg);
        delay(50);
    }
    
    void printInfo(const char* msg) {
        tft->setTextColor(TFT_WHITE, TFT_BLACK);
        tft->print("      ");
        tft->println(msg);
        delay(30);
    }
    
public:
    BootScreen(TFT_eSPI* display) : tft(display), currentY(0) {}
    
    void show() {
        tft->fillScreen(TFT_BLACK);
        tft->setTextSize(1);
        tft->setCursor(0, 0);
        tft->setTextDatum(TL_DATUM);
        
        // Boot header
        tft->setTextColor(TFT_CYAN, TFT_BLACK);
        tft->println("ESP32 Smart Watch v1.0");
        tft->setTextColor(TFT_WHITE, TFT_BLACK);
        tft->println("Booting system...");
        tft->println("");
        delay(300);
        
        // System checks
        printHeader("System initialization:");
        printLine("Initializing CPU cores", true, 80);
        printInfo("Core 0: 240 MHz");
        printInfo("Core 1: 240 MHz");
        
        printLine("Checking memory", true, 80);
        printInfo("PSRAM: 8 MB");
        printInfo("Flash: 16 MB");
        printInfo("Free heap: 320 KB");
        
        printLine("Initializing peripherals", true, 80);
        printInfo("I2C bus @ 100kHz");
        printInfo("SPI bus @ 40MHz");
        printInfo("I2S audio @ 16kHz");
        
        // Filesystem
        printHeader("Filesystem check:");
        printLine("Mounting FFat partition", true, 100);
        
        if (FFat.begin(true, "/ffat", 2)) {
            uint64_t totalBytes = FFat.totalBytes();
            uint64_t usedBytes = FFat.usedBytes();
            char buf[64];
            sprintf(buf, "Total: %llu KB", totalBytes / 1024);
            printInfo(buf);
            sprintf(buf, "Used: %llu KB", usedBytes / 1024);
            printInfo(buf);
            sprintf(buf, "Free: %llu KB", (totalBytes - usedBytes) / 1024);
            printInfo(buf);
            
            // Check directories
            printLine("Checking directory structure", true, 80);
            printInfo("/wallpapers");
            printInfo("/icons");
            printInfo("/sounds");
            printInfo("/team");
            printInfo("/data");
        } else {
            printLine("Formatting filesystem", true, 80);
        }
        
        // Hardware
        printHeader("Hardware detection:");
        printLine("DS3231 RTC", true, 80);
        printInfo("Clock synced");
        
        printLine("TFT display 320x240", true, 80);
        printInfo("ILI9341 driver");
        
        printLine("Rotary encoder", true, 80);
        printInfo("4 steps per detent");
        
        printLine("I2S microphone", true, 80);
        printInfo("INMP441 MEMS");
        
        printLine("I2S amplifier", true, 80);
        printInfo("MAX98357A DAC");
        
        // Network
        printHeader("Network initialization:");
        printLine("WiFi radio ready", true, 80);
        printInfo("ESP32 WiFi 802.11 b/g/n");
        
        // Services
        printHeader("Starting services:");
        printLine("Audio manager", true, 60);
        printLine("Display manager", true, 60);
        printLine("WiFi scanner", true, 60);
        printLine("AI assistant", true, 60);
        printLine("Menu system", true, 60);
        
        // Ready
        tft->println("");
        printHeader("System ready!");
        delay(500);
        
        // Fade out
        for (int i = 255; i >= 0; i -= 5) {
            analogWrite(TFT_BL, i);
            delay(5);
        }
        
        delay(200);
        
        for (int i = 0; i <= 255; i += 5) {
            analogWrite(TFT_BL, i);
            delay(5);
        }
    }
};

#endif