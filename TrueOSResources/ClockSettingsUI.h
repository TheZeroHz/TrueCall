#ifndef CLOCKSETTINGSUI_H
#define CLOCKSETTINGSUI_H

#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <DS3231.h>
#include <Preferences.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"

class ClockSettingsUI {
private:
    TFT_eSprite* sprite;
    AiEsp32RotaryEncoder* encoder;
    DS3231* rtc;
    Preferences prefs;
    
    enum SettingMode {
        MODE_MENU,
        MODE_SET_TIME,
        MODE_SET_DATE,
        MODE_SET_ALARM,
        MODE_ALARM_LIST
    };
    
    struct Alarm {
        uint8_t hour;
        uint8_t minute;
        bool enabled;
        String label;
    };
    
    Alarm alarms[5];
    int alarmCount = 0;
    
    void drawSettingItem(const char* label, const char* value, int y, bool selected) {
        uint16_t bgColor = selected ? COLOR_SELECTED : COLOR_CARD;
        sprite->fillRoundRect(20, y, 280, 35, 8, bgColor);
        sprite->drawRoundRect(20, y, 280, 35, 8, selected ? COLOR_ACCENT : COLOR_BORDER);
        
        sprite->setTextColor(COLOR_TEXT);
        sprite->setTextDatum(ML_DATUM);
        sprite->drawString(label, 30, y + 17, 2);
        
        sprite->setTextColor(COLOR_ACCENT);
        sprite->setTextDatum(MR_DATUM);
        sprite->drawString(value, 290, y + 17, 2);
    }
    
    void showTimeSettings() {
        bool h12, PM;
        uint8_t hour = rtc->getHour(h12, PM);
        uint8_t minute = rtc->getMinute();
        uint8_t second = rtc->getSecond();
        
        int editField = 0; // 0=hour, 1=minute, 2=second
        encoder->reset();
        encoder->setBoundaries(0, 2, true);
        
        bool done = false;
        bool editing = false;
        
        while (!done) {
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
                continue;
            }
            
            if (encoder->encoderChanged()) {
                if (editing) {
                    int val = encoder->readEncoder();
                    if (editField == 0) hour = val;
                    else if (editField == 1) minute = val;
                    else second = val;
                } else {
                    editField = encoder->readEncoder();
                }
            }
            
            if (encoder->isEncoderButtonClicked()) {
                if (editing) {
                    // Save and exit editing
                    rtc->setHour(hour);
                    rtc->setMinute(minute);
                    rtc->setSecond(second);
                    editing = false;
                    encoder->reset();
                    encoder->setBoundaries(0, 2, true);
                } else {
                    // Enter editing mode
                    editing = true;
                    if (editField == 0) {
                        encoder->reset();
                        encoder->setBoundaries(0, 23, true);
                        encoder->setEncoderValue(hour);
                    } else if (editField == 1) {
                        encoder->reset();
                        encoder->setBoundaries(0, 59, true);
                        encoder->setEncoderValue(minute);
                    } else {
                        encoder->reset();
                        encoder->setBoundaries(0, 59, true);
                        encoder->setEncoderValue(second);
                    }
                }
                delay(300);
            }
            
            // Draw
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Set Time", editing ? "EDITING" : "Select");
            
            char timeStr[10];
            sprintf(timeStr, "%02d:%02d:%02d", hour, minute, second);
            
            sprite->setTextColor(COLOR_ACCENT);
            sprite->setTextDatum(MC_DATUM);
            sprite->setTextSize(4);
            sprite->drawString(timeStr, SCREEN_WIDTH / 2, 80);
            
            // Field indicators
            int fieldY = 120;
            const char* fields[] = {"Hour", "Minute", "Second"};
            for (int i = 0; i < 3; i++) {
                uint16_t color = (i == editField) ? 
                    (editing ? COLOR_SUCCESS : COLOR_ACCENT) : COLOR_TEXT_DIM;
                sprite->setTextColor(color);
                sprite->setTextSize(1);
                sprite->drawString(fields[i], 80 + i * 80, fieldY);
            }
            
            UIHelper::drawFooter(sprite, 
                editing ? "Rotate: Change  |  Press: Save" : 
                         "Rotate: Select  |  Press: Edit  |  HOME: Back");
            
            sprite->pushSprite(0, 0);
            delay(10);
        }
    }
    
public:
    
    ClockSettingsUI(TFT_eSprite* spr, AiEsp32RotaryEncoder* enc, DS3231* r) 
        : sprite(spr), encoder(enc), rtc(r) {
        prefs.begin("alarms", false);
        loadAlarms();
    }
    void showDateSettings() {
        bool century;
        uint8_t day = rtc->getDate();
        uint8_t month = rtc->getMonth(century);
        uint16_t year = rtc->getYear() + 2000;
        
        int editField = 0; // 0=day, 1=month, 2=year
        encoder->reset();
        encoder->setBoundaries(0, 2, true);
        
        bool done = false;
        bool editing = false;
        
        while (!done) {
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
                continue;
            }
            
            if (encoder->encoderChanged()) {
                if (editing) {
                    int val = encoder->readEncoder();
                    if (editField == 0) day = val;
                    else if (editField == 1) month = val;
                    else year = val;
                } else {
                    editField = encoder->readEncoder();
                }
            }
            
            if (encoder->isEncoderButtonClicked()) {
                if (editing) {
                    // Save and exit editing
                    rtc->setDate(day);
                    rtc->setMonth(month);
                    rtc->setYear(year - 2000);
                    editing = false;
                    encoder->reset();
                    encoder->setBoundaries(0, 2, true);
                } else {
                    // Enter editing mode
                    editing = true;
                    if (editField == 0) {
                        encoder->reset();
                        encoder->setBoundaries(1, 31, true);
                        encoder->setEncoderValue(day);
                    } else if (editField == 1) {
                        encoder->reset();
                        encoder->setBoundaries(1, 12, true);
                        encoder->setEncoderValue(month);
                    } else {
                        encoder->reset();
                        encoder->setBoundaries(2020, 2100, true);
                        encoder->setEncoderValue(year);
                    }
                }
                delay(300);
            }
            
            // Draw
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Set Date", editing ? "EDITING" : "Select");
            
            char dateStr[15];
            sprintf(dateStr, "%02d/%02d/%04d", day, month, year);
            
            sprite->setTextColor(COLOR_ACCENT);
            sprite->setTextDatum(MC_DATUM);
            sprite->setTextSize(4);
            sprite->drawString(dateStr, SCREEN_WIDTH / 2, 80);
            
            // Field indicators
            int fieldY = 120;
            const char* fields[] = {"Day", "Month", "Year"};
            for (int i = 0; i < 3; i++) {
                uint16_t color = (i == editField) ? 
                    (editing ? COLOR_SUCCESS : COLOR_ACCENT) : COLOR_TEXT_DIM;
                sprite->setTextColor(color);
                sprite->setTextSize(1);
                sprite->drawString(fields[i], 80 + i * 80, fieldY);
            }
            
            UIHelper::drawFooter(sprite, 
                editing ? "Rotate: Change  |  Press: Save" : 
                         "Rotate: Select  |  Press: Edit  |  HOME: Back");
            
            sprite->pushSprite(0, 0);
            delay(10);
        }
    }

    void showAlarmSettings() {
        int selectedAlarm = 0;
        encoder->reset();
        encoder->setBoundaries(0, alarmCount, true); // +1 for "Add New"
        
        bool done = false;
        
        while (!done) {
            if (encoder->encoderChanged()) {
                selectedAlarm = encoder->readEncoder();
            }
            
            if (encoder->isEncoderButtonClicked()) {
                if (selectedAlarm < alarmCount) {
                    editAlarm(selectedAlarm);
                } else if (alarmCount < 5) {
                    addNewAlarm();
                    encoder->setBoundaries(0, alarmCount, true);
                }
                delay(300);
            }
            
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
            }
            
            // Draw
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Alarms", nullptr);
            
            int y = 50;
            for (int i = 0; i < alarmCount && y < SCREEN_HEIGHT - 60; i++) {
                bool selected = (i == selectedAlarm);
                uint16_t bgColor = selected ? COLOR_SELECTED : COLOR_CARD;
                
                sprite->fillRoundRect(10, y, 300, 30, 8, bgColor);
                sprite->drawRoundRect(10, y, 300, 30, 8, selected ? COLOR_ACCENT : COLOR_BORDER);
                
                char timeStr[10];
                sprintf(timeStr, "%02d:%02d", alarms[i].hour, alarms[i].minute);
                
                sprite->setTextColor(alarms[i].enabled ? COLOR_SUCCESS : COLOR_TEXT_DIM);
                sprite->setTextDatum(ML_DATUM);
                sprite->drawString(timeStr, 20, y + 15, 2);
                
                sprite->setTextColor(COLOR_TEXT);
                sprite->drawString(alarms[i].label, 100, y + 15, 2);
                
                y += 35;
            }
            
            // Add New button
            if (alarmCount < 5) {
                bool selected = (selectedAlarm == alarmCount);
                uint16_t bgColor = selected ? COLOR_SELECTED : COLOR_CARD;
                
                sprite->fillRoundRect(10, y, 300, 30, 8, bgColor);
                sprite->drawRoundRect(10, y, 300, 30, 8, selected ? COLOR_ACCENT : COLOR_BORDER);
                
                sprite->setTextColor(COLOR_ACCENT);
                sprite->setTextDatum(MC_DATUM);
                sprite->drawString("+ Add New Alarm", SCREEN_WIDTH / 2, y + 15, 2);
            }
            
            UIHelper::drawFooter(sprite, "Rotate: Select  |  Press: Edit  |  HOME: Back");
            sprite->pushSprite(0, 0);
            delay(10);
        }
    }

        void addNewAlarm() {
        if (alarmCount >= 5) return;
        
        alarms[alarmCount].hour = 8;
        alarms[alarmCount].minute = 0;
        alarms[alarmCount].enabled = true;
        alarms[alarmCount].label = "Alarm " + String(alarmCount + 1);
        alarmCount++;
        saveAlarms();
    }
    
    void editAlarm(int index) {
        if (index < 0 || index >= alarmCount) return;
        
        int editField = 0; // 0=hour, 1=minute, 2=toggle
        encoder->reset();
        encoder->setBoundaries(0, 2, true);
        
        bool done = false;
        bool editing = false;
        uint8_t hour = alarms[index].hour;
        uint8_t minute = alarms[index].minute;
        bool enabled = alarms[index].enabled;
        
        while (!done) {
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
                continue;
            }
            
            if (digitalRead(BTN_DEL) == LOW) {
                // Delete alarm
                for (int i = index; i < alarmCount - 1; i++) {
                    alarms[i] = alarms[i + 1];
                }
                alarmCount--;
                saveAlarms();
                done = true;
                delay(300);
                continue;
            }
            
            if (encoder->encoderChanged()) {
                if (editing) {
                    int val = encoder->readEncoder();
                    if (editField == 0) hour = val;
                    else if (editField == 1) minute = val;
                } else {
                    editField = encoder->readEncoder();
                }
            }
            
            if (encoder->isEncoderButtonClicked()) {
                if (editing) {
                    alarms[index].hour = hour;
                    alarms[index].minute = minute;
                    alarms[index].enabled = enabled;
                    saveAlarms();
                    editing = false;
                    encoder->reset();
                    encoder->setBoundaries(0, 2, true);
                } else {
                    if (editField == 2) {
                        enabled = !enabled;
                        alarms[index].enabled = enabled;
                        saveAlarms();
                    } else {
                        editing = true;
                        if (editField == 0) {
                            encoder->reset();
                            encoder->setBoundaries(0, 23, true);
                            encoder->setEncoderValue(hour);
                        } else {
                            encoder->reset();
                            encoder->setBoundaries(0, 59, true);
                            encoder->setEncoderValue(minute);
                        }
                    }
                }
                delay(300);
            }
            
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Edit Alarm", editing ? "EDITING" : nullptr);
            
            char timeStr[10];
            sprintf(timeStr, "%02d:%02d", hour, minute);
            
            sprite->setTextColor(enabled ? COLOR_SUCCESS : COLOR_TEXT_DIM);
            sprite->setTextDatum(MC_DATUM);
            sprite->setTextSize(4);
            sprite->drawString(timeStr, SCREEN_WIDTH / 2, 80);
            
            int fieldY = 120;
            const char* fields[] = {"Hour", "Minute", enabled ? "ON" : "OFF"};
            for (int i = 0; i < 3; i++) {
                uint16_t color = (i == editField) ? 
                    (editing ? COLOR_SUCCESS : COLOR_ACCENT) : COLOR_TEXT_DIM;
                sprite->setTextColor(color);
                sprite->setTextSize(1);
                sprite->drawString(fields[i], 60 + i * 100, fieldY);
            }
            
            UIHelper::drawFooter(sprite, "CAM: Delete  |  Press: Save  |  HOME: Back");
            sprite->pushSprite(0, 0);
            delay(10);
        }
    }
    void loadAlarms() {
        alarmCount = prefs.getInt("count", 0);
        for (int i = 0; i < alarmCount && i < 5; i++) {
            String key = "alarm" + String(i);
            alarms[i].hour = prefs.getUChar((key + "h").c_str(), 0);
            alarms[i].minute = prefs.getUChar((key + "m").c_str(), 0);
            alarms[i].enabled = prefs.getBool((key + "e").c_str(), false);
            alarms[i].label = prefs.getString((key + "l").c_str(), "Alarm");
        }
    }
    
    void saveAlarms() {
        prefs.putInt("count", alarmCount);
        for (int i = 0; i < alarmCount; i++) {
            String key = "alarm" + String(i);
            prefs.putUChar((key + "h").c_str(), alarms[i].hour);
            prefs.putUChar((key + "m").c_str(), alarms[i].minute);
            prefs.putBool((key + "e").c_str(), alarms[i].enabled);
            prefs.putString((key + "l").c_str(), alarms[i].label);
        }
    }
    
    void show() {
        int selectedOption = 0;
        encoder->reset();
        encoder->setBoundaries(0, 2, true);
        
        bool done = false;
        
        while (!done) {
            if (encoder->encoderChanged()) {
                selectedOption = encoder->readEncoder();
            }
            
            if (encoder->isEncoderButtonClicked()) {
                if (selectedOption == 0) {
                    showTimeSettings();
                } else if (selectedOption == 1) {
                    // Show date settings (similar to time)
                } else if (selectedOption == 2) {
                    // Show alarm settings
                }
                delay(300);
            }
            
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
            }
            
            // Draw menu
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Clock Settings", nullptr);
            
            bool h12, PM;
            uint8_t hour = rtc->getHour(h12, PM);
            uint8_t minute = rtc->getMinute();
            
            char timeStr[10], dateStr[20];
            sprintf(timeStr, "%02d:%02d", hour, minute);
            
            bool century;
            sprintf(dateStr, "%02d/%02d/%04d", 
                rtc->getDate(), rtc->getMonth(century), rtc->getYear() + 2000);
            
            drawSettingItem("Set Time", timeStr, 60, selectedOption == 0);
            drawSettingItem("Set Date", dateStr, 100, selectedOption == 1);
            
            char alarmStr[20];
            sprintf(alarmStr, "%d alarms", alarmCount);
            drawSettingItem("Alarms", alarmStr, 140, selectedOption == 2);
            
            UIHelper::drawFooter(sprite, "Rotate: Select  |  Press: Open  |  HOME: Back");
            
            sprite->pushSprite(0, 0);
            delay(10);
        }
    }
};

#endif