// DisplayManager.h
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include <FFat.h>
#include <TJpg_Decoder.h>
#include "Config.h"

class DisplayManager {
private:
    TFT_eSPI tft;
    TFT_eSprite* mainSprite;
    TFT_eSprite* statusBarSprite;
    
    static DisplayManager* instance;
    static bool tftOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    
public:
    DisplayManager();
    ~DisplayManager();
    
    bool begin();
    TFT_eSprite* getMainSprite() { return mainSprite; }
    TFT_eSprite* getStatusBarSprite() { return statusBarSprite; }
    TFT_eSPI* getTFT() { return &tft; }
    
    void pushSprite();
    void pushStatusBar();
    void drawJpegFromFile(const char* filename, int16_t x, int16_t y);
    bool loadJpegToSprite(TFT_eSprite* sprite, const char* filename);
    
    // Utility drawing functions
    void drawRoundRect(TFT_eSprite* spr, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
    void drawSignalStrength(TFT_eSprite* spr, int16_t x, int16_t y, int rssi);
    void drawStatusIcon(TFT_eSprite* spr, int16_t x, int16_t y, bool connected, uint8_t alpha);
};

#endif // DISPLAY_MANAGER_H