#include <Arduino.h>
#include <math.h>
#include <FFat.h>
#include <TFT_eSPI.h>
#include "TFT_GUI.h"

#ifndef TFT_BL
#define TFT_BL 8   // PWM-capable backlight pin
#endif

TFT_eSPI tft = TFT_eSPI();
TFT_GUI gui(tft, 320, 240, 30, 250);

TFT_eSprite background = TFT_eSprite(&tft); // 320x240

// Menu system variables
struct MenuItem {
  String label;
  String avatarPath;
  int x, y;  // Position on screen
};

const int MENU_ITEMS_COUNT = 6;
MenuItem menuItems[MENU_ITEMS_COUNT] = {
  {"Home", "/ps.jpg", 0, 0},
  {"Settings", "/ps.jpg", 0, 0},
  {"Gallery", "/ps.jpg", 0, 0},
  {"Music", "/ps.jpg", 0, 0},
  {"Games", "/ps.jpg", 0, 0},
  {"About", "/ps.jpg", 0, 0}
};

int selectedMenuItem = 0;
const int MENU_RADIUS = 80;        // Distance from center
const int AVATAR_SIZE = 40;        // Size of each circular avatar
const int CENTER_X = 160;          // Screen center X
const int CENTER_Y = 120;          // Screen center Y

// Copy ONLY the circular region from src -> dst with full clipping & correct byte order.
void blitCircleSpriteToSprite(TFT_eSprite& src, TFT_eSprite& dst, int dstX, int dstY) {
  const int W = src.width();
  const int H = src.height();
  if (W <= 0 || H <= 0) return;

  const int cx = W / 2;
  const int cy = H / 2;
  const int r  = (cx < cy ? cx : cy);
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

// Draw a circular border around a menu item
void drawCircularBorder(TFT_eSprite& sprite, int centerX, int centerY, int radius, uint16_t color, int thickness) {
  for (int t = 0; t < thickness; t++) {
    sprite.drawCircle(centerX, centerY, radius + t, color);
  }
}

// Calculate positions for circular menu layout
void calculateMenuPositions() {
  for (int i = 0; i < MENU_ITEMS_COUNT; i++) {
    float angle = (2.0 * PI * i) / MENU_ITEMS_COUNT - PI/2; // Start from top
    menuItems[i].x = CENTER_X + cos(angle) * MENU_RADIUS - AVATAR_SIZE/2;
    menuItems[i].y = CENTER_Y + sin(angle) * MENU_RADIUS - AVATAR_SIZE/2;
  }
}

// Draw the circular menu
void drawCircularMenu() {
  // Clear background
  background.fillSprite(TFT_BLACK);
  
  // Draw background image if available
  gui.drawJpegToSprite(&background, "/nightsky.jpg", 0, 0);
  
  // Create temporary sprite for avatars
  TFT_eSprite avatar = TFT_eSprite(&tft);
  avatar.setColorDepth(16);
  avatar.createSprite(AVATAR_SIZE, AVATAR_SIZE);
  
  // Draw each menu item
  for (int i = 0; i < MENU_ITEMS_COUNT; i++) {
    // Clear avatar sprite
    avatar.fillSprite(TFT_BLACK);
    
    // Try to load avatar image, fallback to colored circle
    if (!gui.drawJpegToSprite(&avatar, menuItems[i].avatarPath.c_str(), 0, 0)) {
      // Fallback: draw a colored circle with first letter
      uint16_t color = tft.color565(100 + i * 25, 150, 200 - i * 20);
      avatar.fillCircle(AVATAR_SIZE/2, AVATAR_SIZE/2, AVATAR_SIZE/2 - 2, color);
      avatar.setTextColor(TFT_WHITE);
      avatar.setTextDatum(MC_DATUM);
      avatar.setTextSize(2);
      avatar.drawString(String(menuItems[i].label[0]), AVATAR_SIZE/2, AVATAR_SIZE/2);
    }
    
    // Draw green border if selected
    if (i == selectedMenuItem) {
      int borderX = menuItems[i].x + AVATAR_SIZE/2;
      int borderY = menuItems[i].y + AVATAR_SIZE/2;
      drawCircularBorder(background, borderX, borderY, AVATAR_SIZE/2 + 5, TFT_GREEN, 3);
      
      // Add a subtle glow effect
      drawCircularBorder(background, borderX, borderY, AVATAR_SIZE/2 + 8, tft.color565(0, 100, 0), 1);
    }
    
    // Blit the circular avatar to background
    blitCircleSpriteToSprite(avatar, background, menuItems[i].x, menuItems[i].y);
    
    // Draw text label below avatar
    background.setTextColor(TFT_WHITE);
    background.setTextDatum(MC_DATUM);
    background.setTextSize(1);
    
    // Calculate text position (below the avatar)
    int textX = menuItems[i].x + AVATAR_SIZE/2;
    int textY = menuItems[i].y + AVATAR_SIZE + 15;
    
    // Draw text with black outline for better visibility
    background.setTextColor(TFT_BLACK);
    for (int dx = -1; dx <= 1; dx++) {
      for (int dy = -1; dy <= 1; dy++) {
        if (dx != 0 || dy != 0) {
          background.drawString(menuItems[i].label, textX + dx, textY + dy);
        }
      }
    }
    
    // Draw the actual text
    background.setTextColor(i == selectedMenuItem ? TFT_GREEN : TFT_WHITE);
    background.drawString(menuItems[i].label, textX, textY);
  }
  
  // Draw center logo/title if needed
  background.setTextColor(TFT_WHITE);
  background.setTextDatum(MC_DATUM);
  background.setTextSize(2);
  background.setTextColor(TFT_BLACK);
  background.drawString("MENU", CENTER_X + 1, CENTER_Y + 1); // Shadow
  background.setTextColor(TFT_CYAN);
  background.drawString("MENU", CENTER_X, CENTER_Y);
  
  // Clean up
  avatar.deleteSprite();
  
  // Display the final result
  background.pushSprite(0, 0);
}

// Handle menu navigation
void navigateMenu(int direction) {
  selectedMenuItem += direction;
  if (selectedMenuItem < 0) selectedMenuItem = MENU_ITEMS_COUNT - 1;
  if (selectedMenuItem >= MENU_ITEMS_COUNT) selectedMenuItem = 0;
}

// Simulate button presses for demo
unsigned long lastInput = 0;
int demoCounter = 0;

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 0);

  bool ffatOk = FFat.begin(true);
  Serial.println(ffatOk ? "FFat mounted" : "FFat mount FAILED");

  tft.init();
  tft.setRotation(3);

  // Create sprites
  background.setColorDepth(16);
  background.createSprite(320, 240);

  // Boot sequence
  gui.bootUP();
  gui.displayHardwareInfo(ffatOk, ffatOk ? FFat.totalBytes() : 0,
                          ffatOk ? FFat.usedBytes() : 0, "FFat");
  gui.fadeUP();
  delay(500);
  gui.setBright(85);

  // Calculate menu positions
  calculateMenuPositions();
  
  // Draw initial menu
  drawCircularMenu();
  
  Serial.println("Circular menu initialized!");
  Serial.println("Menu will auto-navigate for demo purposes");
}

void loop() {
  // Demo: auto-navigate menu every 2 seconds
  if (millis() - lastInput > 2000) {
    lastInput = millis();
    navigateMenu(1); // Move to next item
    drawCircularMenu();
    
    Serial.print("Selected: ");
    Serial.println(menuItems[selectedMenuItem].label);
    
    demoCounter++;
    if (demoCounter >= MENU_ITEMS_COUNT * 2) {
      Serial.println("Demo completed. Menu will continue cycling...");
      demoCounter = 0;
    }
  }
  
  // In a real implementation, you would handle button inputs here:
  // if (digitalRead(BUTTON_NEXT) == LOW) {
  //   navigateMenu(1);
  //   drawCircularMenu();
  //   delay(200); // debounce
  // }
  // if (digitalRead(BUTTON_PREV) == LOW) {
  //   navigateMenu(-1);
  //   drawCircularMenu();
  //   delay(200); // debounce
  // }
}