// DisplayManager.cpp
#include "DisplayManager.h"

DisplayManager* DisplayManager::instance = nullptr;

DisplayManager::DisplayManager() : mainSprite(nullptr), statusBarSprite(nullptr) {
    instance = this;
}

DisplayManager::~DisplayManager() {
    if (mainSprite) {
        mainSprite->deleteSprite();
        delete mainSprite;
    }
    if (statusBarSprite) {
        statusBarSprite->deleteSprite();
        delete statusBarSprite;
    }
}

bool DisplayManager::tftOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!instance || !instance->mainSprite) return false;
    if (y >= DISPLAY_HEIGHT) return false;
    
    instance->mainSprite->pushImage(x, y, w, h, bitmap);
    return true;
}

bool DisplayManager::begin() {
    // Initialize TFT
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(COLOR_BG_DARK);
    
    // Mount FFat filesystem
    if (!FFat.begin(true)) {
        Serial.println("FFat mount failed!");
        return false;
    }
    Serial.println("FFat mounted successfully");
    
    // Create main sprite
    mainSprite = new TFT_eSprite(&tft);
    mainSprite->setColorDepth(16);
    if (!mainSprite->createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT)) {
        Serial.println("Failed to create main sprite!");
        return false;
    }
    
    // Create status bar sprite
    statusBarSprite = new TFT_eSprite(&tft);
    statusBarSprite->setColorDepth(16);
    if (!statusBarSprite->createSprite(DISPLAY_WIDTH, 30)) {
        Serial.println("Failed to create status bar sprite!");
        return false;
    }
    
    // Initialize JPEG decoder
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tftOutputCallback);
    
    Serial.println("Display initialized successfully");
    return true;
}

void DisplayManager::pushSprite() {
    if (mainSprite) {
        mainSprite->pushSprite(0, 0);
    }
}

void DisplayManager::pushStatusBar() {
    if (statusBarSprite) {
        statusBarSprite->pushSprite(0, DISPLAY_HEIGHT - 30);
    }
}

void DisplayManager::drawJpegFromFile(const char* filename, int16_t x, int16_t y) {
    if (mainSprite) {
        TJpgDec.drawFsJpg(x, y, filename, FFat);
    }
}

bool DisplayManager::loadJpegToSprite(TFT_eSprite* sprite, const char* filename) {
    if (!sprite || !FFat.exists(filename)) {
        return false;
    }
    
    // Decode JPEG directly to sprite
    uint16_t w = 0, h = 0;
    
    // Read file into buffer
    File file = FFat.open(filename, "r");
    if (!file) return false;
    
    size_t fileSize = file.size();
    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    if (!buffer) {
        file.close();
        return false;
    }
    
    file.read(buffer, fileSize);
    file.close();
    
    // Get JPEG dimensions
    TJpgDec.getJpgSize(&w, &h, buffer, fileSize);
    
    // Decode
    bool result = (TJpgDec.drawJpg(0, 0, buffer, fileSize) == 0);
    
    free(buffer);
    return result;
}

void DisplayManager::drawRoundRect(TFT_eSprite* spr, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    if (!spr) return;
    
    spr->fillCircle(x + r, y + r, r, color);
    spr->fillCircle(x + w - r - 1, y + r, r, color);
    spr->fillCircle(x + r, y + h - r - 1, r, color);
    spr->fillCircle(x + w - r - 1, y + h - r - 1, r, color);
    spr->fillRect(x + r, y, w - 2 * r, h, color);
    spr->fillRect(x, y + r, w, h - 2 * r, color);
}

void DisplayManager::drawSignalStrength(TFT_eSprite* spr, int16_t x, int16_t y, int rssi) {
    if (!spr) return;
    
    int bars = 0;
    if (rssi > -55) bars = 4;
    else if (rssi > -65) bars = 3;
    else if (rssi > -75) bars = 2;
    else if (rssi > -85) bars = 1;
    
    uint16_t color = (rssi > -70) ? COLOR_SUCCESS : (rssi > -80) ? COLOR_WARNING : COLOR_ERROR;
    
    for (int i = 0; i < 4; i++) {
        int barHeight = 4 + (i * 3);
        uint16_t barColor = (i < bars) ? color : COLOR_TEXT_SECONDARY;
        spr->fillRect(x + (i * 4), y + (12 - barHeight), 3, barHeight, barColor);
    }
}

void DisplayManager::drawStatusIcon(TFT_eSprite* spr, int16_t x, int16_t y, bool connected, uint8_t alpha) {
    if (!spr) return;
    
    if (connected) {
        uint16_t color = COLOR_SUCCESS;
        spr->fillCircle(x + 6, y + 6, 6, color);
        spr->drawLine(x + 3, y + 6, x + 5, y + 8, COLOR_BG_DARK);
        spr->drawLine(x + 5, y + 8, x + 9, y + 4, COLOR_BG_DARK);
    } else {
        uint16_t color = COLOR_WARNING;
        spr->fillCircle(x + 6, y + 6, 6, color);
        spr->drawLine(x + 3, y + 3, x + 9, y + 9, COLOR_BG_DARK);
        spr->drawLine(x + 9, y + 3, x + 3, y + 9, COLOR_BG_DARK);
    }
}