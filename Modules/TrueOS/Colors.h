// Colors.h - Multi-Theme Color System with Custom Theme
#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>
#include <Preferences.h>

// Theme definitions
enum ColorTheme {
    THEME_DARK_BLUE = 0,
    THEME_PURE_BLACK = 1,
    THEME_AMOLED = 2,
    THEME_OCEAN = 3,
    THEME_FOREST = 4,
    THEME_SUNSET = 5,
    THEME_CUSTOM = 6
};

// Custom theme storage
struct CustomTheme {
    uint16_t bg;
    uint16_t card;
    uint16_t accent;
    
    void save() {
        Preferences prefs;
        prefs.begin("customtheme", false);
        prefs.putUShort("bg", bg);
        prefs.putUShort("card", card);
        prefs.putUShort("accent", accent);
        prefs.end();
    }
    
    void load() {
        Preferences prefs;
        prefs.begin("customtheme", true);
        bg = prefs.getUShort("bg", 0x0841);
        card = prefs.getUShort("card", 0x1903);
        accent = prefs.getUShort("accent", 0xFD20);
        prefs.end();
    }
};

// Current theme (extern, defined in WiFiApp.ino)
extern ColorTheme currentTheme;
extern CustomTheme customTheme;

// Dynamic color getters
inline uint16_t getColorBG() {
    switch(currentTheme) {
        case THEME_PURE_BLACK: return 0x0000;
        case THEME_AMOLED: return 0x0000;
        case THEME_OCEAN: return 0x0010;
        case THEME_FOREST: return 0x0020;
        case THEME_SUNSET: return 0x2000;
        case THEME_CUSTOM: return customTheme.bg;
        default: return 0x0841;
    }
}

inline uint16_t getColorAccent() {
    switch(currentTheme) {
        case THEME_PURE_BLACK: return 0xFFFF;
        case THEME_AMOLED: return 0x07FF;
        case THEME_OCEAN: return 0x05FF;
        case THEME_FOREST: return 0x07E0;
        case THEME_SUNSET: return 0xFD20;
        case THEME_CUSTOM: return customTheme.accent;
        default: return 0xFD20;
    }
}

inline uint16_t getColorCard() {
    switch(currentTheme) {
        case THEME_PURE_BLACK: return 0x1082;
        case THEME_AMOLED: return 0x0841;
        case THEME_OCEAN: return 0x1082;
        case THEME_FOREST: return 0x1082;
        case THEME_SUNSET: return 0x2104;
        case THEME_CUSTOM: return customTheme.card;
        default: return 0x1903;
    }
}

// Backward compatibility macros
#define COLOR_BG getColorBG()
#define COLOR_HEADER 0x1082
#define COLOR_CARD getColorCard()
#define COLOR_SELECTED 0x2945
#define COLOR_ACCENT getColorAccent()
#define COLOR_ACCENT_DIM 0xF800

// ================== Text Colors ==================
#define COLOR_TEXT        0xFFFF
#define COLOR_TEXT_DIM    0x8410
#define COLOR_TEXT_DARK   0x4208

// ================== Signal Quality Colors ==================
#define COLOR_EXCELLENT   0x07E0
#define COLOR_GOOD        0x7FE0
#define COLOR_FAIR        0xFD20
#define COLOR_POOR        0xF800

// ================== UI Element Colors ==================
#define COLOR_BORDER      0x2965
#define COLOR_SHADOW      0x0020
#define COLOR_KEY_BG      0x2124
#define COLOR_KEY_ACTIVE  0x3186
#define COLOR_KEY_BORDER  0x4A69

// ================== Status Colors ==================
#define COLOR_SUCCESS     0x07E0
#define COLOR_ERROR       0xF800
#define COLOR_WARNING     0xFD20
#define COLOR_INFO        0x07FF

// ================== RSSI Thresholds ==================
#define RSSI_EXCELLENT   -50
#define RSSI_GOOD        -60
#define RSSI_FAIR        -70

// ================== Helper Functions ==================
inline uint16_t getSignalColor(int rssi) {
    if (rssi >= RSSI_EXCELLENT) return COLOR_EXCELLENT;
    if (rssi >= RSSI_GOOD) return COLOR_GOOD;
    if (rssi >= RSSI_FAIR) return COLOR_FAIR;
    return COLOR_POOR;
}

inline const char* getSignalText(int rssi) {
    if (rssi >= RSSI_EXCELLENT) return "Excellent";
    if (rssi >= RSSI_GOOD) return "Good";
    if (rssi >= RSSI_FAIR) return "Fair";
    return "Poor";
}

inline int getSignalBars(int rssi) {
    if (rssi >= RSSI_EXCELLENT) return 4;
    if (rssi >= RSSI_GOOD) return 3;
    if (rssi >= RSSI_FAIR) return 2;
    return 1;
}

// RGB565 color converter helper
inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#endif // COLORS_H