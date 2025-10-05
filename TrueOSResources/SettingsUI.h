// SettingsUI.h - FIXED: Faster button response in all settings
#ifndef SETTINGSUI_H
#define SETTINGSUI_H

#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <Preferences.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"
#include "AudioManager.h"
#include "ButtonHandle.h"

class SettingsUI {
private:
    TFT_eSprite* sprite;
    AiEsp32RotaryEncoder* encoder;
    Preferences prefs;
    ButtonHandle* buttons; // âœ… Added button handler
    
    void drawSlider(int x, int y, int width, int value, int maxValue, const char* label, const char* unit) {
        sprite->fillRoundRect(x, y, width, 60, 8, COLOR_CARD);
        sprite->drawRoundRect(x, y, width, 60, 8, COLOR_BORDER);
        
        sprite->setTextColor(COLOR_TEXT, COLOR_CARD);
        sprite->setTextDatum(TL_DATUM);
        sprite->drawString(label, x + 10, y + 10, 2);
        
        char valueStr[20];
        snprintf(valueStr, sizeof(valueStr), "%d%s", value, unit);
        sprite->setTextColor(COLOR_ACCENT, COLOR_CARD);
        sprite->setTextDatum(TR_DATUM);
        sprite->drawString(valueStr, x + width - 10, y + 10, 2);
        
        int sliderY = y + 38;
        int sliderWidth = width - 20;
        sprite->fillRoundRect(x + 10, sliderY, sliderWidth, 8, 4, COLOR_TEXT_DARK);
        
        int fillWidth = (sliderWidth * value) / maxValue;
        sprite->fillRoundRect(x + 10, sliderY, fillWidth, 8, 4, COLOR_ACCENT);
        
        int thumbX = x + 10 + fillWidth - 6;
        sprite->fillCircle(thumbX + 6, sliderY + 4, 10, COLOR_ACCENT);
        sprite->fillCircle(thumbX + 6, sliderY + 4, 8, COLOR_BG);
        sprite->fillCircle(thumbX + 6, sliderY + 4, 6, COLOR_ACCENT);
    }
    
    void drawConfirmDialog(const char* title, const char* message, bool selected) {
        int dialogW = 260;
        int dialogH = 140;
        int dialogX = (SCREEN_WIDTH - dialogW) / 2;
        int dialogY = (SCREEN_HEIGHT - dialogH) / 2;
        
        sprite->fillRoundRect(dialogX + 3, dialogY + 3, dialogW, dialogH, 12, COLOR_SHADOW);
        sprite->fillRoundRect(dialogX, dialogY, dialogW, dialogH, 12, COLOR_CARD);
        sprite->drawRoundRect(dialogX, dialogY, dialogW, dialogH, 12, COLOR_ERROR);
        sprite->drawRoundRect(dialogX + 1, dialogY + 1, dialogW - 2, dialogH - 2, 12, COLOR_ERROR);
        
        sprite->setTextColor(COLOR_ERROR, COLOR_CARD);
        sprite->setTextDatum(MC_DATUM);
        sprite->drawString(title, SCREEN_WIDTH / 2, dialogY + 25, 4);
        
        sprite->setTextColor(COLOR_TEXT, COLOR_CARD);
        sprite->drawString(message, SCREEN_WIDTH / 2, dialogY + 60, 2);
        
        int btnY = dialogY + 95;
        int btnW = 100;
        int btnH = 30;
        
        int cancelX = dialogX + 30;
        int confirmX = dialogX + dialogW - btnW - 30;
        
        uint16_t cancelBg = !selected ? COLOR_SELECTED : COLOR_TEXT_DARK;
        uint16_t confirmBg = selected ? COLOR_ERROR : COLOR_TEXT_DARK;
        
        sprite->fillRoundRect(cancelX, btnY, btnW, btnH, 8, cancelBg);
        sprite->drawRoundRect(cancelX, btnY, btnW, btnH, 8, !selected ? COLOR_ACCENT : COLOR_BORDER);
        sprite->setTextColor(COLOR_TEXT, cancelBg);
        sprite->drawString("Cancel", cancelX + btnW/2, btnY + btnH/2, 2);
        
        sprite->fillRoundRect(confirmX, btnY, btnW, btnH, 8, confirmBg);
        sprite->drawRoundRect(confirmX, btnY, btnW, btnH, 8, selected ? COLOR_ERROR : COLOR_BORDER);
        sprite->setTextColor(COLOR_TEXT, confirmBg);
        sprite->drawString("Confirm", confirmX + btnW/2, btnY + btnH/2, 2);
    }
    
public:
    SettingsUI(TFT_eSprite* spr, AiEsp32RotaryEncoder* enc) 
        : sprite(spr), encoder(enc), buttons(nullptr) {
        
        // âœ… Initialize button handler
        buttons = new ButtonHandle();
        buttons->begin();
    }
    
    void begin() {
        prefs.begin("settings", false);
    }

    void selectTheme() {
        extern ColorTheme currentTheme;
        int theme = prefs.getInt("theme", THEME_DARK_BLUE);
        
        encoder->reset();
        encoder->setBoundaries(0, 6, true);
        encoder->setEncoderValue(theme);
        
        bool done = false;
        int lastEncoderVal = theme;
        
        const char* themeNames[] = {
            "Dark Blue", "Pure Black", "AMOLED", 
            "Ocean", "Forest", "Sunset", "Custom"
        };
        
        while (!done) {
            // âœ… Update buttons every loop
            if (buttons) buttons->update();
            
            if (encoder->encoderChanged()) {
                int val = encoder->readEncoder();
                if (val != lastEncoderVal) {
                    if (FFat.exists("/sounds/feedback/rotary_encoder.wav")) {
                        audioConnecttoFFat("/sounds/feedback/rotary_encoder.wav");
                    }
                    lastEncoderVal = val;
                }
                theme = val;
                currentTheme = (ColorTheme)theme;
            }
            
            if (encoder->isEncoderButtonClicked()) {
                if (theme == THEME_CUSTOM) {
                    createCustomTheme();
                    encoder->reset();
                    encoder->setBoundaries(0, 6, true);
                    encoder->setEncoderValue(THEME_CUSTOM);
                    lastEncoderVal = THEME_CUSTOM;
                } else {
                    done = true;
                    prefs.putInt("theme", theme);
                }
                delay(200);
            }
            
            // âœ… Use ButtonHandle for faster response
            if (buttons && buttons->specialPressed()) {
                done = true;
                prefs.putInt("theme", theme);
            }
            
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Color Theme", nullptr);
            
            int previewY = 50;
            for (int i = 0; i < 7; i++) {
                bool selected = (i == theme);
                int y = previewY + (i * 24);
                
                uint16_t bgColor = selected ? COLOR_SELECTED : COLOR_CARD;
                sprite->fillRoundRect(20, y, 280, 20, 6, bgColor);
                sprite->drawRoundRect(20, y, 280, 20, 6, selected ? COLOR_ACCENT : COLOR_BORDER);
                
                sprite->setTextColor(COLOR_TEXT, bgColor);
                sprite->setTextDatum(ML_DATUM);
                sprite->drawString(themeNames[i], 30, y + 10, 2);
                
                sprite->fillCircle(270, y + 10, 6, selected ? COLOR_ACCENT : COLOR_TEXT_DIM);
            }
            
            UIHelper::drawFooter(sprite, "Rotate: Select  |  Press: Apply  |  HOME: Save");
            sprite->pushSprite(0, 0);
            
            delay(10);
        }
    }
    
    void createCustomTheme() {
        extern CustomTheme customTheme;
        customTheme.load();
        
        enum ColorComponent { BG, CARD, ACCENT };
        ColorComponent editing = BG;
        
        uint8_t r = 0, g = 0, b = 0;
        uint8_t* currentChannel = &r;
        int channelIndex = 0;
        
        auto extractRGB = [](uint16_t color565, uint8_t& r, uint8_t& g, uint8_t& b) {
            r = (color565 >> 11) << 3;
            g = ((color565 >> 5) & 0x3F) << 2;
            b = (color565 & 0x1F) << 3;
        };
        
        extractRGB(customTheme.bg, r, g, b);
        
        encoder->reset();
        encoder->setBoundaries(0, 255, false);
        encoder->setEncoderValue(r);
        
        bool done = false;
        int lastEncoderVal = r;
        
        while (!done) {
            // âœ… Update buttons every loop
            if (buttons) buttons->update();
            
            if (encoder->encoderChanged()) {
                int val = encoder->readEncoder();
                if (val != lastEncoderVal) {
                    if (FFat.exists("/sounds/feedback/rotary_encoder.wav")) {
                        audioConnecttoFFat("/sounds/feedback/rotary_encoder.wav");
                    }
                    lastEncoderVal = val;
                }
                *currentChannel = val;
                
                uint16_t newColor = rgb565(r, g, b);
                if (editing == BG) customTheme.bg = newColor;
                else if (editing == CARD) customTheme.card = newColor;
                else customTheme.accent = newColor;
            }
            
            if (encoder->isEncoderButtonClicked()) {
                channelIndex = (channelIndex + 1) % 3;
                if (channelIndex == 0) currentChannel = &r;
                else if (channelIndex == 1) currentChannel = &g;
                else currentChannel = &b;
                
                encoder->setEncoderValue(*currentChannel);
                delay(200);
            }
            
            // âœ… Use ButtonHandle for component switch
            if (buttons && buttons->delPressed()) {
                editing = (ColorComponent)((editing + 1) % 3);
                
                uint16_t currentColor = (editing == BG) ? customTheme.bg : 
                                       (editing == CARD) ? customTheme.card : customTheme.accent;
                extractRGB(currentColor, r, g, b);
                
                channelIndex = 0;
                currentChannel = &r;
                encoder->setEncoderValue(r);
            }
            
            // âœ… Use ButtonHandle for save
            if (buttons && buttons->specialPressed()) {
                customTheme.save();
                done = true;
            }
            
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Custom Theme", nullptr);
            
            const char* componentNames[] = {"Background", "Card", "Accent"};
            sprite->setTextColor(COLOR_TEXT);
            sprite->setTextDatum(MC_DATUM);
            sprite->drawString(componentNames[editing], SCREEN_WIDTH/2, 50, 2);
            
            uint16_t previewColor = rgb565(r, g, b);
            sprite->fillRoundRect(80, 70, 160, 60, 12, previewColor);
            sprite->drawRoundRect(80, 70, 160, 60, 12, COLOR_BORDER);
            
            const char* channels[] = {"R", "G", "B"};
            uint8_t values[] = {r, g, b};
            
            for (int i = 0; i < 3; i++) {
                int y = 140 + (i * 20);
                bool active = (i == channelIndex);
                
                sprite->setTextColor(active ? COLOR_ACCENT : COLOR_TEXT_DIM);
                sprite->setTextDatum(ML_DATUM);
                sprite->drawString(channels[i], 20, y + 8, 2);
                
                sprite->fillRoundRect(50, y, 200, 16, 4, COLOR_CARD);
                int barWidth = map(values[i], 0, 255, 0, 200);
                sprite->fillRoundRect(50, y, barWidth, 16, 4, active ? COLOR_ACCENT : COLOR_TEXT_DIM);
                
                sprite->setTextColor(COLOR_TEXT);
                sprite->setTextDatum(MR_DATUM);
                sprite->drawString(String(values[i]), 280, y + 8, 2);
            }
            
            UIHelper::drawFooter(sprite, "Press: Channel  |  CAM: Component  |  HOME: Save");
            sprite->pushSprite(0, 0);
            
            delay(10);
        }
    }

    void adjustBrightness() {
        int brightness = prefs.getInt("brightness", 255);
        
        encoder->reset();
        encoder->setBoundaries(20, 255, false);
        encoder->setEncoderValue(brightness);
        
        bool done = false;
        
        while (!done) {
            // âœ… Update buttons every loop
            if (buttons) buttons->update();
            
            if (encoder->encoderChanged()) {
                brightness = encoder->readEncoder();
                analogWrite(TFT_BL, brightness);
            }
            
            // âœ… Use ButtonHandle for save/cancel
            if (encoder->isEncoderButtonClicked() || (buttons && buttons->specialPressed())) {
                done = true;
                prefs.putInt("brightness", brightness);
            }
            
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Brightness", nullptr);
            
            drawSlider(30, 80, 260, brightness, 255, "Display Brightness", "");
            
            UIHelper::drawFooter(sprite, "Rotate: Adjust  |  Press: Save  |  HOME: Cancel");
            sprite->pushSprite(0, 0);
            
            delay(10);
        }
        
        analogWrite(TFT_BL, brightness);
    }
    
    void adjustTimeout() {
        int timeout = prefs.getInt("timeout", 30);
        
        encoder->reset();
        encoder->setBoundaries(10, 300, false);
        encoder->setEncoderValue(timeout);
        
        bool done = false;
        
        while (!done) {
            // âœ… Update buttons every loop
            if (buttons) buttons->update();
            
            if (encoder->encoderChanged()) {
                timeout = encoder->readEncoder();
            }
            
            // âœ… Use ButtonHandle for save/cancel
            if (encoder->isEncoderButtonClicked() || (buttons && buttons->specialPressed())) {
                done = true;
                prefs.putInt("timeout", timeout);
            }
            
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Display Timeout", nullptr);
            
            drawSlider(30, 80, 260, timeout, 300, "Sleep Timeout", "s");
            
            UIHelper::drawFooter(sprite, "Rotate: Adjust  |  Press: Save  |  HOME: Cancel");
            sprite->pushSprite(0, 0);
            
            delay(10);
        }
    }
    
    void adjustVolume() {
        int volume = prefs.getInt("volume", 15);
        
        encoder->reset();
        encoder->setBoundaries(0, 21, false);
        encoder->setEncoderValue(volume);
        
        bool done = false;
        
        while (!done) {
            // âœ… Update buttons every loop
            if (buttons) buttons->update();
            
            if (encoder->encoderChanged()) {
                volume = encoder->readEncoder();
            }
            
            // âœ… Use ButtonHandle for save/cancel
            if (encoder->isEncoderButtonClicked() || (buttons && buttons->specialPressed())) {
                done = true;
                prefs.putInt("volume", volume);
            }
            
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Volume Control", nullptr);
            
            drawSlider(30, 80, 260, volume, 21, "Audio Volume", "");
            
            UIHelper::drawFooter(sprite, "Rotate: Adjust  |  Press: Save  |  HOME: Cancel");
            sprite->pushSprite(0, 0);
            
            delay(10);
        }
    }
    
    bool confirmRestart() {
        encoder->reset();
        encoder->setBoundaries(0, 1, true);
        encoder->setEncoderValue(0);
        
        bool confirmed = false;
        bool done = false;
        
        while (!done) {
            // âœ… Update buttons every loop
            if (buttons) buttons->update();
            
            int selection = encoder->readEncoder();
            
            if (encoder->isEncoderButtonClicked()) {
                confirmed = (selection == 1);
                done = true;
                delay(200);
            }
            
            // âœ… Use ButtonHandle for cancel
            if (buttons && buttons->specialPressed()) {
                done = true;
            }
            
            sprite->fillSprite(COLOR_BG);
            drawConfirmDialog("Restart Device", "Are you sure?", selection == 1);
            sprite->pushSprite(0, 0);
            
            delay(10);
        }
        
        return confirmed;
    }
    
    bool confirmFactoryReset() {
        encoder->reset();
        encoder->setBoundaries(0, 1, true);
        encoder->setEncoderValue(0);
        
        bool confirmed = false;
        bool done = false;
        
        while (!done) {
            // âœ… Update buttons every loop
            if (buttons) buttons->update();
            
            int selection = encoder->readEncoder();
            
            if (encoder->isEncoderButtonClicked()) {
                confirmed = (selection == 1);
                done = true;
                delay(200);
            }
            
            // âœ… Use ButtonHandle for cancel
            if (buttons && buttons->specialPressed()) {
                done = true;
            }
            
            sprite->fillSprite(COLOR_BG);
            drawConfirmDialog("Factory Reset", "This will erase ALL data!", selection == 1);
            sprite->pushSprite(0, 0);
            
            delay(10);
        }
        
        return confirmed;
    }
    
    void factoryReset() {
        prefs.clear();
        prefs.end();
        
        FFat.format();
        
        sprite->fillSprite(COLOR_BG);
        UIHelper::drawHeader(sprite, "Factory Reset", nullptr);
        UIHelper::drawMessage(sprite, "Reset complete. Restarting...", COLOR_SUCCESS);
        sprite->pushSprite(0, 0);
        
        delay(2000);
        ESP.restart();
    }
    
    int getBrightness() {
        return prefs.getInt("brightness", 255);
    }
    
    int getTimeout() {
        return prefs.getInt("timeout", 30);
    }
    
    int getVolume() {
        return prefs.getInt("volume", 15);
    }
};

#endif