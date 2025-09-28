// Config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Display settings
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

// WiFi credentials
#define WIFI_SSID "Rakib"
#define WIFI_PASSWORD "rakib@2024"

// Camera server
#define CAMERA_HOST "192.168.0.122"
#define CAMERA_PORT 8888

// Weather API
#define WEATHER_API_KEY "dff65ece7cd56e5609a961090c1815a5"
#define WEATHER_CITY "Dhaka,bd"

// Time settings
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (6 * 3600)  // GMT+6 for Dhaka
#define DAYLIGHT_OFFSET_SEC 0

// Timing constants
#define MENU_TIMEOUT 8000
#define WEATHER_UPDATE_INTERVAL 300000  // 5 minutes
#define CAMERA_RECONNECT_INTERVAL 2000

// Memory settings
#define MAX_JPEG_SIZE 50000

// Color palette
#define COLOR_BG_DARK 0x0841
#define COLOR_ACCENT 0x07FF
#define COLOR_SUCCESS 0x07E0
#define COLOR_WARNING 0xFD20
#define COLOR_ERROR 0xF800
#define COLOR_TEXT_PRIMARY 0xFFFF
#define COLOR_TEXT_SECONDARY 0x8410
#define COLOR_CARD_BG 0x2124
#define COLOR_OVERLAY 0x18C3

// System modes
enum SystemMode {
    MODE_WATCH,
    MODE_MENU,
    MODE_CAMERA
};

// File paths
#define WALLPAPER_PATH "/nightsky.jpg"

#endif // CONFIG_H