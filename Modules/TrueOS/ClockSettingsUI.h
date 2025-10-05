// ClockSettingsUI.h - Fixed with WiFi Sync, Date & Alarm Settings
#ifndef CLOCKSETTINGSUI_H
#define CLOCKSETTINGSUI_H

#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <DS3231.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"
#include "AudioManager.h"
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
        MODE_ALARM_LIST,
        MODE_WIFI_SYNC
    };
    
    struct Alarm {
        uint8_t hour;
        uint8_t minute;
        bool enabled;
        String label;
    };
    
    Alarm alarms[5];
    int alarmCount = 0;
    
    void playTickSound() {
        if (FFat.exists("/sounds/feedback/rotary_encoder.wav")) {
            audioConnecttoFFat("/sounds/feedback/rotary_encoder.wav");
        }
    }
    
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
    
    bool syncTimeWithWiFi() {
        if (WiFi.status() != WL_CONNECTED) {
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "WiFi Sync", nullptr);
            UIHelper::drawMessage(sprite, "WiFi not connected!", COLOR_ERROR);
            sprite->pushSprite(0, 0);
            delay(2000);
            return false;
        }
        
        sprite->fillSprite(COLOR_BG);
        UIHelper::drawHeader(sprite, "Syncing Time", nullptr);
        sprite->setTextColor(COLOR_TEXT);
        sprite->setTextDatum(MC_DATUM);
        sprite->drawString("Connecting to NTP...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 2);
        sprite->pushSprite(0, 0);
        
        configTime(6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
        
        struct tm timeInfo;
        int attempts = 0;
        while (!getLocalTime(&timeInfo) && attempts < 10) {
            delay(500);
            attempts++;
        }
        
        if (attempts >= 10) {
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Sync Failed", nullptr);
            UIHelper::drawMessage(sprite, "Could not reach NTP server", COLOR_ERROR);
            sprite->pushSprite(0, 0);
            delay(2000);
            return false;
        }
        
        // Set RTC
        rtc->setClockMode(false);
        rtc->setYear(timeInfo.tm_year - 100);
        rtc->setMonth(timeInfo.tm_mon + 1);
        rtc->setDate(timeInfo.tm_mday);
        rtc->setDoW(timeInfo.tm_wday + 1);
        rtc->setHour(timeInfo.tm_hour);
        rtc->setMinute(timeInfo.tm_min);
        rtc->setSecond(timeInfo.tm_sec);
        
        sprite->fillSprite(COLOR_BG);
        UIHelper::drawHeader(sprite, "Sync Complete", nullptr);
        sprite->setTextColor(COLOR_SUCCESS);
        sprite->setTextDatum(MC_DATUM);
        sprite->drawString("Time synchronized!", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 2);
        
        char timeStr[30];
        sprintf(timeStr, "%02d:%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
        sprite->setTextColor(COLOR_TEXT);
        sprite->drawString(timeStr, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10, 2);
        
        char dateStr[30];
        sprintf(dateStr, "%02d/%02d/%04d", timeInfo.tm_mday, timeInfo.tm_mon + 1, timeInfo.tm_year + 1900);
        sprite->drawString(dateStr, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 35, 2);
        
        sprite->pushSprite(0, 0);
        delay(2000);
        return true;
    }
    
    void showTimeSettings() {
        bool h12, PM;
        uint8_t hour = rtc->getHour(h12, PM);
        uint8_t minute = rtc->getMinute();
        uint8_t second = rtc->getSecond();
        
        int editField = 0;
        encoder->reset();
        encoder->setBoundaries(0, 2, true);
        
        bool done = false;
        bool editing = false;
        int lastEncoderVal = 0;
        
        while (!done) {
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
                continue;
            }
            
            if (encoder->encoderChanged()) {
                int val = encoder->readEncoder();
                if (val != lastEncoderVal) {
                    playTickSound();
                    lastEncoderVal = val;
                }
                
                if (editing) {
                    if (editField == 0) hour = val;
                    else if (editField == 1) minute = val;
                    else second = val;
                } else {
                    editField = val;
                }
            }
            
            if (encoder->isEncoderButtonClicked()) {
                if (editing) {
                    rtc->setHour(hour);
                    rtc->setMinute(minute);
                    rtc->setSecond(second);
                    editing = false;
                    encoder->reset();
                    encoder->setBoundaries(0, 2, true);
                    lastEncoderVal = 0;
                } else {
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
                    lastEncoderVal = encoder->readEncoder();
                }
                delay(200);
            }
            
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Set Time", editing ? "EDITING" : "Select");
            
            char timeStr[10];
            sprintf(timeStr, "%02d:%02d:%02d", hour, minute, second);
            
            sprite->setTextColor(COLOR_ACCENT);
            sprite->setTextDatum(MC_DATUM);
            sprite->setTextSize(4);
            sprite->drawString(timeStr, SCREEN_WIDTH / 2, 80);
            sprite->setTextSize(1);
            
            int fieldY = 130;
            const char* fields[] = {"Hour", "Minute", "Second"};
            for (int i = 0; i < 3; i++) {
                uint16_t color = (i == editField) ? 
                    (editing ? COLOR_SUCCESS : COLOR_ACCENT) : COLOR_TEXT_DIM;
                sprite->setTextColor(color);
                sprite->drawString(fields[i], 80 + i * 80, fieldY, 2);
            }
            
            UIHelper::drawFooter(sprite, 
                editing ? "Rotate: Change  |  Press: Save" : 
                         "Rotate: Select  |  Press: Edit  |  HOME: Back");
            
            sprite->pushSprite(0, 0);
            delay(10);
        }
    }
    
    void showDateSettings() {
        bool century;
        uint8_t day = rtc->getDate();
        uint8_t month = rtc->getMonth(century);
        uint16_t year = rtc->getYear() + 2000;
        
        int editField = 0;
        encoder->reset();
        encoder->setBoundaries(0, 2, true);
        
        bool done = false;
        bool editing = false;
        int lastEncoderVal = 0;
        
        while (!done) {
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
                continue;
            }
            
            if (encoder->encoderChanged()) {
                int val = encoder->readEncoder();
                if (val != lastEncoderVal) {
                    playTickSound();
                    lastEncoderVal = val;
                }
                
                if (editing) {
                    if (editField == 0) day = val;
                    else if (editField == 1) month = val;
                    else year = val;
                } else {
                    editField = val;
                }
            }
            
            if (encoder->isEncoderButtonClicked()) {
                if (editing) {
                    rtc->setDate(day);
                    rtc->setMonth(month);
                    rtc->setYear(year - 2000);
                    editing = false;
                    encoder->reset();
                    encoder->setBoundaries(0, 2, true);
                    lastEncoderVal = 0;
                } else {
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
                    lastEncoderVal = encoder->readEncoder();
                }
                delay(200);
            }
            
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Set Date", editing ? "EDITING" : "Select");
            
            char dateStr[15];
            sprintf(dateStr, "%02d/%02d/%04d", day, month, year);
            
            sprite->setTextColor(COLOR_ACCENT);
            sprite->setTextDatum(MC_DATUM);
            sprite->setTextSize(4);
            sprite->drawString(dateStr, SCREEN_WIDTH / 2, 80);
            sprite->setTextSize(1);
            
            int fieldY = 130;
            const char* fields[] = {"Day", "Month", "Year"};
            for (int i = 0; i < 3; i++) {
                uint16_t color = (i == editField) ? 
                    (editing ? COLOR_SUCCESS : COLOR_ACCENT) : COLOR_TEXT_DIM;
                sprite->setTextColor(color);
                sprite->drawString(fields[i], 80 + i * 80, fieldY, 2);
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
        encoder->setBoundaries(0, alarmCount, true);
        
        bool done = false;
        int lastEncoderVal = 0;
        
        while (!done) {
            if (encoder->encoderChanged()) {
                int val = encoder->readEncoder();
                if (val != lastEncoderVal) {
                    playTickSound();
                    lastEncoderVal = val;
                }
                selectedAlarm = val;
            }
            
            if (encoder->isEncoderButtonClicked()) {
                if (selectedAlarm < alarmCount) {
                    editAlarm(selectedAlarm);
                    encoder->reset();
                    encoder->setBoundaries(0, alarmCount, true);
                } else if (alarmCount < 5) {
                    addNewAlarm();
                    encoder->reset();
                    encoder->setBoundaries(0, alarmCount, true);
                }
                lastEncoderVal = encoder->readEncoder();
                delay(200);
            }
            
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
            }
            
            sprite->fillSprite(COLOR_BG);
            char subtitle[10];
            snprintf(subtitle, sizeof(subtitle), "%d/5", alarmCount);
            UIHelper::drawHeader(sprite, "Alarms", subtitle);
            
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
        
        int editField = 0;
        encoder->reset();
        encoder->setBoundaries(0, 2, true);
        
        bool done = false;
        bool editing = false;
        uint8_t hour = alarms[index].hour;
        uint8_t minute = alarms[index].minute;
        bool enabled = alarms[index].enabled;
        int lastEncoderVal = 0;
        
        while (!done) {
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
                continue;
            }
            
            if (digitalRead(BTN_DEL) == LOW) {
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
                int val = encoder->readEncoder();
                if (val != lastEncoderVal) {
                    playTickSound();
                    lastEncoderVal = val;
                }
                
                if (editing) {
                    if (editField == 0) hour = val;
                    else if (editField == 1) minute = val;
                } else {
                    editField = val;
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
                    lastEncoderVal = 0;
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
                        lastEncoderVal = encoder->readEncoder();
                    }
                }
                delay(200);
            }
            
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Edit Alarm", editing ? "EDITING" : nullptr);
            
            char timeStr[10];
            sprintf(timeStr, "%02d:%02d", hour, minute);
            
            sprite->setTextColor(enabled ? COLOR_SUCCESS : COLOR_TEXT_DIM);
            sprite->setTextDatum(MC_DATUM);
            sprite->setTextSize(4);
            sprite->drawString(timeStr, SCREEN_WIDTH / 2, 80);
            sprite->setTextSize(1);
            
            int fieldY = 130;
            const char* fields[] = {"Hour", "Minute", enabled ? "ON" : "OFF"};
            for (int i = 0; i < 3; i++) {
                uint16_t color = (i == editField) ? 
                    (editing ? COLOR_SUCCESS : COLOR_ACCENT) : COLOR_TEXT_DIM;
                sprite->setTextColor(color);
                sprite->drawString(fields[i], 60 + i * 100, fieldY, 2);
            }
            
            UIHelper::drawFooter(sprite, "CAM: Delete  |  Press: Save  |  HOME: Back");
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
        encoder->setBoundaries(0, 3, true);
        
        bool done = false;
        int lastEncoderVal = 0;
        
        while (!done) {
            if (encoder->encoderChanged()) {
                int val = encoder->readEncoder();
                if (val != lastEncoderVal) {
                    playTickSound();
                    lastEncoderVal = val;
                }
                selectedOption = val;
            }
            
            if (encoder->isEncoderButtonClicked()) {
                if (selectedOption == 0) {
                    showTimeSettings();
                } else if (selectedOption == 1) {
                    showDateSettings();
                } else if (selectedOption == 2) {
                    showAlarmSettings();
                } else if (selectedOption == 3) {
                    syncTimeWithWiFi();
                }
                encoder->reset();
                encoder->setBoundaries(0, 3, true);
                lastEncoderVal = 0;
                delay(200);
            }
            
            if (digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
            }
            
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Clock Settings", nullptr);
            
            bool h12, PM;
            uint8_t hour = rtc->getHour(h12, PM);
            uint8_t minute = rtc->getMinute();
            
            char timeStr[10], dateStr[20], alarmStr[20];
            sprintf(timeStr, "%02d:%02d", hour, minute);
            
            bool century;
            sprintf(dateStr, "%02d/%02d/%04d", 
                rtc->getDate(), rtc->getMonth(century), rtc->getYear() + 2000);
            
            sprintf(alarmStr, "%d alarm%s", alarmCount, alarmCount == 1 ? "" : "s");
            
            drawSettingItem("Set Time", timeStr, 60, selectedOption == 0);
            drawSettingItem("Set Date", dateStr, 100, selectedOption == 1);
            drawSettingItem("Alarms", alarmStr, 140, selectedOption == 2);
            drawSettingItem("WiFi Sync", WiFi.status() == WL_CONNECTED ? "Ready" : "Offline", 
                          180, selectedOption == 3);
            
            UIHelper::drawFooter(sprite, "Rotate: Select  |  Press: Open  |  HOME: Back");
            
            sprite->pushSprite(0, 0);
            delay(10);
        }
    }
};

#endif