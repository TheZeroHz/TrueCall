// WallpaperManager.h - Fixed with proper TJpg implementation
#ifndef WALLPAPERMANAGER_H
#define WALLPAPERMANAGER_H

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <FFat.h>
#include <AiEsp32RotaryEncoder.h>
#include <Preferences.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"
#include "AudioManager.h"

#define MAX_WALLPAPERS 20

// Global sprite pointer for TJpg callback
TFT_eSprite* g_wallpaperSprite = nullptr;

// TJpg callback for rendering to sprite
bool tjpgWallpaperCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= SCREEN_HEIGHT || !g_wallpaperSprite) return 0;
    g_wallpaperSprite->pushImage(x, y, w, h, bitmap);
    return 1;
}

class WallpaperManager {
private:
    TFT_eSprite* sprite;
    TFT_eSPI* tft;
    AiEsp32RotaryEncoder* encoder;
    String wallpapers[MAX_WALLPAPERS];
    int wallpaperCount;
    int currentWallpaper;
    String currentWallpaperPath;
    
    void scanWallpapers() {
        wallpaperCount = 0;
        
        File root = FFat.open("/wallpapers");
        if (!root || !root.isDirectory()) {
            Serial.println("[Wallpaper] /wallpapers directory not found");
            return;
        }
        
        File file = root.openNextFile();
        while (file && wallpaperCount < MAX_WALLPAPERS) {
            String filename = String(file.name());
            if (!file.isDirectory() && 
                (filename.endsWith(".jpg") || filename.endsWith(".jpeg"))) {
                wallpapers[wallpaperCount] = filename;
                Serial.printf("[Wallpaper] Found: %s\n", filename.c_str());
                wallpaperCount++;
            }
            file = root.openNextFile();
        }
        
        Serial.printf("[Wallpaper] Total found: %d\n", wallpaperCount);
    }
    
    void drawWallpaperPreview(const String& filename, int x, int y, int w, int h) {
        String fullPath = "/wallpapers/" + filename;
        
        if (!FFat.exists(fullPath.c_str())) {
            sprite->fillRect(x, y, w, h, COLOR_TEXT_DARK);
            sprite->setTextColor(COLOR_TEXT_DIM);
            sprite->setTextDatum(MC_DATUM);
            sprite->drawString("No Preview", x + w/2, y + h/2, 2);
            return;
        }
        
        // Create preview sprite using TFT reference
        TFT_eSprite preview = TFT_eSprite(tft);
        preview.setColorDepth(16);
        preview.createSprite(w, h);
        preview.fillSprite(TFT_BLACK);
        
        // Set global sprite pointer for callback
        g_wallpaperSprite = &preview;
        
        // Setup TJpg decoder
        TJpgDec.setJpgScale(1);
        TJpgDec.setSwapBytes(true);
        TJpgDec.setCallback(tjpgWallpaperCallback);
        
        // Decode and render
        if (TJpgDec.drawFsJpg(0, 0, fullPath.c_str(), FFat) != 0) {
            // Error decoding
            preview.fillSprite(COLOR_TEXT_DARK);
            preview.setTextColor(COLOR_ERROR);
            preview.setTextDatum(MC_DATUM);
            preview.drawString("Error", w/2, h/2, 2);
        }
        
        // Push preview to main sprite
        preview.pushToSprite(sprite, x, y);
        preview.deleteSprite();
        
        // Clear global pointer
        g_wallpaperSprite = nullptr;
    }
    
    String getWallpaperName(const String& filename) {
        String name = filename;
        name.replace(".jpg", "");
        name.replace(".jpeg", "");
        return name;
    }
    
public:
    WallpaperManager(TFT_eSprite* spr, AiEsp32RotaryEncoder* enc, TFT_eSPI* tftRef) 
        : sprite(spr), tft(tftRef), encoder(enc), wallpaperCount(0), currentWallpaper(0) {
        
        // Initialize TJpg decoder
        TJpgDec.setJpgScale(1);
        TJpgDec.setSwapBytes(true);
        
        scanWallpapers();
        loadCurrentWallpaper();
    }
    
    void loadCurrentWallpaper() {
        Preferences prefs;
        prefs.begin("wallpaper", true);
        currentWallpaperPath = prefs.getString("current", "default.jpg");
        prefs.end();
        
        // Find index of current wallpaper
        for (int i = 0; i < wallpaperCount; i++) {
            if (wallpapers[i] == currentWallpaperPath) {
                currentWallpaper = i;
                break;
            }
        }
    }
    
    void saveCurrentWallpaper() {
        Preferences prefs;
        prefs.begin("wallpaper", false);
        prefs.putString("current", currentWallpaperPath);
        prefs.end();
        Serial.printf("[Wallpaper] Saved: %s\n", currentWallpaperPath.c_str());
    }
    
    String getCurrentWallpaperPath() {
        return "/wallpapers/" + currentWallpaperPath;
    }
    
    void show() {
        if (wallpaperCount == 0) {
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Wallpapers", nullptr);
            UIHelper::drawMessage(sprite, "No wallpapers found", COLOR_ERROR);
            UIHelper::drawFooter(sprite, "Press HOME to exit");
            sprite->pushSprite(0, 0);
            
            while (digitalRead(BTN_SPECIAL) != LOW) delay(10);
            delay(300);
            return;
        }
        
        encoder->reset();
        encoder->setBoundaries(0, wallpaperCount - 1, true);
        encoder->setEncoderValue(currentWallpaper);
        
        bool done = false;
        int lastEncoderVal = currentWallpaper;
        
        while (!done) {
            // Encoder navigation
            if (encoder->encoderChanged()) {
                int val = encoder->readEncoder();
                if (val != lastEncoderVal) {
                    lastEncoderVal = val;
                    currentWallpaper = val;
                    
                    // Play tick sound
                    if (FFat.exists("/sounds/feedback/rotary_encoder.wav")) {
                        audioConnecttoFFat("/sounds/feedback/rotary_encoder.wav");
                    }
                }
            }
            
            // Select wallpaper
            if (encoder->isEncoderButtonClicked()) {
                currentWallpaperPath = wallpapers[currentWallpaper];
                saveCurrentWallpaper();
                
                // Show confirmation
                sprite->fillSprite(COLOR_BG);
                UIHelper::drawHeader(sprite, "Wallpaper Set", nullptr);
                sprite->setTextColor(COLOR_SUCCESS);
                sprite->setTextDatum(MC_DATUM);
                sprite->drawString("Wallpaper changed!", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 2);
                sprite->setTextColor(COLOR_TEXT);
                sprite->drawString(getWallpaperName(currentWallpaperPath), 
                                 SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10, 2);
                sprite->pushSprite(0, 0);
                delay(1500);
            }
            
            // Exit
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
            }
            
            // Draw UI - FULL SCREEN PREVIEW
            sprite->fillSprite(COLOR_BG);
            
            // Full screen preview
            drawWallpaperPreview(wallpapers[currentWallpaper], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            
            // Semi-transparent overlay at top
            for (int i = 0; i < 40; i++) {
                uint8_t alpha = 255 - (i * 6);
                uint16_t overlayColor = sprite->color565(alpha/8, alpha/8, alpha/8);
                sprite->drawFastHLine(0, i, SCREEN_WIDTH, overlayColor);
            }
            
            // Header
            char subtitle[20];
            snprintf(subtitle, sizeof(subtitle), "%d/%d", currentWallpaper + 1, wallpaperCount);
            sprite->setTextColor(TFT_WHITE);
            sprite->setTextDatum(MC_DATUM);
            sprite->drawString("Wallpaper", 60, 15, 2);
            sprite->setTextColor(COLOR_ACCENT);
            sprite->setTextDatum(MR_DATUM);
            sprite->drawString(subtitle, SCREEN_WIDTH - 10, 15, 2);
            
            // Semi-transparent overlay at bottom
            for (int i = 0; i < 60; i++) {
                uint8_t alpha = i * 4;
                uint16_t overlayColor = sprite->color565(alpha/8, alpha/8, alpha/8);
                sprite->drawFastHLine(0, SCREEN_HEIGHT - 60 + i, SCREEN_WIDTH, overlayColor);
            }
            
            // Name at bottom
            sprite->setTextColor(TFT_WHITE);
            sprite->setTextDatum(MC_DATUM);
            sprite->drawString(getWallpaperName(wallpapers[currentWallpaper]), 
                             SCREEN_WIDTH/2, SCREEN_HEIGHT - 35, 2);
            
            sprite->setTextColor(COLOR_TEXT_DIM);
            sprite->drawString("Rotate: Browse  |  Press: Set  |  HOME: Back", 
                             SCREEN_WIDTH/2, SCREEN_HEIGHT - 15, 1);
            
            sprite->pushSprite(0, 0);
            delay(10);
        }
    }
};

#endif