// UIHelper.h - UI Drawing Helper Functions (Fixed)
#ifndef UIHELPER_H
#define UIHELPER_H

#include <TFT_eSPI.h>
#include "Colors.h"
#include "Config.h"

class UIHelper {
public:
    static void drawCard(TFT_eSprite* sprite, int x, int y, int w, int h, uint16_t color, bool selected = false) {
        sprite->fillRoundRect(x, y, w, h, CARD_RADIUS, color);
        
        if (selected) {
            sprite->drawRoundRect(x, y, w, h, CARD_RADIUS, COLOR_ACCENT);
            sprite->drawRoundRect(x + 1, y + 1, w - 2, h - 2, CARD_RADIUS, COLOR_ACCENT);
        }
    }
    
    static void drawSignalBars(TFT_eSprite* sprite, int x, int y, int rssi) {
        const int barWidth = 4;
        const int barSpacing = 2;
        const int maxHeight = 16;
        int bars = getSignalBars(rssi);
        uint16_t color = getSignalColor(rssi);
        
        for (int i = 0; i < 4; i++) {
            int barHeight = 4 + (i * 4);
            int barY = y + (maxHeight - barHeight);
            uint16_t barColor = (i < bars) ? color : COLOR_TEXT_DARK;
            sprite->fillRect(x + (i * (barWidth + barSpacing)), barY, barWidth, barHeight, barColor);
        }
    }
    
    static void drawLockIcon(TFT_eSprite* sprite, int x, int y, uint16_t color) {
        sprite->fillRoundRect(x + 3, y + 7, 10, 9, 2, color);
        sprite->drawCircle(x + 8, y + 7, 4, color);
        sprite->drawCircle(x + 8, y + 7, 3, color);
        sprite->fillRect(x + 5, y + 7, 6, 4, COLOR_CARD);
    }
    
    static void drawWiFiIcon(TFT_eSprite* sprite, int x, int y, int strength, uint16_t color) {
        for (int i = 0; i < strength; i++) {
            int radius = 4 + (i * 3);
            sprite->drawCircle(x + 8, y + 12, radius, color);
        }
        sprite->fillCircle(x + 8, y + 12, 2, color);
    }
    
    // Fixed: Header with transparent text background
    static void drawHeader(TFT_eSprite* sprite, const char* title, const char* subtitle = nullptr) {
        // Gradient background
        for (int i = 0; i < HEADER_HEIGHT; i++) {
            uint8_t brightness = 8 + (i / 3);
            uint16_t color = sprite->color565(brightness, brightness * 2, brightness * 4);
            sprite->drawFastHLine(0, i, SCREEN_WIDTH, color);
        }
        
        // Title - NO background color (transparent)
        sprite->setTextColor(COLOR_TEXT);
        sprite->setTextDatum(ML_DATUM);
        sprite->drawString(title, 10, HEADER_HEIGHT / 2 - 2, 4);
        
        // Subtitle (right-aligned) - NO background color
        if (subtitle) {
            sprite->setTextColor(COLOR_TEXT_DIM);
            sprite->setTextDatum(MR_DATUM);
            sprite->drawString(subtitle, SCREEN_WIDTH - 10, HEADER_HEIGHT / 2, 2);
        }
        
        // Bottom accent line
        sprite->drawFastHLine(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, COLOR_ACCENT);
    }
    
    static void drawFooter(TFT_eSprite* sprite, const char* text) {
        int footerY = SCREEN_HEIGHT - FOOTER_HEIGHT;
        
        sprite->fillRect(0, footerY, SCREEN_WIDTH, FOOTER_HEIGHT, COLOR_HEADER);
        sprite->drawFastHLine(0, footerY, SCREEN_WIDTH, COLOR_ACCENT);
        
        sprite->setTextColor(COLOR_TEXT_DIM);
        sprite->setTextDatum(MC_DATUM);
        sprite->drawString(text, SCREEN_WIDTH / 2, footerY + FOOTER_HEIGHT / 2, 1);
    }
    
    static void drawScrollbar(TFT_eSprite* sprite, int currentItem, int totalItems, int visibleItems) {
        if (totalItems <= visibleItems) return;
        
        const int scrollbarX = SCREEN_WIDTH - 5;
        const int scrollbarY = HEADER_HEIGHT + 4;
        const int scrollbarHeight = SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 8;
        
        sprite->fillRoundRect(scrollbarX, scrollbarY, 3, scrollbarHeight, 2, COLOR_CARD);
        
        int thumbHeight = max(12, (visibleItems * scrollbarHeight) / totalItems);
        int maxScroll = totalItems - visibleItems;
        int thumbY = scrollbarY + ((currentItem * (scrollbarHeight - thumbHeight)) / maxScroll);
        
        sprite->fillRoundRect(scrollbarX, thumbY, 3, thumbHeight, 2, COLOR_ACCENT);
    }
    
    static void drawSpinner(TFT_eSprite* sprite, int x, int y, int angle) {
        for (int i = 0; i < 8; i++) {
            int a = (angle + i * 45) % 360;
            float rad = a * PI / 180.0;
            int dx = x + cos(rad) * 15;
            int dy = y + sin(rad) * 15;
            int size = 3 - (i / 3);
            uint8_t brightness = 255 - (i * 25);
            uint16_t color = sprite->color565(0, brightness / 2, brightness);
            sprite->fillCircle(dx, dy, size, color);
        }
    }
    
    static void drawMessage(TFT_eSprite* sprite, const char* message, uint16_t color = COLOR_TEXT) {
        sprite->setTextColor(color);
        sprite->setTextDatum(MC_DATUM);
        sprite->drawString(message, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
    }
};

#endif