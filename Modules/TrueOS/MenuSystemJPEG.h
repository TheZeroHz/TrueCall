// MenuSystemJPEG.h - Professional Menu with JPEG Icons
// Save this file as: MenuSystemJPEG.h in your TrueCallDesk folder
#ifndef MENUSYSTEMJPEG_H
#define MENUSYSTEMJPEG_H

#include <TFT_eSPI.h>
#include <JPEGDecoder.h>
#include <FFat.h>
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
    String iconPath;        // JPEG icon path
    uint16_t fallbackColor; // Fallback color if JPEG not found
    AppMode mode;
    bool isSettingsItem;
};

class MenuSystem {
private:
    TFT_eSprite* sprite;
    TFT_eSPI* tft;
    MenuItem items[12];
    int itemCount;
    int selectedIndex;
    int scrollOffset;
    bool showingSettings;
    
    const int VISIBLE_MENU_ITEMS = 2;
    const int MENU_ITEM_HEIGHT = 80;
    const int ICON_SIZE = 48;
    
    void drawJPEGIcon(int x, int y, const char* jpegPath, uint16_t fallbackColor) {
        if (FFat.exists(jpegPath)) {
            File jpegFile = FFat.open(jpegPath, FILE_READ);
            if (jpegFile && JpegDec.decodeFsFile(jpegFile)) {
                // Create temporary sprite for icon
                TFT_eSprite iconSprite = TFT_eSprite(tft);
                iconSprite.setColorDepth(16);
                iconSprite.createSprite(ICON_SIZE, ICON_SIZE);
                iconSprite.fillSprite(TFT_BLACK);
                
                uint16_t *pImg;
                while (JpegDec.read()) {
                    pImg = JpegDec.pImage;
                    int mcu_x = JpegDec.MCUx * JpegDec.MCUWidth;
                    int mcu_y = JpegDec.MCUy * JpegDec.MCUHeight;
                    
                    // Only draw if within icon bounds
                    if (mcu_x < ICON_SIZE && mcu_y < ICON_SIZE) {
                        iconSprite.pushImage(mcu_x, mcu_y, JpegDec.MCUWidth, JpegDec.MCUHeight, pImg);
                    }
                }
                
                // Push to main sprite
                iconSprite.pushToSprite(sprite, x, y, TFT_WHITE);
                iconSprite.deleteSprite();
            } else {
                // JPEG decode failed - use fallback
                drawFallbackIcon(x, y, fallbackColor);
            }
            jpegFile.close();
        } else {
            // File not found - use fallback
            drawFallbackIcon(x, y, fallbackColor);
        }
    }
    
    void drawFallbackIcon(int x, int y, uint16_t color) {
        // Simple gradient circle as fallback
        for (int r = ICON_SIZE/2; r > 0; r -= 2) {
            uint8_t brightness = map(r, 0, ICON_SIZE/2, 50, 255);
            uint16_t shadeColor = sprite->color565(
                ((color >> 11) & 0x1F) * brightness / 255,
                ((color >> 5) & 0x3F) * brightness / 255,
                (color & 0x1F) * brightness / 255
            );
            sprite->fillCircle(x + ICON_SIZE/2, y + ICON_SIZE/2, r, shadeColor);
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
        
        // Card background
        sprite->fillRoundRect(x, y, cardWidth, cardHeight, 12, bgColor);
        
        // Border
        if (selected) {
            sprite->drawRoundRect(x, y, cardWidth, cardHeight, 12, COLOR_ACCENT);
            sprite->drawRoundRect(x + 1, y + 1, cardWidth - 2, cardHeight - 2, 12, COLOR_ACCENT);
        } else {
            sprite->drawRoundRect(x, y, cardWidth, cardHeight, 12, COLOR_BORDER);
        }
        
        // Draw JPEG icon
        int iconX = x + 10;
        int iconY = y + 11;
        drawJPEGIcon(iconX, iconY, item->iconPath.c_str(), item->fallbackColor);
        
        // Title
        sprite->setTextColor(COLOR_TEXT, bgColor);
        sprite->setTextDatum(TL_DATUM);
        sprite->drawString(item->title, x + 70, y + 15, 4);
        
        // Description
        sprite->setTextColor(COLOR_TEXT_DIM, bgColor);
        sprite->drawString(item->description, x + 70, y + 42, 1);
    }
    
public:
    MenuSystem(TFT_eSprite* spr, TFT_eSPI* tftRef) 
        : sprite(spr), tft(tftRef), itemCount(0), selectedIndex(0), 
          scrollOffset(0), showingSettings(false) {
        
        // Main menu items with JPEG icons
        items[0] = {"WiFi Manger", "Scan and connect to networks", 
                    "/menu_icons/wifi.jpg", COLOR_ACCENT, APP_WIFI_SCANNER, false};
        items[1] = {"AI Assistant", "Voice-powered AI chat", 
                    "/menu_icons/ai.jpg", COLOR_SUCCESS, APP_AI_ASSISTANT, false};
        items[2] = {"Clock", "Set time, date, and alarms", 
                    "/menu_icons/clock.jpg", COLOR_INFO, APP_CLOCK_SETTINGS, false};
        items[3] = {"Credits", "Meet the team", 
                    "/menu_icons/about.jpg", COLOR_WARNING, APP_ABOUT_CREDITS, false};
        items[4] = {"Settings", "App configuration", 
                    "/menu_icons/settings.jpg", COLOR_ACCENT, APP_SETTINGS, false};
        itemCount = 5;
    }
    
    void showSettings() {
        showingSettings = true;
        selectedIndex = 0;
        scrollOffset = 0;
        
        // Settings menu items with JPEG icons
        items[0] = {"Brightness", "Adjust display brightness", 
                    "/menu_icons/brightness.jpg", COLOR_ACCENT, APP_DISPLAY_BRIGHTNESS, true};
        items[1] = {"Timeout", "Set display sleep timeout", 
                    "/menu_icons/timeout.jpg", COLOR_INFO, APP_DISPLAY_TIMEOUT, true};
        items[2] = {"Volume", "Adjust audio volume", 
                    "/menu_icons/volume.jpg", COLOR_SUCCESS, APP_VOLUME_CONTROL, true};
        items[3] = {"Theme", "Change color theme", 
                    "/menu_icons/theme.jpg", COLOR_WARNING, APP_THEME_SELECTOR, true};
        items[4] = {"Wallpaper", "Change home wallpaper", 
                    "/menu_icons/wallpaper.jpg", COLOR_ACCENT, APP_WALLPAPER_CHANGER, true};
        items[5] = {"Restart", "Restart the device", 
                    "/menu_icons/restart.jpg", COLOR_WARNING, APP_RESTART, true};
        items[6] = {"Factory Reset", "Reset all settings", 
                    "/menu_icons/reset.jpg", COLOR_ERROR, APP_FACTORY_RESET, true};
        itemCount = 7;
    }
    
    void showMainMenu() {
        showingSettings = false;
        selectedIndex = 0;
        scrollOffset = 0;
        
        // Restore main menu
        items[0] = {"WiFi Scanner", "Scan and connect to networks", 
                    "/menu_icons/wifi.jpg", COLOR_ACCENT, APP_WIFI_SCANNER, false};
        items[1] = {"AI Assistant", "Voice-powered AI chat", 
                    "/menu_icons/ai.jpg", COLOR_SUCCESS, APP_AI_ASSISTANT, false};
        items[2] = {"Clock Settings", "Set time, date, and alarms", 
                    "/menu_icons/clock.jpg", COLOR_INFO, APP_CLOCK_SETTINGS, false};
        items[3] = {"About/Credits", "Meet the team", 
                    "/menu_icons/about.jpg", COLOR_WARNING, APP_ABOUT_CREDITS, false};
        items[4] = {"Settings", "App configuration", 
                    "/menu_icons/settings.jpg", COLOR_ACCENT, APP_SETTINGS, false};
        itemCount = 5;
    }
    
    void draw() {
        sprite->fillSprite(COLOR_BG);
        
        char subtitle[20];
        snprintf(subtitle, sizeof(subtitle), "%d/%d", selectedIndex + 1, itemCount);
        UIHelper::drawHeader(sprite, showingSettings ? "Settings" : "Main Menu", subtitle);
        
        int startY = HEADER_HEIGHT + 10;
        
        // Auto-scroll
        if (selectedIndex < scrollOffset) {
            scrollOffset = selectedIndex;
        } else if (selectedIndex >= scrollOffset + VISIBLE_MENU_ITEMS) {
            scrollOffset = selectedIndex - VISIBLE_MENU_ITEMS + 1;
        }
        
        // Draw visible menu items
        for (int i = 0; i < VISIBLE_MENU_ITEMS && (scrollOffset + i) < itemCount; i++) {
            int index = scrollOffset + i;
            int y = startY + (i * MENU_ITEM_HEIGHT);
            drawMenuItem(index, y, index == selectedIndex);
        }
        
        // Scrollbar
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