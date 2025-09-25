#include <Arduino.h>
#include <math.h>
#include <FFat.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "TFT_GUI.h"

#ifndef TFT_BL
#define TFT_BL 8
#endif

// Display and GUI objects
TFT_eSPI tft = TFT_eSPI();
TFT_GUI gui(tft, 320, 240, 30, 250);
TFT_eSprite background = TFT_eSprite(&tft);

// WiFi credentials
const char* ssid = "Rakib";  // Change to your WiFi
const char* password = "rakib@2024";          // Change to your WiFi password

// Weather API
const String apiKey = "dff65ece7cd56e5609a961090c1815a5";
const String city = "Dhaka,bd";

// Control variables for serial commands
bool autoMode = false;
bool menuActive = false;
bool weatherEnabled = false;
bool demoMode = false;
unsigned long demoModeTimer = 0;
const unsigned long DEMO_INTERVAL = 3000; // 3 seconds between actions in demo mode

// Weather Client Class (Integrated)
class WeatherClient {
private:
    String apiKey;
    String city;
    String endpoint;
    String payload;
    float temperature = 0;
    float feelsLike = 0;
    String weatherDescription = "";
    String weatherIcon = "";
    int humidity = 0;
    float windSpeed = 0;
    int cloudiness = 0;
    String country = "";
    String cityName = "";
    unsigned long lastUpdateTime = 0;
    bool validData = false;

    void parseWeatherData(const String &jsonPayload) {
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, jsonPayload);

        if (!error) {
            temperature = doc["main"]["temp"];
            feelsLike = doc["main"]["feels_like"];
            weatherDescription = doc["weather"][0]["description"].as<String>();
            weatherIcon = doc["weather"][0]["icon"].as<String>();
            humidity = doc["main"]["humidity"];
            windSpeed = doc["wind"]["speed"];
            cloudiness = doc["clouds"]["all"];
            country = doc["sys"]["country"].as<String>();
            cityName = doc["name"].as<String>();
            
            if (weatherDescription.length() > 0) {
                weatherDescription[0] = toupper(weatherDescription[0]);
            }
        } else {
            Serial.println("Error parsing weather JSON");
            validData = false;
        }
    }

public:
    WeatherClient(const String &apiKey, const String &city)
        : apiKey(apiKey), city(city), lastUpdateTime(0), validData(false) {
        endpoint = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&APPID=" + apiKey + "&units=metric";
    }

    void fetchWeather() {
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(endpoint);
            http.setTimeout(5000);
            
            int httpCode = http.GET();
            
            if (httpCode > 0) {
                payload = http.getString();
                parseWeatherData(payload);
                lastUpdateTime = millis();
                validData = true;
                Serial.println("Weather data updated successfully");
            } else {
                Serial.println("Error on HTTP request: " + String(httpCode));
                validData = false;
            }
            
            http.end();
        } else {
            Serial.println("WiFi not connected");
            validData = false;
        }
    }

    float getTemperature() { return temperature; }
    float getFeelsLike() { return feelsLike; }
    String getWeatherDescription() { return weatherDescription; }
    String getWeatherIcon() { return weatherIcon; }
    int getHumidity() { return humidity; }
    float getWindSpeed() { return windSpeed; }
    String getFormattedTemperature() { return String((int)temperature) + "°C"; }
    String getFormattedFeelsLike() { return "Feels " + String((int)feelsLike) + "°C"; }
    String getFormattedWindSpeed() { return String(windSpeed, 1) + " m/s"; }
    String getFormattedHumidity() { return String(humidity) + "%"; }
    bool isDataValid() { return validData && (millis() - lastUpdateTime < 300000); }

    uint16_t getWeatherColor() {
        if (temperature < 0) return TFT_BLUE;
        else if (temperature < 10) return TFT_CYAN;
        else if (temperature < 20) return TFT_GREEN;
        else if (temperature < 30) return TFT_YELLOW;
        else if (temperature < 35) return TFT_ORANGE;
        else return TFT_RED;
    }

    void drawWeatherIcon(TFT_eSprite& sprite, int x, int y, int size) {
        if (weatherIcon.startsWith("01")) {
            // Sun
            sprite.fillCircle(x + size/2, y + size/2, size/3, TFT_YELLOW);
            for (int i = 0; i < 8; i++) {
                float angle = i * PI / 4;
                int x1 = x + size/2 + cos(angle) * (size/2 - 5);
                int y1 = y + size/2 + sin(angle) * (size/2 - 5);
                int x2 = x + size/2 + cos(angle) * (size/2);
                int y2 = y + size/2 + sin(angle) * (size/2);
                sprite.drawLine(x1, y1, x2, y2, TFT_YELLOW);
            }
        }
        else if (weatherIcon.startsWith("02") || weatherIcon.startsWith("03")) {
            // Partly cloudy
            sprite.fillCircle(x + size/3, y + size/3, size/4, TFT_YELLOW);
            sprite.fillCircle(x + size/2, y + size/2, size/3, TFT_LIGHTGREY);
            sprite.drawCircle(x + size/2, y + size/2, size/3, TFT_DARKGREY);
        }
        else if (weatherIcon.startsWith("10") || weatherIcon.startsWith("09")) {
            // Rain
            sprite.fillCircle(x + size/2, y + size/3, size/3, TFT_DARKGREY);
            for (int i = 0; i < 5; i++) {
                int rainX = x + size/4 + (i * size/8);
                sprite.drawLine(rainX, y + 2*size/3, rainX, y + size - 5, TFT_CYAN);
            }
        }
        else {
            // Default cloud
            sprite.fillCircle(x + size/2, y + size/2, size/3, TFT_LIGHTGREY);
            sprite.drawCircle(x + size/2, y + size/2, size/3, TFT_DARKGREY);
        }
    }
};

// Menu system variables
struct MenuItem {
    String label;
    String avatarPath;
    float angle;
    float targetAngle;
    bool isSelected;
};

const int MENU_ITEMS_COUNT = 6;
MenuItem menuItems[MENU_ITEMS_COUNT] = {
    {"Home", "/ps.jpg", 0, 0, false},
    {"Weather", "/ps.jpg", PI/3, PI/3, false},
    {"Music", "/ps.jpg", 2*PI/3, 2*PI/3, false},
    {"Gallery", "/ps.jpg", PI, PI, false},
    {"Settings", "/ps.jpg", 4*PI/3, 4*PI/3, false},
    {"Games", "/ps.jpg", 5*PI/3, 5*PI/3, false}
};

// Global variables
int selectedMenuItem = 0;
const int MENU_RADIUS = 85;
const int AVATAR_SIZE = 45;
const int CENTER_X = 160;
const int CENTER_Y = 120;
bool isRotating = false;
float menuRotationOffset = 0;
unsigned long lastMenuActivity = 0;
unsigned long animationStart = 0;
const unsigned long MENU_TIMEOUT = 8000; // 8 seconds
const int ANIMATION_DURATION = 400;

// Time and weather
struct tm timeInfo;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 6 * 3600; // GMT+6 for Dhaka
const int daylightOffset_sec = 0;
WeatherClient* weatherClient = nullptr;
unsigned long lastWeatherFetch = 0;
const unsigned long WEATHER_UPDATE_INTERVAL = 300000; // 5 minutes

// Utility Functions
void blitCircleSpriteToSprite(TFT_eSprite& src, TFT_eSprite& dst, int dstX, int dstY) {
    const int W = src.width();
    const int H = src.height();
    if (W <= 0 || H <= 0) return;

    const int cx = W / 2;
    const int cy = H / 2;
    const int r = (cx < cy ? cx : cy);
    const int r2 = r * r;

    const uint16_t* srcPtr = (const uint16_t*)src.getPointer();
    if (!srcPtr) return;

    bool prevSwapDst = dst.getSwapBytes();
    dst.setSwapBytes(false);

    const int DW = dst.width();
    const int DH = dst.height();

    for (int y = 0; y < H; ++y) {
        int dy = y - cy;
        int inside = r2 - dy * dy;
        if (inside <= 0) continue;

        int xr = (int)(sqrtf((float)inside) + 0.0001f);
        int x0 = cx - xr;
        int x1 = cx + xr - 1;
        if (x0 < 0) x0 = 0;
        if (x1 >= W) x1 = W - 1;

        int spanW = x1 - x0 + 1;
        if (spanW <= 0) continue;

        int outY = dstY + y;
        if (outY < 0 || outY >= DH) continue;

        int outX = dstX + x0;
        int skipL = 0;
        if (outX < 0) {
            skipL = -outX;
            outX = 0;
        }
        int maxW = DW - outX;
        if (maxW <= 0) continue;
        if (spanW > maxW) spanW = maxW;

        const uint16_t* rowData = srcPtr + (y * W + x0 + skipL);
        if (spanW > 0)
            dst.pushImage(outX, outY, spanW, 1, rowData);
    }
    dst.setSwapBytes(prevSwapDst);
}

void drawCircularBorder(TFT_eSprite& sprite, int centerX, int centerY, int radius, uint16_t color, int thickness) {
    for (int t = 0; t < thickness; t++) {
        sprite.drawCircle(centerX, centerY, radius + t, color);
    }
}

// Watch Face Function
void drawAestheticWatchFace() {
    background.fillSprite(TFT_BLACK);
    
    // Draw background
    gui.drawJpegToSprite(&background, "/nightsky.jpg", 0, 0);
    
    // Add twinkling stars
    for (int i = 0; i < 30; i++) {
        int starX = random(0, 320);
        int starY = random(0, 240);
        uint16_t starBrightness = random(50, 255);
        uint16_t starColor = background.color565(starBrightness, starBrightness, starBrightness);
        background.drawPixel(starX, starY, starColor);
    }
    
    // Get current time
    if (!getLocalTime(&timeInfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    
    // Main digital time display - Large and prominent
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
    
    // Time display background panel with gradient effect
    int timeY = CENTER_Y - 20;
    int panelWidth = 200;
    int panelHeight = 60;
    int panelX = CENTER_X - panelWidth/2;
    
    // Gradient background for time panel
    for (int i = 0; i < panelHeight; i++) {
        float ratio = (float)i / panelHeight;
        uint16_t gradientColor = background.color565(
            (int)(20 * (1 - ratio) + 60 * ratio),
            (int)(30 * (1 - ratio) + 80 * ratio),
            (int)(60 * (1 - ratio) + 120 * ratio)
        );
        background.drawFastHLine(panelX, timeY - panelHeight/2 + i, panelWidth, gradientColor);
    }
    
    // Panel borders with glow effect
    for (int glow = 0; glow < 3; glow++) {
        uint16_t glowColor = background.color565(0, 150 - glow * 30, 255 - glow * 50);
        background.drawRoundRect(panelX - glow, timeY - panelHeight/2 - glow, 
                               panelWidth + 2*glow, panelHeight + 2*glow, 15 + glow, glowColor);
    }
    
    // Main time text - very large
    background.setTextDatum(MC_DATUM);
    
    // Create shadow effect for time
    background.setTextColor(TFT_BLACK);
    background.setTextSize(6);  // Very large font
    background.drawString(timeStr, CENTER_X + 2, timeY + 2);
    
    // Main time text
    background.setTextColor(TFT_CYAN);
    background.drawString(timeStr, CENTER_X, timeY);
    
    // Add glowing outline effect to time
    background.setTextColor(background.color565(0, 200, 255));
    background.setTextSize(6);
    for (int offset = 1; offset <= 1; offset++) {
        background.drawString(timeStr, CENTER_X - offset, timeY);
        background.drawString(timeStr, CENTER_X + offset, timeY);
        background.drawString(timeStr, CENTER_X, timeY - offset);
        background.drawString(timeStr, CENTER_X, timeY + offset);
    }
    
    // Pulsing colon effect
    if (millis() % 1000 < 500) {  // Blink every 500ms
        background.setTextColor(TFT_RED);
        background.setTextSize(6);
        background.drawString(":", CENTER_X, timeY);
    }
    
    // Date display - elegant and positioned below time
    char dateStr[20];
    const char* months[] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", 
                           "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
    sprintf(dateStr, "%02d %s %04d", timeInfo.tm_mday, months[timeInfo.tm_mon], timeInfo.tm_year + 1900);
    
    // Date panel with modern design
    int dateY = timeY + 50;
    int datePanelWidth = 250;
    int datePanelHeight = 25;
    int datePanelX = CENTER_X - datePanelWidth/2;
    
    background.fillRoundRect(datePanelX, dateY - datePanelHeight/2, 
                           datePanelWidth, datePanelHeight, 12, 
                           background.color565(20, 40, 80));
    background.drawRoundRect(datePanelX, dateY - datePanelHeight/2, 
                           datePanelWidth, datePanelHeight, 12, TFT_GOLD);
    
    background.setTextColor(TFT_WHITE);
    background.setTextSize(1);
    background.drawString(dateStr, CENTER_X, dateY);
    
    // Day of week - prominent display
    const char* days[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
    String dayStr = days[timeInfo.tm_wday];
    
    int dayY = timeY - 50;
    background.setTextColor(TFT_YELLOW);
    background.setTextSize(2);
    background.drawString(dayStr, CENTER_X, dayY);
    
    // Add decorative elements around the digital clock
    // Left and right accent lines
    for (int i = 0; i < 3; i++) {
        uint16_t accentColor = background.color565(0, 100 + i * 50, 255 - i * 30);
        background.drawFastVLine(panelX - 20 - i * 3, timeY - 30, 60, accentColor);
        background.drawFastVLine(panelX + panelWidth + 20 + i * 3, timeY - 30, 60, accentColor);
    }
    
    // Weather display below date (only if enabled)
    if (weatherEnabled && weatherClient && weatherClient->isDataValid()) {
        int weatherY = dateY + 40;  // Position below date
        
        // Subtle weather panel background - positioned FIRST so it's behind the text
        int weatherPanelWidth = 280;
        int weatherPanelHeight = 55;
        int weatherPanelX = CENTER_X - weatherPanelWidth/2;
        int weatherPanelY = weatherY - 15;  // Adjusted to properly contain all weather info
        
        background.fillRoundRect(weatherPanelX, weatherPanelY, 
                               weatherPanelWidth, weatherPanelHeight, 8, 
                               background.color565(10, 25, 40));
        background.drawRoundRect(weatherPanelX, weatherPanelY, 
                               weatherPanelWidth, weatherPanelHeight, 8, 
                               background.color565(0, 100, 150));
        
        // Temperature display - prominent and clean
        String tempStr = weatherClient->getFormattedTemperature();
        background.setTextDatum(MC_DATUM);
        background.setTextSize(2);  // Larger than date text
        uint16_t tempColor = weatherClient->getWeatherColor();
        background.setTextColor(tempColor);
        background.drawString(tempStr, CENTER_X, weatherY);
        
        // Weather description below temperature
        String desc = weatherClient->getWeatherDescription();
        background.setTextColor(TFT_CYAN);
        background.setTextSize(1);
        background.drawString(desc, CENTER_X, weatherY + 18);
        
        // Additional weather info in a horizontal layout
        int infoY = weatherY + 32;
        String weatherInfo = "Feels " + String((int)weatherClient->getFeelsLike()) + "°C  •  " + 
                           "Humidity " + weatherClient->getFormattedHumidity() + "  •  " +
                           "Wind " + weatherClient->getFormattedWindSpeed();
        
        background.setTextColor(TFT_LIGHTGREY);
        background.setTextSize(1);
        background.drawString(weatherInfo, CENTER_X, infoY);
    }
    
    // Battery indicator - modern design
    int batteryLevel = 85;
    int batteryX = 270;
    int batteryY = 10;
    int batteryWidth = 40;
    int batteryHeight = 20;
    
    // Battery body with rounded corners
    background.fillRoundRect(batteryX, batteryY, batteryWidth, batteryHeight, 4, TFT_DARKGREY);
    background.drawRoundRect(batteryX, batteryY, batteryWidth, batteryHeight, 4, TFT_WHITE);
    
    // Battery tip
    background.fillRoundRect(batteryX + batteryWidth, batteryY + 6, 4, 8, 2, TFT_WHITE);
    
    // Battery fill with gradient effect
    int fillWidth = map(batteryLevel, 0, 100, 0, batteryWidth - 4);
    uint16_t battColor = batteryLevel > 50 ? TFT_GREEN : 
                        batteryLevel > 20 ? TFT_YELLOW : TFT_RED;
    
    for (int i = 0; i < fillWidth; i++) {
        float ratio = (float)i / fillWidth;
        uint16_t fillColor = background.color565(
            ((battColor >> 11) & 0x1F) * (0.5 + 0.5 * ratio),
            ((battColor >> 5) & 0x3F) * (0.5 + 0.5 * ratio),
            (battColor & 0x1F) * (0.5 + 0.5 * ratio)
        );
        background.drawFastVLine(batteryX + 2 + i, batteryY + 2, batteryHeight - 4, fillColor);
    }
    
    // Battery percentage text
    background.setTextColor(TFT_WHITE);
    background.setTextSize(1);
    background.setTextDatum(MC_DATUM);
    background.drawString(String(batteryLevel) + "%", batteryX + batteryWidth/2, batteryY + 35);
    
    // System status indicators (top right corner)
    int statusX = 240;
    int statusY = batteryY;
    
    // WiFi indicator
    if (WiFi.status() == WL_CONNECTED) {
        // WiFi signal strength bars
        int rssi = WiFi.RSSI();
        int bars = map(rssi, -100, -30, 1, 4);
        bars = constrain(bars, 1, 4);
        
        for (int i = 0; i < 4; i++) {
            uint16_t barColor = (i < bars) ? TFT_GREEN : TFT_DARKGREY;
            int barHeight = 3 + i * 2;
            background.fillRect(statusX + i * 4, statusY + 12 - barHeight, 3, barHeight, barColor);
        }
        
        // WiFi label
        background.setTextColor(TFT_GREEN);
        background.setTextSize(1);
        background.setTextDatum(TL_DATUM);
        background.drawString("WiFi", statusX, statusY + 15);
    } else {
        background.setTextColor(TFT_RED);
        background.setTextSize(1);
        background.setTextDatum(TL_DATUM);
        background.drawString("No WiFi", statusX, statusY + 15);
    }
    
    // Time format indicator (12/24 hour)
    background.setTextColor(TFT_LIGHTGREY);
    background.setTextSize(1);
    background.setTextDatum(TL_DATUM);
    background.drawString("24H", statusX, statusY + 28);
    
    // Add some decorative corner elements
    // Top corners
    for (int i = 0; i < 5; i++) {
        uint16_t cornerColor = background.color565(50 + i * 20, 100 + i * 20, 200 - i * 20);
        background.drawPixel(i, i, cornerColor);
        background.drawPixel(319 - i, i, cornerColor);
        background.drawPixel(i, 239 - i, cornerColor);
        background.drawPixel(319 - i, 239 - i, cornerColor);
    }
    
    // Add subtle animation effect - moving particles
    static unsigned long lastParticleUpdate = 0;
    static int particlePositions[5] = {0, 50, 100, 150, 200};
    
    if (millis() - lastParticleUpdate > 100) {
        for (int i = 0; i < 5; i++) {
            particlePositions[i] += 2;
            if (particlePositions[i] > 320) particlePositions[i] = -10;
        }
        lastParticleUpdate = millis();
    }
    
    for (int i = 0; i < 5; i++) {
        uint16_t particleColor = background.color565(0, 100 + i * 30, 255 - i * 40);
        background.drawPixel(particlePositions[i], 235 - i * 2, particleColor);
        background.drawPixel(particlePositions[i] + 1, 235 - i * 2, particleColor);
    }
    
    background.pushSprite(0, 0);
}

// Rotating Menu Function
void drawSmoothRotatingMenu() {
    background.fillSprite(TFT_BLACK);
    
    // Draw cosmic background
    gui.drawJpegToSprite(&background, "/nightsky.jpg", 0, 0);
    
    // Add nebula effect
    for (int i = 0; i < 50; i++) {
        float angle = random(0, 360) * PI / 180;
        int radius = random(100, 130);
        int nebulaX = CENTER_X + cos(angle) * radius;
        int nebulaY = CENTER_Y + sin(angle) * radius;
        
        if (nebulaX >= 0 && nebulaX < 320 && nebulaY >= 0 && nebulaY < 240) {
            uint16_t nebulaColor = background.color565(
                random(20, 80), random(0, 50), random(50, 150)
            );
            background.drawPixel(nebulaX, nebulaY, nebulaColor);
        }
    }
    
    // Draw ornate outer ring (ship's wheel)
    int outerRadius = MENU_RADIUS + 35;
    int innerRadius = MENU_RADIUS + 20;
    
    // Outer decorative circles
    for (int r = 0; r < 5; r++) {
        uint16_t ringColor = background.color565(
            100 - r * 15, 100 - r * 10, 150 - r * 20
        );
        background.drawCircle(CENTER_X, CENTER_Y, outerRadius + r, ringColor);
    }
    
    // Wheel spokes - rotating with menu
    for (int i = 0; i < 12; i++) {
        float spokeAngle = (PI * 2 * i / 12) + menuRotationOffset;
        int x1 = CENTER_X + cos(spokeAngle) * innerRadius;
        int y1 = CENTER_Y + sin(spokeAngle) * innerRadius;
        int x2 = CENTER_X + cos(spokeAngle) * outerRadius;
        int y2 = CENTER_Y + sin(spokeAngle) * outerRadius;
        
        uint16_t spokeColor = (i % 2 == 0) ? TFT_GOLD : TFT_SILVER;
        background.drawLine(x1, y1, x2, y2, spokeColor);
        
        // Decorative dots
        int dotX = CENTER_X + cos(spokeAngle) * (innerRadius + 10);
        int dotY = CENTER_Y + sin(spokeAngle) * (innerRadius + 10);
        background.fillCircle(dotX, dotY, 2, spokeColor);
    }
    
    // Menu items
    TFT_eSprite avatar = TFT_eSprite(&tft);
    avatar.setColorDepth(16);
    avatar.createSprite(AVATAR_SIZE, AVATAR_SIZE);
    
    for (int i = 0; i < MENU_ITEMS_COUNT; i++) {
        float currentAngle = menuItems[i].angle + menuRotationOffset;
        
        int x = CENTER_X + cos(currentAngle) * MENU_RADIUS - AVATAR_SIZE/2;
        int y = CENTER_Y + sin(currentAngle) * MENU_RADIUS - AVATAR_SIZE/2;
        
        avatar.fillSprite(TFT_BLACK);
        
        if (!gui.drawJpegToSprite(&avatar, menuItems[i].avatarPath.c_str(), 0, 0)) {
            // Enhanced fallback with gradients
            uint16_t color1 = background.color565(100 + i * 30, 150 + i * 20, 255 - i * 25);
            uint16_t color2 = background.color565(50 + i * 15, 100 + i * 10, 200 - i * 20);
            
            // Radial gradient
            for (int r = AVATAR_SIZE/2; r > 0; r--) {
                float ratio = (float)r / (AVATAR_SIZE/2);
                uint16_t blendColor = background.color565(
                    ((color1 >> 11) & 0x1F) * ratio + ((color2 >> 11) & 0x1F) * (1 - ratio),
                    ((color1 >> 5) & 0x3F) * ratio + ((color2 >> 5) & 0x3F) * (1 - ratio),
                    (color1 & 0x1F) * ratio + (color2 & 0x1F) * (1 - ratio)
                );
                avatar.drawCircle(AVATAR_SIZE/2, AVATAR_SIZE/2, r, blendColor);
            }
            
            // Icon letter
            avatar.setTextColor(TFT_BLACK);
            avatar.setTextDatum(MC_DATUM);
            avatar.setTextSize(2);
            avatar.drawString(String(menuItems[i].label[0]), AVATAR_SIZE/2 + 1, AVATAR_SIZE/2 + 1);
            
            avatar.setTextColor(TFT_WHITE);
            avatar.drawString(String(menuItems[i].label[0]), AVATAR_SIZE/2, AVATAR_SIZE/2);
        }
        
        // Selection effects
        if (i == selectedMenuItem) {
            // Pulsing glow
            float pulse = sin(millis() * 0.01) * 0.5 + 0.5;
            int glowRadius = AVATAR_SIZE/2 + 12 + (int)(pulse * 8);
            
            for (int glow = 4; glow > 0; glow--) {
                uint16_t glowColor = background.color565(0, (int)(255 * pulse / glow), 0);
                background.drawCircle(x + AVATAR_SIZE/2, y + AVATAR_SIZE/2, glowRadius + glow * 2, glowColor);
            }
            
            // Selection ring
            background.drawCircle(x + AVATAR_SIZE/2, y + AVATAR_SIZE/2, AVATAR_SIZE/2 + 8, TFT_GREEN);
            background.drawCircle(x + AVATAR_SIZE/2, y + AVATAR_SIZE/2, AVATAR_SIZE/2 + 9, TFT_GREEN);
            background.drawCircle(x + AVATAR_SIZE/2, y + AVATAR_SIZE/2, AVATAR_SIZE/2 + 10, TFT_GREEN);
        }
        
        blitCircleSpriteToSprite(avatar, background, x, y);
        
        // Enhanced text labels
        int textX = x + AVATAR_SIZE/2;
        int textY = y + AVATAR_SIZE + 20;
        
        // Text background
        int textWidth = menuItems[i].label.length() * 6;
        background.fillRoundRect(textX - textWidth/2 - 2, textY - 6, textWidth + 4, 14, 3, 
                               background.color565(0, 0, 50));
        
        // Text shadow
        background.setTextColor(TFT_BLACK);
        background.setTextDatum(MC_DATUM);
        background.setTextSize(1);
        background.drawString(menuItems[i].label, textX + 1, textY + 1);
        
        // Main text
        uint16_t textColor = (i == selectedMenuItem) ? TFT_GREEN : TFT_WHITE;
        background.setTextColor(textColor);
        background.drawString(menuItems[i].label, textX, textY);
    }
    
    // Enhanced center hub
    background.drawCircle(CENTER_X, CENTER_Y, 35, TFT_GOLD);
    background.drawCircle(CENTER_X, CENTER_Y, 33, TFT_DARKGREY);
    
    // Metallic center
    for (int r = 30; r > 0; r--) {
        uint16_t hubColor = background.color565(
            map(r, 0, 30, 50, 120),
            map(r, 0, 30, 50, 120),
            map(r, 0, 30, 50, 150)
        );
        background.fillCircle(CENTER_X, CENTER_Y, r, hubColor);
    }
    
    // Center highlight
    background.fillCircle(CENTER_X - 8, CENTER_Y - 8, 8, background.color565(200, 200, 255));
    
    // Center text with glow
    background.setTextColor(TFT_BLACK);
    background.setTextDatum(MC_DATUM);
    background.setTextSize(2);
    background.drawString("MENU", CENTER_X + 1, CENTER_Y + 1);
    
    background.setTextColor(TFT_CYAN);
    background.drawString("MENU", CENTER_X, CENTER_Y);
    
    avatar.deleteSprite();
    background.pushSprite(0, 0);
}

// Menu Navigation
void navigateMenu(int direction) {
    if (isRotating) return;
    
    selectedMenuItem += direction;
    if (selectedMenuItem < 0) selectedMenuItem = MENU_ITEMS_COUNT - 1;
    if (selectedMenuItem >= MENU_ITEMS_COUNT) selectedMenuItem = 0;
    
    // Start rotation animation
    isRotating = true;
    animationStart = millis();
    lastMenuActivity = millis();
}

void updateMenuRotation() {
    if (!isRotating) return;
    
    unsigned long elapsed = millis() - animationStart;
    float progress = (float)elapsed / ANIMATION_DURATION;
    
    if (progress >= 1.0) {
        progress = 1.0;
        isRotating = false;
    }
    
    // Smooth easing
    progress = progress * progress * (3.0f - 2.0f * progress);
    
    float rotationStep = (2.0 * PI / MENU_ITEMS_COUNT);
    menuRotationOffset = -selectedMenuItem * rotationStep;
}

// Weather Update
void updateWeather() {
    if (!weatherClient || !weatherEnabled) return;
    
    unsigned long now = millis();
    if (now - lastWeatherFetch > WEATHER_UPDATE_INTERVAL) {
        Serial.println("Updating weather...");
        weatherClient->fetchWeather();
        lastWeatherFetch = now;
        
        if (weatherClient->isDataValid()) {
            Serial.println("Weather updated:");
            Serial.println("Temperature: " + weatherClient->getFormattedTemperature());
            Serial.println("Description: " + weatherClient->getWeatherDescription());
        }
    }
}

// WiFi Connection
void connectWiFi() {
    WiFi.begin(ssid, password);
    
    Serial.print("Connecting to WiFi");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" Connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        // Initialize NTP
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        Serial.println("NTP configured");
    } else {
        Serial.println(" Failed to connect!");
    }
}

// Initialize Weather Client
void initializeWeather() {
    weatherClient = new WeatherClient(apiKey, city);
}

// Handle Menu Selection
void handleMenuSelection(int selectedIndex) {
    String selectedItem = menuItems[selectedIndex].label;
    
    Serial.print("Menu action: ");
    Serial.println(selectedItem);
    
    // Visual feedback
    gui.setBright(50);  // Dim briefly
    delay(100);
    gui.setBright(85);  // Restore brightness
    
    // Handle different menu items
    if (selectedItem == "Home") {
        Serial.println("-> Going to home screen");
        // Add your home screen logic here
        
    } else if (selectedItem == "Weather") {
        Serial.println("-> Showing detailed weather");
        // Force weather update
        if (weatherClient) {
            weatherClient->fetchWeather();
        }
        
    } else if (selectedItem == "Music") {
        Serial.println("-> Opening music player");
        // Add your music player logic here
        
    } else if (selectedItem == "Gallery") {
        Serial.println("-> Opening gallery");
        // Add your gallery logic here
        
    } else if (selectedItem == "Settings") {
        Serial.println("-> Opening settings");
        // Add your settings logic here
        
    } else if (selectedItem == "Games") {
        Serial.println("-> Opening games");
        // Add your games logic here
    }
}

// Display System Status
void displaySystemStatus() {
    Serial.println("\n=== SignalMaster Status ===");
    Serial.println("Auto Mode: " + String(autoMode ? "ON" : "OFF"));
    Serial.println("Menu Active: " + String(menuActive ? "ON" : "OFF"));
    Serial.println("Weather Enabled: " + String(weatherEnabled ? "ON" : "OFF"));
    Serial.println("Demo Mode: " + String(demoMode ? "ON" : "OFF"));
    Serial.println("Current View: " + String(menuActive ? "Menu" : "Watch"));
    Serial.println("Selected Menu Item: " + menuItems[selectedMenuItem].label);
    Serial.println("========================\n");
}

// Setup Function
void setup() {
    Serial.begin(115200);
    Serial.println("SignalMaster starting...");
    
    // Initialize display backlight
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, 0);
    
    // Mount filesystem
    bool ffatOk = FFat.begin(true);
    Serial.println(ffatOk ? "FFat mounted successfully" : "FFat mount FAILED");
    if(ffatOk)audioInit();
    audioSetVolume(21);
    audioConnecttoFFat("/boot.mp3");

    // Initialize TFT display
    tft.init();
    tft.setRotation(3); // Landscape mode
    
    // Create background sprite
    background.setColorDepth(16);
    background.createSprite(320, 240);
    
    // Boot sequence
    Serial.println("Starting boot sequence...");
    gui.bootUP();
    gui.displayHardwareInfo(ffatOk, 
                           ffatOk ? FFat.totalBytes() : 0,
                           ffatOk ? FFat.usedBytes() : 0, 
                           "FFat");
    gui.fadeUP();
    delay(1000);
    gui.setBright(85);
    
    // Initialize WiFi and weather
    Serial.println("Connecting to WiFi...");
    connectWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Initializing weather client...");
        initializeWeather();
        weatherEnabled = true;
        
        // Initial weather fetch
        if (weatherClient) {
            Serial.println("Fetching initial weather data...");
            weatherClient->fetchWeather();
            lastWeatherFetch = millis();
        }
    } else {
        Serial.println("WiFi not connected - weather features disabled");
        weatherEnabled = false;
    }
    
    // Initialize in watch mode by default
    menuActive = false;
    
    Serial.println("=================================");
    Serial.println("SignalMaster System Ready!");
    Serial.println("=================================");
    Serial.println("Serial Commands Available:");
    Serial.println("- help         : Show all commands");
    Serial.println("- status       : Show current status");
    Serial.println("- watch        : Switch to watch mode");
    Serial.println("- menu         : Switch to menu mode");
    Serial.println("- next         : Next menu item");
    Serial.println("- prev         : Previous menu item");
    Serial.println("- select       : Select current menu item");
    Serial.println("- auto [on/off]: Toggle auto mode");
    Serial.println("- demo [on/off]: Toggle demo mode");
    Serial.println("- weather      : Update weather");
    Serial.println("- weatheron    : Enable weather display");
    Serial.println("- weatheroff   : Disable weather display");
    Serial.println("=================================");
}

// Enhanced Serial Command Handler
void handleSerialCommands() {
    if (Serial.available() > 0) {
        String command = Serial.readString();
        command.trim();
        command.toLowerCase();
        
        if (command == "help") {
            Serial.println("\n=== SignalMaster Commands ===");
            Serial.println("System Control:");
            Serial.println("  help         - Show this help menu");
            Serial.println("  status       - Show current system status");
            Serial.println("  info         - Show detailed system info");
            Serial.println("\nDisplay Control:");
            Serial.println("  watch        - Switch to watch face mode");
            Serial.println("  menu         - Switch to menu mode");
            Serial.println("\nMenu Navigation:");
            Serial.println("  next         - Navigate to next menu item");
            Serial.println("  prev         - Navigate to previous menu item");
            Serial.println("  select       - Select current menu item");
            Serial.println("\nAutomation:");
            Serial.println("  auto on      - Enable automatic mode");
            Serial.println("  auto off     - Disable automatic mode");
            Serial.println("  demo on      - Enable demo mode");
            Serial.println("  demo off     - Disable demo mode");
            Serial.println("\nWeather Control:");
            Serial.println("  weather      - Force weather update");
            Serial.println("  weatheron    - Enable weather display");
            Serial.println("  weatheroff   - Disable weather display");
            Serial.println("=============================\n");
            
        } else if (command == "status") {
            displaySystemStatus();
            
        } else if (command == "info") {
            Serial.println("\n=== System Information ===");
            Serial.println("Compiled: " + String(__DATE__) + " " + String(__TIME__));
            Serial.println("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
            Serial.println("Flash size: " + String(ESP.getFlashChipSize()) + " bytes");
            Serial.println("CPU frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("WiFi: Connected");
                Serial.println("IP: " + WiFi.localIP().toString());
                Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
            } else {
                Serial.println("WiFi: Disconnected");
            }
            
            if (weatherClient && weatherClient->isDataValid()) {
                Serial.println("Weather: " + weatherClient->getFormattedTemperature() + 
                              " - " + weatherClient->getWeatherDescription());
            } else {
                Serial.println("Weather: No data");
            }
            Serial.println("=========================\n");
            
        } else if (command == "watch") {
            menuActive = false;
            autoMode = false;
            demoMode = false;
            Serial.println("Switched to watch mode");
            
        } else if (command == "menu") {
            menuActive = true;
            lastMenuActivity = millis();
            Serial.println("Switched to menu mode");
            
        } else if (command == "next") {
            if (menuActive) {
                navigateMenu(1);
                Serial.println("Next: " + menuItems[selectedMenuItem].label);
            } else {
                Serial.println("Not in menu mode - use 'menu' command first");
            }
            
        } else if (command == "prev") {
            if (menuActive) {
                navigateMenu(-1);
                Serial.println("Previous: " + menuItems[selectedMenuItem].label);
            } else {
                Serial.println("Not in menu mode - use 'menu' command first");
            }
            
        } else if (command == "select") {
            if (menuActive) {
                handleMenuSelection(selectedMenuItem);
            } else {
                Serial.println("Not in menu mode - use 'menu' command first");
            }
            
        } else if (command == "auto on") {
            autoMode = true;
            menuActive = true;
            lastMenuActivity = millis();
            Serial.println("Auto mode enabled - system will cycle between watch and menu");
            
        } else if (command == "auto off") {
            autoMode = false;
            demoMode = false;
            Serial.println("Auto mode disabled");
            
        } else if (command == "demo on") {
            demoMode = true;
            autoMode = true;
            menuActive = true;
            demoModeTimer = millis();
            Serial.println("Demo mode enabled - automatic navigation active");
            
        } else if (command == "demo off") {
            demoMode = false;
            Serial.println("Demo mode disabled");
            
        } else if (command == "weather") {
            if (weatherClient) {
                Serial.println("Updating weather...");
                weatherClient->fetchWeather();
                lastWeatherFetch = millis();
            } else {
                Serial.println("Weather client not initialized");
            }
            
        } else if (command == "weatheron") {
            weatherEnabled = true;
            Serial.println("Weather display enabled");
            
        } else if (command == "weatheroff") {
            weatherEnabled = false;
            Serial.println("Weather display disabled");
            
        } else {
            Serial.println("Unknown command: '" + command + "'");
            Serial.println("Type 'help' for available commands");
        }
    }
}

// Demo Mode Logic
void handleDemoMode() {
    if (!demoMode || !menuActive) return;
    
    if (millis() - demoModeTimer > DEMO_INTERVAL) {
        static int demoStep = 0;
        static int completedCycles = 0;
        
        switch (demoStep % 4) {
            case 0:
            case 1:
            case 2:
                // Navigate through menu
                if (!isRotating) {
                    navigateMenu(1);
                    Serial.println("Demo: " + menuItems[selectedMenuItem].label);
                    demoStep++;
                }
                break;
                
            case 3:
                // Select current item
                if (!isRotating) {
                    handleMenuSelection(selectedMenuItem);
                    demoStep++;
                    
                    // Check if we completed a full cycle
                    if (selectedMenuItem == 0) {
                        completedCycles++;
                        Serial.println("Demo: Completed cycle #" + String(completedCycles));
                        
                        // After 2 cycles, switch to watch face
                        if (completedCycles >= 2) {
                            menuActive = false;
                            demoStep = 0;
                            completedCycles = 0;
                            Serial.println("Demo: Switching to watch face");
                        }
                    }
                }
                break;
        }
        
        demoModeTimer = millis();
    }
}

// Auto Mode Logic
void handleAutoMode() {
    if (!autoMode) return;
    
    static unsigned long autoModeTimer = 0;
    static bool inWatchMode = false;
    
    if (millis() - autoModeTimer > (inWatchMode ? 10000 : 15000)) { // Watch: 10s, Menu: 15s
        if (inWatchMode) {
            // Switch to menu
            menuActive = true;
            lastMenuActivity = millis();
            inWatchMode = false;
            Serial.println("Auto: Switching to menu mode");
        } else {
            // Switch to watch
            menuActive = false;
            inWatchMode = true;
            Serial.println("Auto: Switching to watch mode");
        }
        autoModeTimer = millis();
    }
}

// Main Loop
void loop() {
    // Handle serial commands first
    handleSerialCommands();
    
    // Handle auto mode if enabled
    if (autoMode && !demoMode) {
        handleAutoMode();
    }
    
    // Handle demo mode if enabled
    if (demoMode) {
        handleDemoMode();
    }
    
    // Update weather periodically if enabled
    if (weatherEnabled) {
        updateWeather();
    }
    
    // Check menu timeout in manual mode
    if (menuActive && !autoMode && !demoMode && 
        (millis() - lastMenuActivity > MENU_TIMEOUT)) {
        menuActive = false;
        Serial.println("Menu timeout - returning to watch face");
    }
    
    // Render appropriate view
    if (menuActive) {
        updateMenuRotation();
        drawSmoothRotatingMenu();
    } else {
        drawAestheticWatchFace();
    }
    
    // Small delay to prevent system overload
    delay(50);
}