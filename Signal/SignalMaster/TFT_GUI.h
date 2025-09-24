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

class TFT_GUI {
public:
    // Public overlay sprites (keep them SMALL to save RAM)
    TFT_eSprite person;
    TFT_eSprite clock;

    // screenWidth/Height = your display size
    // itemHeight/animationDuration = menu animation parameters (optional)
    TFT_GUI(TFT_eSPI& tftInstance, int screenWidth, int screenHeight, int itemHeight, int animationDuration);

    // ----- Menu API (optional; kept for compatibility) -----
    void setItems(String items[], int itemCount);
    void setSelectedItem(int index);
    void drawMenu();
    void updateMenuTransition();
    void travarse(int pre, int select);

    // ----- System / boot info -----
    void displayHardwareInfo(bool sdMounted, uint64_t sdTotalSize, uint64_t sdUsedSpace, const char* sdType);
    void bootUP();

    // ----- JPEG helpers -----
    // Draw a JPEG file directly to the TFT at (x,y)
    void drawJpeg(const char *filename, int xpos, int ypos);

    // Draw a JPEG file into a sprite at sprite-local (x,y)
    bool drawJpegToSprite(TFT_eSprite* dst, const char* filename, int x, int y);

    // Convenience aliases
    bool drawsprjpeg(const char* filename, int x, int y);                    // into the main full-screen sprite (spr) & push
    bool drawsprjpeg(TFT_eSprite* dst, const char* filename, int x, int y);  // into any sprite

    // Build the main UI (loads BG_PATH into the full-screen sprite and shows it)
    void renderUI();

    // ----- Backlight controls (requires TFT_BL in your sketch) -----
    void setBright(int percentage); // 0..100 mapped to 0..255 PWM
    void fadeUP();
    void fadeDown();
    void off();

private:
    TFT_eSPI& tft;

    // The ONLY full-screen sprite (canvas)
    TFT_eSprite spr;

    int SCREEN_WIDTH;
    int SCREEN_HEIGHT;
    int ITEM_HEIGHT;
    int animationDuration;

    // menu state
    int selectedItem;
    int itemCount;
    String* menuItems;

    unsigned long animationStartTime;
    bool isAnimating;
    int fromItem;
    int toItem;

    // internals
    void drawGradientBackground();
    void drawMenuItem(String item, int y, bool selected, int textSize, float fadeProgress);
    void startMenuTransition(int from, int to);

    // JPEG internals
    void jpegRender(int xpos, int ypos);                     // JPEG -> TFT
    bool jpegRenderToSprite(TFT_eSprite* dst, int x, int y); // JPEG -> Sprite
    void jpegInfo();
    void showTime(uint32_t msTime);
};

#endif // TFT_GUI_H
