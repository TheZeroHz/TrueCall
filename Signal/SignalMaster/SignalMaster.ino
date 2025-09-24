#include <Arduino.h>
#include <math.h>
#include <FFat.h>
#include <TFT_eSPI.h>
#include "TFT_GUI.h"

#ifndef TFT_BL
#define TFT_BL 8   // PWM-capable backlight pin
#endif
int avatarX = 40;  // top-left position on background
int avatarY = 40;
TFT_eSPI tft = TFT_eSPI();
TFT_GUI gui(tft, 320, 240, 30, 250);

TFT_eSprite background = TFT_eSprite(&tft); // 320x240
TFT_eSprite person     = TFT_eSprite(&tft); // 128x128

// Copy ONLY the circular region from src -> dst with full clipping & correct byte order.
// Assumes both sprites are 16-bit (RGB565). Works regardless of current swap settings.
void blitCircleSpriteToSprite(TFT_eSprite& src, TFT_eSprite& dst, int dstX, int dstY) {
  const int W = src.width();
  const int H = src.height();
  if (W <= 0 || H <= 0) return;

  // circle in the src sprite
  const int cx = W / 2;
  const int cy = H / 2;
  const int r  = (cx < cy ? cx : cy);
  const int r2 = r * r;

  // pointers to src pixel data (packed RGB565)
  const uint16_t* srcPtr = (const uint16_t*)src.getPointer();
  if (!srcPtr) return;

  // Save & force NO swapping for sprite->sprite pushImage()
  // (swap is for raw arrays with different endianness; sprite memory matches)
  bool prevSwapDst = dst.getSwapBytes();
  dst.setSwapBytes(false);

  // Clip against destination sprite bounds
  const int DW = dst.width();
  const int DH = dst.height();

  for (int y = 0; y < H; ++y) {
    int dy = y - cy;
    int inside = r2 - dy * dy;        // r^2 - dy^2
    if (inside <= 0) continue;        // nothing on this row

    // horizontal span within the circle on this scanline
    int xr = (int)(sqrtf((float)inside) + 0.0001f);
    int x0 = cx - xr;
    int x1 = cx + xr - 1;
    if (x0 < 0) x0 = 0;
    if (x1 >= W) x1 = W - 1;

    int spanW = x1 - x0 + 1;
    if (spanW <= 0) continue;

    int outY = dstY + y;
    if (outY < 0 || outY >= DH) continue; // vertical clip

    // horizontal clipping on destination
    int outX = dstX + x0;
    int skipL = 0;
    if (outX < 0) {
      skipL = -outX;
      outX = 0;
    }
    int maxW = DW - outX;
    if (maxW <= 0) continue;
    if (spanW > maxW) spanW = maxW;

    // source row pointer (advance by clipped left skip)
    const uint16_t* rowData = srcPtr + (y * W + x0 + skipL);

    // draw one contiguous RGB565 span (no swap!)
    if (spanW > 0)
      dst.pushImage(outX, outY, spanW, 1, rowData);
  }

  // Restore previous swap state
  dst.setSwapBytes(prevSwapDst);
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 0);

  bool ffatOk = FFat.begin(true);
  Serial.println(ffatOk ? "FFat mounted" : "FFat mount FAILED");

  tft.init();
  tft.setRotation(3);
  // Do NOT enable tft.setSwapBytes(true) globally here.
  // Your GUI/JPEG paths manage swap appropriately when needed.

  // Create sprites
  background.setColorDepth(16);
  background.createSprite(320, 240);
  // Leave sprite swap default (false). We only enable swap when pushing raw arrays.
  // background.setSwapBytes(false);

  person.setColorDepth(16);
  person.createSprite(96, 96);
  // person.setSwapBytes(false);

  // (Optional) boot + info pages via your GUI
  gui.bootUP();
  gui.displayHardwareInfo(ffatOk, ffatOk ? FFat.totalBytes() : 0,
                          ffatOk ? FFat.usedBytes() : 0, "FFat");
  gui.fadeUP();
  delay(500);
  gui.setBright(85);

  // 1) Draw background JPEG into background sprite
  background.fillSprite(TFT_BLACK);
  gui.drawJpegToSprite(&background, "/tree.jpg", 0, 0);

  // 2) Draw avatar JPEG into person sprite (will be cropped if not 128x128)
  person.fillSprite(TFT_BLACK);
  gui.drawJpegToSprite(&person, "/ps.jpg", 0, 0);


  blitCircleSpriteToSprite(person, background, avatarX, avatarY);

  // 4) Present the frame
  background.pushSprite(0, 0);
}

void loop() {
    // 1) Draw background JPEG into background sprite
  background.fillSprite(TFT_BLACK);
  gui.drawJpegToSprite(&background, "/tree.jpg", 0, 0);

  // 2) Draw avatar JPEG into person sprite (will be cropped if not 128x128)
  person.fillSprite(TFT_BLACK);
  gui.drawJpegToSprite(&person, "/ps.jpg", 0, 0);
  
  blitCircleSpriteToSprite(person, background, avatarX, avatarY);

  // 4) Present the frame
  background.pushSprite(0, 0);
  avatarX+=100;
  if(avatarX>320){
    avatarX=-100;
  }
}
