// AppStateManager.h - FIXED: Responsive buttons + Auto WiFi reconnect
#ifndef APPSTATEMANAGER_H
#define APPSTATEMANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <DS3231.h>
#include "DisplayManager.h"
#include "WiFiScanner.h"
#include "CredentialsManager.h"
#include "KeyboardUI.h"
#include "AIAssistant.h"
#include "AIChatUI.h"
#include "SettingsUI.h"
#include "AboutUI.h"
#include "ClockSettingsUI.h"
#include "WallpaperManager.h"
#include "AudioManager.h"
#include "ButtonHandle.h"

enum AppMode;
class MenuSystem;

enum SystemMode {
    MODE_WATCH,
    MODE_MENU,
    MODE_WIFI_SCANNER,
    MODE_AI_ASSISTANT
};

class AppStateManager {
private:
    TFT_eSprite* sprite;
    AiEsp32RotaryEncoder* encoder;
    DS3231* rtc;
    TFT_eSPI* tft;
    
    WiFiScanner* wifiScanner;
    CredentialsManager* credsMgr;
    DisplayManager* displayMgr;
    KeyboardUI* keyboard;
    AIAssistant* aiAssistant;
    AIChatUI* aiChatUI;
    MenuSystem* menu;
    SettingsUI* settings;
    AboutUI* aboutUI;
    ClockSettingsUI* clockSettingsUI;
    WallpaperManager* wallpaperMgr;
    ButtonHandle* buttons; // âœ… Added button handler
    
    SystemMode currentMode;
    int previousMode;
    int selectedNetwork;
    int scrollOffset;
    bool isScanning;
    
    // âœ… Button state tracking for better responsiveness
    unsigned long lastScanPress = 0;
    unsigned long lastDelPress = 0;
    unsigned long lastHomePress = 0;
    
    bool connectToNetwork(WiFiNetwork* network) {
        if (!network) return false;
        
        Serial.printf("\n>>> Connecting to: %s\n", network->ssid.c_str());
        
        String password = "";
        bool hasSavedCreds = credsMgr && credsMgr->getCredentials(network->ssid, password);
        
        if (!hasSavedCreds && network->isEncrypted()) {
            if (!keyboard) {
                keyboard = new KeyboardUI(sprite, encoder);
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
        
        if (success && credsMgr && password.length() > 0) {
            credsMgr->save(network->ssid, password, network->rssi);
        }
        
        displayMgr->showConnectionResult(success, 
            success ? "Connected!" : "Failed", 
            success ? WiFi.localIP().toString() : "Check password");
        
        while (digitalRead(ENC_SW) != LOW) delay(10);
        delay(150); // âœ… Reduced delay
        
        return success;
    }
    
    void forgetPassword(WiFiNetwork* network) {
        if (!network || !credsMgr) return;
        
        sprite->fillSprite(COLOR_BG);
        UIHelper::drawHeader(sprite, "Forget Password", nullptr);
        
        sprite->setTextColor(COLOR_TEXT);
        sprite->setTextDatum(MC_DATUM);
        sprite->drawString("Forget password for:", SCREEN_WIDTH / 2, 80, 2);
        sprite->setTextColor(COLOR_ACCENT);
        sprite->drawString(network->ssid, SCREEN_WIDTH / 2, 110, 4);
        sprite->setTextColor(COLOR_TEXT_DIM);
        sprite->drawString("Press encoder = confirm", SCREEN_WIDTH / 2, 160, 2);
        sprite->drawString("Press HOME = cancel", SCREEN_WIDTH / 2, 180, 1);
        sprite->pushSprite(0, 0);
        
        while (true) {
            if (encoder->isEncoderButtonClicked()) {
                credsMgr->remove(network->ssid);
                sprite->fillSprite(COLOR_BG);
                UIHelper::drawHeader(sprite, "Success", nullptr);
                sprite->setTextColor(COLOR_SUCCESS);
                sprite->setTextDatum(MC_DATUM);
                sprite->drawString("Password forgotten!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
                sprite->pushSprite(0, 0);
                delay(1500);
                break;
            }
            
            // âœ… Use ButtonHandle for faster response
            if (buttons) {
                buttons->update();
                if (buttons->specialPressed()) {
                    break;
                }
            } else if (digitalRead(BTN_SPECIAL) == LOW) {
                delay(150);
                break;
            }
            delay(10);
        }
    }
    
    // âœ… NEW: Auto-reconnect to saved WiFi after exiting scanner
    void autoReconnectWiFi() {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("[WiFi] Already connected");
            return;
        }
        
        if (!credsMgr || credsMgr->getCount() == 0) {
            Serial.println("[WiFi] No saved networks");
            return;
        }
        
        Serial.println("[WiFi] Auto-reconnecting to saved network...");
        
        SavedNetwork* recent = credsMgr->getMostRecent();
        if (recent) {
            Serial.printf("[WiFi] Connecting to '%s'\n", recent->ssid.c_str());
            
            WiFi.disconnect(true);
            delay(300);
            WiFi.mode(WIFI_STA);
            WiFi.begin(recent->ssid.c_str(), recent->password.c_str());
            
            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
                delay(100);
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WiFi] Auto-reconnected!");
            } else {
                Serial.println("[WiFi] Auto-reconnect failed");
            }
        }
    }
    
public:
    AppStateManager(TFT_eSprite* spr, AiEsp32RotaryEncoder* enc, DS3231* r, TFT_eSPI* t) 
        : sprite(spr), encoder(enc), rtc(r), tft(t),
          wifiScanner(nullptr), credsMgr(nullptr), displayMgr(nullptr),
          keyboard(nullptr), aiAssistant(nullptr), aiChatUI(nullptr),
          menu(nullptr), settings(nullptr), aboutUI(nullptr), 
          clockSettingsUI(nullptr), wallpaperMgr(nullptr), buttons(nullptr),
          currentMode(MODE_WATCH), previousMode(MODE_WATCH),
          selectedNetwork(0), scrollOffset(0), isScanning(false) {}
    
    void initializeManagers(DisplayManager* dm, CredentialsManager* cm) {
        displayMgr = dm;
        credsMgr = cm;
        
        settings = new SettingsUI(sprite, encoder);
        settings->begin();
        
        wallpaperMgr = new WallpaperManager(sprite, encoder, tft);
        
        // âœ… Initialize button handler
        buttons = new ButtonHandle();
        buttons->begin();
    }
    
    void handleWiFiScanner() {
        static int lastEncoderVal = 0;
        
        // âœ… Update buttons at start of each call
        if (buttons) buttons->update();
        
        if (encoder->encoderChanged()) {
            int val = encoder->readEncoder();
            if (val != lastEncoderVal) {
                //playTickSound();
                lastEncoderVal = val;
            }
            selectedNetwork = val;
            if (selectedNetwork < scrollOffset) {
                scrollOffset = selectedNetwork;
            } else if (selectedNetwork >= scrollOffset + VISIBLE_ITEMS) {
                scrollOffset = selectedNetwork - VISIBLE_ITEMS + 1;
            }
        }
        
        if (encoder->isEncoderButtonClicked()) {
            WiFiNetwork* network = wifiScanner->getNetwork(selectedNetwork);
            if (network) connectToNetwork(network);
            delay(150); // âœ… Reduced delay
        }
        
        // âœ… IMPROVED: Scan button with ButtonHandle
        if (buttons && buttons->scanPressed()) {
            displayMgr->resetSpinner();
            isScanning = true;
            
            WiFi.scanNetworks(true, true);
            
            unsigned long scanStart = millis();
            while (WiFi.scanComplete() < 0 && millis() - scanStart < 10000) {
                sprite->fillSprite(COLOR_BG);
                UIHelper::drawHeader(sprite, "Scanning WiFi", nullptr);
                
                int progress = map(millis() - scanStart, 0, 10000, 0, 99);
                
                int barY = SCREEN_HEIGHT / 2;
                sprite->fillRoundRect(40, barY, 240, 30, 8, COLOR_CARD);
                sprite->drawRoundRect(40, barY, 240, 30, 8, COLOR_BORDER);
                
                int fillWidth = map(progress, 0, 100, 0, 236);
                sprite->fillRoundRect(42, barY + 2, fillWidth, 26, 6, COLOR_ACCENT);
                
                sprite->setTextColor(COLOR_TEXT);
                sprite->setTextDatum(MC_DATUM);
                char progStr[10];
                sprintf(progStr, "%d%%", progress);
                sprite->drawString(progStr, SCREEN_WIDTH / 2, barY + 15, 2);
                
                sprite->pushSprite(0, 0);
                delay(100);
            }
            
            int n = WiFi.scanComplete();
            if (n >= 0) {
                wifiScanner->startScanBlocking();
            }
            
            isScanning = false;
            encoder->setBoundaries(0, max(0, wifiScanner->getNetworkCount() - 1), true);
            selectedNetwork = 0;
            scrollOffset = 0;
        }
        
        // âœ… IMPROVED: Delete button with ButtonHandle
        if (buttons && buttons->delPressed()) {
            WiFiNetwork* network = wifiScanner->getNetwork(selectedNetwork);
            if (network && credsMgr && credsMgr->hasCredentials(network->ssid)) {
                forgetPassword(network);
            }
        }
        
        // âœ… IMPROVED: Home button with ButtonHandle + Auto-reconnect
        if (buttons && buttons->specialPressed()) {
            currentMode = MODE_MENU;
            
            // âœ… Auto-reconnect to saved WiFi when exiting
            autoReconnectWiFi();
            
            delay(150);
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
        
        // âœ… Update buttons at start of each call
        if (buttons) buttons->update();
        
        if (encoder->encoderChanged()) {
            int val = encoder->readEncoder();
            int delta = val - lastEncoderVal;
            //if (delta != 0) playTickSound();
            lastEncoderVal = val;
            if (aiChatUI) {
                aiChatUI->scroll(-delta * 3);
            }
        }
        
        // âœ… IMPROVED: Mic button with ButtonHandle
        if (buttons && buttons->shiftPressed()) {
            if (aiAssistant && !aiAssistant->getIsSpeaking()) {
                aiAssistant->startListening();
                audioConnecttoSpeech(aiAssistant->getSTT().c_str(),"en");
                if (aiChatUI) aiChatUI->scrollToBottom();
            }
        }
        
        // âœ… IMPROVED: Home button with ButtonHandle
        if (buttons && buttons->specialPressed()) {
            currentMode = MODE_MENU;
            encoder->reset();
            encoder->setBoundaries(0, menu->getItemCount() - 1, true);
            delay(150);
        }
        
        if (aiChatUI) aiChatUI->draw();
    }
    
    void handleMenuSelection(AppMode selectedApp);
    
    SystemMode getCurrentMode() { return currentMode; }
    void setCurrentMode(SystemMode mode) { currentMode = mode; }
    void setMenu(MenuSystem* m) { menu = m; }
    WiFiScanner* getWiFiScanner() { return wifiScanner; }
    ButtonHandle* getButtonHandler() { return buttons; } // âœ… Added getter
};

#endif