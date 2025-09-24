#ifndef TFT_GUI_H
#define TFT_GUI_H

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <JPEGDecoder.h>

// Safe path constants (no macro collisions)
static constexpr const char* BOOT0_PATH = "/boot.jpg";
static constexpr const char* BOOT1_PATH = "/bubt.jpg";
static constexpr const char* BG_PATH    = "/bg.jpg";

// Circular Menu Item Structure
struct CircularMenuItem {
    String label;
    String avatarPath;
    int x, y;           // Screen position
    float angle;        // Angle in radians
    bool enabled;       // Whether the item is selectable
    uint16_t color;     // Custom color for fallback rendering
};

class TFT_GUI {
public:
    // Public overlay sprites (keep them SMALL to save RAM)
    TFT_eSprite person;
    TFT_eSprite clock;

    // screenWidth/Height = your display size
    // itemHeight/animationDuration = menu animation parameters (optional)
    TFT_GUI(TFT_eSPI& tftInstance, int screenWidth, int screenHeight, int itemHeight, int animationDuration);

    // ----- Original Menu API (optional; kept for compatibility) -----
    void setItems(String items[], int itemCount);
    void setSelectedItem(int index);
    void drawMenu();
    void updateMenuTransition();
    void travarse(int pre, int select);

    // ----- NEW: Circular Menu API -----
    void initCircularMenu(CircularMenuItem* items, int itemCount, int centerX, int centerY, int radius, int avatarSize);
    void setCircularMenuItems(CircularMenuItem* items, int itemCount);
    void drawCircularMenu();
    void selectCircularMenuItem(int index);
    void navigateCircularMenu(int direction);
    int getSelectedCircularMenuItem() const { return selectedCircularItem; }
    void setCircularMenuRadius(int radius) { circularMenuRadius = radius; calculateCircularPositions(); }
    void setCircularMenuCenter(int x, int y) { circularCenterX = x; circularCenterY = y; calculateCircularPositions(); }
    void setAvatarSize(int size) { avatarSize = size; }
    
    // Enhanced circular menu with animations
    void animateToCircularMenuItem(int targetIndex, int animationTimeMs = 300);
    void updateCircularMenuAnimation();
    bool isCircularMenuAnimating() const { return circularMenuAnimating; }

    // ----- System / boot info -----
    void displayHardwareInfo(bool sdMounted, uint64_t sdTotalSize, uint64_t sdUsedSpace, const char* sdType);
    void bootUP();

    // ----- JPEG helpers -----
    void drawJpeg(const char *filename, int xpos, int ypos);
    bool drawJpegToSprite(TFT_eSprite* dst, const char* filename, int x, int y);
    bool drawsprjpeg(const char* filename, int x, int y);
    bool drawsprjpeg(TFT_eSprite* dst, const char* filename, int x, int y);
    void renderUI();

    // ----- Enhanced drawing utilities -----
    void drawCircularBorder(TFT_eSprite& sprite, int centerX, int centerY, int radius, uint16_t color, int thickness = 1);
    void drawCircularAvatar(TFT_eSprite& dst, const char* imagePath, int x, int y, int size, uint16_t fallbackColor, const String& fallbackText);
    void blitCircleSprite(TFT_eSprite& src, TFT_eSprite& dst, int dstX, int dstY);
    
    // Text rendering with outlines
    void drawTextWithOutline(TFT_eSprite& sprite, const String& text, int x, int y, uint16_t textColor, uint16_t outlineColor, int textSize = 1);

    // ----- Backlight controls -----
    void setBright(int percentage);
    void fadeUP();
    void fadeDown();
    void off();

private:
    TFT_eSPI& tft;
    TFT_eSprite spr;  // The main full-screen sprite

    int SCREEN_WIDTH;
    int SCREEN_HEIGHT;
    int ITEM_HEIGHT;
    int animationDuration;

    // Original menu state
    int selectedItem;
    int itemCount;
    String* menuItems;
    unsigned long animationStartTime;
    bool isAnimating;
    int fromItem;
    int toItem;

    // Circular menu state
    CircularMenuItem* circularMenuItems;
    int circularMenuItemCount;
    int selectedCircularItem;
    int circularCenterX;
    int circularCenterY;
    int circularMenuRadius;
    int avatarSize;
    
    // Circular menu animation
    bool circularMenuAnimating;
    unsigned long circularAnimationStart;
    int circularAnimationDuration;
    int circularFromItem;
    int circularToItem;

    // Internal methods
    void drawGradientBackground();
    void drawMenuItem(String item, int y, bool selected, int textSize, float fadeProgress);
    void startMenuTransition(int from, int to);
    
    // Circular menu internals
    void calculateCircularPositions();
    void drawCircularMenuItem(int index, bool selected, float animationProgress = 0.0f);
    
    // JPEG internals
    void jpegRender(int xpos, int ypos);
    bool jpegRenderToSprite(TFT_eSprite* dst, int x, int y);
    void jpegInfo();
    void showTime(uint32_t msTime);
};

#endif // TFT_GUI_H