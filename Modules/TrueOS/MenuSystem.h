// MenuSystem.h - Professional Menu with Scrolling Support
#ifndef MENUSYSTEM_H
#define MENUSYSTEM_H

#include <TFT_eSPI.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"

enum AppMode {
    APP_WIFI_SCANNER,
    APP_AI_ASSISTANT,
    APP_CLOCK_SETTINGS,
    APP_ABOUT_CREDITS,
    APP_SETTINGS,
    APP_DISPLAY_BRIGHTNESS,
    APP_DISPLAY_TIMEOUT,
    APP_VOLUME_CONTROL,
    APP_THEME_SELECTOR,
    APP_WALLPAPER_CHANGER,
    APP_RESTART,
    APP_FACTORY_RESET
};

struct MenuItem {
    String title;
    String description;
    uint16_t iconColor;
    AppMode mode;
    bool isSettingsItem;
};

class MenuSystem {
private:
    TFT_eSprite* sprite;
    MenuItem items[8];
    int itemCount;
    int selectedIndex;
    int scrollOffset;
    bool showingSettings;
    
    const int VISIBLE_MENU_ITEMS = 2;
    const int MENU_ITEM_HEIGHT = 80;
    
    void drawIcon(int x, int y, AppMode mode, uint16_t color) {
        int centerX = x + 30;
        int centerY = y + 30;
        
        switch (mode) {
            case APP_WIFI_SCANNER:
                for (int i = 0; i < 3; i++) {
                    int radius = 8 + (i * 6);
                    sprite->drawCircle(centerX, centerY + 8, radius, color);
                }
                sprite->fillCircle(centerX, centerY + 8, 3, color);
                break;
                
            case APP_AI_ASSISTANT:
                sprite->fillRoundRect(centerX - 6, centerY - 12, 12, 16, 4, color);
                sprite->drawLine(centerX, centerY + 4, centerX, centerY + 12, color);
                sprite->drawLine(centerX - 8, centerY + 12, centerX + 8, centerY + 12, color);
                sprite->drawCircle(centerX, centerY - 4, 10, color);
                break;
                
            case APP_SETTINGS:
            case APP_DISPLAY_BRIGHTNESS:
            case APP_DISPLAY_TIMEOUT:
            case APP_VOLUME_CONTROL:
            case APP_THEME_SELECTOR:
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
                
            case APP_RESTART:
                sprite->drawCircle(centerX, centerY, 12, color);
                for (int i = 0; i < 3; i++) {
                    float angle = (270 + i * 15) * PI / 180.0;
                    int x1 = centerX + cos(angle) * 12;
                    int y1 = centerY + sin(angle) * 12;
                    sprite->drawLine(centerX, centerY, x1, y1, color);
                }
                sprite->fillTriangle(centerX + 12, centerY - 3, 
                                   centerX + 12, centerY + 3,
                                   centerX + 17, centerY, color);
                break;
                
            case APP_FACTORY_RESET:
                sprite->drawCircle(centerX, centerY, 12, color);
                sprite->drawLine(centerX - 8, centerY - 8, centerX + 8, centerY + 8, color);
                sprite->drawLine(centerX + 8, centerY - 8, centerX - 8, centerY + 8, color);
                sprite->drawLine(centerX - 7, centerY - 8, centerX + 9, centerY + 8, color);
                sprite->drawLine(centerX + 8, centerY - 7, centerX - 8, centerY + 9, color);
                break;
        }
    }
    
    void drawMenuItem(int index, int y, bool selected) {
        MenuItem* item = &items[index];
        
        int cardHeight = 70;
        int cardWidth = SCREEN_WIDTH - 16;
        int x = 8;
        
        uint16_t bgColor = selected ? COLOR_SELECTED : COLOR_CARD;
        
        sprite->fillRoundRect(x + 3, y + 3, cardWidth, cardHeight, 12, COLOR_SHADOW);
        sprite->fillRoundRect(x, y, cardWidth, cardHeight, 12, bgColor);
        
        if (selected) {
            sprite->drawRoundRect(x, y, cardWidth, cardHeight, 12, COLOR_ACCENT);
            sprite->drawRoundRect(x + 1, y + 1, cardWidth - 2, cardHeight - 2, 12, COLOR_ACCENT);
        } else {
            sprite->drawRoundRect(x, y, cardWidth, cardHeight, 12, COLOR_BORDER);
        }
        
        drawIcon(x + 10, y + 10, item->mode, selected ? COLOR_ACCENT : item->iconColor);
        
        sprite->setTextColor(COLOR_TEXT, bgColor);
        sprite->setTextDatum(TL_DATUM);
        sprite->drawString(item->title, x + 75, y + 15, 4);
        
        sprite->setTextColor(COLOR_TEXT_DIM, bgColor);
        sprite->drawString(item->description, x + 75, y + 42, 1);
    }
    
public:
    MenuSystem(TFT_eSprite* spr) : sprite(spr), itemCount(0), selectedIndex(0), 
                                    scrollOffset(0), showingSettings(false) {
        items[0] = {"WiFi Scanner", "Scan and connect to networks", COLOR_ACCENT, APP_WIFI_SCANNER, false};
        items[1] = {"AI Assistant", "Voice-powered AI chat", COLOR_SUCCESS, APP_AI_ASSISTANT, false};
        items[2] = {"Settings", "App configuration", COLOR_WARNING, APP_SETTINGS, false};
        itemCount = 3;
    }
    
    void showSettings() {
        showingSettings = true;
        selectedIndex = 0;
        scrollOffset = 0;
        
        items[0] = {"Brightness", "Adjust display brightness", COLOR_ACCENT, APP_DISPLAY_BRIGHTNESS, true};
        items[1] = {"Timeout", "Set display sleep timeout", COLOR_INFO, APP_DISPLAY_TIMEOUT, true};
        items[2] = {"Volume", "Adjust audio volume", COLOR_SUCCESS, APP_VOLUME_CONTROL, true};
        items[3] = {"Theme", "Change color theme", COLOR_WARNING, APP_THEME_SELECTOR, true};
        items[4] = {"Wallpaper", "Change home wallpaper", COLOR_ACCENT, APP_WALLPAPER_CHANGER, true};
        items[5] = {"Restart", "Restart the device", COLOR_WARNING, APP_RESTART, true};
        items[6] = {"Factory Reset", "Reset all settings", COLOR_ERROR, APP_FACTORY_RESET, true};
        itemCount = 7;
    }
    
    void showMainMenu() {
        showingSettings = false;
        selectedIndex = 0;
        scrollOffset = 0;
        
        items[0] = {"WiFi Scanner", "Scan and connect to networks", COLOR_ACCENT, APP_WIFI_SCANNER, false};
        items[1] = {"AI Assistant", "Voice-powered AI chat", COLOR_SUCCESS, APP_AI_ASSISTANT, false};
        items[2] = {"Clock Settings", "Set time, date, and alarms", COLOR_INFO, APP_CLOCK_SETTINGS, false};
        items[3] = {"About/Credits", "Meet the team", COLOR_WARNING, APP_ABOUT_CREDITS, false};
        items[4] = {"Settings", "App configuration", COLOR_ACCENT, APP_SETTINGS, false};
        itemCount = 5;
    }
    
    void draw() {
        sprite->fillSprite(COLOR_BG);
        
        char subtitle[20];
        snprintf(subtitle, sizeof(subtitle), "%d/%d", selectedIndex + 1, itemCount);
        UIHelper::drawHeader(sprite, showingSettings ? "Settings" : "Main Menu", subtitle);
        
        int startY = HEADER_HEIGHT + 10;
        
        if (selectedIndex < scrollOffset) {
            scrollOffset = selectedIndex;
        } else if (selectedIndex >= scrollOffset + VISIBLE_MENU_ITEMS) {
            scrollOffset = selectedIndex - VISIBLE_MENU_ITEMS + 1;
        }
        
        for (int i = 0; i < VISIBLE_MENU_ITEMS && (scrollOffset + i) < itemCount; i++) {
            int index = scrollOffset + i;
            int y = startY + (i * MENU_ITEM_HEIGHT);
            drawMenuItem(index, y, index == selectedIndex);
        }
        
        if (itemCount > VISIBLE_MENU_ITEMS) {
            int scrollbarX = SCREEN_WIDTH - 6;
            int scrollbarY = startY;
            int scrollbarHeight = VISIBLE_MENU_ITEMS * MENU_ITEM_HEIGHT;
            
            sprite->fillRoundRect(scrollbarX, scrollbarY, 4, scrollbarHeight, 2, COLOR_CARD);
            
            int thumbHeight = max(20, (VISIBLE_MENU_ITEMS * scrollbarHeight) / itemCount);
            int maxScroll = itemCount - VISIBLE_MENU_ITEMS;
            int thumbY = scrollbarY + ((scrollOffset * (scrollbarHeight - thumbHeight)) / maxScroll);
            
            sprite->fillRoundRect(scrollbarX, thumbY, 4, thumbHeight, 2, COLOR_ACCENT);
        }
        
        const char* footer = showingSettings ? 
            "Rotate: Select  |  Press: Open  |  HOME: Back" :
            "Rotate: Select  |  Press: Open  |  HOME: Exit";
        UIHelper::drawFooter(sprite, footer);
        
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
    
    bool isShowingSettings() const {
        return showingSettings;
    }
};

#endif