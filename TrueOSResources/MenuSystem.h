// MenuSystem.h - Professional Menu for App Selection
#ifndef MENUSYSTEM_H
#define MENUSYSTEM_H

#include <TFT_eSPI.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"

enum AppMode {
    APP_WIFI_SCANNER,
    APP_AI_ASSISTANT,
    APP_SETTINGS
};

struct MenuItem {
    String title;
    String description;
    uint16_t iconColor;
    AppMode mode;
};

class MenuSystem {
private:
    TFT_eSprite* sprite;
    MenuItem items[3];
    int itemCount;
    int selectedIndex;
    
    void drawIcon(int x, int y, AppMode mode, uint16_t color) {
        int centerX = x + 30;
        int centerY = y + 30;
        
        switch (mode) {
            case APP_WIFI_SCANNER:
                // WiFi icon
                for (int i = 0; i < 3; i++) {
                    int radius = 8 + (i * 6);
                    sprite->drawCircle(centerX, centerY + 8, radius, color);
                }
                sprite->fillCircle(centerX, centerY + 8, 3, color);
                break;
                
            case APP_AI_ASSISTANT:
                // Microphone icon
                sprite->fillRoundRect(centerX - 6, centerY - 12, 12, 16, 4, color);
                sprite->drawLine(centerX, centerY + 4, centerX, centerY + 12, color);
                sprite->drawLine(centerX - 8, centerY + 12, centerX + 8, centerY + 12, color);
                sprite->drawCircle(centerX, centerY - 4, 10, color);
                break;
                
            case APP_SETTINGS:
                // Gear icon
                sprite->fillCircle(centerX, centerY, 8, color);
                for (int i = 0; i < 8; i++) {
                    float angle = i * 45 * PI / 180.0;
                    int x1 = centerX + cos(angle) * 6;
                    int y1 = centerY + sin(angle) * 6;
                    int x2 = centerX + cos(angle) * 14;
                    int y2 = centerY + sin(angle) * 14;
                    sprite->drawLine(x1, y1, x2, y2, color);
                    sprite->drawLine(x1 + 1, y1, x2 + 1, y2, color);
                }
                sprite->fillCircle(centerX, centerY, 4, COLOR_BG);
                break;
        }
    }
    
    void drawMenuItem(int index, int y, bool selected) {
        MenuItem* item = &items[index];
        
        int cardHeight = 70;
        int cardWidth = SCREEN_WIDTH - 16;
        int x = 8;
        
        uint16_t bgColor = selected ? COLOR_SELECTED : COLOR_CARD;
        
        // Shadow
        sprite->fillRoundRect(x + 3, y + 3, cardWidth, cardHeight, 12, COLOR_SHADOW);
        
        // Card
        sprite->fillRoundRect(x, y, cardWidth, cardHeight, 12, bgColor);
        
        if (selected) {
            sprite->drawRoundRect(x, y, cardWidth, cardHeight, 12, COLOR_ACCENT);
            sprite->drawRoundRect(x + 1, y + 1, cardWidth - 2, cardHeight - 2, 12, COLOR_ACCENT);
        } else {
            sprite->drawRoundRect(x, y, cardWidth, cardHeight, 12, COLOR_BORDER);
        }
        
        // Icon
        drawIcon(x + 10, y + 10, item->mode, selected ? COLOR_ACCENT : item->iconColor);
        
        // Title
        sprite->setTextColor(COLOR_TEXT, bgColor);
        sprite->setTextDatum(TL_DATUM);
        sprite->drawString(item->title, x + 75, y + 15, 4);
        
        // Description
        sprite->setTextColor(COLOR_TEXT_DIM, bgColor);
        sprite->drawString(item->description, x + 75, y + 42, 1);
    }
    
public:
    MenuSystem(TFT_eSprite* spr) : sprite(spr), itemCount(0), selectedIndex(0) {
        // Initialize menu items
        items[0] = {"WiFi Scanner", "Scan and connect to networks", COLOR_ACCENT, APP_WIFI_SCANNER};
        items[1] = {"AI Assistant", "Voice-powered AI chat", COLOR_SUCCESS, APP_AI_ASSISTANT};
        items[2] = {"Settings", "App configuration", COLOR_WARNING, APP_SETTINGS};
        itemCount = 3;
    }
    
    void draw() {
        sprite->fillSprite(COLOR_BG);
        
        // Header
        UIHelper::drawHeader(sprite, "Main Menu", nullptr);
        
        // Menu items
        int startY = HEADER_HEIGHT + 10;
        for (int i = 0; i < itemCount; i++) {
            int y = startY + (i * 80);
            drawMenuItem(i, y, i == selectedIndex);
        }
        
        // Footer
        UIHelper::drawFooter(sprite, "Rotate: Select  |  Press: Open  |  HOME: Exit");
        
        sprite->pushSprite(0, 0);
    }
    
    void setSelected(int index) {
        selectedIndex = constrain(index, 0, itemCount - 1);
    }
    
    int getSelected() const {
        return selectedIndex;
    }
    
    AppMode getSelectedMode() const {
        return items[selectedIndex].mode;
    }
    
    int getItemCount() const {
        return itemCount;
    }
};

#endif