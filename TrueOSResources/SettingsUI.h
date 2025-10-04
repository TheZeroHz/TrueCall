// SettingsUI.h - Settings interface for various device settings
#ifndef SETTINGSUI_H
#define SETTINGSUI_H

#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <Preferences.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"

class SettingsUI {
private:
    TFT_eSprite* sprite;
    AiEsp32RotaryEncoder* encoder;
    Preferences prefs;
    
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
        : sprite(spr), encoder(enc) {}
    
    void begin() {
        prefs.begin("settings", false);
    }
    
    void adjustBrightness() {
        int brightness = prefs.getInt("brightness", 255);
        
        encoder->reset();
        encoder->setBoundaries(20, 255, false);
        encoder->setEncoderValue(brightness);
        
        bool done = false;
        
        while (!done) {
            if (encoder->encoderChanged()) {
                brightness = encoder->readEncoder();
                analogWrite(TFT_BL, brightness);
            }
            
            if (encoder->isEncoderButtonClicked() || digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                prefs.putInt("brightness", brightness);
                delay(300);
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
            if (encoder->encoderChanged()) {
                timeout = encoder->readEncoder();
            }
            
            if (encoder->isEncoderButtonClicked() || digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                prefs.putInt("timeout", timeout);
                delay(300);
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
            if (encoder->encoderChanged()) {
                volume = encoder->readEncoder();
            }
            
            if (encoder->isEncoderButtonClicked() || digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                prefs.putInt("volume", volume);
                delay(300);
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
            int selection = encoder->readEncoder();
            
            if (encoder->isEncoderButtonClicked()) {
                confirmed = (selection == 1);
                done = true;
                delay(300);
            }
            
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
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
            int selection = encoder->readEncoder();
            
            if (encoder->isEncoderButtonClicked()) {
                confirmed = (selection == 1);
                done = true;
                delay(300);
            }
            
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
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