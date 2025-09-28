// CameraManager.h
#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <WiFi.h>
#include <TJpg_Decoder.h>
#include "Config.h"
#include "DisplayManager.h"

class CameraManager {
public:
    WiFiClient client;
    DisplayManager* display;
    uint8_t* jpegBuffer;
    
    unsigned long frameCount;
    unsigned long droppedFrames;
    float currentFPS;
    float avgFPS;
    unsigned long lastFPSUpdate;
    unsigned long lastFrameTime;
    
    bool connectionStatus;  // Renamed from isConnected to avoid conflict
    unsigned long lastConnectionAttempt;
    
    uint8_t pulseAlpha;
    bool pulseDirection;
    unsigned long lastPulse;
    uint8_t connectionDots;
    unsigned long lastDotUpdate;
    float fpsBarWidth;
    float targetFpsBarWidth;
    
    unsigned long lastStatusUpdate;
    
    bool connectToServer();
    bool receiveFrame();
    void updateStatusBar();
    void showDisconnectedScreen();
    CameraManager();
    ~CameraManager();
    
    bool begin();
    void update();
    bool isConnected() const { return connectionStatus; }  // Now returns the renamed member
    float getFPS() const { return currentFPS; }
};

#endif