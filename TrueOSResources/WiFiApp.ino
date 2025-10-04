/*
 * Unified WiFi Scanner + AI Assistant - Complete with Deep Sleep & New Features
 * Includes: Clock Settings, About/Credits, JPEG Icons, Deep Sleep
 */

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <Wire.h>
#include <DS3231.h>
#include <JPEGDecoder.h>
#include "esp_sleep.h"
// Local includes
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
#include "MenuSystem.h"
#include "SettingsUI.h"
#include "AboutUI.h"
#include "ClockSettingsUI.h"

// ==================== System Mode ====================
enum SystemMode {
    MODE_WATCH,
    MODE_MENU,
    MODE_WIFI_SCANNER,
    MODE_AI_ASSISTANT
};

// ==================== Global Objects ====================
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite(&tft);
AiEsp32RotaryEncoder encoder(ENC_CLK, ENC_DT, ENC_SW, -1, ROTARY_STEPS);
DS3231 rtc;

WiFiScanner* wifiScanner = nullptr;
CredentialsManager* credsMgr = nullptr;
DisplayManager* displayMgr = nullptr;
KeyboardUI* keyboard = nullptr;
AIAssistant* aiAssistant = nullptr;
AIChatUI* aiChatUI = nullptr;
MenuSystem* menu = nullptr;
SettingsUI* settings = nullptr;
AboutUI* aboutUI = nullptr;
ClockSettingsUI* clockSettingsUI = nullptr;

// State variables
SystemMode currentMode = MODE_WATCH;
int previousMode = MODE_WATCH;
int selectedNetwork = 0;
int scrollOffset = 0;
uint16_t rtcYear = 2024;
uint8_t rtcMonth = 10, rtcDay = 4, rtcHour = 12, rtcMin = 0, rtcSec = 0;
unsigned long lastTimeUpdate = 0;
unsigned long lastButtonCheck = 0;
unsigned long powerButtonHoldStart = 0;
bool powerButtonHeld = false;
bool isScanning = false;
bool rtcAvailable = false;
bool colonBlink = false; 

// Deep Sleep Configuration
#define POWER_HOLD_TIME 3000  // 3 seconds to trigger sleep
#define WAKEUP_PIN BTN_SELECT  // Wake with CAST button

// ==================== Button Handlers ====================
void IRAM_ATTR encoderISR() {
    encoder.readEncoder_ISR();
}

bool checkButtonPress(int pin, unsigned long& lastPress, unsigned long debounce = 250) {
    if (digitalRead(pin) == LOW && millis() - lastPress > debounce) {
        lastPress = millis();
        return true;
    }
    return false;
}

// ==================== Deep Sleep Functions ====================
void enterDeepSleep() {
    Serial.println("[SLEEP] Entering deep sleep mode...");
    
    // Show sleep animation
    for (int i = 0; i < 10; i++) {
        sprite.fillSprite(COLOR_BG);
        sprite.setTextColor(COLOR_TEXT);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("Shutting Down", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 4);
        
        int dotCount = (i % 3) + 1;
        String dots = "";
        for (int j = 0; j < dotCount; j++) dots += ".";
        sprite.drawString(dots, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 30, 2);
        
        sprite.pushSprite(0, 0);
        delay(100);
    }
    
    // Fade out display
    for (int brightness = 255; brightness >= 0; brightness -= 5) {
        analogWrite(TFT_BL, brightness);
        delay(10);
    }
    
    // Configure wakeup sources
    // --------- Enable wake-up source ---------
  // Wake on encoder rotation (CLK) or button press (SW)
  esp_sleep_enable_ext1_wakeup(
    (1ULL << ENC_CLK) | (1ULL << ENC_SW),
    ESP_EXT1_WAKEUP_ANY_LOW
  );

    
    // Disconnect WiFi to save power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    
    // Enter deep sleep
    esp_deep_sleep_start();
}

void checkPowerButton() {
    static bool lastPowerState = HIGH;
    bool currentPowerState = digitalRead(BTN_SCAN);
    
    // Button pressed
    if (currentPowerState == LOW && lastPowerState == HIGH) {
        powerButtonHoldStart = millis();
        powerButtonHeld = true;
    }
    
    // Button released
    if (currentPowerState == HIGH && lastPowerState == LOW) {
        powerButtonHeld = false;
        powerButtonHoldStart = 0;
    }
    
    // Check if held long enough
    if (powerButtonHeld && (millis() - powerButtonHoldStart >= POWER_HOLD_TIME)) {
        enterDeepSleep();
    }
    
    lastPowerState = currentPowerState;
}

// ==================== Home Screen Icon Drawing ====================
void drawHomeIcon(int x, int y, const char* jpegPath, const char* label, uint16_t fallbackColor, bool isPressed) {
    // Icon background
    uint16_t bgColor = isPressed ? COLOR_SELECTED : COLOR_CARD;
    sprite.fillRoundRect(x, y, 60, 60, 12, bgColor);
    sprite.drawRoundRect(x, y, 60, 60, 12, isPressed ? COLOR_ACCENT : COLOR_BORDER);
    
    // Try to load JPEG icon
    TFT_eSprite iconSprite(&tft);
    iconSprite.setColorDepth(16);
    iconSprite.createSprite(50, 50);
    //iconSprite.fillSprite(TFT_BLACK);
    
    if (FFat.exists(jpegPath)) {
        File jpegFile = FFat.open(jpegPath, FILE_READ);
        if (jpegFile && JpegDec.decodeFsFile(jpegFile)) {
            // Render JPEG to icon sprite
            uint16_t *pImg;
            while (JpegDec.read()) {
                pImg = JpegDec.pImage;
                int mcu_x = JpegDec.MCUx * JpegDec.MCUWidth;
                int mcu_y = JpegDec.MCUy * JpegDec.MCUHeight;
                if (mcu_x < 50 && mcu_y < 50) {
                    iconSprite.pushImage(mcu_x, mcu_y, JpegDec.MCUWidth, JpegDec.MCUHeight, pImg);
                }
            }
        }
        jpegFile.close();
    } else {
        // Fallback: draw simple icon
        iconSprite.fillCircle(25, 25, 20, fallbackColor);
        iconSprite.setTextColor(TFT_WHITE);
        iconSprite.setTextDatum(MC_DATUM);
        iconSprite.setTextSize(2);
        iconSprite.drawString(String(label[0]), 25, 25);
    }
    
    // Push icon sprite to main sprite
    iconSprite.pushToSprite(&sprite, x + 5, y + 5, TFT_BLACK);
    iconSprite.deleteSprite();
    
    // Label with better styling
    sprite.setTextColor(isPressed ? COLOR_ACCENT : COLOR_TEXT_DIM);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextSize(1);
    sprite.drawString(label, x + 30, y + 72, 2);
}

// ==================== Watch Face ====================
void drawWatchFace() {
    // Draw JPEG wallpaper if exists
    if (FFat.exists("/wallpapers/redmapple.jpg")) {
        File jpegFile = FFat.open("/wallpapers/redmapple.jpg", FILE_READ);
        if (jpegFile && JpegDec.decodeFsFile(jpegFile)) {
            uint16_t *pImg;
            sprite.fillSprite(COLOR_BG); // Fallback
            while (JpegDec.read()) {
                pImg = JpegDec.pImage;
                int mcu_x = JpegDec.MCUx * JpegDec.MCUWidth;
                int mcu_y = JpegDec.MCUy * JpegDec.MCUHeight;
                sprite.pushImage(mcu_x, mcu_y, JpegDec.MCUWidth, JpegDec.MCUHeight, pImg);
            }
        }
        jpegFile.close();
    } else {
        sprite.fillSprite(COLOR_BG);
    }
    
    // Update time and blink colon
    if (millis() - lastTimeUpdate > 500) {
        colonBlink = !colonBlink;
        
        if (millis() - lastTimeUpdate > 1000) {
            if (rtcAvailable) {
                bool h12, PM;
                rtcHour = rtc.getHour(h12, PM);
                rtcMin = rtc.getMinute();
                rtcSec = rtc.getSecond();
                bool century = false;
                rtcDay = rtc.getDate();
                rtcMonth = rtc.getMonth(century);
                rtcYear = rtc.getYear() + 2000;
            } else {
                rtcSec++;
                if (rtcSec >= 60) { rtcSec = 0; rtcMin++; }
                if (rtcMin >= 60) { rtcMin = 0; rtcHour++; }
                if (rtcHour >= 24) { rtcHour = 0; }
            }
            lastTimeUpdate = millis();
        }
    }
    
    // Time display with blinking colon
    char timeStr[10];
    if (colonBlink) {
        sprintf(timeStr, "%02d:%02d", rtcHour, rtcMin);
    } else {
        sprintf(timeStr, "%02d %02d", rtcHour, rtcMin); // Space instead of colon
    }
    
    int panelY = 50;
    sprite.fillRoundRect(40, panelY, 240, 70, 16, COLOR_CARD);
    sprite.drawRoundRect(40, panelY, 240, 70, 16, COLOR_ACCENT);
    
    sprite.setTextColor(COLOR_ACCENT);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString(timeStr, SCREEN_WIDTH / 2, panelY + 35, 7);
    
    // Date
    char dateStr[20];
    sprintf(dateStr, "%02d/%02d/%04d", rtcDay, rtcMonth, rtcYear);
    
    sprite.setTextColor(COLOR_TEXT);
    sprite.drawString(dateStr, SCREEN_WIDTH / 2, panelY + 80, 2);
    
    // WiFi status bar in top-right corner
    int wifiX = SCREEN_WIDTH - 35;
    int wifiY = 5;
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        UIHelper::drawSignalBars(&sprite, wifiX, wifiY, rssi);
    } else {
        // Draw X for disconnected
        sprite.drawLine(wifiX, wifiY + 8, wifiX + 16, wifiY + 16, COLOR_ERROR);
        sprite.drawLine(wifiX + 16, wifiY + 8, wifiX, wifiY + 16, COLOR_ERROR);
    }
    
    // Button icons at bottom - CENTERED and EVENLY SPACED with FIXED icons
    int iconY = SCREEN_HEIGHT - 85;
    int totalWidth = 4 * 60 + 3 * 15;
    int startX = (SCREEN_WIDTH - totalWidth) / 2;
    
    bool btn1Press = digitalRead(BTN_SCAN) == LOW;
    bool btn2Press = digitalRead(BTN_DEL) == LOW;
    bool btn3Press = digitalRead(BTN_SHIFT) == LOW;
    bool btn4Press = digitalRead(BTN_SPECIAL) == LOW;
    
    drawHomeIconFixed(startX, iconY, "/icons/wifi.jpg", "WiFi", COLOR_ACCENT, btn1Press);
    drawHomeIconFixed(startX + 75, iconY, "/icons/camera.jpg", "Camera", COLOR_WARNING, btn2Press);
    drawHomeIconFixed(startX + 150, iconY, "/icons/voice.jpg", "Voice", COLOR_SUCCESS, btn3Press);
    drawHomeIconFixed(startX + 225, iconY, "/icons/menu.jpg", "Menu", COLOR_INFO, btn4Press);
    
    // Temperature from RTC
    if (rtcAvailable) {
        float temp = rtc.getTemperature();
        char tempStr[10];
        sprintf(tempStr, "%.1fC", temp);
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.setTextDatum(TR_DATUM);
        sprite.drawString(tempStr, SCREEN_WIDTH - 5, 5, 1);
    }
    
    // Show sleep indicator if power button held
    if (powerButtonHeld) {
        unsigned long holdDuration = millis() - powerButtonHoldStart;
        int progress = map(holdDuration, 0, POWER_HOLD_TIME, 0, 100);
        progress = constrain(progress, 0, 100);
        
        sprite.fillRoundRect(50, SCREEN_HEIGHT/2 - 30, 220, 60, 12, COLOR_CARD);
        sprite.drawRoundRect(50, SCREEN_HEIGHT/2 - 30, 220, 60, 12, COLOR_ACCENT);
        
        sprite.setTextColor(COLOR_TEXT);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("Hold to Sleep...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 10, 2);
        
        // Progress bar
        int barWidth = map(progress, 0, 100, 0, 200);
        sprite.fillRoundRect(60, SCREEN_HEIGHT/2 + 10, barWidth, 10, 5, COLOR_ACCENT);
        sprite.drawRoundRect(60, SCREEN_HEIGHT/2 + 10, 200, 10, 5, COLOR_BORDER);
    }
    
    sprite.pushSprite(0, 0);
}

void drawHomeIconFixed(int x, int y, const char* jpegPath, const char* label, uint16_t fallbackColor, bool isPressed) {
    // Icon background
    uint16_t bgColor = isPressed ? COLOR_SELECTED : COLOR_CARD;
    sprite.fillRoundRect(x, y, 60, 60, 12, TFT_BLACK);
    sprite.drawRoundRect(x, y, 60, 60, 12, isPressed ? COLOR_ACCENT : COLOR_BORDER);
    
    // Try to load JPEG icon - CENTERED at 40x40
    if (FFat.exists(jpegPath)) {
        File jpegFile = FFat.open(jpegPath, FILE_READ);
        if (jpegFile && JpegDec.decodeFsFile(jpegFile)) {
            // Create temporary sprite for icon
            TFT_eSprite iconSprite(&tft);
            iconSprite.setColorDepth(16);
            iconSprite.createSprite(40, 40);
            iconSprite.fillSprite(TFT_BLACK);
            
            uint16_t *pImg;
            while (JpegDec.read()) {
                pImg = JpegDec.pImage;
                int mcu_x = JpegDec.MCUx * JpegDec.MCUWidth;
                int mcu_y = JpegDec.MCUy * JpegDec.MCUHeight;
                
                // Scale and center the image
                if (mcu_x < 40 && mcu_y < 40) {
                    iconSprite.pushImage(mcu_x, mcu_y, JpegDec.MCUWidth, JpegDec.MCUHeight, pImg);
                }
            }
            
            // Push centered icon (10px margin on all sides)
            iconSprite.pushToSprite(&sprite, x + 10, y + 10, TFT_BLACK);
            iconSprite.deleteSprite();
        }
        jpegFile.close();
    } else {
        // Fallback: draw simple centered icon
        sprite.fillCircle(x + 30, y + 30, 18, fallbackColor);
        sprite.setTextColor(TFT_WHITE);
        sprite.setTextDatum(MC_DATUM);
        sprite.setTextSize(2);
        sprite.drawString(String(label[0]), x + 30, y + 30);
    }
    
    // Label with better styling
    sprite.setTextColor(isPressed ? COLOR_ACCENT : COLOR_TEXT_DIM);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextSize(1);
    sprite.drawString(label, x + 30, y + 72, 2);
}


// ==================== WiFi Connection ====================
bool connectToNetwork(WiFiNetwork* network) {
    if (!network) return false;
    
    Serial.printf("\n>>> Connecting to: %s\n", network->ssid.c_str());
    
    String password = "";
    bool hasSavedCreds = credsMgr && credsMgr->getCredentials(network->ssid, password);
    
    if (!hasSavedCreds && network->isEncrypted()) {
        if (!keyboard) {
            keyboard = new KeyboardUI(&sprite, &encoder);
            keyboard->begin();
        }
        
        password = keyboard->show("Enter Password");
        if (password.length() == 0) return false;
    }
    
    displayMgr->showConnecting(network->ssid);
    
    WiFi.disconnect(true);
    delay(500);
    WiFi.mode(WIFI_STA);
    
    if (network->isEncrypted()) {
        WiFi.begin(network->ssid.c_str(), password.c_str());
    } else {
        WiFi.begin(network->ssid.c_str());
    }
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < CONNECT_TIMEOUT) {
        displayMgr->showConnecting(network->ssid);
        delay(100);
    }
    
    bool success = (WiFi.status() == WL_CONNECTED);
    
    if (success) {
        if (credsMgr && password.length() > 0) {
            credsMgr->save(network->ssid, password, network->rssi);
        }
        displayMgr->showConnectionResult(true, "Connected!", WiFi.localIP().toString());
        
        // Sync RTC if available
        if (rtcAvailable) {
            configTime(6 * 3600, 0, "pool.ntp.org");
            delay(2000);
            struct tm timeInfo;
            if (getLocalTime(&timeInfo)) {
                rtc.setClockMode(false);
                rtc.setYear(timeInfo.tm_year - 100);
                rtc.setMonth(timeInfo.tm_mon + 1);
                rtc.setDate(timeInfo.tm_mday);
                rtc.setDoW(timeInfo.tm_wday + 1);
                rtc.setHour(timeInfo.tm_hour);
                rtc.setMinute(timeInfo.tm_min);
                rtc.setSecond(timeInfo.tm_sec);
                
                bool h12, PM, century;
                rtcHour = rtc.getHour(h12, PM);
                rtcMin = rtc.getMinute();
                rtcSec = rtc.getSecond();
                rtcDay = rtc.getDate();
                rtcMonth = rtc.getMonth(century);
                rtcYear = rtc.getYear() + 2000;
                Serial.println("RTC synced");
            }
        }
    } else {
        displayMgr->showConnectionResult(false, "Failed", "Check password");
    }
    
    while (digitalRead(ENC_SW) != LOW) delay(10);
    delay(300);
    
    return success;
}

// ==================== Forget Password ====================
void forgetPassword(WiFiNetwork* network) {
    if (!network || !credsMgr) return;
    
    sprite.fillSprite(COLOR_BG);
    UIHelper::drawHeader(&sprite, "Forget Password", nullptr);
    
    sprite.setTextColor(COLOR_TEXT);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString("Forget password for:", SCREEN_WIDTH / 2, 80, 2);
    sprite.setTextColor(COLOR_ACCENT);
    sprite.drawString(network->ssid, SCREEN_WIDTH / 2, 110, 4);
    sprite.setTextColor(COLOR_TEXT_DIM);
    sprite.drawString("Press encoder = confirm", SCREEN_WIDTH / 2, 160, 2);
    sprite.drawString("Press HOME = cancel", SCREEN_WIDTH / 2, 180, 1);
    sprite.pushSprite(0, 0);
    
    while (true) {
        if (encoder.isEncoderButtonClicked()) {
            credsMgr->remove(network->ssid);
            sprite.fillSprite(COLOR_BG);
            UIHelper::drawHeader(&sprite, "Success", nullptr);
            sprite.setTextColor(COLOR_SUCCESS);
            sprite.setTextDatum(MC_DATUM);
            sprite.drawString("Password forgotten!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
            sprite.pushSprite(0, 0);
            delay(1500);
            break;
        }
        if (digitalRead(BTN_SPECIAL) == LOW) {
            delay(300);
            break;
        }
        delay(10);
    }
}

// ==================== Mode Handlers ====================
void handleWatchMode() {
    if (previousMode != MODE_WATCH) {
        previousMode = MODE_WATCH;
        delay(300);
        return;
    }
    
    // Check power button for sleep
    checkPowerButton();
    
    drawWatchFace();
    
    static unsigned long lastHomePress = 0;
    if (checkButtonPress(BTN_SPECIAL, lastHomePress)) {
        currentMode = MODE_MENU;
        if (!menu) menu = new MenuSystem(&sprite);
        menu->showMainMenu();
        encoder.reset();
        encoder.setBoundaries(0, menu->getItemCount() - 1, true);
    }
}

void handleMenuMode() {
    if (previousMode != MODE_MENU) {
        previousMode = MODE_MENU;
        delay(300);
        menu->draw();
        return;
    }
    
    static int lastEncoderVal = 0;
    
    if (encoder.encoderChanged()) {
        int val = encoder.readEncoder();
        if (val != lastEncoderVal) {
            lastEncoderVal = val;
            menu->setSelected(val);
            menu->draw();
        }
    }
    
    if (encoder.isEncoderButtonClicked()) {
        AppMode selectedApp = menu->getSelectedMode();
        
        if (selectedApp == APP_WIFI_SCANNER) {
            currentMode = MODE_WIFI_SCANNER;
            if (!wifiScanner) {
                wifiScanner = new WiFiScanner();
                wifiScanner->begin();
            }
            displayMgr->resetSpinner();
            isScanning = true;
            unsigned long scanStart = millis();
            wifiScanner->startScanBlocking();
            while (millis() - scanStart < 1000) {
                displayMgr->showScanning();
                delay(50);
            }
            isScanning = false;
            encoder.reset();
            encoder.setBoundaries(0, max(0, wifiScanner->getNetworkCount() - 1), true);
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
                if (!aiChatUI) {
                    aiChatUI = new AIChatUI(&sprite, aiAssistant);
                }
                encoder.reset();
                encoder.setBoundaries(-1000, 1000, false);
            } else {
                sprite.fillSprite(COLOR_BG);
                UIHelper::drawHeader(&sprite, "Error", nullptr);
                UIHelper::drawMessage(&sprite, "WiFi required!", COLOR_ERROR);
                sprite.pushSprite(0, 0);
                delay(2000);
            }
            
        } else if (selectedApp == APP_CLOCK_SETTINGS) {
            if (!clockSettingsUI) {
                clockSettingsUI = new ClockSettingsUI(&sprite, &encoder, &rtc);
            }
            clockSettingsUI->show();
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
            
        } else if (selectedApp == APP_ABOUT_CREDITS) {
            if (!aboutUI) {
                aboutUI = new AboutUI(&sprite, &encoder,&tft);
            }
            aboutUI->show();
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
            
        } else if (selectedApp == APP_SETTINGS) {
            menu->showSettings();
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
            menu->draw();
            
        } else if (selectedApp == APP_DISPLAY_BRIGHTNESS) {
            if (!settings) {
                settings = new SettingsUI(&sprite, &encoder);
                settings->begin();
            }
            settings->adjustBrightness();
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
            
        } else if (selectedApp == APP_DISPLAY_TIMEOUT) {
            if (!settings) {
                settings = new SettingsUI(&sprite, &encoder);
                settings->begin();
            }
            settings->adjustTimeout();
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
            
        } else if (selectedApp == APP_VOLUME_CONTROL) {
            if (!settings) {
                settings = new SettingsUI(&sprite, &encoder);
                settings->begin();
            }
            settings->adjustVolume();
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
            
        } else if (selectedApp == APP_RESTART) {
            if (!settings) {
                settings = new SettingsUI(&sprite, &encoder);
                settings->begin();
            }
            if (settings->confirmRestart()) {
                ESP.restart();
            }
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
            
        } else if (selectedApp == APP_FACTORY_RESET) {
            if (!settings) {
                settings = new SettingsUI(&sprite, &encoder);
                settings->begin();
            }
            if (settings->confirmFactoryReset()) {
                settings->factoryReset();
            }
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
        }
        
        delay(200);
    }
    
    static unsigned long lastHomePress = 0;
    if (checkButtonPress(BTN_SPECIAL, lastHomePress)) {
        if (menu->isShowingSettings()) {
            menu->showMainMenu();
            encoder.reset();
            encoder.setBoundaries(0, menu->getItemCount() - 1, true);
            menu->draw();
        } else {
            currentMode = MODE_WATCH;
        }
    }
}

void handleWiFiScanner() {
    static int lastEncoderVal = 0;
    
    if (encoder.encoderChanged()) {
        int val = encoder.readEncoder();
        if (val != lastEncoderVal) {
            lastEncoderVal = val;
        }
        selectedNetwork = val;
        if (selectedNetwork < scrollOffset) {
            scrollOffset = selectedNetwork;
        } else if (selectedNetwork >= scrollOffset + VISIBLE_ITEMS) {
            scrollOffset = selectedNetwork - VISIBLE_ITEMS + 1;
        }
    }
    
    if (encoder.isEncoderButtonClicked()) {
        WiFiNetwork* network = wifiScanner->getNetwork(selectedNetwork);
        if (network) connectToNetwork(network);
        delay(200);
    }
    
    static unsigned long lastScanPress = 0;
    if (checkButtonPress(BTN_SCAN, lastScanPress, 500)) {
        displayMgr->resetSpinner();
        isScanning = true;
        
        // Start async scan
        WiFi.scanNetworks(true, true);
        
        // Show progress while scanning
        unsigned long scanStart = millis();
        while (WiFi.scanComplete() < 0 && millis() - scanStart < 10000) {
            sprite.fillSprite(COLOR_BG);
            UIHelper::drawHeader(&sprite, "Scanning WiFi", nullptr);
            
            // Progress bar
            int progress = map(millis() - scanStart, 0, 10000, 0, 100);
            progress = constrain(progress, 0, 99);
            
            int barY = SCREEN_HEIGHT / 2;
            sprite.fillRoundRect(40, barY, 240, 30, 8, COLOR_CARD);
            sprite.drawRoundRect(40, barY, 240, 30, 8, COLOR_BORDER);
            
            int fillWidth = map(progress, 0, 100, 0, 236);
            sprite.fillRoundRect(42, barY + 2, fillWidth, 26, 6, COLOR_ACCENT);
            
            sprite.setTextColor(COLOR_TEXT);
            sprite.setTextDatum(MC_DATUM);
            char progStr[10];
            sprintf(progStr, "%d%%", progress);
            sprite.drawString(progStr, SCREEN_WIDTH / 2, barY + 15, 2);
            
            sprite.setTextColor(COLOR_TEXT_DIM);
            sprite.drawString("Searching for networks...", SCREEN_WIDTH / 2, barY + 50, 2);
            
            sprite.pushSprite(0, 0);
            delay(100);
        }
        
        // Wait for scan to complete
        int n = WiFi.scanComplete();
        if (n >= 0) {
            wifiScanner->startScanBlocking(); // Process results
        }
        
        isScanning = false;
        encoder.setBoundaries(0, max(0, wifiScanner->getNetworkCount() - 1), true);
        selectedNetwork = 0;
        scrollOffset = 0;
    }
    
    static unsigned long lastDelPress = 0;
    if (checkButtonPress(BTN_DEL, lastDelPress)) {
        WiFiNetwork* network = wifiScanner->getNetwork(selectedNetwork);
        if (network && credsMgr && credsMgr->hasCredentials(network->ssid)) {
            forgetPassword(network);
        }
    }
    
    static unsigned long lastHomePress = 0;
    if (checkButtonPress(BTN_SPECIAL, lastHomePress)) {
        currentMode = MODE_MENU;
    }
    
    if (isScanning) {
        displayMgr->showScanning();
    } else {
        displayMgr->drawNetworkListWithCreds(
            wifiScanner->getNetworks(),
            wifiScanner->getNetworkCount(),
            selectedNetwork,
            scrollOffset,
            credsMgr
        );
    }
}

void handleAIAssistant() {
    static int lastEncoderVal = 0;
    
    if (encoder.encoderChanged()) {
        int val = encoder.readEncoder();
        int delta = val - lastEncoderVal;
        lastEncoderVal = val;
        if (aiChatUI) {
            aiChatUI->scroll(-delta * 3);
        }
    }
    
    static unsigned long lastMicPress = 0;
    if (checkButtonPress(BTN_SHIFT, lastMicPress, 500)) {
        if (aiAssistant && !aiAssistant->getIsSpeaking()) {
            aiAssistant->startListening();
            audioConnecttoSpeech(aiAssistant->getSTT().c_str(),"en");
            if (aiChatUI) aiChatUI->scrollToBottom();
        }
    }
    
    static unsigned long lastHomePress = 0;
    if (checkButtonPress(BTN_SPECIAL, lastHomePress)) {
        currentMode = MODE_MENU;
        encoder.reset();
        encoder.setBoundaries(0, menu->getItemCount() - 1, true);
    }
    
    if (aiChatUI) aiChatUI->draw();
}

// ==================== Setup ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=================================");
    Serial.println("ESP32 Unified System Starting...");
    Serial.println("=================================\n");
    
    // Initialize I2C for RTC
    Serial.println("[INIT] I2C and RTC...");
    Wire.begin(I2C_SDA, I2C_SCL);
    
    rtcAvailable = true;
    
    bool h12, PM;
    rtcHour = rtc.getHour(h12, PM);
    rtcMin = rtc.getMinute();
    rtcSec = rtc.getSecond();
    bool century = false;
    rtcDay = rtc.getDate();
    rtcMonth = rtc.getMonth(century);
    rtcYear = rtc.getYear() + 2000;
    
    if (rtcYear < 2020 || rtcYear > 2100) {
        Serial.println("[WARN] RTC not found - using fallback");
        rtcAvailable = false;
    } else {
        Serial.println("[OK] DS3231 found");
        Serial.printf("[RTC] %02d:%02d:%02d\n", rtcHour, rtcMin, rtcSec);
    }
    
    // Initialize display
    Serial.println("[INIT] Display...");
    tft.init();
    tft.setRotation(DISPLAY_ROTATION);
    tft.fillScreen(COLOR_BG);
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, 255);
        // Check wakeup reason
// --------- Check wake-up reason ---------
esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

tft.setCursor(0, 100);
tft.println("Boot Reason:");
switch (wakeup_reason) {
  case ESP_SLEEP_WAKEUP_UNDEFINED:
    tft.println("  Power-on Reset (Normal Boot)");
    break;
  case ESP_SLEEP_WAKEUP_ALL:
    tft.println("  Wakeup from All Sources (mask)");
    break;
  case ESP_SLEEP_WAKEUP_EXT0:
    tft.println("  Wakeup from EXT0 GPIO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    tft.println("  Wakeup from EXT1 GPIO(s)");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    tft.println("  Wakeup from Timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    tft.println("  Wakeup from Touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    tft.println("  Wakeup from ULP program");
    break;
  case ESP_SLEEP_WAKEUP_GPIO:
    tft.println("  Wakeup from GPIO (light sleep only)");
    break;
  case ESP_SLEEP_WAKEUP_UART:
    tft.println("  Wakeup from UART (light sleep only)");
    break;
  case ESP_SLEEP_WAKEUP_WIFI:
    tft.println("  Wakeup from WiFi (modem sleep)");
    break;
  case ESP_SLEEP_WAKEUP_COCPU:
    tft.println("  Wakeup from Co-Processor (ULP/RISCV)");
    break;
  case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
    tft.println("  Wakeup from Co-Processor Trap Trigger");
    break;
  default:
    tft.println("  Unknown / Error Wakeup");
    break;
}

delay(2000);

    sprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
    sprite.setSwapBytes(true);
    sprite.setTextWrap(false);
    
    // Initialize filesystem
    Serial.println("[INIT] Filesystem...");
    if (!FFat.begin(true, "/ffat", 2)) {
        Serial.println("[ERROR] FFat failed, formatting...");
        FFat.format();
        if (!FFat.begin(true, "/ffat", 2)) {
            Serial.println("[ERROR] FFat still failed!");
        } else {
            Serial.println("[OK] FFat formatted and mounted");
        }
    } else {
        Serial.println("[OK] FFat mounted");
    }
    
    // Initialize encoder
    Serial.println("[INIT] Encoder...");
    encoder.begin();
    encoder.setup(encoderISR);
    encoder.setBoundaries(0, 2, true);
    encoder.setAcceleration(50);
    
    // Initialize buttons
    Serial.println("[INIT] Buttons...");
    pinMode(BTN_SCAN, INPUT_PULLUP);
    pinMode(BTN_SHIFT, INPUT_PULLUP);
    pinMode(BTN_DEL, INPUT_PULLUP);
    pinMode(BTN_SPECIAL, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
      // --------- Setup encoder pins ---------
      pinMode(ENC_CLK, INPUT_PULLUP);
      pinMode(ENC_DT, INPUT_PULLUP);
      pinMode(ENC_SW, INPUT_PULLUP);
    
    // Initialize managers
    Serial.println("[INIT] Display Manager...");
    displayMgr = new DisplayManager(&tft);
    displayMgr->begin();
    displayMgr->showSplashScreen();
    delay(2000);
    
    Serial.println("[INIT] Credentials Manager...");
    credsMgr = new CredentialsManager();
    if (!credsMgr->begin()) {
        Serial.println("[WARN] Credentials manager init failed");
    }
    
    Serial.println("[INIT] Settings...");
    settings = new SettingsUI(&sprite, &encoder);
    settings->begin();
    int brightness = settings->getBrightness();
    analogWrite(TFT_BL, brightness);
    
    // WiFi
    Serial.println("[INIT] WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Auto-connect
    if (credsMgr && credsMgr->getCount() > 0) {
        SavedNetwork* recent = credsMgr->getMostRecent();
        if (recent) {
            Serial.printf("[WiFi] Auto-connecting '%s'\n", recent->ssid.c_str());
            WiFi.begin(recent->ssid.c_str(), recent->password.c_str());
            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
                delay(100);
            }
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WiFi] Connected!");
                if (rtcAvailable) {
                    configTime(6 * 3600, 0, "pool.ntp.org");
                    delay(2000);
                    struct tm ti;
                    if (getLocalTime(&ti)) {
                        rtc.setHour(ti.tm_hour);
                        rtc.setMinute(ti.tm_min);
                        rtc.setSecond(ti.tm_sec);
                        rtc.setDate(ti.tm_mday);
                        rtc.setMonth(ti.tm_mon + 1);
                        rtc.setYear(ti.tm_year - 100);
                        
                        bool h12, PM;
                        rtcHour = rtc.getHour(h12, PM);
                        rtcMin = rtc.getMinute();
                        rtcSec = rtc.getSecond();
                        Serial.println("[RTC] Synced from NTP");
                    }
                }
            }
        }
    }
    
    Serial.println("\n=================================");
    Serial.println("System Ready!");
    Serial.println("=================================");
    Serial.println("Features:");
    Serial.println("- Hold POWER button for 3s to sleep");
    Serial.println("- Wake with CAST/POWER/Encoder");
    Serial.println("- Icons load from /icons/*.jpg");
    Serial.println("- Clock Settings in menu");
    Serial.println("- About/Credits in menu");
    Serial.println("=================================\n");
    audioInit();
    audioSetVolume(15);
}

// ==================== Main Loop ====================
void loop() {
    if (millis() - lastButtonCheck > 50) {
        lastButtonCheck = millis();
        
        switch (currentMode) {
            case MODE_WATCH:
                handleWatchMode();
                break;
            case MODE_MENU:
                handleMenuMode();
                break;
            case MODE_WIFI_SCANNER:
                handleWiFiScanner();
                break;
            case MODE_AI_ASSISTANT:
                handleAIAssistant();
                break;
        }
    }
    delay(10);
}