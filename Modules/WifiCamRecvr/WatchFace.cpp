// WatchFace.cpp
#include "WatchFace.h"
#include <WiFi.h>

WatchFace::WatchFace() : display(nullptr), weather(nullptr) {}

void WatchFace::begin(DisplayManager* disp, WeatherManager* wth) {
    display = disp;
    weather = wth;
}

void WatchFace::draw() {
    if (!display) return;
    
    TFT_eSprite* sprite = display->getMainSprite();
    if (!sprite) return;
    
    drawBackground();
    
    if (!getLocalTime(&timeInfo)) {
        sprite->setTextDatum(MC_DATUM);
        sprite->setTextColor(COLOR_ERROR);
        sprite->setTextSize(1);
        sprite->drawString("Time sync failed", DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 2);
        display->pushSprite();
        return;
    }
    
    drawTimePanel();
    drawDateDisplay();
    drawWeatherDisplay();
    drawSystemIndicators();
    
    display->pushSprite();
}

void WatchFace::drawBackground() {
    TFT_eSprite* sprite = display->getMainSprite();
    
    // Try to load wallpaper
    if (FFat.exists(WALLPAPER_PATH)) {
        display->loadJpegToSprite(sprite, WALLPAPER_PATH);
        
        // Add semi-transparent overlay for better text visibility
        for (int y = 0; y < DISPLAY_HEIGHT; y++) {
            for (int x = 0; x < DISPLAY_WIDTH; x += 2) {
                sprite->drawPixel(x, y, COLOR_BG_DARK);
            }
        }
    } else {
        // Fallback gradient background
        for (int y = 0; y < DISPLAY_HEIGHT; y++) {
            float ratio = (float)y / DISPLAY_HEIGHT;
            uint16_t gradientColor = sprite->color565(
                (int)(8 * (1 - ratio) + 40 * ratio),
                (int)(33 * (1 - ratio) + 60 * ratio),
                (int)(65 * (1 - ratio) + 100 * ratio)
            );
            sprite->drawFastHLine(0, y, DISPLAY_WIDTH, gradientColor);
        }
    }
}

void WatchFace::drawTimePanel() {
    TFT_eSprite* sprite = display->getMainSprite();
    
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
    
    int timeY = DISPLAY_HEIGHT/2 - 20;
    int panelWidth = 200;
    int panelHeight = 60;
    int panelX = DISPLAY_WIDTH/2 - panelWidth/2;
    
    // Panel gradient background
    for (int i = 0; i < panelHeight; i++) {
        float ratio = (float)i / panelHeight;
        uint16_t gradientColor = sprite->color565(
            (int)(20 * (1 - ratio) + 60 * ratio),
            (int)(30 * (1 - ratio) + 80 * ratio),
            (int)(60 * (1 - ratio) + 120 * ratio)
        );
        sprite->drawFastHLine(panelX, timeY - panelHeight/2 + i, panelWidth, gradientColor);
    }
    
    // Panel glow effect
    for (int glow = 0; glow < 3; glow++) {
        uint16_t glowColor = sprite->color565(0, 150 - glow * 30, 255 - glow * 50);
        sprite->drawRoundRect(panelX - glow, timeY - panelHeight/2 - glow, 
                           panelWidth + 2*glow, panelHeight + 2*glow, 15 + glow, glowColor);
    }
    
    // Time text with shadow
    sprite->setTextDatum(MC_DATUM);
    sprite->setTextColor(TFT_BLACK);
    sprite->setTextSize(6);
    sprite->drawString(timeStr, DISPLAY_WIDTH/2 + 2, timeY + 2);
    
    sprite->setTextColor(TFT_CYAN);
    sprite->drawString(timeStr, DISPLAY_WIDTH/2, timeY);
}

void WatchFace::drawDateDisplay() {
    TFT_eSprite* sprite = display->getMainSprite();
    
    char dateStr[20];
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", 
                           "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    sprintf(dateStr, "%02d %s %04d", timeInfo.tm_mday, months[timeInfo.tm_mon], timeInfo.tm_year + 1900);
    
    int dateY = DISPLAY_HEIGHT/2 + 30;
    
    sprite->setTextDatum(MC_DATUM);
    sprite->setTextColor(TFT_WHITE);
    sprite->setTextSize(2);  // Bigger date
    sprite->drawString(dateStr, DISPLAY_WIDTH/2, dateY);
}

void WatchFace::drawWeatherDisplay() {
    if (!weather || !weather->isDataValid()) return;
    
    TFT_eSprite* sprite = display->getMainSprite();
    
    int weatherY = DISPLAY_HEIGHT/2 + 55;
    
    // Temperature - larger
    sprite->setTextDatum(MC_DATUM);
    sprite->setTextColor(COLOR_ACCENT);
    sprite->setTextSize(3);  // Bigger temperature
    String tempStr = String((int)weather->getTemperature()) + "C";
    sprite->drawString(tempStr, DISPLAY_WIDTH/2, weatherY);
    
    // Description - larger
    sprite->setTextColor(COLOR_TEXT_SECONDARY);
    sprite->setTextSize(2);  // Bigger description
    sprite->drawString(weather->getDescription(), DISPLAY_WIDTH/2, weatherY + 25);
}

void WatchFace::drawSystemIndicators() {
    TFT_eSprite* sprite = display->getMainSprite();
    
    // WiFi indicator
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        display->drawSignalStrength(sprite, 280, 10, rssi);
    } else {
        sprite->setTextDatum(TR_DATUM);
        sprite->setTextColor(COLOR_ERROR);
        sprite->setTextSize(1);
        sprite->drawString("No WiFi", 310, 10, 1);
    }
}