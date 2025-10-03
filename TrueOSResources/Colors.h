// Colors.h - Volos-inspired Color Palette
#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>

// ================== Primary Colors ==================
#define COLOR_BG          0x0841   // Dark blue-grey background
#define COLOR_HEADER      0x1082   // Header background
#define COLOR_CARD        0x1903   // Card background
#define COLOR_SELECTED    0x2945   // Selected item highlight

// ================== Accent Colors ==================
#define COLOR_ACCENT      0x07FF   // Cyan accent (primary)
#define COLOR_ACCENT_DIM  0x0438   // Dimmed cyan

// ================== Text Colors ==================
#define COLOR_TEXT        0xFFFF   // White text
#define COLOR_TEXT_DIM    0x8410   // Grey text (secondary)
#define COLOR_TEXT_DARK   0x4208   // Dark grey text

// ================== Signal Quality Colors ==================
#define COLOR_EXCELLENT   0x07E0   // Green (RSSI >= -50)
#define COLOR_GOOD        0x7FE0   // Yellow-green (RSSI >= -60)
#define COLOR_FAIR        0xFD20   // Orange (RSSI >= -70)
#define COLOR_POOR        0xF800   // Red (RSSI < -70)

// ================== UI Element Colors ==================
#define COLOR_BORDER      0x2965   // Border color
#define COLOR_SHADOW      0x0020   // Shadow color
#define COLOR_KEY_BG      0x2124   // Keyboard key background
#define COLOR_KEY_ACTIVE  0x3186   // Active keyboard key
#define COLOR_KEY_BORDER  0x4A69   // Key border

// ================== Status Colors ==================
#define COLOR_SUCCESS     0x07E0   // Success green
#define COLOR_ERROR       0xF800   // Error red
#define COLOR_WARNING     0xFD20   // Warning orange
#define COLOR_INFO        0x07FF   // Info cyan

// ================== RSSI Thresholds ==================
#define RSSI_EXCELLENT   -50
#define RSSI_GOOD        -60
#define RSSI_FAIR        -70
// Below -70 is POOR

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

#endif // COLORS_H