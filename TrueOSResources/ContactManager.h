// ContactManager.h - FIXED: ESP-NOW works after WiFi reconnect
#ifndef CONTACTMANAGER_H
#define CONTACTMANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <JPEGDecoder.h>
#include <FFat.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"
#include "AudioManager.h"
#include "ButtonHandle.h"

#define MAX_CONTACTS 10
#define FIREBASE_URL "https://your-project.firebaseio.com"
#define FIREBASE_AUTH "your-firebase-secret-key"

struct Contact {
    String name;
    String role;
    String phone;
    String imagePath;
    uint8_t espnowMAC[6];
    
    Contact() : name(""), role(""), phone(""), imagePath("") {
        memset(espnowMAC, 0, 6);
    }
};

class ContactManager {
private:
    TFT_eSprite* sprite;
    AiEsp32RotaryEncoder* encoder;
    TFT_eSPI* tft;
    ButtonHandle* buttons;
    
    Contact contacts[MAX_CONTACTS];
    int contactCount;
    int currentContact;
    bool espnowInitialized;
    
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
        Serial.print("[ESP-NOW] Send Status: ");
        Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
    }
    
    void playTickSound() {
        if (FFat.exists("/sounds/feedback/rotary_encoder.wav")) {
            audioConnecttoFFat("/sounds/feedback/rotary_encoder.wav");
        }
    }
    
    void drawCircularPhoto(int centerX, int centerY, int radius, const char* imagePath) {
        TFT_eSprite photo = TFT_eSprite(tft);
        photo.setColorDepth(16);
        photo.createSprite(radius * 2, radius * 2);
        photo.fillSprite(TFT_BLACK);
        
        if (FFat.exists(imagePath)) {
            File jpegFile = FFat.open(imagePath, FILE_READ);
            if (jpegFile && JpegDec.decodeFsFile(jpegFile)) {
                uint16_t *pImg;
                while (JpegDec.read()) {
                    pImg = JpegDec.pImage;
                    int mcu_x = JpegDec.MCUx * JpegDec.MCUWidth;
                    int mcu_y = JpegDec.MCUy * JpegDec.MCUHeight;
                    if (mcu_x < radius * 2 && mcu_y < radius * 2) {
                        photo.pushImage(mcu_x, mcu_y, JpegDec.MCUWidth, JpegDec.MCUHeight, pImg);
                    }
                }
            }
            jpegFile.close();
        } else {
            uint16_t colors[] = {COLOR_ACCENT, COLOR_SUCCESS, COLOR_WARNING, COLOR_INFO};
            photo.fillCircle(radius, radius, radius - 5, colors[currentContact % 4]);
            photo.setTextColor(TFT_WHITE);
            photo.setTextDatum(MC_DATUM);
            photo.setTextSize(3);
            
            String initials = String(contacts[currentContact].name[0]);
            int spacePos = contacts[currentContact].name.indexOf(' ');
            if (spacePos > 0) {
                initials += String(contacts[currentContact].name[spacePos + 1]);
            }
            photo.drawString(initials, radius, radius);
        }
        
        const uint16_t* srcPtr = (const uint16_t*)photo.getPointer();
        for (int y = 0; y < radius * 2; y++) {
            for (int x = 0; x < radius * 2; x++) {
                int dx = x - radius;
                int dy = y - radius;
                if (dx*dx + dy*dy <= (radius-2)*(radius-2)) {
                    sprite->drawPixel(centerX - radius + x, centerY - radius + y, 
                                    srcPtr[y * radius * 2 + x]);
                }
            }
        }
        
        for (int i = 0; i < 3; i++) {
            sprite->drawCircle(centerX, centerY, radius + i, COLOR_ACCENT);
        }
        
        photo.deleteSprite();
    }
    
    // âœ… FIXED: Complete ESP-NOW reinitialization
    bool initESPNow() {
        Serial.println("\n[ESP-NOW] ========================================");
        Serial.println("[ESP-NOW] Initializing ESP-NOW...");
        
        // Check WiFi connection
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[ESP-NOW] ERROR: WiFi not connected!");
            Serial.println("[ESP-NOW] ========================================\n");
            return false;
        }
        
        // Get current WiFi channel
        uint8_t channel;
        wifi_second_chan_t secondChan;
        esp_wifi_get_channel(&channel, &secondChan);
        
        Serial.printf("[ESP-NOW] WiFi SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("[ESP-NOW] WiFi RSSI: %d dBm\n", WiFi.RSSI());
        Serial.printf("[ESP-NOW] WiFi Channel: %d\n", channel);
        Serial.printf("[ESP-NOW] Local MAC: %s\n", WiFi.macAddress().c_str());
        
        // âœ… CRITICAL: Deinitialize if already initialized
        if (espnowInitialized) {
            Serial.println("[ESP-NOW] Deinitializing previous instance...");
            esp_now_deinit();
            espnowInitialized = false;
            delay(100);
        }
        
        // âœ… Set WiFi mode to STA+AP (required for ESP-NOW)
        Serial.println("[ESP-NOW] Setting WiFi mode to STA+AP...");
        WiFi.mode(WIFI_AP_STA);
        delay(100);
        
        // Verify channel didn't change
        esp_wifi_get_channel(&channel, &secondChan);
        Serial.printf("[ESP-NOW] Channel after mode change: %d\n", channel);
        
        // âœ… Initialize ESP-NOW
        Serial.println("[ESP-NOW] Calling esp_now_init()...");
        esp_err_t initResult = esp_now_init();
        
        if (initResult != ESP_OK) {
            Serial.printf("[ESP-NOW] ERROR: Init failed! Error code: 0x%x\n", initResult);
            
            // Try to recover
            Serial.println("[ESP-NOW] Attempting recovery...");
            WiFi.mode(WIFI_STA);
            delay(200);
            WiFi.mode(WIFI_AP_STA);
            delay(200);
            
            initResult = esp_now_init();
            if (initResult != ESP_OK) {
                Serial.println("[ESP-NOW] ERROR: Recovery failed!");
                Serial.println("[ESP-NOW] ========================================\n");
                return false;
            }
        }
        
        Serial.println("[ESP-NOW] âœ… Initialization successful!");
        
        // Register send callback
        esp_err_t callbackResult = esp_now_register_send_cb(onDataSent);
        if (callbackResult != ESP_OK) {
            Serial.printf("[ESP-NOW] WARNING: Callback registration failed: 0x%x\n", callbackResult);
        } else {
            Serial.println("[ESP-NOW] âœ… Send callback registered");
        }
        
        espnowInitialized = true;
        
        Serial.println("[ESP-NOW] ========================================\n");
        return true;
    }
    
    bool sendFirebaseCall(Contact* contact) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Firebase] WiFi not connected!");
            return false;
        }
        
        HTTPClient http;
        String url = String(FIREBASE_URL) + "/calls.json?auth=" + String(FIREBASE_AUTH);
        
        Serial.printf("[Firebase] URL: %s\n", url.c_str());
        
        if (!http.begin(url)) {
            Serial.println("[Firebase] Failed to begin HTTP");
            return false;
        }
        
        http.addHeader("Content-Type", "application/json");
        
        DynamicJsonDocument doc(512);
        doc["caller"] = "ESP32 Watch";
        doc["callee"] = contact->name;
        doc["phone"] = contact->phone;
        doc["timestamp"] = millis();
        doc["type"] = "firebase";
        
        String payload;
        serializeJson(doc, payload);
        
        Serial.printf("[Firebase] Payload: %s\n", payload.c_str());
        
        int httpCode = http.POST(payload);
        Serial.printf("[Firebase] Response code: %d\n", httpCode);
        
        bool success = false;
        if (httpCode == HTTP_CODE_OK || httpCode == 200) {
            String response = http.getString();
            Serial.printf("[Firebase] Response: %s\n", response.c_str());
            success = true;
        } else {
            Serial.printf("[Firebase] Error: %s\n", http.errorToString(httpCode).c_str());
        }
        
        http.end();
        return success;
    }
    
    // âœ… FIXED: Complete ESP-NOW send with proper peer management
    bool sendESPNowCall(Contact* contact) {
        Serial.println("\n[ESP-NOW] ========================================");
        Serial.println("[ESP-NOW] Preparing to send call...");
        
        // Initialize ESP-NOW (handles reinit if needed)
        if (!initESPNow()) {
            Serial.println("[ESP-NOW] Failed to initialize");
            Serial.println("[ESP-NOW] ========================================\n");
            return false;
        }
        
        // Validate MAC address
        bool validMAC = false;
        for (int i = 0; i < 6; i++) {
            if (contact->espnowMAC[i] != 0) {
                validMAC = true;
                break;
            }
        }
        
        if (!validMAC) {
            Serial.println("[ESP-NOW] ERROR: Invalid MAC address (all zeros)!");
            Serial.println("[ESP-NOW] ========================================\n");
            return false;
        }
        
        // Print target MAC
        Serial.printf("[ESP-NOW] Target MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            contact->espnowMAC[0], contact->espnowMAC[1], contact->espnowMAC[2],
            contact->espnowMAC[3], contact->espnowMAC[4], contact->espnowMAC[5]);
        
        // âœ… Get current WiFi channel
        uint8_t channel;
        wifi_second_chan_t secondChan;
        esp_wifi_get_channel(&channel, &secondChan);
        Serial.printf("[ESP-NOW] Using WiFi channel: %d\n", channel);
        
        // âœ… Remove peer if already exists
        if (esp_now_is_peer_exist(contact->espnowMAC)) {
            Serial.println("[ESP-NOW] Peer already exists, removing...");
            esp_err_t delResult = esp_now_del_peer(contact->espnowMAC);
            if (delResult != ESP_OK) {
                Serial.printf("[ESP-NOW] WARNING: Failed to delete peer: 0x%x\n", delResult);
            } else {
                Serial.println("[ESP-NOW] âœ… Peer removed");
            }
            delay(50);
        }
        
        // âœ… Add peer with correct channel
        Serial.println("[ESP-NOW] Adding peer...");
        esp_now_peer_info_t peerInfo;
        memset(&peerInfo, 0, sizeof(peerInfo));
        memcpy(peerInfo.peer_addr, contact->espnowMAC, 6);
        peerInfo.channel = channel;  // âœ… Use actual WiFi channel
        peerInfo.encrypt = false;
        peerInfo.ifidx = WIFI_IF_STA;  // âœ… Use STA interface
        
        esp_err_t addResult = esp_now_add_peer(&peerInfo);
        if (addResult != ESP_OK) {
            Serial.printf("[ESP-NOW] ERROR: Failed to add peer! Error: 0x%x\n", addResult);
            
            // Error codes:
            // 0x306A = ESP_ERR_ESPNOW_FULL (peer list full)
            // 0x306B = ESP_ERR_ESPNOW_EXIST (peer already exists)
            // 0x3069 = ESP_ERR_ESPNOW_NOT_INIT (not initialized)
            
            if (addResult == 0x306B) {
                Serial.println("[ESP-NOW] Peer already exists (shouldn't happen)");
            } else if (addResult == 0x306A) {
                Serial.println("[ESP-NOW] Peer list full, clearing all peers...");
                // This shouldn't happen with only 1-3 peers
            }
            
            Serial.println("[ESP-NOW] ========================================\n");
            return false;
        }
        
        Serial.println("[ESP-NOW] âœ… Peer added successfully");
        
        // âœ… Create call data
        struct CallData {
            char caller[32];
            char callee[32];
            uint32_t timestamp;
        } callData;
        
        strncpy(callData.caller, "ESP32 Watch", sizeof(callData.caller));
        strncpy(callData.callee, contact->name.c_str(), sizeof(callData.callee));
        callData.timestamp = millis();
        
        Serial.printf("[ESP-NOW] Call data: caller='%s', callee='%s'\n", 
            callData.caller, callData.callee);
        Serial.printf("[ESP-NOW] Data size: %d bytes\n", sizeof(callData));
        
        // âœ… Send data
        Serial.println("[ESP-NOW] Sending data...");
        esp_err_t sendResult = esp_now_send(contact->espnowMAC, (uint8_t*)&callData, sizeof(callData));
        
        if (sendResult != ESP_OK) {
            Serial.printf("[ESP-NOW] ERROR: Send failed! Error: 0x%x\n", sendResult);
            Serial.println("[ESP-NOW] ========================================\n");
            return false;
        }
        
        Serial.println("[ESP-NOW] âœ… Send command successful!");
        Serial.println("[ESP-NOW] Waiting for send callback...");
        
        // Wait a bit for callback (async)
        delay(100);
        
        Serial.println("[ESP-NOW] ========================================\n");
        return true;
    }
    
    void showCallAnimation(Contact* contact, const char* method) {
        unsigned long startTime = millis();
        int ringPhase = 0;
        
        while (millis() - startTime < 3000) {
            if (buttons) {
                buttons->update();
                if (buttons->specialPressed()) {
                    Serial.println("[Contacts] Call cancelled");
                    break;
                }
            } else if (digitalRead(BTN_SPECIAL) == LOW) {
                Serial.println("[Contacts] Call cancelled");
                delay(150);
                break;
            }
            
            sprite->fillSprite(COLOR_BG);
            
            int pulse = abs((int)(sin((millis() % 1000) * PI / 500.0) * 20));
            for (int i = 0; i < SCREEN_HEIGHT; i++) {
                uint8_t brightness = pulse + (i / 10);
                uint16_t color = sprite->color565(0, brightness/2, brightness);
                sprite->drawFastHLine(0, i, SCREEN_WIDTH, color);
            }
            
            UIHelper::drawHeader(sprite, "Calling...", method);
            
            drawCircularPhoto(SCREEN_WIDTH / 2, 100, 50, contact->imagePath.c_str());
            
            sprite->setTextColor(COLOR_TEXT);
            sprite->setTextDatum(MC_DATUM);
            sprite->drawString(contact->name, SCREEN_WIDTH / 2, 170, 4);
            
            sprite->setTextColor(COLOR_TEXT_DIM);
            sprite->drawString(contact->role, SCREEN_WIDTH / 2, 195, 2);
            
            for (int i = 0; i < 3; i++) {
                int radius = 60 + (ringPhase + i * 20) % 60;
                int alpha = 255 - ((ringPhase + i * 20) % 60) * 4;
                if (alpha > 0) {
                    uint16_t ringColor = sprite->color565(0, alpha/2, alpha);
                    sprite->drawCircle(SCREEN_WIDTH / 2, 100, radius, ringColor);
                }
            }
            
            ringPhase = (ringPhase + 3) % 60;
            
            sprite->setTextColor(COLOR_TEXT_DIM);
            sprite->drawString("Connecting...", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 20, 2);
            
            sprite->pushSprite(0, 0);
            delay(50);
        }
    }
    
public:
    ContactManager(TFT_eSprite* spr, AiEsp32RotaryEncoder* enc, TFT_eSPI* tftRef)
        : sprite(spr), encoder(enc), tft(tftRef), buttons(nullptr),
          contactCount(0), currentContact(0), espnowInitialized(false) {
        
        buttons = new ButtonHandle();
        buttons->begin();
        
        loadContacts();
    }
    
    void loadContacts() {
        contactCount = 3;
        
        contacts[0].name = "Rakib Hasan";
        contacts[0].role = "CEO - N.A.M.O.R. Inc.";
        contacts[0].phone = "+880 1234-567890";
        contacts[0].imagePath = "/team/rakib.jpg";
        contacts[0].espnowMAC[0] = 0xAA;
        contacts[0].espnowMAC[1] = 0xBB;
        contacts[0].espnowMAC[2] = 0xCC;
        contacts[0].espnowMAC[3] = 0xDD;
        contacts[0].espnowMAC[4] = 0xEE;
        contacts[0].espnowMAC[5] = 0x01;
        
        contacts[1].name = "Sahadat Sani";
        contacts[1].role = "CTO - N.A.M.O.R. Inc.";
        contacts[1].phone = "+880 1234-567891";
        contacts[1].imagePath = "/team/sani.jpg";
        contacts[1].espnowMAC[0] = 0xAA;
        contacts[1].espnowMAC[1] = 0xBB;
        contacts[1].espnowMAC[2] = 0xCC;
        contacts[1].espnowMAC[3] = 0xDD;
        contacts[1].espnowMAC[4] = 0xEE;
        contacts[1].espnowMAC[5] = 0x02;
        
        contacts[2].name = "A B M Shawkat Ali";
        contacts[2].role = "Director - N.A.M.O.R. Inc.";
        contacts[2].phone = "+880 1234-567892";
        contacts[2].imagePath = "/team/shawkat.jpg";
        contacts[2].espnowMAC[0] = 0xAA;
        contacts[2].espnowMAC[1] = 0xBB;
        contacts[2].espnowMAC[2] = 0xCC;
        contacts[2].espnowMAC[3] = 0xDD;
        contacts[2].espnowMAC[4] = 0xEE;
        contacts[2].espnowMAC[5] = 0x03;
        
        Serial.printf("[Contacts] Loaded %d contacts\n", contactCount);
    }
    
    void show() {
        if (contactCount == 0) {
            sprite->fillSprite(COLOR_BG);
            UIHelper::drawHeader(sprite, "Contacts", nullptr);
            UIHelper::drawMessage(sprite, "No contacts available", COLOR_TEXT_DIM);
            sprite->pushSprite(0, 0);
            delay(1500);
            return;
        }
        
        encoder->reset();
        encoder->setBoundaries(0, contactCount - 1, true);
        encoder->setEncoderValue(currentContact);
        
        bool done = false;
        int lastEncoderVal = currentContact;
        
        Serial.println("[Contacts] Entering contact view");
        
        while (!done) {
            if (buttons) buttons->update();
            
            if (encoder->encoderChanged()) {
                int val = encoder->readEncoder();
                if (val != lastEncoderVal) {
                    playTickSound();
                    lastEncoderVal = val;
                    currentContact = val;
                    Serial.printf("[Contacts] Selected contact %d\n", currentContact);
                }
            }
            
            if (encoder->isEncoderButtonClicked()) {
                Contact* contact = &contacts[currentContact];
                
                Serial.println("[Contacts] Firebase call initiated");
                
                sprite->fillSprite(COLOR_BG);
                UIHelper::drawHeader(sprite, "Firebase Call", nullptr);
                UIHelper::drawMessage(sprite, "Initiating call...", COLOR_ACCENT);
                sprite->pushSprite(0, 0);
                
                bool success = sendFirebaseCall(contact);
                
                if (success) {
                    showCallAnimation(contact, "Firebase");
                    
                    sprite->fillSprite(COLOR_BG);
                    UIHelper::drawHeader(sprite, "Call Sent", nullptr);
                    sprite->setTextColor(COLOR_SUCCESS);
                    sprite->setTextDatum(MC_DATUM);
                    sprite->drawString("Firebase call initiated!", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 2);
                    sprite->setTextColor(COLOR_TEXT);
                    sprite->drawString(contact->name, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10, 2);
                } else {
                    sprite->fillSprite(COLOR_BG);
                    UIHelper::drawHeader(sprite, "Call Failed", nullptr);
                    UIHelper::drawMessage(sprite, "Check WiFi connection", COLOR_ERROR);
                }
                
                sprite->pushSprite(0, 0);
                delay(1500);
                
                Serial.println("[Contacts] Resetting encoder after Firebase call");
                encoder->reset();
                encoder->setBoundaries(0, contactCount - 1, true);
                encoder->setEncoderValue(currentContact);
                lastEncoderVal = currentContact;
                
                delay(200);
            }
            
            if (buttons && buttons->selectPressed()) {
                Contact* contact = &contacts[currentContact];
                
                Serial.println("[Contacts] ESP-NOW call initiated");
                
                sprite->fillSprite(COLOR_BG);
                UIHelper::drawHeader(sprite, "ESP-NOW Call", nullptr);
                UIHelper::drawMessage(sprite, "Initiating call...", COLOR_ACCENT);
                sprite->pushSprite(0, 0);
                
                bool success = sendESPNowCall(contact);
                
                if (success) {
                    showCallAnimation(contact, "ESP-NOW");
                    
                    sprite->fillSprite(COLOR_BG);
                    UIHelper::drawHeader(sprite, "Call Sent", nullptr);
                    sprite->setTextColor(COLOR_SUCCESS);
                    sprite->setTextDatum(MC_DATUM);
                    sprite->drawString("ESP-NOW call initiated!", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 2);
                    sprite->setTextColor(COLOR_TEXT);
                    sprite->drawString(contact->name, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10, 2);
                } else {
                    sprite->fillSprite(COLOR_BG);
                    UIHelper::drawHeader(sprite, "Call Failed", nullptr);
                    UIHelper::drawMessage(sprite, "Check ESP-NOW setup", COLOR_ERROR);
                }
                
                sprite->pushSprite(0, 0);
                delay(1500);
                
                Serial.println("[Contacts] Resetting encoder after ESP-NOW call");
                encoder->reset();
                encoder->setBoundaries(0, contactCount - 1, true);
                encoder->setEncoderValue(currentContact);
                lastEncoderVal = currentContact;
                
                delay(200);
            }
            
            if (buttons && buttons->specialPressed()) {
                Serial.println("[Contacts] Exiting contact view");
                done = true;
            }
            
            sprite->fillSprite(COLOR_BG);
            
            char subtitle[10];
            sprintf(subtitle, "%d/%d", currentContact + 1, contactCount);
            UIHelper::drawHeader(sprite, "Contacts", subtitle);
            
            Contact* contact = &contacts[currentContact];
            
            int cardX = 10;
            int cardY = 55;
            int cardWidth = SCREEN_WIDTH - 20;
            int cardHeight = 120;
            
            sprite->fillRoundRect(cardX + 3, cardY + 3, cardWidth, cardHeight, 16, COLOR_SHADOW);
            sprite->fillRoundRect(cardX, cardY, cardWidth, cardHeight, 16, COLOR_CARD);
            sprite->drawRoundRect(cardX, cardY, cardWidth, cardHeight, 16, COLOR_ACCENT);
            sprite->drawRoundRect(cardX + 1, cardY + 1, cardWidth - 2, cardHeight - 2, 16, COLOR_ACCENT);
            
            int photoX = cardX + 50;
            int photoY = cardY + 60;
            drawCircularPhoto(photoX, photoY, 40, contact->imagePath.c_str());
            
            int infoX = cardX + 110;
            int infoY = cardY + 20;
            
            sprite->setTextColor(COLOR_TEXT);
            sprite->setTextDatum(TL_DATUM);
            sprite->drawString(contact->name, infoX, infoY, 4);
            
            sprite->setTextColor(COLOR_ACCENT);
            sprite->drawString(contact->role, infoX, infoY + 30, 2);
            
            sprite->setTextColor(COLOR_TEXT_DIM);
            sprite->drawString(contact->phone, infoX, infoY + 55, 2);
            
            int iconY = cardY + cardHeight + 15;
            
            sprite->fillRoundRect(40, iconY - 15, 100, 25, 8, COLOR_INFO);
            sprite->setTextColor(TFT_WHITE);
            sprite->setTextDatum(MC_DATUM);
            sprite->drawString("PUSH: Firebase", 90, iconY - 2, 1);
            
            sprite->fillRoundRect(180, iconY - 15, 100, 25, 8, COLOR_SUCCESS);
            sprite->setTextColor(TFT_WHITE);
            sprite->drawString("CAST: ESP-NOW", 230, iconY - 2, 1);
            
            sprite->setTextColor(COLOR_TEXT_DIM);
            sprite->setTextDatum(MC_DATUM);
            sprite->drawString("HOME: Exit", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 10, 1);
            
            sprite->pushSprite(0, 0);
            delay(10);
        }
        
        Serial.println("[Contacts] Exited contact view");
    }
    
    int getContactCount() const { return contactCount; }
};

#endif