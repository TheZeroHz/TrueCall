// CameraManager.cpp
#include "CameraManager.h"

CameraManager::CameraManager() 
    : display(nullptr), jpegBuffer(nullptr), frameCount(0), droppedFrames(0),
      currentFPS(0), avgFPS(0), lastFPSUpdate(0), lastFrameTime(0),
      connectionStatus(false), lastConnectionAttempt(0), pulseAlpha(0),
      pulseDirection(true), lastPulse(0), connectionDots(0), lastDotUpdate(0),
      fpsBarWidth(0), targetFpsBarWidth(0), lastStatusUpdate(0) {}

CameraManager::~CameraManager() {
    if (jpegBuffer) {
        free(jpegBuffer);
    }
}

bool CameraManager::begin() {
    // Allocate JPEG buffer
    if (ESP.getFreePsram() > MAX_JPEG_SIZE + 100000) {
        jpegBuffer = (uint8_t*)ps_malloc(MAX_JPEG_SIZE);
        Serial.println("Camera: Using PSRAM for buffer");
    } else {
        jpegBuffer = (uint8_t*)heap_caps_malloc(MAX_JPEG_SIZE, MALLOC_CAP_8BIT);
        Serial.println("Camera: Using heap for buffer");
    }
    
    if (!jpegBuffer) {
        Serial.println("Camera: Buffer allocation failed!");
        return false;
    }
    
    client.setNoDelay(true);
    client.setTimeout(10000);
    
    lastFrameTime = millis();
    lastFPSUpdate = millis();
    lastPulse = millis();
    
    Serial.println("Camera initialized");
    return true;
}

bool CameraManager::connectToServer() {
    if (client.connected()) return true;
    
    unsigned long now = millis();
    if (now - lastConnectionAttempt < CAMERA_RECONNECT_INTERVAL) return false;
    
    lastConnectionAttempt = now;
    
    client.stop();
    delay(100);
    
    if (client.connect(CAMERA_HOST, CAMERA_PORT)) {
        connectionStatus = true;
        return true;
    }
    
    connectionStatus = false;
    return false;
}

bool CameraManager::receiveFrame() {
    if (!client.connected()) return false;
    
    bool headerComplete = false;
    size_t contentLength = 0;
    unsigned long startTime = millis();
    
    while (!headerComplete && client.connected() && (millis() - startTime < 2000)) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            line.trim();
            
            if (line.startsWith("Content-Length:")) {
                contentLength = line.substring(15).toInt();
            }
            
            if (line.length() == 0) {
                headerComplete = true;
            }
        } else {
            delay(1);
        }
    }
    
    if (!headerComplete || contentLength == 0 || contentLength > MAX_JPEG_SIZE) {
        return false;
    }
    
    size_t totalReceived = 0;
    startTime = millis();
    
    while (totalReceived < contentLength && client.connected() && 
           (millis() - startTime < 5000)) {
        
        size_t available = client.available();
        if (available > 0) {
            size_t toRead = min(available, contentLength - totalReceived);
            size_t actualRead = client.read(jpegBuffer + totalReceived, toRead);
            
            if (actualRead > 0) {
                totalReceived += actualRead;
            }
        } else {
            delay(1);
        }
        
        if (totalReceived % 8192 == 0) {
            yield();
        }
    }
    
    if (totalReceived != contentLength) {
        droppedFrames++;
        return false;
    }
    
    frameCount++;
    
    TFT_eSprite* sprite = display->getMainSprite();
    sprite->fillSprite(COLOR_BG_DARK);
    
    if (TJpgDec.drawJpg(0, 0, jpegBuffer, totalReceived) == JDR_OK) {
        return true;
    } else {
        droppedFrames++;
        return false;
    }
}

void CameraManager::updateStatusBar() {
    TFT_eSprite* statusBar = display->getStatusBarSprite();
    statusBar->fillSprite(0x1082);
    
    unsigned long now = millis();
    if (now - lastFPSUpdate >= 1000) {
        currentFPS = frameCount * 1000.0 / (now - lastFrameTime);
        avgFPS = (avgFPS * 0.8) + (currentFPS * 0.2);
        lastFPSUpdate = now;
        lastFrameTime = now;
        frameCount = 0;
        targetFpsBarWidth = min(1.0f, currentFPS / 30.0f);
    }
    
    fpsBarWidth += (targetFpsBarWidth - fpsBarWidth) * 0.2;
    
    if (now - lastPulse > 30) {
        if (pulseDirection) {
            pulseAlpha += 10;
            if (pulseAlpha >= 200) pulseDirection = false;
        } else {
            pulseAlpha -= 10;
            if (pulseAlpha <= 50) pulseDirection = true;
        }
        lastPulse = now;
    }
    
    // FPS
    statusBar->setTextColor(COLOR_TEXT_SECONDARY);
    statusBar->drawString("FPS", 2, 5, 1);
    statusBar->setTextColor(fpsBarWidth > 0.7 ? COLOR_SUCCESS : 
                            fpsBarWidth > 0.4 ? COLOR_WARNING : COLOR_ERROR);
    statusBar->drawString(String(currentFPS, 0), 2, 15, 2);
    
    // CPU
    statusBar->setTextColor(COLOR_TEXT_SECONDARY);
    statusBar->drawString("CPU", 42, 5, 1);
    int cpuLoad = 100 - (int)(fpsBarWidth * 100);
    statusBar->setTextColor(cpuLoad < 50 ? COLOR_SUCCESS : 
                            cpuLoad < 80 ? COLOR_WARNING : COLOR_ERROR);
    statusBar->drawString(String(cpuLoad) + "%", 42, 15, 1);
    
    // RAM
    int heapKB = ESP.getFreeHeap() / 1024;
    statusBar->setTextColor(COLOR_TEXT_SECONDARY);
    statusBar->drawString("RAM", 88, 5, 1);
    statusBar->setTextColor(heapKB > 100 ? COLOR_SUCCESS : 
                            heapKB > 50 ? COLOR_WARNING : COLOR_ERROR);
    statusBar->drawString(String(heapKB) + "K", 88, 15, 1);
    
    // PSRAM
    int psramKB = ESP.getFreePsram() / 1024;
    statusBar->setTextColor(COLOR_TEXT_SECONDARY);
    statusBar->drawString("PSRAM", 145, 5, 1);
    statusBar->setTextColor(psramKB > 1000 ? COLOR_SUCCESS : 
                            psramKB > 500 ? COLOR_WARNING : COLOR_ERROR);
    
    String psramStr = (psramKB > 1024) ? String(psramKB / 1024) + "M" : String(psramKB) + "K";
    statusBar->drawString(psramStr, 145, 15, 1);
    
    // Network
    int rssi = WiFi.RSSI();
    statusBar->setTextColor(COLOR_TEXT_SECONDARY);
    statusBar->drawString("NET", 205, 5, 1);
    display->drawSignalStrength(statusBar, 205, 15, rssi);
    
    // Link status
    statusBar->setTextColor(COLOR_TEXT_SECONDARY);
    statusBar->drawString("LINK", 255, 5, 1);
    display->drawStatusIcon(statusBar, 258, 15, connectionStatus, pulseAlpha);
    
    // Quality
    float quality = 100.0;
    if (frameCount + droppedFrames > 0) {
        quality = 100.0 - ((float)droppedFrames / (frameCount + droppedFrames) * 100.0);
    }
    statusBar->setTextColor(COLOR_TEXT_SECONDARY);
    statusBar->drawString("Q", 295, 5, 1);
    uint16_t qualityColor = quality > 95 ? COLOR_SUCCESS : 
                            quality > 85 ? COLOR_WARNING : COLOR_ERROR;
    statusBar->setTextColor(qualityColor);
    statusBar->drawString(String((int)quality), 295, 15, 1);
}

void CameraManager::showDisconnectedScreen() {
    TFT_eSprite* sprite = display->getMainSprite();
    sprite->fillSprite(COLOR_BG_DARK);
    
    unsigned long now = millis();
    if (now - lastDotUpdate > 500) {
        connectionDots = (connectionDots + 1) % 4;
        lastDotUpdate = now;
    }
    
    int cardX = 40;
    int cardY = 50;
    int cardW = 240;
    int cardH = 120;
    
    display->drawRoundRect(sprite, cardX, cardY, cardW, cardH, 8, COLOR_CARD_BG);
    
    sprite->fillCircle(DISPLAY_WIDTH/2, cardY + 30, 15, COLOR_WARNING);
    sprite->drawLine(DISPLAY_WIDTH/2 - 8, cardY + 22, DISPLAY_WIDTH/2 + 8, cardY + 38, COLOR_BG_DARK);
    sprite->drawLine(DISPLAY_WIDTH/2 + 8, cardY + 22, DISPLAY_WIDTH/2 - 8, cardY + 38, COLOR_BG_DARK);
    
    sprite->setTextColor(COLOR_TEXT_PRIMARY);
    sprite->setTextDatum(MC_DATUM);
    sprite->drawString("Camera Disconnected", DISPLAY_WIDTH/2, cardY + 55, 2);
    
    String dotsText = "Reconnecting";
    for (int i = 0; i < connectionDots; i++) {
        dotsText += ".";
    }
    sprite->setTextColor(COLOR_TEXT_SECONDARY);
    sprite->drawString(dotsText, DISPLAY_WIDTH/2, cardY + 80, 2);
    
    sprite->setTextColor(COLOR_TEXT_SECONDARY);
    sprite->drawString("Press 'watch' to exit", DISPLAY_WIDTH/2, cardY + 105, 1);
    
    updateStatusBar();
    display->pushSprite();
    display->pushStatusBar();
}

void CameraManager::update() {
    if (!display) return;
    
    if (connectToServer()) {
        if (receiveFrame()) {
            display->pushSprite();
            
            if (millis() - lastStatusUpdate > 100) {
                updateStatusBar();
                lastStatusUpdate = millis();
            }
            
            display->pushStatusBar();
        } else {
            client.stop();
            connectionStatus = false;
        }
    } else {
        static unsigned long lastScreenUpdate = 0;
        if (millis() - lastScreenUpdate > 100) {
            showDisconnectedScreen();
            lastScreenUpdate = millis();
        }
    }
}