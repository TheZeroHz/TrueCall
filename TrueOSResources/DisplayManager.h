// DisplayManager.h - Display with saved credentials indicator
#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <TFT_eSPI.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"
#include "WiFiNetwork.h"
#include "CredentialsManager.h"

class DisplayManager {
private:
    TFT_eSPI* tft;
    TFT_eSprite sprite;
    int spinnerAngle;
    
public:
    DisplayManager(TFT_eSPI* display) : tft(display), sprite(display), spinnerAngle(0) {}
    
    void begin() {
        tft->init();
        tft->setRotation(DISPLAY_ROTATION);
        tft->fillScreen(COLOR_BG);
        
        if (TFT_BL >= 0) {
            pinMode(TFT_BL, OUTPUT);
            digitalWrite(TFT_BL, HIGH);
        }
        
        sprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
        sprite.setSwapBytes(true);
        sprite.setTextWrap(false);
    }
    
    TFT_eSprite* getSprite() {
        return &sprite;
    }
    
    void showSplashScreen() {
        sprite.fillSprite(COLOR_BG);
        
        for (int i = 3; i > 0; i--) {
            uint16_t color = sprite.color565(0, 128 - (i * 30), 255 - (i * 40));
            sprite.drawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 10, 20 + (i * 12), color);
        }
        
        sprite.setTextColor(COLOR_TEXT);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("WiFi Scanner", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 40, 4);
        
        sprite.setTextColor(COLOR_ACCENT);
        sprite.drawString("v1.1", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 65, 2);
        
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.drawString("Initializing...", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 30, 2);
        
        sprite.pushSprite(0, 0);
    }
    
    void drawNetworkListWithCreds(WiFiNetwork* networks, int count, int selectedIndex, int scrollOffset, CredentialsManager* creds) {
        sprite.fillSprite(COLOR_BG);
        
        char subtitle[20];
        snprintf(subtitle, sizeof(subtitle), "%d APs", count);
        UIHelper::drawHeader(&sprite, "WiFi Networks", subtitle);
        
        int listY = HEADER_HEIGHT + 2;
        
        if (count == 0) {
            UIHelper::drawMessage(&sprite, "No networks found", COLOR_TEXT_DIM);
        } else {
            for (int i = 0; i < VISIBLE_ITEMS && (scrollOffset + i) < count; i++) {
                int index = scrollOffset + i;
                int y = listY + (i * ITEM_HEIGHT) + CARD_MARGIN;
                bool hasSaved = creds ? creds->hasCredentials(networks[index].ssid) : false;
                drawNetworkCard(&networks[index], y, (index == selectedIndex), hasSaved);
            }
            
            UIHelper::drawScrollbar(&sprite, scrollOffset, count, VISIBLE_ITEMS);
        }
        
        UIHelper::drawFooter(&sprite, "Rotate: Scroll  |  Press: Connect  |  Power: Scan");
        
        sprite.pushSprite(0, 0);
    }
    
    void drawNetworkList(WiFiNetwork* networks, int count, int selectedIndex, int scrollOffset) {
        drawNetworkListWithCreds(networks, count, selectedIndex, scrollOffset, nullptr);
    }
    
    void drawNetworkCard(WiFiNetwork* network, int y, bool selected, bool hasSavedCreds = false) {
        int cardWidth = SCREEN_WIDTH - (CARD_MARGIN * 2) - 8;
        int cardHeight = ITEM_HEIGHT - (CARD_MARGIN * 2);
        int x = CARD_MARGIN;
        
        uint16_t bgColor = selected ? COLOR_SELECTED : COLOR_CARD;
        UIHelper::drawCard(&sprite, x, y, cardWidth, cardHeight, bgColor, selected);
        
        // Signal bars
        UIHelper::drawSignalBars(&sprite, x + 8, y + 14, network->rssi);
        
        // Saved credential indicator (key icon in top-right)
        if (hasSavedCreds) {
            int keyX = x + cardWidth - 40;
            int keyY = y + 8;
            
            // Draw key icon with star/checkmark
            sprite.fillCircle(keyX, keyY, 6, COLOR_ACCENT);
            sprite.setTextColor(COLOR_BG);
            sprite.setTextDatum(MC_DATUM);
            sprite.drawString("S", keyX, keyY, 1); // "S" for Saved
        }
        
        // Lock icon
        if (network->isEncrypted()) {
            UIHelper::drawLockIcon(&sprite, x + cardWidth - 22, y + 12, COLOR_TEXT_DIM);
        }
        
        // SSID
        sprite.setTextColor(COLOR_TEXT);
        sprite.setTextDatum(TL_DATUM);
        String ssid = network->ssid;
        if (ssid.length() > 20) {
            ssid = ssid.substring(0, 20) + "...";
        }
        sprite.drawString(ssid, x + 42, y + 6, 2);
        
        // Signal quality + Channel
        sprite.setTextColor(getSignalColor(network->rssi));
        sprite.setTextDatum(TL_DATUM);
        char info[32];
        snprintf(info, sizeof(info), "%s  •  Ch%d  •  %s", 
                 getSignalText(network->rssi), 
                 network->channel,
                 network->getEncryptionName().c_str());
        sprite.drawString(info, x + 42, y + 26, 1);
    }
    
    void showScanning() {
        sprite.fillSprite(COLOR_BG);
        
        UIHelper::drawHeader(&sprite, "Scanning...", nullptr);
        
        for (int i = 0; i < 3; i++) {
            int radius = 20 + (i * 15);
            
            for (int a = 0; a < 360; a += 3) {
                float rad = (a + spinnerAngle + (i * 30)) * PI / 180.0;
                int x = SCREEN_WIDTH / 2 + cos(rad) * radius;
                int y = SCREEN_HEIGHT / 2 + sin(rad) * radius;
                
                if (a % 120 < 60) {
                    uint8_t brightness = 128 + ((60 - (a % 60)) * 2);
                    uint16_t color = sprite.color565(0, brightness / 2, brightness);
                    sprite.fillCircle(x, y, 2, color);
                }
            }
        }
        
        spinnerAngle = (spinnerAngle + 8) % 360;
        
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("Searching for networks...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 60, 2);
        
        sprite.pushSprite(0, 0);
    }
    
    void showConnecting(const String& ssid) {
        sprite.fillSprite(COLOR_BG);
        
        UIHelper::drawHeader(&sprite, "Connecting", nullptr);
        
        sprite.setTextColor(COLOR_TEXT);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString("Network:", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 30, 2);
        
        sprite.setTextColor(COLOR_ACCENT);
        String displaySsid = ssid;
        if (displaySsid.length() > 28) {
            displaySsid = displaySsid.substring(0, 28) + "...";
        }
        sprite.drawString(displaySsid, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 4);
        
        UIHelper::drawSpinner(&sprite, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 50, spinnerAngle);
        spinnerAngle = (spinnerAngle + 15) % 360;
        
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.drawString("Please wait...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 80, 1);
        
        sprite.pushSprite(0, 0);
    }
    
    void showConnectionResult(bool success, const String& message, const String& details = "") {
        sprite.fillSprite(COLOR_BG);
        
        UIHelper::drawHeader(&sprite, success ? "Connected" : "Failed", nullptr);
        
        uint16_t statusColor = success ? COLOR_SUCCESS : COLOR_ERROR;
        
        int iconY = SCREEN_HEIGHT / 2 - 30;
        if (success) {
            sprite.drawCircle(SCREEN_WIDTH / 2, iconY, 25, statusColor);
            sprite.drawCircle(SCREEN_WIDTH / 2, iconY, 24, statusColor);
            sprite.drawLine(SCREEN_WIDTH / 2 - 12, iconY, SCREEN_WIDTH / 2 - 5, iconY + 8, statusColor);
            sprite.drawLine(SCREEN_WIDTH / 2 - 12, iconY + 1, SCREEN_WIDTH / 2 - 5, iconY + 9, statusColor);
            sprite.drawLine(SCREEN_WIDTH / 2 - 5, iconY + 9, SCREEN_WIDTH / 2 + 12, iconY - 7, statusColor);
        } else {
            sprite.drawCircle(SCREEN_WIDTH / 2, iconY, 25, statusColor);
            sprite.drawCircle(SCREEN_WIDTH / 2, iconY, 24, statusColor);
            sprite.drawLine(SCREEN_WIDTH / 2 - 10, iconY - 10, SCREEN_WIDTH / 2 + 10, iconY + 10, statusColor);
            sprite.drawLine(SCREEN_WIDTH / 2 + 10, iconY - 10, SCREEN_WIDTH / 2 - 10, iconY + 10, statusColor);
            sprite.drawLine(SCREEN_WIDTH / 2 - 10, iconY - 9, SCREEN_WIDTH / 2 + 10, iconY + 11, statusColor);
            sprite.drawLine(SCREEN_WIDTH / 2 + 10, iconY - 9, SCREEN_WIDTH / 2 - 10, iconY + 11, statusColor);
        }
        
        sprite.setTextColor(statusColor);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString(message, SCREEN_WIDTH / 2, iconY + 45, 2);
        
        if (details.length() > 0) {
            sprite.setTextColor(COLOR_TEXT);
            sprite.drawString(details, SCREEN_WIDTH / 2, iconY + 70, 2);
        }
        
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.drawString("Press to continue", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 30, 1);
        
        sprite.pushSprite(0, 0);
    }
    
    void resetSpinner() {
        spinnerAngle = 0;
    }
};

#endif