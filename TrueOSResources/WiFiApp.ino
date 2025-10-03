/*
 * Unified WiFi Scanner + AI Assistant - Memory Optimized
 * Fixed I2S conflicts and proper resource management
 */

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>

// Local includes - ORDER MATTERS!
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

// ==================== System Mode ====================
enum SystemMode {
    MODE_WATCH,
    MODE_MENU,
    MODE_WIFI_SCANNER,
    MODE_AI_ASSISTANT
};
int previousMode = MODE_WATCH;
// ==================== Global Objects ====================
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite(&tft);
AiEsp32RotaryEncoder encoder(ENC_CLK, ENC_DT, ENC_SW, -1, ROTARY_STEPS);

// System components (pointers for lazy initialization)
WiFiScanner* wifiScanner = nullptr;
CredentialsManager* credsMgr = nullptr;
DisplayManager* displayMgr = nullptr;
KeyboardUI* keyboard = nullptr;
AIAssistant* aiAssistant = nullptr;
AIChatUI* aiChatUI = nullptr;
MenuSystem* menu = nullptr;

// State variables
SystemMode currentMode = MODE_WATCH;
int selectedNetwork = 0;
int scrollOffset = 0;
struct tm timeInfo;
unsigned long lastTimeUpdate = 0;
unsigned long lastButtonCheck = 0;

// ==================== Memory Monitoring ====================
void printMemoryUsage(const char* label) {
    Serial.printf("[MEM] %s - Free: %d bytes, Largest block: %d bytes\n",
                  label, 
                  ESP.getFreeHeap(), 
                  ESP.getMaxAllocHeap());
}

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

// ==================== Watch Face ====================
void drawWatchFace() {
    sprite.fillSprite(COLOR_BG);
    
    // Update time
    if (millis() - lastTimeUpdate > 1000) {
        getLocalTime(&timeInfo);
        lastTimeUpdate = millis();
    }
    
    // Time display
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
    
    // Time panel
    int panelY = 80;
    sprite.fillRoundRect(40, panelY, 240, 70, 16, COLOR_CARD);
    sprite.drawRoundRect(40, panelY, 240, 70, 16, COLOR_ACCENT);
    
    sprite.setTextColor(COLOR_ACCENT);
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextSize(1);
    sprite.drawString(timeStr, SCREEN_WIDTH / 2, panelY + 35, 7);
    
    // Date
    char dateStr[20];
    sprintf(dateStr, "%02d/%02d/%04d", 
            timeInfo.tm_mday, timeInfo.tm_mon + 1, timeInfo.tm_year + 1900);
    
    sprite.setTextColor(COLOR_TEXT);
    sprite.drawString(dateStr, SCREEN_WIDTH / 2, panelY + 90, 2);
    
    // WiFi status
    if (WiFi.status() == WL_CONNECTED) {
        sprite.setTextColor(COLOR_SUCCESS);
        sprite.drawString("WiFi Connected", SCREEN_WIDTH / 2, panelY + 115, 2);
    } else {
        sprite.setTextColor(COLOR_TEXT_DIM);
        sprite.drawString("WiFi Disconnected", SCREEN_WIDTH / 2, panelY + 115, 2);
    }
    
    // Instructions
    sprite.setTextColor(COLOR_TEXT_DIM);
    sprite.drawString("Press HOME for menu", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 20, 1);
    
    sprite.pushSprite(0, 0);
}

// ==================== WiFi Connection ====================
bool connectToNetwork(WiFiNetwork* network) {
    if (!network) return false;
    
    Serial.printf("\n>>> Attempting to connect to: %s\n", network->ssid.c_str());
    
    String password = "";
    bool hasSavedCreds = credsMgr && credsMgr->getCredentials(network->ssid, password);
    
    if (!hasSavedCreds && network->isEncrypted()) {
        Serial.println("Network requires password");
        
        if (!keyboard) {
            keyboard = new KeyboardUI(&sprite, &encoder);
            keyboard->begin();
        }
        
        password = keyboard->show("Enter Password");
        
        if (password.length() == 0) {
            Serial.println("Connection cancelled (no password entered)");
            return false;
        }
        
        Serial.printf("Password entered (%d chars)\n", password.length());
    }
    
    // Show connecting screen
    displayMgr->showConnecting(network->ssid);
    
    // Disconnect first
    WiFi.disconnect(true);
    delay(500);
    
    // Connect
    WiFi.mode(WIFI_STA);
    
    if (network->isEncrypted()) {
        WiFi.begin(network->ssid.c_str(), password.c_str());
    } else {
        WiFi.begin(network->ssid.c_str());
    }
    
    Serial.println("Waiting for connection...");
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < CONNECT_TIMEOUT) {
        displayMgr->showConnecting(network->ssid);
        delay(100);
    }
    
    bool success = (WiFi.status() == WL_CONNECTED);
    
    if (success) {
        Serial.println("Connected successfully!");
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        
        if (credsMgr && password.length() > 0) {
            credsMgr->save(network->ssid, password, network->rssi);
        }
        
        displayMgr->showConnectionResult(true, "Connected!", WiFi.localIP().toString());
        
        // Sync time if connected
        configTime(6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    } else {
        Serial.println("Connection failed!");
        displayMgr->showConnectionResult(false, "Connection Failed", "Check password");
    }
    
    // Wait for user to acknowledge
    while (digitalRead(ENC_SW) != LOW) {
        delay(10);
    }
    delay(300);
    
    return success;
}

// ==================== Mode Handlers ====================
void handleWatchMode() {
    // Clear button check on mode entry
    if (previousMode != MODE_WATCH) {
        previousMode = MODE_WATCH;
        delay(300); // Give time for button release
        return; // Skip first frame after mode change
    }
    
    drawWatchFace();
    
    // Home button goes to menu
    static unsigned long lastHomePress = 0;
    if (checkButtonPress(BTN_SPECIAL, lastHomePress)) {
        currentMode = MODE_MENU;
        
        if (!menu) {
            menu = new MenuSystem(&sprite);
        }
        
        encoder.reset();
        encoder.setBoundaries(0, menu->getItemCount() - 1, true);
        //play tick
    }
}

void handleMenuMode() {
    // Clear button check on mode entry
    if (previousMode != MODE_MENU) {
        previousMode = MODE_MENU;
        delay(300); // Give time for button release
        menu->draw(); // Draw once before returning
        return; // Skip first frame after mode change
    }
    
    static int lastEncoderVal = 0;
    
    // Encoder navigation
    if (encoder.encoderChanged()) {
        int val = encoder.readEncoder();
        if (val != lastEncoderVal) {
            //play tick
            lastEncoderVal = val;
        }
        menu->setSelected(val);
    }
    
    // Encoder button - select menu item
    if (encoder.isEncoderButtonClicked()) {
        AppMode selectedApp = menu->getSelectedMode();
        
        if (selectedApp == APP_WIFI_SCANNER) {
            currentMode = MODE_WIFI_SCANNER;
            
            // Initialize WiFi scanner if needed
            if (!wifiScanner) {
                wifiScanner = new WiFiScanner();
                wifiScanner->begin();
            }
            
            // Start scan
            displayMgr->resetSpinner();
            wifiScanner->startScanBlocking();
            
            encoder.reset();
            encoder.setBoundaries(0, max(0, wifiScanner->getNetworkCount() - 1), true);
            selectedNetwork = 0;
            scrollOffset = 0;
            
        } else if (selectedApp == APP_AI_ASSISTANT) {
            if (WiFi.status() == WL_CONNECTED) {
                currentMode = MODE_AI_ASSISTANT;
                
                // Lazy-init AI components
                if (!aiAssistant) {
                    Serial.println("[AI] Initializing assistant...");
                    aiAssistant = new AIAssistant();
                    if (!aiAssistant->begin()) {
                        Serial.println("[AI] Failed to initialize!");
                        delete aiAssistant;
                        aiAssistant = nullptr;
                        currentMode = MODE_MENU;
                        return;
                    }
                }
                
                if (!aiChatUI) {
                    aiChatUI = new AIChatUI(&sprite, aiAssistant);
                }
                
            } else {
                // Show error
                sprite.fillSprite(COLOR_BG);
                UIHelper::drawHeader(&sprite, "Error", nullptr);
                UIHelper::drawMessage(&sprite, "WiFi not connected!", COLOR_ERROR);
                sprite.pushSprite(0, 0);
                delay(2000);
            }
        }
        
        delay(200);
    }
    
    // Home button - back to watch
    static unsigned long lastHomePress = 0;
    if (checkButtonPress(BTN_SPECIAL, lastHomePress)) {
        currentMode = MODE_WATCH;
        //play tick
    }
    
    menu->draw();
}

void handleWiFiScanner() {
    static int lastEncoderVal = 0;
    
    // Encoder navigation
    if (encoder.encoderChanged()) {
        int val = encoder.readEncoder();
        if (val != lastEncoderVal) {
            //play tick
            lastEncoderVal = val;
        }
        selectedNetwork = val;
        
        // Auto-scroll
        if (selectedNetwork < scrollOffset) {
            scrollOffset = selectedNetwork;
        } else if (selectedNetwork >= scrollOffset + VISIBLE_ITEMS) {
            scrollOffset = selectedNetwork - VISIBLE_ITEMS + 1;
        }
    }
    
    // Encoder button - connect
    if (encoder.isEncoderButtonClicked()) {
        WiFiNetwork* network = wifiScanner->getNetwork(selectedNetwork);
        if (network) {
            connectToNetwork(network);
        }
        delay(200);
    }
    
    // Scan button
    static unsigned long lastScanPress = 0;
    if (checkButtonPress(BTN_SCAN, lastScanPress, 500)) {
        displayMgr->resetSpinner();
        wifiScanner->startScanBlocking();
        
        encoder.setBoundaries(0, max(0, wifiScanner->getNetworkCount() - 1), true);
        selectedNetwork = 0;
        scrollOffset = 0;
    }
    
    // Home button - back to menu
    static unsigned long lastHomePress = 0;
    if (checkButtonPress(BTN_SPECIAL, lastHomePress)) {
        currentMode = MODE_MENU;
        //play tick
    }
    
    // Draw
    if (wifiScanner->getIsScanning()) {
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
    
    // Encoder - scroll chat
    if (encoder.encoderChanged()) {
        int val = encoder.readEncoder();
        int delta = val - lastEncoderVal;
        lastEncoderVal = val;
        
        if (aiChatUI) {
            aiChatUI->scroll(-delta);
        }
    }
    
    // MIC button - start listening
    static unsigned long lastMicPress = 0;
    if (checkButtonPress(BTN_SHIFT, lastMicPress, 500)) {
        if (aiAssistant && !aiAssistant->getIsSpeaking()) {
            Serial.println("[AI] Starting voice input...");
            aiAssistant->startListening();
            
            if (aiChatUI) {
                aiChatUI->scrollToBottom();
            }
        }
    }
    
    // Home button - back to menu
    static unsigned long lastHomePress = 0;
    if (checkButtonPress(BTN_SPECIAL, lastHomePress)) {
        currentMode = MODE_MENU;
        //play tick
    }
    
    // Draw
    if (aiChatUI) {
        aiChatUI->draw();
    }
}

// ==================== Setup ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=================================");
    Serial.println("ESP32 Unified System Starting...");
    Serial.println("=================================\n");
    
    printMemoryUsage("Startup");
    
    // Initialize display
    Serial.println("[INIT] Display...");
    tft.init();
    tft.setRotation(DISPLAY_ROTATION);
    tft.fillScreen(COLOR_BG);
    
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    sprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
    sprite.setSwapBytes(true);
    sprite.setTextWrap(false);
    
    // Initialize filesystem
    Serial.println("[INIT] Filesystem...");
    if (!FFat.begin(true)) {
        Serial.println("[ERROR] FFat mount failed!");
    } else {
        Serial.println("[OK] FFat mounted");
        
        // Check for tick.wav
        if (FFat.exists("/tick.wav")) {
            Serial.println("[OK] tick.wav found");
        } else {
            Serial.println("[WARN] tick.wav not found - feedback will be silent");
        }
    }
    
    printMemoryUsage("After filesystem");
    
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
    
    // Initialize core components
    Serial.println("[INIT] Display Manager...");
    displayMgr = new DisplayManager(&tft);
    displayMgr->begin();
    displayMgr->showSplashScreen();
    delay(2000);
    
    Serial.println("[INIT] Credentials Manager...");
    credsMgr = new CredentialsManager();
    credsMgr->begin();
    
    
    
    printMemoryUsage("After core init");
    
    // Initialize WiFi
    Serial.println("[INIT] WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Try to auto-connect to saved network
    if (credsMgr->getCount() > 0) {
        SavedNetwork* recent = credsMgr->getMostRecent();
        if (recent) {
            Serial.printf("[WiFi] Auto-connecting to '%s'...\n", recent->ssid.c_str());
            WiFi.begin(recent->ssid.c_str(), recent->password.c_str());
            
            unsigned long startTime = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
                delay(100);
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WiFi] Auto-connected!");
                Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
            } else {
                Serial.println("[WiFi] Auto-connect failed");
            }
        }
    }
    
    // Initialize time
    Serial.println("[INIT] Time sync...");
    configTime(6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    
    printMemoryUsage("Setup complete");
    
    Serial.println("\n=================================");
    Serial.println("System Ready!");
    Serial.println("=================================\n");
    Serial.println("Controls:");
    Serial.println("  HOME button: Switch modes");
    Serial.println("  Encoder: Navigate");
    Serial.println("  Encoder button: Select");
    Serial.println("  MIC button: Voice input (AI mode)");
    Serial.println("  POWER button: Scan WiFi");
    Serial.println();
}

// ==================== Main Loop ====================
void loop() {
    
    // Global button check (throttled)
    if (millis() - lastButtonCheck > 50) {
        lastButtonCheck = millis();
        
        // Handle current mode
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
    
    delay(10); // Small delay to prevent watchdog issues
}