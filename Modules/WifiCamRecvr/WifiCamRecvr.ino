#include <WiFi.h>
#include <TFT_eSPI.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include <TJpg_Decoder.h>
#include <time.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// WiFi credentials
const char* ssid = "Rakib";
const char* password = "rakib@2024";

// Camera server
const char* cameraHost = "192.168.0.122";
const uint16_t cameraPort = 8888;

// Display settings
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

// Modern color palette
#define COLOR_BG_DARK 0x0841
#define COLOR_ACCENT 0x07FF
#define COLOR_SUCCESS 0x07E0
#define COLOR_WARNING 0xFD20
#define COLOR_ERROR 0xF800
#define COLOR_TEXT_PRIMARY 0xFFFF
#define COLOR_TEXT_SECONDARY 0x8410
#define COLOR_CARD_BG 0x2124
#define COLOR_OVERLAY 0x18C3

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite statusBar = TFT_eSprite(&tft);
WiFiClient cameraClient;

// System mode enum
enum SystemMode {
    MODE_WATCH,
    MODE_MENU,
    MODE_CAMERA
};

SystemMode currentMode = MODE_WATCH;

// JPEG buffer for camera
uint8_t* jpegBuffer = nullptr;
const size_t MAX_JPEG_SIZE = 50000;

// Camera performance monitoring
unsigned long frameCount = 0;
unsigned long droppedFrames = 0;
float currentFPS = 0;
float avgFPS = 0;
unsigned long lastFPSUpdate = 0;
unsigned long lastFrameTime = 0;

// Camera connection
bool cameraConnected = false;
unsigned long lastCameraConnectionAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 2000;

// Animation variables
float fpsBarWidth = 0;
float targetFpsBarWidth = 0;
uint8_t pulseAlpha = 0;
bool pulseDirection = true;
unsigned long lastPulse = 0;
uint8_t connectionDots = 0;
unsigned long lastDotUpdate = 0;

// Time and weather
struct tm timeInfo;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 6 * 3600;
const int daylightOffset_sec = 0;

// Blinking colon for clock
bool colonVisible = true;
unsigned long lastColonBlink = 0;
const unsigned long COLON_BLINK_INTERVAL = 1000; // 1 second

// Weather API
const String apiKey = "dff65ece7cd56e5609a961090c1815a5";
const String city = "Dhaka,bd";
bool weatherEnabled = true;
unsigned long lastWeatherFetch = 0;
const unsigned long WEATHER_UPDATE_INTERVAL = 300000;

// Weather data
float temperature = 0;
float feelsLike = 0;
String weatherDescription = "";
int humidity = 0;
float windSpeed = 0;
bool weatherDataValid = false;

// Menu system
struct MenuItem {
    String label;
    SystemMode targetMode;
};

const int MENU_ITEMS_COUNT = 6;
MenuItem menuItems[MENU_ITEMS_COUNT] = {
    {"Watch", MODE_WATCH},
    {"Camera", MODE_CAMERA},
    {"Music", MODE_WATCH},
    {"Gallery", MODE_WATCH},
    {"Settings", MODE_WATCH},
    {"Games", MODE_WATCH}
};

int selectedMenuItem = 0;
unsigned long lastMenuActivity = 0;
const unsigned long MENU_TIMEOUT = 8000;

// TJpg_Decoder callback
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= DISPLAY_HEIGHT) return 0;
    sprite.pushImage(x, y, w, h, bitmap);
    return 1;
}

// Utility functions
void drawRoundRect(TFT_eSprite &spr, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    spr.fillCircle(x + r, y + r, r, color);
    spr.fillCircle(x + w - r - 1, y + r, r, color);
    spr.fillCircle(x + r, y + h - r - 1, r, color);
    spr.fillCircle(x + w - r - 1, y + h - r - 1, r, color);
    spr.fillRect(x + r, y, w - 2 * r, h, color);
    spr.fillRect(x, y + r, w, h - 2 * r, color);
}

void drawSignalStrength(TFT_eSprite &spr, int16_t x, int16_t y, int rssi) {
    int bars = 0;
    if (rssi > -55) bars = 4;
    else if (rssi > -65) bars = 3;
    else if (rssi > -75) bars = 2;
    else if (rssi > -85) bars = 1;
    
    uint16_t color = (rssi > -70) ? COLOR_SUCCESS : (rssi > -80) ? COLOR_WARNING : COLOR_ERROR;
    
    for (int i = 0; i < 4; i++) {
        int barHeight = 4 + (i * 3);
        uint16_t barColor = (i < bars) ? color : COLOR_TEXT_SECONDARY;
        spr.fillRect(x + (i * 4), y + (12 - barHeight), 3, barHeight, barColor);
    }
}

void drawStatusIcon(TFT_eSprite &spr, int16_t x, int16_t y, bool connected, uint8_t alpha) {
    if (connected) {
        uint16_t color = COLOR_SUCCESS;
        spr.fillCircle(x + 6, y + 6, 6, color);
        spr.drawLine(x + 3, y + 6, x + 5, y + 8, COLOR_BG_DARK);
        spr.drawLine(x + 5, y + 8, x + 9, y + 4, COLOR_BG_DARK);
    } else {
        uint16_t color = COLOR_WARNING;
        spr.fillCircle(x + 6, y + 6, 6, color);
        spr.drawLine(x + 3, y + 3, x + 9, y + 9, COLOR_BG_DARK);
        spr.drawLine(x + 9, y + 3, x + 3, y + 9, COLOR_BG_DARK);
    }
}

// Weather functions
void fetchWeather() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - skipping weather update");
        return;
    }
    
    Serial.println("Fetching weather data...");
    String endpoint = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&APPID=" + apiKey + "&units=metric";
    
    HTTPClient http;
    http.begin(endpoint);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    Serial.println("Weather HTTP response: " + String(httpCode));
    
    if (httpCode > 0) {
        String payload = http.getString();
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            temperature = doc["main"]["temp"];
            feelsLike = doc["main"]["feels_like"];
            weatherDescription = doc["weather"][0]["description"].as<String>();
            humidity = doc["main"]["humidity"];
            windSpeed = doc["wind"]["speed"];
            
            if (weatherDescription.length() > 0) {
                weatherDescription[0] = toupper(weatherDescription[0]);
            }
            
            weatherDataValid = true;
            Serial.println("Weather updated: " + String(temperature) + "°C - " + weatherDescription);
        } else {
            Serial.println("Weather JSON parse failed: " + String(error.c_str()));
            weatherDataValid = false;
        }
    } else {
        Serial.println("Weather HTTP request failed: " + String(httpCode));
        weatherDataValid = false;
    }
    
    http.end();
}

// Camera functions
bool connectToCamera() {
    if (cameraClient.connected()) return true;
    
    unsigned long now = millis();
    if (now - lastCameraConnectionAttempt < RECONNECT_INTERVAL) return false;
    
    lastCameraConnectionAttempt = now;
    Serial.println("Attempting to connect to camera at " + String(cameraHost) + ":" + String(cameraPort));
    
    cameraClient.stop();
    delay(100);
    
    if (cameraClient.connect(cameraHost, cameraPort)) {
        Serial.println("Successfully connected to camera");
        cameraConnected = true;
        
        // Send HTTP request for MJPEG stream
        cameraClient.print("GET / HTTP/1.1\r\n");
        cameraClient.print("Host: " + String(cameraHost) + "\r\n");
        cameraClient.print("Connection: keep-alive\r\n");
        cameraClient.print("\r\n");
        
        return true;
    } else {
        Serial.println("Failed to connect to camera");
        cameraConnected = false;
        return false;
    }
}

bool receiveJpegFrame() {
    if (!cameraClient.connected()) {
        Serial.println("Camera client not connected");
        return false;
    }
    
    // Look for boundary or Content-Length
    bool headerComplete = false;
    size_t contentLength = 0;
    unsigned long startTime = millis();
    const unsigned long HEADER_TIMEOUT = 3000;
    
    String line = "";
    while (!headerComplete && cameraClient.connected() && (millis() - startTime < HEADER_TIMEOUT)) {
        if (cameraClient.available()) {
            char c = cameraClient.read();
            if (c == '\n') {
                line.trim();
                
                if (line.startsWith("Content-Length:") || line.startsWith("Content-length:")) {
                    contentLength = line.substring(line.indexOf(':') + 1).toInt();
                    Serial.println("Found Content-Length: " + String(contentLength));
                }
                
                if (line.length() == 0) {
                    headerComplete = true;
                    break;
                }
                line = "";
            } else if (c != '\r') {
                line += c;
            }
        } else {
            delay(1);
        }
        
        yield(); // Allow other tasks
    }
    
    if (!headerComplete) {
        Serial.println("Header timeout or incomplete");
        return false;
    }
    
    if (contentLength == 0 || contentLength > MAX_JPEG_SIZE) {
        Serial.println("Invalid content length: " + String(contentLength));
        return false;
    }
    
    // Read JPEG data
    size_t totalReceived = 0;
    startTime = millis();
    const unsigned long DATA_TIMEOUT = 5000;
    
    while (totalReceived < contentLength && cameraClient.connected() && 
           (millis() - startTime < DATA_TIMEOUT)) {
        
        size_t available = cameraClient.available();
        if (available > 0) {
            size_t toRead = min(available, contentLength - totalReceived);
            size_t actualRead = cameraClient.read(jpegBuffer + totalReceived, toRead);
            
            if (actualRead > 0) {
                totalReceived += actualRead;
            }
        } else {
            delay(1);
        }
        
        if (totalReceived % 4096 == 0) {
            yield();
        }
    }
    
    bool success = (totalReceived == contentLength);
    
    if (success) {
        frameCount++;
        sprite.fillSprite(COLOR_BG_DARK);
        
        // Try to decode and display the JPEG
        if (TJpgDec.drawJpg(0, 0, jpegBuffer, totalReceived) == JDR_OK) {
            return true;
        } else {
            Serial.println("JPEG decode failed");
            droppedFrames++;
            return false;
        }
    } else {
        Serial.println("Frame receive failed. Got " + String(totalReceived) + " of " + String(contentLength) + " bytes");
        droppedFrames++;
        return false;
    }
}

void updateCameraStatusBar() {
    statusBar.fillSprite(0x1082);
    
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
    statusBar.setTextColor(COLOR_TEXT_SECONDARY);
    statusBar.drawString("FPS", 2, 5, 1);
    statusBar.setTextColor(fpsBarWidth > 0.7 ? COLOR_SUCCESS : fpsBarWidth > 0.4 ? COLOR_WARNING : COLOR_ERROR);
    statusBar.drawString(String(currentFPS, 0), 2, 15, 2);
    
    // CPU load
    statusBar.setTextColor(COLOR_TEXT_SECONDARY);
    statusBar.drawString("CPU", 42, 5, 1);
    int cpuLoad = 100 - (int)(fpsBarWidth * 100);
    statusBar.setTextColor(cpuLoad < 50 ? COLOR_SUCCESS : cpuLoad < 80 ? COLOR_WARNING : COLOR_ERROR);
    statusBar.drawString(String(cpuLoad) + "%", 42, 15, 1);
    
    // RAM
    int heapKB = ESP.getFreeHeap() / 1024;
    statusBar.setTextColor(COLOR_TEXT_SECONDARY);
    statusBar.drawString("RAM", 88, 5, 1);
    statusBar.setTextColor(heapKB > 100 ? COLOR_SUCCESS : heapKB > 50 ? COLOR_WARNING : COLOR_ERROR);
    statusBar.drawString(String(heapKB) + "K", 88, 15, 1);
    
    // PSRAM
    int psramKB = ESP.getFreePsram() / 1024;
    statusBar.setTextColor(COLOR_TEXT_SECONDARY);
    statusBar.drawString("PSRAM", 145, 5, 1);
    statusBar.setTextColor(psramKB > 1000 ? COLOR_SUCCESS : psramKB > 500 ? COLOR_WARNING : COLOR_ERROR);
    
    String psramStr;
    if (psramKB > 1024) {
        psramStr = String(psramKB / 1024) + "M";
    } else {
        psramStr = String(psramKB) + "K";
    }
    statusBar.drawString(psramStr, 145, 15, 1);
    
    // Network
    int rssi = WiFi.RSSI();
    statusBar.setTextColor(COLOR_TEXT_SECONDARY);
    statusBar.drawString("NET", 205, 5, 1);
    drawSignalStrength(statusBar, 205, 15, rssi);
    
    // Connection
    statusBar.setTextColor(COLOR_TEXT_SECONDARY);
    statusBar.drawString("LINK", 255, 5, 1);
    drawStatusIcon(statusBar, 258, 15, cameraConnected, pulseAlpha);
    
    // Quality
    float quality = 100.0;
    if (frameCount + droppedFrames > 0) {
        quality = 100.0 - ((float)droppedFrames / (frameCount + droppedFrames) * 100.0);
    }
    statusBar.setTextColor(COLOR_TEXT_SECONDARY);
    statusBar.drawString("Q", 295, 5, 1);
    uint16_t qualityColor = quality > 95 ? COLOR_SUCCESS : quality > 85 ? COLOR_WARNING : COLOR_ERROR;
    statusBar.setTextColor(qualityColor);
    statusBar.drawString(String((int)quality), 295, 15, 1);
}

void showCameraDisconnectedScreen() {
    sprite.fillSprite(COLOR_BG_DARK);
    
    unsigned long now = millis();
    if (now - lastDotUpdate > 500) {
        connectionDots = (connectionDots + 1) % 4;
        lastDotUpdate = now;
    }
    
    int cardX = 40;
    int cardY = 50;
    int cardW = 240;
    int cardH = 120;
    
    drawRoundRect(sprite, cardX, cardY, cardW, cardH, 8, COLOR_CARD_BG);
    
    sprite.fillCircle(DISPLAY_WIDTH/2, cardY + 30, 15, COLOR_WARNING);
    sprite.drawLine(DISPLAY_WIDTH/2 - 8, cardY + 22, DISPLAY_WIDTH/2 + 8, cardY + 38, COLOR_BG_DARK);
    sprite.drawLine(DISPLAY_WIDTH/2 + 8, cardY + 22, DISPLAY_WIDTH/2 - 8, cardY + 38, COLOR_BG_DARK);
    
    sprite.setTextColor(COLOR_TEXT_PRIMARY);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString("Camera Disconnected", DISPLAY_WIDTH/2, cardY + 55, 2);
    
    String dotsText = "Reconnecting";
    for (int i = 0; i < connectionDots; i++) {
        dotsText += ".";
    }
    sprite.setTextColor(COLOR_TEXT_SECONDARY);
    sprite.drawString(dotsText, DISPLAY_WIDTH/2, cardY + 80, 2);
    
    sprite.setTextColor(COLOR_TEXT_SECONDARY);
    sprite.drawString("Press 'watch' to exit", DISPLAY_WIDTH/2, cardY + 105, 1);
    
    updateCameraStatusBar();
    sprite.pushSprite(0, 0);
    statusBar.pushSprite(0, DISPLAY_HEIGHT - 30);
}

// Watch face with blinking colon
void drawWatchFace() {
    sprite.fillSprite(COLOR_BG_DARK);
    
    // Update blinking colon
    if (millis() - lastColonBlink >= COLON_BLINK_INTERVAL) {
        colonVisible = !colonVisible;
        lastColonBlink = millis();
    }
    
    // Gradient background
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        float ratio = (float)y / DISPLAY_HEIGHT;
        uint16_t gradientColor = sprite.color565(
            (int)(8 * (1 - ratio) + 40 * ratio),
            (int)(33 * (1 - ratio) + 60 * ratio),
            (int)(65 * (1 - ratio) + 100 * ratio)
        );
        sprite.drawFastHLine(0, y, DISPLAY_WIDTH, gradientColor);
    }
    
    if (!getLocalTime(&timeInfo)) {
        sprite.setTextDatum(MC_DATUM);
        sprite.setTextColor(COLOR_ERROR);
        sprite.drawString("Time sync failed", DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 2);
        sprite.pushSprite(0, 0);
        return;
    }
    
    // Time display with blinking colon
    char timeStr[10];
    if (colonVisible) {
        sprintf(timeStr, "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
    } else {
        sprintf(timeStr, "%02d %02d", timeInfo.tm_hour, timeInfo.tm_min);
    }
    
    int timeY = DISPLAY_HEIGHT/2 - 20;
    int panelWidth = 200;
    int panelHeight = 60;
    int panelX = DISPLAY_WIDTH/2 - panelWidth/2;
    
    // Panel gradient background
    for (int i = 0; i < panelHeight; i++) {
        float ratio = (float)i / panelHeight;
        uint16_t gradientColor = sprite.color565(
            (int)(20 * (1 - ratio) + 60 * ratio),
            (int)(30 * (1 - ratio) + 80 * ratio),
            (int)(60 * (1 - ratio) + 120 * ratio)
        );
        sprite.drawFastHLine(panelX, timeY - panelHeight/2 + i, panelWidth, gradientColor);
    }
    
    // Panel glow effect
    for (int glow = 0; glow < 3; glow++) {
        uint16_t glowColor = sprite.color565(0, 150 - glow * 30, 255 - glow * 50);
        sprite.drawRoundRect(panelX - glow, timeY - panelHeight/2 - glow, 
                           panelWidth + 2*glow, panelHeight + 2*glow, 15 + glow, glowColor);
    }
    
    // Time text with shadow
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextColor(TFT_BLACK);
    sprite.setTextSize(6);
    sprite.drawString(timeStr, DISPLAY_WIDTH/2 + 2, timeY + 2);
    
    sprite.setTextColor(TFT_CYAN);
    sprite.drawString(timeStr, DISPLAY_WIDTH/2, timeY);
    
    // Date
    char dateStr[20];
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", 
                           "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    sprintf(dateStr, "%02d %s %04d", timeInfo.tm_mday, months[timeInfo.tm_mon], timeInfo.tm_year + 1900);
    
    int dateY = timeY + 50;
    sprite.setTextColor(TFT_WHITE);
    sprite.setTextSize(2);
    sprite.drawString(dateStr, DISPLAY_WIDTH/2, dateY);
    
    // Weather
    if (weatherEnabled && weatherDataValid) {
        int weatherY = dateY + 35;
        sprite.setTextColor(COLOR_ACCENT);
        sprite.setTextSize(3);
        sprite.drawString(String((int)temperature) + "C", DISPLAY_WIDTH/2, weatherY);
        
        sprite.setTextColor(COLOR_TEXT_SECONDARY);
        sprite.setTextSize(2);
        sprite.drawString(weatherDescription, DISPLAY_WIDTH/2, weatherY + 25);
    }
    
    // WiFi indicator
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        drawSignalStrength(sprite, 280, 10, rssi);
    } else {
        sprite.setTextDatum(TR_DATUM);
        sprite.setTextColor(COLOR_ERROR);
        sprite.drawString("No WiFi", 315, 10, 1);
        sprite.setTextDatum(MC_DATUM);  // Reset datum
    }
    
    sprite.pushSprite(0, 0);
}

// Simple menu
void drawMenu() {
    sprite.fillSprite(COLOR_BG_DARK);
    
    sprite.setTextDatum(MC_DATUM);
    sprite.setTextColor(COLOR_ACCENT);
    sprite.setTextSize(2);
    sprite.drawString("MENU", DISPLAY_WIDTH/2, 30);
    
    int itemY = 70;
    for (int i = 0; i < MENU_ITEMS_COUNT; i++) {
        if (i == selectedMenuItem) {
            drawRoundRect(sprite, 60, itemY - 15, 200, 30, 5, COLOR_ACCENT);
            sprite.setTextColor(COLOR_BG_DARK);
        } else {
            sprite.setTextColor(COLOR_TEXT_PRIMARY);
        }
        
        sprite.setTextSize(1);
        sprite.drawString(menuItems[i].label, DISPLAY_WIDTH/2, itemY);
        itemY += 35;
        
        if (itemY > DISPLAY_HEIGHT - 20) break;
    }
    
    sprite.setTextColor(COLOR_TEXT_SECONDARY);
    sprite.setTextSize(1);
    sprite.drawString("Use: next/prev/select", DISPLAY_WIDTH/2, DISPLAY_HEIGHT - 10);
    
    sprite.pushSprite(0, 0);
}

// Command handler
void handleSerialCommands() {
    if (Serial.available() > 0) {
        String command = Serial.readString();
        command.trim();
        command.toLowerCase();
        
        if (command == "help") {
            Serial.println("\n=== Commands ===");
            Serial.println("watch    - Watch face");
            Serial.println("menu     - Menu");
            Serial.println("camera   - Camera view");
            Serial.println("next     - Next menu item");
            Serial.println("prev     - Previous menu item");
            Serial.println("select   - Select menu item");
            Serial.println("weather  - Update weather");
            Serial.println("status   - Show system status");
            Serial.println("===============\n");
            
        } else if (command == "watch") {
            currentMode = MODE_WATCH;
            Serial.println("Switched to watch mode");
            
        } else if (command == "menu") {
            currentMode = MODE_MENU;
            lastMenuActivity = millis();
            Serial.println("Switched to menu mode");
            
        } else if (command == "camera") {
            currentMode = MODE_CAMERA;
            Serial.println("Switched to camera mode");
            
        } else if (command == "next") {
            if (currentMode == MODE_MENU) {
                selectedMenuItem = (selectedMenuItem + 1) % MENU_ITEMS_COUNT;
                lastMenuActivity = millis();
                Serial.println("Selected: " + menuItems[selectedMenuItem].label);
            }
            
        } else if (command == "prev") {
            if (currentMode == MODE_MENU) {
                selectedMenuItem--;
                if (selectedMenuItem < 0) selectedMenuItem = MENU_ITEMS_COUNT - 1;
                lastMenuActivity = millis();
                Serial.println("Selected: " + menuItems[selectedMenuItem].label);
            }
            
        } else if (command == "select") {
            if (currentMode == MODE_MENU) {
                currentMode = menuItems[selectedMenuItem].targetMode;
                Serial.println("Activated: " + menuItems[selectedMenuItem].label);
            }
            
        } else if (command == "weather") {
            fetchWeather();
            
        } else if (command == "status") {
            Serial.println("\n=== System Status ===");
            Serial.println("Mode: " + String(currentMode == MODE_WATCH ? "Watch" : currentMode == MODE_MENU ? "Menu" : "Camera"));
            Serial.println("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected"));
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
            }
            Serial.println("Free Heap: " + String(ESP.getFreeHeap() / 1024) + " KB");
            Serial.println("Free PSRAM: " + String(ESP.getFreePsram() / 1024) + " KB");
            if (weatherDataValid) {
                Serial.println("Weather: " + String(temperature) + "°C - " + weatherDescription);
            }
            Serial.println("Camera: " + String(cameraConnected ? "Connected" : "Disconnected"));
            if (currentMode == MODE_CAMERA) {
                Serial.println("FPS: " + String(currentFPS, 1));
            }
            Serial.println("==================\n");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=================================");
    Serial.println("SignalMaster with Camera v2.1");
    Serial.println("=================================");
    
    // Initialize display
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(COLOR_BG_DARK);
    
    // Create sprites
    if (!sprite.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT)) {
        Serial.println("Failed to create main sprite!");
        while(1) delay(1000);
    }
    
    if (!statusBar.createSprite(DISPLAY_WIDTH, 30)) {
        Serial.println("Failed to create status bar!");
        while(1) delay(1000);
    }
    
    Serial.println("Display initialized successfully");
    
    // Initialize JPEG decoder
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);
    Serial.println("JPEG decoder initialized");
    
    // Allocate JPEG buffer
    if (ESP.getFreePsram() > MAX_JPEG_SIZE + 100000) {
        jpegBuffer = (uint8_t*)ps_malloc(MAX_JPEG_SIZE);
        Serial.println("Using PSRAM for JPEG buffer (" + String(MAX_JPEG_SIZE) + " bytes)");
    } else {
        jpegBuffer = (uint8_t*)heap_caps_malloc(MAX_JPEG_SIZE, MALLOC_CAP_8BIT);
        Serial.println("Using heap for JPEG buffer (" + String(MAX_JPEG_SIZE) + " bytes)");
    }
    
    if (!jpegBuffer) {
        Serial.println("CRITICAL: JPEG buffer allocation failed!");
        while(1) delay(1000);
    }
    
    // Connect WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    Serial.print("Connecting to WiFi");
    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
        delay(500);
        Serial.print(".");
        wifiAttempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected successfully!");
        Serial.println("IP Address: " + WiFi.localIP().toString());
        Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
        
        // Initialize time
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        Serial.println("NTP time sync initiated");
        
        // Wait a moment for time to sync
        delay(2000);
        
        // Fetch initial weather data
        fetchWeather();
        lastWeatherFetch = millis();
    } else {
        Serial.println("\nWiFi connection failed!");
        Serial.println("System will continue without internet features");
    }
    
    // Initialize camera client settings
    cameraClient.setNoDelay(true);
    cameraClient.setTimeout(10000);
    
    // Initialize timing variables
    lastFrameTime = millis();
    lastFPSUpdate = millis();
    lastPulse = millis();
    lastColonBlink = millis();
    
    Serial.println("\n=== System Ready ===");
    Serial.println("Available commands:");
    Serial.println("- help: Show command list");
    Serial.println("- watch: Switch to watch face");
    Serial.println("- camera: Switch to camera view");
    Serial.println("- menu: Open menu system");
    Serial.println("- weather: Update weather data");
    Serial.println("- status: Show system status");
    Serial.println("====================\n");
}

void loop() {
    handleSerialCommands();
    
    // Update weather periodically (only if WiFi connected)
    if (WiFi.status() == WL_CONNECTED && weatherEnabled && 
        (millis() - lastWeatherFetch > WEATHER_UPDATE_INTERVAL)) {
        fetchWeather();
        lastWeatherFetch = millis();
    }
    
    // Handle current mode
    switch (currentMode) {
        case MODE_WATCH:
            drawWatchFace();
            delay(50);  // Reduced delay for smoother colon blinking
            break;
            
        case MODE_MENU:
            drawMenu();
            if (millis() - lastMenuActivity > MENU_TIMEOUT) {
                currentMode = MODE_WATCH;
                Serial.println("Menu timeout - returning to watch face");
            }
            delay(100);
            break;
            
        case MODE_CAMERA:
            if (connectToCamera()) {
                if (receiveJpegFrame()) {
                    sprite.pushSprite(0, 0);
                    
                    // Update status bar periodically
                    static unsigned long lastStatusUpdate = 0;
                    if (millis() - lastStatusUpdate > 100) {
                        updateCameraStatusBar();
                        lastStatusUpdate = millis();
                    }
                    
                    statusBar.pushSprite(0, DISPLAY_HEIGHT - 30);
                } else {
                    // Frame receive failed, disconnect and retry
                    Serial.println("Frame receive failed, disconnecting...");
                    cameraClient.stop();
                    cameraConnected = false;
                    delay(100);
                }
            } else {
                // Show disconnected screen
                static unsigned long lastScreenUpdate = 0;
                if (millis() - lastScreenUpdate > 100) {
                    showCameraDisconnectedScreen();
                    lastScreenUpdate = millis();
                }
                delay(100);
            }
            break;
    }
    
    yield(); // Allow other tasks to run
}