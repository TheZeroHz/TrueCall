/*
 * TrueCallDesk - COMPLETE VERSION PART 1
 * ALL ORIGINAL FEATURES + Power button network checks + Encoder fixes
 * 
 * NEW FEATURES ADDED (not removed):
 *  Power single click = Check WiFi connection
 *  Power double click = ESP-NOW availability check  
 *  Power hold = Deep sleep
 *  Encoder resets on every mode change
 *  All original features preserved
 */

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <JPEGDecoder.h>
#include <AiEsp32RotaryEncoder.h>
#include <Wire.h>
#include <DS3231.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <driver/i2s.h>  //  NEW: For I2S tone generation
#include "esp_sleep.h"

#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"
#include "WiFiNetwork.h"
#include "WiFiScanner.h"
#include "CredentialsManager.h"
#include "DisplayManager.h"
#include "KeyboardUI.h"
#include "AIAssistant.h"
#include "AIChatUI.h"
#include "MenuSystemJPEG.h"
#include "SettingsUI.h"
#include "AboutUI.h"
#include "ClockSettingsUI.h"
#include "WallpaperManager.h"
#include "AppStateManager.h"
#include "AudioManager.h"
#include "BootScreen.h"
#include "ContactManager.h"
#include "ButtonHandle.h"
#include "SoundEffects.h"

// ==================== Global Objects (UNCHANGED) ====================
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite(&tft);
AiEsp32RotaryEncoder encoder(ENC_CLK, ENC_DT, ENC_SW, -1, ROTARY_STEPS);
DS3231 rtc;

DisplayManager* displayMgr = nullptr;
CredentialsManager* credsMgr = nullptr;
MenuSystem* menu = nullptr;
AppStateManager* appState = nullptr;
WallpaperManager* wallpaperMgr = nullptr;
BootScreen* bootScreen = nullptr;
ContactManager* contactMgr = nullptr;
ButtonHandle* buttons = nullptr;

ColorTheme currentTheme = THEME_DARK_BLUE;
CustomTheme customTheme;

// RTC State
uint16_t rtcYear = 2025;
uint8_t rtcMonth = 10, rtcDay = 4, rtcHour = 12, rtcMin = 0, rtcSec = 0;
uint8_t rtcDoW = 1;
unsigned long lastRTCSync = 0;
unsigned long lastButtonCheck = 0;
unsigned long powerButtonHoldStart = 0;
bool powerButtonHeld = false;
bool rtcAvailable = false;
volatile bool colonBlink = false;

//Power button click detection (ADDED, not replaced)
unsigned long lastPowerClick = 0;
int powerClickCount = 0;
#define DOUBLE_CLICK_TIME 500
#define POWER_HOLD_TIME 2500
#define WAKEUP_PIN BTN_SELECT
#define RTC_SYNC_INTERVAL 1000

TaskHandle_t colonBlinkTaskHandle = NULL;
TaskHandle_t rtcSyncTaskHandle = NULL;

// ==================== FreeRTOS Tasks (UNCHANGED) ====================
void colonBlinkTask(void* parameter) {
    while (true) {
        colonBlink = !colonBlink;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void rtcSyncTask(void* parameter) {
    while (true) {
        if (rtcAvailable) {
            bool h12, PM;
            rtcHour = rtc.getHour(h12, PM);
            rtcMin = rtc.getMinute();
            rtcSec = rtc.getSecond();
            bool century = false;
            rtcDay = rtc.getDate();
            rtcMonth = rtc.getMonth(century);
            rtcYear = rtc.getYear() + 2000;
            rtcDoW = rtc.getDoW();
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void IRAM_ATTR encoderISR() {
    encoder.readEncoder_ISR();
}


// WiFi connectivity check (no external library needed)
bool checkInternetConnection() {
    if (credsMgr && credsMgr->load() && credsMgr->getCount() > 0) {
        SavedNetwork* recent = credsMgr->getMostRecent();
        if (recent) {
            WiFi.begin(recent->ssid.c_str(), recent->password.c_str());
            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
                delay(100);
            }
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    // Try to connect to Google's DNS
    WiFiClient client;
    if (client.connect("8.8.8.8", 53, 2000)) { // 2 second timeout
        client.stop();
        return true;
    }
    
    // Try Cloudflare DNS as backup
    if (client.connect("1.1.1.1", 53, 2000)) {
        client.stop();
        return true;
    }
    
    return false;
}

bool checkESPNowPeer() {
    if (WiFi.status() != WL_CONNECTED) return false;
    
    uint8_t channel;
    wifi_second_chan_t secondChan;
    esp_wifi_get_channel(&channel, &secondChan);
    
    esp_now_deinit();
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    
    if (esp_now_init() != ESP_OK) return false;
    
    uint8_t testMAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, testMAC, 6);
    peerInfo.channel = channel;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        esp_now_deinit();
        return false;
    }
    
    char testMsg[] = "PING";
    esp_err_t result = esp_now_send(testMAC, (uint8_t*)testMsg, strlen(testMsg));
    esp_now_del_peer(testMAC);
    esp_now_deinit();
    return (result == ESP_OK);
}

void showNetworkCheckResult(bool success, const char* type) {
    //Play appropriate sound
    if (success) {
        //playSuccessSound();
    } else {
        //playErrorSound();
    }
    
    sprite.fillSprite(COLOR_BG);
    UIHelper::drawHeader(&sprite, type, nullptr);
    
    uint16_t statusColor = success ? COLOR_SUCCESS : COLOR_ERROR;
    int iconY = SCREEN_HEIGHT / 2 - 30;
    
    if (success) {
        sprite.drawCircle(SCREEN_WIDTH / 2, iconY, 25, statusColor);
        sprite.drawCircle(SCREEN_WIDTH / 2, iconY, 24, statusColor);
        sprite.drawLine(SCREEN_WIDTH / 2 - 12, iconY, SCREEN_WIDTH / 2 - 5, iconY + 8, statusColor);
        sprite.drawLine(SCREEN_WIDTH / 2 - 5, iconY + 9, SCREEN_WIDTH / 2 + 12, iconY - 7, statusColor);
    } else {
        sprite.drawCircle(SCREEN_WIDTH / 2, iconY, 25, statusColor);
        sprite.drawCircle(SCREEN_WIDTH / 2, iconY, 24, statusColor);
        sprite.drawLine(SCREEN_WIDTH / 2 - 10, iconY - 10, SCREEN_WIDTH / 2 + 10, iconY + 10, statusColor);
        sprite.drawLine(SCREEN_WIDTH / 2 + 10, iconY - 10, SCREEN_WIDTH / 2 - 10, iconY + 10, statusColor);
    }
    
    sprite.setTextColor(statusColor);
    sprite.setTextDatum(MC_DATUM);
    
    if (strcmp(type, "Internet Check") == 0) {
        sprite.drawString(success ? "Internet OK!" : "No Internet", SCREEN_WIDTH / 2, iconY + 45, 2);
        
        if (success) {
            sprite.setTextColor(COLOR_TEXT_DIM);
            char ipStr[20];
            sprintf(ipStr, "IP: %s", WiFi.localIP().toString().c_str());
            sprite.drawString(ipStr, SCREEN_WIDTH / 2, iconY + 70, 1);
        }
    } else {
        sprite.drawString(success ? "ESP-NOW Ready" : "ESP-NOW Failed", SCREEN_WIDTH / 2, iconY + 45, 2);
    }
    
    sprite.setTextColor(COLOR_TEXT_DIM);
    sprite.drawString("Auto-closing...", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 30, 1);
    sprite.pushSprite(0, 0);
    delay(2000);
}

//  COPY THIS ENTIRE PART 1 FIRST, THEN PART 2 REMAINS THE SAME

//  PART 2 - PASTE THIS AFTER PART 1

// ==================== AppStateManager Functions (WITH ENCODER RESETS) ====================
void AppStateManager::handleMenuSelection(AppMode selectedApp) {
    if (selectedApp == APP_WIFI_SCANNER) {
        currentMode = MODE_WIFI_SCANNER;
        if (!wifiScanner) {
            wifiScanner = new WiFiScanner();
            wifiScanner->begin();
        }
        
        displayMgr->resetSpinner();
        isScanning = true;
        
        sprite->fillSprite(COLOR_BG);
        UIHelper::drawHeader(sprite, "WiFi Scanner", "Starting...");
        sprite->pushSprite(0, 0);
        delay(800);
        
        wifiScanner->startScanBlocking();
        while (millis() < 1500) {
            displayMgr->showScanning();
            delay(50);
        }
        isScanning = false;
        
        // Encoder reset
        encoder->reset();
        encoder->setBoundaries(0, max(0, wifiScanner->getNetworkCount() - 1), true);
        encoder->setEncoderValue(0);
        selectedNetwork = 0;
        scrollOffset = 0;
        
    } else if (selectedApp == APP_AI_ASSISTANT) {
        if (WiFi.status() == WL_CONNECTED) {
            currentMode = MODE_AI_ASSISTANT;
            if (!aiAssistant) {
                aiAssistant = new AIAssistant();
                if (!aiAssistant->begin()) {
                    delete aiAssistant;
                    aiAssistant = nullptr;
                    currentMode = MODE_MENU;
                    return;
                }
            }
            if (!aiChatUI) aiChatUI = new AIChatUI(sprite, aiAssistant);
            // Encoder reset
            encoder->reset();
            encoder->setBoundaries(-1000, 1000, false);
            encoder->setEncoderValue(0);
        } else {
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Error", nullptr);
            UIHelper::drawMessage(sprite, "WiFi required!", COLOR_ERROR);
            sprite->pushSprite(0, 0);
            delay(1500);
        }
    } else if (selectedApp == APP_CLOCK_SETTINGS) {
        if (!clockSettingsUI) clockSettingsUI = new ClockSettingsUI(sprite, encoder, rtc);
        clockSettingsUI->show();
        //  Encoder reset
        encoder->reset();
        encoder->setBoundaries(0, menu->getItemCount() - 1, true);
        encoder->setEncoderValue(menu->getSelected());
    } else if (selectedApp == APP_ABOUT_CREDITS) {
        if (!aboutUI) aboutUI = new AboutUI(sprite, encoder, tft);
        aboutUI->show();
        //  Encoder reset
        encoder->reset();
        encoder->setBoundaries(0, menu->getItemCount() - 1, true);
        encoder->setEncoderValue(menu->getSelected());
    } else if (selectedApp == APP_SETTINGS) {
        menu->showSettings();
        //  Encoder reset
        encoder->reset();
        encoder->setBoundaries(0, menu->getItemCount() - 1, true);
        encoder->setEncoderValue(0);
        menu->draw();
    } else if (selectedApp == APP_DISPLAY_BRIGHTNESS) {
        settings->adjustBrightness();
        encoder->reset();
        encoder->setBoundaries(0, menu->getItemCount() - 1, true);
        encoder->setEncoderValue(menu->getSelected());
    } else if (selectedApp == APP_DISPLAY_TIMEOUT) {
        settings->adjustTimeout();
        encoder->reset();
        encoder->setBoundaries(0, menu->getItemCount() - 1, true);
        encoder->setEncoderValue(menu->getSelected());
    } else if (selectedApp == APP_VOLUME_CONTROL) {
        settings->adjustVolume();
        encoder->reset();
        encoder->setBoundaries(0, menu->getItemCount() - 1, true);
        encoder->setEncoderValue(menu->getSelected());
    } else if (selectedApp == APP_THEME_SELECTOR) {
        settings->selectTheme();
        encoder->reset();
        encoder->setBoundaries(0, menu->getItemCount() - 1, true);
        encoder->setEncoderValue(menu->getSelected());
    } else if (selectedApp == APP_RESTART) {
        if (settings->confirmRestart()) ESP.restart();
        encoder->reset();
        encoder->setBoundaries(0, menu->getItemCount() - 1, true);
        encoder->setEncoderValue(menu->getSelected());
    } else if (selectedApp == APP_FACTORY_RESET) {
        if (settings->confirmFactoryReset()) settings->factoryReset();
        encoder->reset();
        encoder->setBoundaries(0, menu->getItemCount() - 1, true);
        encoder->setEncoderValue(menu->getSelected());
    }
}



void syncWithRTC() {
    if (!rtcAvailable) return;
    bool h12, PM;
    rtcHour = rtc.getHour(h12, PM);
    rtcMin = rtc.getMinute();
    rtcSec = rtc.getSecond();
    bool century = false;
    rtcDay = rtc.getDate();
    rtcMonth = rtc.getMonth(century);
    rtcYear = rtc.getYear() + 2000;
    rtcDoW = rtc.getDoW();
}

void enterDeepSleep() {
    for (int i = 0; i < 10; i++) {
        sprite.fillSprite(COLOR_BG);
        sprite.setTextColor(COLOR_TEXT);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("Shutting Down", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 4);
        sprite.pushSprite(0, 0);
        delay(100);
    }
    
    for (int brightness = 255; brightness >= 0; brightness -= 5) {
        analogWrite(TFT_BL, brightness);
        delay(10);
    }
    
    esp_sleep_enable_ext1_wakeup((1ULL << ENC_CLK) | (1ULL << ENC_SW), ESP_EXT1_WAKEUP_ANY_LOW);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_deep_sleep_start();
}

//  ENHANCED: Power button with click detection
void checkPowerButton() {
    buttons->update();
    
    // Check for release (click detection)
    if (!buttons->scanHeld() && powerButtonHeld) {
        unsigned long holdDuration = millis() - powerButtonHoldStart;
        
        if (holdDuration < POWER_HOLD_TIME) {
            // It was a click, not a hold
            if (millis() - lastPowerClick < DOUBLE_CLICK_TIME) {
                // Double click!
                powerClickCount = 2;
                bool result = checkESPNowPeer();
                showNetworkCheckResult(result, "ESP-NOW Check");
                powerClickCount = 0;
                lastPowerClick = 0;
            } else {
                // First click
                powerClickCount = 1;
                lastPowerClick = millis();
            }
        }
        powerButtonHeld = false;
        powerButtonHoldStart = 0;
    }
    
    // Check for single click timeout
    if (powerClickCount == 1 && millis() - lastPowerClick > DOUBLE_CLICK_TIME) {
        // Single click confirmed!
        bool result = checkInternetConnection(); //CHANGED from pingGoogle()
        showNetworkCheckResult(result, "Internet Check");
        powerClickCount = 0;
        lastPowerClick = 0;
    }
    
    // Check for hold (sleep)
    if (buttons->scanHeld()) {
        if (!powerButtonHeld) {
            powerButtonHeld = true;
            powerButtonHoldStart = millis();
        }
        
        unsigned long holdDuration = millis() - powerButtonHoldStart;
        if (holdDuration >= POWER_HOLD_TIME) {
            enterDeepSleep();
        }
    }
}

void drawTopStatusBar() {
    for (int i = 0; i < 24; i++) {
        uint8_t alpha = 255 - (i * 10);
        uint16_t overlayColor = sprite.color565(alpha/16, alpha/16, alpha/16);
        sprite.drawFastHLine(0, i, SCREEN_WIDTH, overlayColor);
    }
    
    const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char* currentDay = (rtcDoW >= 1 && rtcDoW <= 7) ? dayNames[rtcDoW - 1] : "---";
    
    char dateStr[30];
    sprintf(dateStr, "%s %02d/%02d/%04d", currentDay, rtcDay, rtcMonth, rtcYear);
    
    sprite.setTextColor(TFT_WHITE);
    sprite.setTextDatum(ML_DATUM);
    sprite.drawString(dateStr, 5, 12, 2);
    
    int wifiX = SCREEN_WIDTH - 30;
    int wifiY = 6;
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        UIHelper::drawSignalBars(&sprite, wifiX, wifiY, rssi);
    } else {
        sprite.drawLine(wifiX, wifiY + 8, wifiX + 16, wifiY + 16, COLOR_ERROR);
        sprite.drawLine(wifiX + 16, wifiY + 8, wifiX, wifiY + 16, COLOR_ERROR);
    }
}

void drawWallpaper() {
    String wallpaperPath = wallpaperMgr ? wallpaperMgr->getCurrentWallpaperPath() : "/wallpapers/default.jpg";
    
    if (FFat.exists(wallpaperPath.c_str())) {
        File jpegFile = FFat.open(wallpaperPath.c_str(), FILE_READ);
        if (jpegFile && JpegDec.decodeFsFile(jpegFile)) {
            uint16_t *pImg;
            sprite.fillSprite(COLOR_BG);
            while (JpegDec.read()) {
                pImg = JpegDec.pImage;
                sprite.pushImage(JpegDec.MCUx * JpegDec.MCUWidth, JpegDec.MCUy * JpegDec.MCUHeight, JpegDec.MCUWidth, JpegDec.MCUHeight, pImg);
            }
        } else {
            sprite.fillSprite(COLOR_BG);
        }
        jpegFile.close();
    } else {
        sprite.fillSprite(COLOR_BG);
    }
}

void drawHomeIconWithJPEG(int x, int y, const char* jpegPath, const char* label, uint16_t fallbackColor, bool isPressed) {
    sprite.fillRoundRect(x, y, 60, 60, 12, TFT_BLACK);
    sprite.drawRoundRect(x, y, 60, 60, 12, isPressed ? COLOR_ACCENT : COLOR_BORDER);
    
    if (FFat.exists(jpegPath)) {
        File jpegFile = FFat.open(jpegPath, FILE_READ);
        if (jpegFile && JpegDec.decodeFsFile(jpegFile)) {
            TFT_eSprite iconSprite = TFT_eSprite(&tft);
            iconSprite.setColorDepth(16);
            iconSprite.createSprite(40, 40);
            iconSprite.fillSprite(TFT_BLACK);
            
            uint16_t *pImg;
            while (JpegDec.read()) {
                pImg = JpegDec.pImage;
                int mcu_x = JpegDec.MCUx * JpegDec.MCUWidth;
                int mcu_y = JpegDec.MCUy * JpegDec.MCUHeight;
                if (mcu_x < 40 && mcu_y < 40) {
                    iconSprite.pushImage(mcu_x, mcu_y, JpegDec.MCUWidth, JpegDec.MCUHeight, pImg);
                }
            }
            iconSprite.pushToSprite(&sprite, x + 10, y + 10, TFT_BLACK);
            iconSprite.deleteSprite();
        }
        jpegFile.close();
    } else {
        sprite.fillCircle(x + 30, y + 30, 18, fallbackColor);
    }
    
    sprite.setTextColor(isPressed ? COLOR_ACCENT : COLOR_TEXT_DIM);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextSize(1);
    sprite.drawString(label, x + 30, y + 72, 2);
}

void drawWatchFace() {
    drawWallpaper();
    drawTopStatusBar();
    
    uint8_t displayHour = rtcHour;
    bool isPM = false;
    
    if (displayHour == 0) displayHour = 12;
    else if (displayHour == 12) isPM = true;
    else if (displayHour > 12) { displayHour -= 12; isPM = true; }
    
    int cardWidth = 215, cardHeight = 80;
    int panelX = (SCREEN_WIDTH - cardWidth) / 2;
    int panelY = (SCREEN_HEIGHT - cardHeight) / 2 - 20;
    
    //sprite.fillRoundRect(panelX+20, panelY, cardWidth, cardHeight, 16, COLOR_CARD);
    //sprite.drawRoundRect(panelX+20, panelY, cardWidth, cardHeight, 16, COLOR_ACCENT);
    
    sprite.setTextColor(COLOR_ACCENT);
    sprite.setTextDatum(MC_DATUM);
    
    char hourStr[4], minStr[4];
    sprintf(hourStr, "%02d", displayHour);
    sprintf(minStr, "%02d", rtcMin);
    
    sprite.drawString(hourStr, SCREEN_WIDTH / 2 - 50, panelY + 35, 7);
    
    uint16_t colonColor = colonBlink ? COLOR_ACCENT : sprite.color565(
        ((COLOR_ACCENT >> 11) & 0x1F) >> 2,
        ((COLOR_ACCENT >> 5) & 0x3F) >> 2,
        (COLOR_ACCENT & 0x1F) >> 2
    );
    sprite.setTextColor(colonColor);
    sprite.drawString(":", SCREEN_WIDTH / 2, panelY + 35, 7);
    
    sprite.setTextColor(COLOR_ACCENT);
    sprite.drawString(minStr, SCREEN_WIDTH / 2 + 42, panelY + 35, 7);
    
    sprite.setTextColor(COLOR_ACCENT);
    sprite.setTextDatum(MR_DATUM);
    sprite.drawString(isPM ? "PM" : "AM", panelX + cardWidth - 10, panelY + 70, 4);
    
    int iconY = SCREEN_HEIGHT - 85;
    int totalWidth = 4 * 60 + 3 * 15;
    int startX = (SCREEN_WIDTH - totalWidth) / 2;
    
    buttons->update();
    drawHomeIconWithJPEG(startX, iconY, "/icons/wifi.jpg", "WiFi", COLOR_ACCENT, buttons->scanHeld());
    drawHomeIconWithJPEG(startX + 75, iconY, "/icons/camera.jpg", "Camera", COLOR_WARNING, buttons->delHeld());
    drawHomeIconWithJPEG(startX + 150, iconY, "/icons/voice.jpg", "Voice", COLOR_SUCCESS, buttons->shiftHeld());
    drawHomeIconWithJPEG(startX + 225, iconY, "/icons/menu.jpg", "Menu", COLOR_INFO, buttons->specialHeld());
    
    if (powerButtonHeld) {
        unsigned long holdDuration = millis() - powerButtonHoldStart;
        int progress = constrain(map(holdDuration, 0, POWER_HOLD_TIME, 0, 100), 0, 100);
        sprite.fillSprite(TFT_BLACK);
        sprite.setTextColor(TFT_WHITE);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("Release to Cancel", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 50, 2);
        sprite.fillRoundRect(40, SCREEN_HEIGHT/2 - 20, 240, 40, 12, TFT_DARKGREY);
        sprite.drawRoundRect(40, SCREEN_HEIGHT/2 - 20, 240, 40, 12, COLOR_ACCENT);
        sprite.setTextColor(COLOR_ACCENT);
        sprite.drawString("Hold to Shutdown...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 5, 4);
        
        int barWidth = map(progress, 0, 100, 0, 220);
        sprite.fillRoundRect(50, SCREEN_HEIGHT/2 + 25, barWidth, 15, 7, COLOR_ACCENT);
        sprite.drawRoundRect(50, SCREEN_HEIGHT/2 + 25, 220, 15, 7, COLOR_BORDER);
        
        sprite.setTextColor(COLOR_TEXT);
        char progressText[10];
        sprintf(progressText, "%d%%", progress);
        sprite.drawString(progressText, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 50, 2);
    }
    
    sprite.pushSprite(0, 0);
}

void handleWatchMode() {
    static int previousMode = -1;
    static int lastEncoderPos = 0;
    
    if (previousMode != MODE_WATCH) {
        previousMode = MODE_WATCH;
        encoder.reset();
        encoder.setBoundaries(-1000, 1000, false);
        encoder.setEncoderValue(0);
        delay(200);
        return;
    }
    
    if (encoder.encoderChanged()) {
        int pos = encoder.readEncoder();
        if (abs(pos - lastEncoderPos) >= 1) {
            if (contactMgr) {
                 // … Play select sound when opening contacts
                contactMgr->show();
                encoder.reset();
                encoder.setBoundaries(-1000, 1000, false);
                encoder.setEncoderValue(0);
                lastEncoderPos = 0;
                previousMode = -1;
                delay(200);
                return;
            }
        }
        lastEncoderPos = pos;
    }
    
    checkPowerButton();
    drawWatchFace();
    
    buttons->update();
    if (buttons->specialPressed()) {
        //Play click for menu button
        appState->setCurrentMode(MODE_MENU);
        if (!menu) menu = new MenuSystem(&sprite, &tft);
        menu->showMainMenu();
        encoder.reset();
        encoder.setBoundaries(0, menu->getItemCount() - 1, true);
        encoder.setEncoderValue(0);
        appState->setMenu(menu);
        delay(150);
    }
}

void handleMenuMode() {
    static int previousMode = -1;
    static int lastEncoderVal = 0;
    
    if (previousMode != MODE_MENU) {
        previousMode = MODE_MENU;
        encoder.reset();
        encoder.setBoundaries(0, menu->getItemCount() - 1, true);
        encoder.setEncoderValue(0);
        lastEncoderVal = 0;
        delay(150);
        menu->draw();
        return;
    }
    
    buttons->update();
    
    if (encoder.encoderChanged()) {
        int val = encoder.readEncoder();
        // play tick when menu item ACTUALLY CHANGES
        if (val != lastEncoderVal) {
           //Tick only on item change
            lastEncoderVal = val;
            menu->setSelected(val);
            menu->draw();
            delay(50);
        }
    }
    
    if (encoder.isEncoderButtonClicked()) {
        //Play select sound for encoder button
        AppMode selectedApp = menu->getSelectedMode();
        
        if (selectedApp == APP_WALLPAPER_CHANGER) {
            if (!wallpaperMgr) wallpaperMgr = new WallpaperManager(&sprite, &encoder, &tft);
            wallpaperMgr->show();
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
            encoder.setEncoderValue(menu->getSelected());
            lastEncoderVal = menu->getSelected();
        } else {
            appState->handleMenuSelection(selectedApp);
            if (appState->getCurrentMode() == MODE_MENU) {
                encoder.reset();
                encoder.setBoundaries(0, menu->getItemCount() - 1, true);
                encoder.setEncoderValue(menu->getSelected());
                lastEncoderVal = menu->getSelected();
            }
        }
        delay(150);
    }
    
    if (buttons->specialPressed()) {
        //Play back sound for home button
        if (menu->isShowingSettings()) {
            menu->showMainMenu();
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
            encoder.setEncoderValue(0);
            lastEncoderVal = 0;
            menu->draw();
        } else {
            previousMode = -1;
            appState->setCurrentMode(MODE_WATCH);
        }
        delay(150);
    }
}

void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(DISPLAY_ROTATION);
    tft.fillScreen(TFT_BLACK);
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, 255);
    
    bootScreen = new BootScreen(&tft);
    bootScreen->show();
    Wire.begin(I2C_SDA, I2C_SCL);
    rtcAvailable = true;
    syncWithRTC();
    
    if (rtcYear < 2020 || rtcYear > 2100) rtcAvailable = false;
    
    sprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
    sprite.setSwapBytes(true);
    sprite.setTextWrap(false);
    
    if (!FFat.begin(true)) {
        Serial.println("FFAT Not Initiated");
    }
    
    encoder.begin();
    encoder.setup(encoderISR);
    encoder.setBoundaries(0, 10, true);
    encoder.setAcceleration(20);
    
    buttons = new ButtonHandle();
    buttons->begin();
    displayMgr = new DisplayManager(&tft);
    displayMgr->begin();
    
    Preferences themePrefs;
    themePrefs.begin("settings", true);
    currentTheme = (ColorTheme)themePrefs.getInt("theme", THEME_DARK_BLUE);
    themePrefs.end();
    customTheme.load();
    credsMgr = new CredentialsManager();
    credsMgr->begin();
    menu = new MenuSystem(&sprite, &tft);
    appState = new AppStateManager(&sprite, &encoder, &rtc, &tft);
    appState->initializeManagers(displayMgr, credsMgr);
    appState->setMenu(menu);
    wallpaperMgr = new WallpaperManager(&sprite, &encoder, &tft);
    contactMgr = new ContactManager(&sprite, &encoder, &tft);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    xTaskCreatePinnedToCore(colonBlinkTask, "ColonBlink", 2048, NULL, 1, &colonBlinkTaskHandle, 1);
    xTaskCreatePinnedToCore(rtcSyncTask, "RTCSync", 2048, NULL, 1, &rtcSyncTaskHandle, 1);
    Serial.println("\n=================================");
    Serial.println("System Ready!");
    Serial.println("=================================");
    Serial.println("Features:");
    Serial.println("I2S sound effects");
    Serial.println("Blinking colon (FreeRTOS)");
    Serial.println("Date in status bar");
    Serial.println("Power single/double click");
    Serial.println("Encoder auto-reset");
    Serial.println("=================================\n");
    audioInit();
    audioSetVolume(15);
    lastRTCSync = millis();
}

void loop() {
    if (millis() - lastButtonCheck > 10) {
        lastButtonCheck = millis();
        
        SystemMode currentMode = appState->getCurrentMode();
        
        switch (currentMode) {
            case MODE_WATCH:
                handleWatchMode();
                break;
            case MODE_MENU:
                handleMenuMode();
                break;
            case MODE_WIFI_SCANNER:
                appState->handleWiFiScanner();
                break;
            case MODE_AI_ASSISTANT:
                appState->handleAIAssistant();
                break;
        }
    }
    
    delay(5);
}