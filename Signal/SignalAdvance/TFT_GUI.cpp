#include "TFT_GUI.h"

// ---------------- Backlight ----------------
void TFT_GUI::setBright(int percentage){
  int val = map(percentage, 0, 100, 0, 255);
  analogWrite(TFT_BL, val);
}

void TFT_GUI::off(){
  digitalWrite(TFT_BL, LOW);
}

void TFT_GUI::fadeUP(){ for(int i=0;i<=255;i++){ analogWrite(TFT_BL,i); delay(4);} }
void TFT_GUI::fadeDown(){ for(int i=255;i>=0;i--){ analogWrite(TFT_BL,i); delay(4);} }

// ---------------- Constructor ----------------
TFT_GUI::TFT_GUI(TFT_eSPI& tftInstance, int screenWidth, int screenHeight, int itemHeight, int animationDuration)
  : tft(tftInstance),
    spr(&tft),
    person(&tft),
    clock(&tft),
    SCREEN_WIDTH(screenWidth),
    SCREEN_HEIGHT(screenHeight),
    ITEM_HEIGHT(itemHeight),
    animationDuration(animationDuration),
    selectedItem(0),
    itemCount(0),
    menuItems(nullptr),
    animationStartTime(0),
    isAnimating(false),
    fromItem(0),
    toItem(0),
    // Circular menu defaults
    circularMenuItems(nullptr),
    circularMenuItemCount(0),
    selectedCircularItem(0),
    circularCenterX(screenWidth/2),
    circularCenterY(screenHeight/2),
    circularMenuRadius(80),
    avatarSize(64),
    circularMenuAnimating(false),
    circularAnimationStart(0),
    circularAnimationDuration(300),
    circularFromItem(0),
    circularToItem(0)
{
  pinMode(TFT_BL, OUTPUT);

  spr.setColorDepth(16);
  spr.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  spr.setSwapBytes(true);
}

// ---------------- Circular Menu Implementation ----------------

void TFT_GUI::initCircularMenu(CircularMenuItem* items, int itemCount, int centerX, int centerY, int radius, int avatarSize) {
  this->circularMenuItems = items;
  this->circularMenuItemCount = itemCount;
  this->circularCenterX = centerX;
  this->circularCenterY = centerY;
  this->circularMenuRadius = radius;
  this->avatarSize = avatarSize;
  this->selectedCircularItem = 0;
  
  calculateCircularPositions();
}

void TFT_GUI::setCircularMenuItems(CircularMenuItem* items, int itemCount) {
  this->circularMenuItems = items;
  this->circularMenuItemCount = itemCount;
  this->selectedCircularItem = 0;
  calculateCircularPositions();
}

void TFT_GUI::calculateCircularPositions() {
  if (!circularMenuItems || circularMenuItemCount == 0) return;
  
  for (int i = 0; i < circularMenuItemCount; i++) {
    float angle = (2.0 * PI * i) / circularMenuItemCount - PI/2; // Start from top
    circularMenuItems[i].angle = angle;
    circularMenuItems[i].x = circularCenterX + cos(angle) * circularMenuRadius - avatarSize/2;
    circularMenuItems[i].y = circularCenterY + sin(angle) * circularMenuRadius - avatarSize/2;
  }
}

void TFT_GUI::selectCircularMenuItem(int index) {
  if (index >= 0 && index < circularMenuItemCount) {
    selectedCircularItem = index;
  }
}

void TFT_GUI::navigateCircularMenu(int direction) {
  if (circularMenuAnimating) return; // Don't navigate during animation
  
  int newIndex = selectedCircularItem + direction;
  if (newIndex < 0) newIndex = circularMenuItemCount - 1;
  if (newIndex >= circularMenuItemCount) newIndex = 0;
  
  animateToCircularMenuItem(newIndex);
}

void TFT_GUI::animateToCircularMenuItem(int targetIndex, int animationTimeMs) {
  if (targetIndex == selectedCircularItem || circularMenuAnimating) return;
  
  circularFromItem = selectedCircularItem;
  circularToItem = targetIndex;
  circularAnimationStart = millis();
  circularAnimationDuration = animationTimeMs;
  circularMenuAnimating = true;
}

void TFT_GUI::updateCircularMenuAnimation() {
  if (!circularMenuAnimating) return;
  
  unsigned long elapsed = millis() - circularAnimationStart;
  if (elapsed >= (unsigned long)circularAnimationDuration) {
    // Animation complete
    circularMenuAnimating = false;
    selectedCircularItem = circularToItem;
    drawCircularMenu();
  } else {
    // Animation in progress
    float progress = (float)elapsed / circularAnimationDuration;
    // Smooth easing function
    progress = progress * progress * (3.0f - 2.0f * progress);
    drawCircularMenu();
  }
}

void TFT_GUI::drawCircularMenu() {
  if (!circularMenuItems || circularMenuItemCount == 0) return;
  
  // Clear the main sprite
  spr.fillSprite(TFT_BLACK);
  
  // Draw background
  if (!drawJpegToSprite(&spr, BG_PATH, 0, 0)) {
    // Fallback gradient background
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
      uint16_t color = tft.color565(0, 0, (100 * y) / SCREEN_HEIGHT);
      spr.drawFastHLine(0, y, SCREEN_WIDTH, color);
    }
  }
  
  // Create temporary sprite for avatars
  TFT_eSprite avatar = TFT_eSprite(&tft);
  avatar.setColorDepth(16);
  avatar.createSprite(avatarSize, avatarSize);
  
  // Draw each menu item
  for (int i = 0; i < circularMenuItemCount; i++) {
    bool isSelected = (i == selectedCircularItem);
    float animProgress = 0.0f;
    
    // Handle animation
    if (circularMenuAnimating) {
      if (i == circularFromItem || i == circularToItem) {
        unsigned long elapsed = millis() - circularAnimationStart;
        animProgress = (float)elapsed / circularAnimationDuration;
        animProgress = constrain(animProgress, 0.0f, 1.0f);
        // Smooth easing
        animProgress = animProgress * animProgress * (3.0f - 2.0f * animProgress);
      }
    }
    
    // Calculate scale and alpha for selection effect
    float scale = isSelected ? 1.1f : 1.0f;
    if (circularMenuAnimating) {
      if (i == circularToItem) {
        scale = 1.0f + 0.1f * animProgress;
      } else if (i == circularFromItem) {
        scale = 1.1f - 0.1f * animProgress;
      }
    }
    
    // Clear avatar sprite
    avatar.fillSprite(TFT_BLACK);
    
    // Try to load image, fallback to generated avatar
    if (!drawJpegToSprite(&avatar, circularMenuItems[i].avatarPath.c_str(), 0, 0)) {
      // Generate a colored circular avatar with the first letter
      uint16_t color = circularMenuItems[i].color != 0 ? 
                       circularMenuItems[i].color : 
                       tft.color565(100 + i * 30, 150, 255 - i * 25);
      
      avatar.fillCircle(avatarSize/2, avatarSize/2, (avatarSize/2) - 2, color);
      avatar.setTextColor(TFT_WHITE);
      avatar.setTextDatum(MC_DATUM);
      avatar.setTextSize(2);
      avatar.drawString(String(circularMenuItems[i].label[0]), avatarSize/2, avatarSize/2);
    }
    
    // Draw selection border
    if (isSelected || (circularMenuAnimating && (i == circularFromItem || i == circularToItem))) {
      int borderRadius = (avatarSize/2) + 5;
      int borderX = circularMenuItems[i].x + avatarSize/2;
      int borderY = circularMenuItems[i].y + avatarSize/2;
      
      uint16_t borderColor = TFT_GREEN;
      int borderThickness = 3;
      
      // Animate border for transitions
      if (circularMenuAnimating) {
        if (i == circularToItem) {
          borderThickness = (int)(3 * animProgress);
        } else if (i == circularFromItem) {
          borderThickness = (int)(3 * (1.0f - animProgress));
        }
      }
      
      drawCircularBorder(spr, borderX, borderY, borderRadius, borderColor, borderThickness);
      
      // Add glow effect
      drawCircularBorder(spr, borderX, borderY, borderRadius + 3, tft.color565(0, 150, 0), 1);
    }
    
    // Blit circular avatar to main sprite
    blitCircleSprite(avatar, spr, circularMenuItems[i].x, circularMenuItems[i].y);
    
    // Draw text label
    int textX = circularMenuItems[i].x + avatarSize/2;
    int textY = circularMenuItems[i].y + avatarSize + 15;
    
    uint16_t textColor = isSelected ? TFT_GREEN : TFT_WHITE;
    if (circularMenuAnimating) {
      if (i == circularToItem) {
        // Fade in green
        int greenComponent = (int)(255 * animProgress);
        textColor = tft.color565(0, greenComponent, 0);
      } else if (i == circularFromItem) {
        // Fade from green to white
        int greenComponent = (int)(255 * (1.0f - animProgress));
        textColor = tft.color565(255 - greenComponent, 255, 255 - greenComponent);
      }
    }
    
    drawTextWithOutline(spr, circularMenuItems[i].label, textX, textY, textColor, TFT_BLACK, 1);
  }
  
  // Draw center title
  drawTextWithOutline(spr, "MENU", circularCenterX, circularCenterY, TFT_CYAN, TFT_BLACK, 2);
  
  // Cleanup
  avatar.deleteSprite();
  
  // Display final result
  spr.pushSprite(0, 0);
}

// ---------------- Enhanced Drawing Utilities ----------------

void TFT_GUI::drawCircularBorder(TFT_eSprite& sprite, int centerX, int centerY, int radius, uint16_t color, int thickness) {
  for (int t = 0; t < thickness; t++) {
    sprite.drawCircle(centerX, centerY, radius + t, color);
  }
}

void TFT_GUI::blitCircleSprite(TFT_eSprite& src, TFT_eSprite& dst, int dstX, int dstY) {
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

void TFT_GUI::drawTextWithOutline(TFT_eSprite& sprite, const String& text, int x, int y, uint16_t textColor, uint16_t outlineColor, int textSize) {
  sprite.setTextSize(textSize);
  sprite.setTextDatum(MC_DATUM);
  
  // Draw outline by drawing text in all 8 directions
  sprite.setTextColor(outlineColor);
  for (int dx = -1; dx <= 1; dx++) {
    for (int dy = -1; dy <= 1; dy++) {
      if (dx != 0 || dy != 0) {
        sprite.drawString(text, x + dx, y + dy);
      }
    }
  }
  
  // Draw main text
  sprite.setTextColor(textColor);
  sprite.drawString(text, x, y);
}

// ---------------- Original Menu API (kept for compatibility) ----------------
void TFT_GUI::setItems(String items[], int itemCount) {
  this->itemCount = itemCount;
  menuItems = new String[itemCount];
  for (int i = 0; i < itemCount; i++) menuItems[i] = items[i];
  drawMenu();
}

void TFT_GUI::setSelectedItem(int index) {
  tft.setTextFont(4);
  selectedItem = index;
  drawMenu();
}

void TFT_GUI::drawGradientBackground() {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    uint16_t color = tft.color565(0, 0, (255 * y) / SCREEN_HEIGHT);
    spr.drawFastHLine(0, y, SCREEN_WIDTH, color);
  }
}

void TFT_GUI::drawMenuItem(String item, int y, bool selected, int textSize, float fadeProgress) {
  spr.setTextSize(textSize);
  uint16_t textColor = selected ? tft.color565(map(fadeProgress, 0, 1, 255, 0),
                                               map(fadeProgress, 0, 1, 255, 0),
                                               map(fadeProgress, 0, 1, 255, 0))
                                : TFT_CYAN;
  spr.setTextColor(textColor);
  spr.setTextDatum(MC_DATUM);
  spr.drawString(item, SCREEN_WIDTH / 2, y);
}

void TFT_GUI::drawMenu() {
  spr.fillSprite(TFT_BLACK);
  drawGradientBackground();
  for (int i = 0; i < itemCount; i++) {
    int y = SCREEN_HEIGHT / 2 + (i - selectedItem) * ITEM_HEIGHT;
    int textSize = (i == selectedItem) ? 2 : 1;
    drawMenuItem(menuItems[i], y, i == selectedItem, textSize, 0.0f);
  }
  spr.pushSprite(0, 0);
}

void TFT_GUI::startMenuTransition(int from, int to) {
  fromItem = from;
  toItem = to;
  animationStartTime = millis();
  isAnimating = true;
}

void TFT_GUI::updateMenuTransition() {
  unsigned long currentTime = millis();
  if (currentTime - animationStartTime >= (unsigned long)animationDuration) {
    isAnimating = false;
    selectedItem = toItem;
    drawMenu();
  } else {
    float progress = (float)(currentTime - animationStartTime) / animationDuration;
    spr.fillSprite(TFT_BLACK);
    drawGradientBackground();
    for (int i = 0; i < itemCount; i++) {
      int yFrom = SCREEN_HEIGHT / 2 + (i - fromItem) * ITEM_HEIGHT;
      int yTo   = SCREEN_HEIGHT / 2 + (i - toItem) * ITEM_HEIGHT;
      int y     = yFrom + (int)((yTo - yFrom) * progress);
      int textSize = (i == toItem) ? 2 : 1;
      drawMenuItem(menuItems[i], y, i == toItem, textSize, progress);
    }
    spr.pushSprite(0, 0);
  }
}

void TFT_GUI::travarse(int pre,int select) {
  static unsigned long previousTime = millis();
  unsigned long currentTime = millis();
  if (!isAnimating && (currentTime - previousTime >= 1000)) {
    previousTime = currentTime;
    startMenuTransition(pre, select);
  }
  if (isAnimating) updateMenuTransition();
}

// ---------------- System Info ----------------
void TFT_GUI::displayHardwareInfo(bool sdMounted, uint64_t sdTotalSize, uint64_t sdUsedSpace, const char* sdType) {
  if (sdMounted) tft.fillScreen(TFT_BLUE);
  else tft.fillScreen(TFT_RED);

  int centerX = tft.width() / 2;
  int cursorY = 20;
  tft.setCursor(40, cursorY+5);
  tft.setTextColor(TFT_BLUE, TFT_WHITE);
  tft.setTextSize(2);
  tft.println("[Boot Loader]");
  tft.setTextColor(TFT_WHITE);

  tft.setTextSize(1);
  cursorY += 30;

  int col1X = 40;
  int col2X = 130;
  int rowSpacing = 15;
  int rowStartY = cursorY;
  int numRows = 9 + (sdMounted ? 4 : 1);

  tft.setCursor(col1X, cursorY); tft.printf("OS Version:");
  tft.setCursor(col2X, cursorY); tft.printf("%s", "3.21");
  cursorY += rowSpacing;

  tft.setCursor(col1X, cursorY); tft.printf("Release Type:");
  tft.setCursor(col2X, cursorY); tft.printf("%s", "Beta");
  cursorY += rowSpacing;

  tft.setCursor(col1X, cursorY); tft.printf("SDK Version:");
  tft.setCursor(col2X, cursorY); tft.printf("%s", ESP.getSdkVersion());
  cursorY += rowSpacing;

  tft.setCursor(col1X, cursorY); tft.printf("Chip Model:");
  tft.setCursor(col2X, cursorY); tft.printf("%s", ESP.getChipModel());
  cursorY += rowSpacing;

  tft.setCursor(col1X, cursorY); tft.printf("CPU Core:");
  tft.setCursor(col2X, cursorY); tft.printf("%d", ESP.getChipCores());
  cursorY += rowSpacing;

  tft.setCursor(col1X, cursorY); tft.printf("CPU Freq:");
  tft.setCursor(col2X, cursorY); tft.printf("%d MHz", ESP.getCpuFreqMHz());
  cursorY += rowSpacing;

  tft.setCursor(col1X, cursorY); tft.printf("Flash Size:");
  tft.setCursor(col2X, cursorY); tft.printf("%d MB", ESP.getFlashChipSize() / (1024 * 1024));
  cursorY += rowSpacing;

  tft.setCursor(col1X, cursorY); tft.printf("Heap Size:");
  tft.setCursor(col2X, cursorY); tft.printf("%d KB", ESP.getHeapSize() / 1024);
  cursorY += rowSpacing;

  tft.setCursor(col1X, cursorY); tft.printf("Free Heap:");
  tft.setCursor(col2X, cursorY); tft.printf("%d KB", ESP.getFreeHeap() / 1024);

  if (sdMounted) {
    cursorY += rowSpacing;
    tft.setCursor(col1X, cursorY); tft.printf("FFat:");
    tft.setCursor(col2X, cursorY); tft.printf("Mounted");

    cursorY += rowSpacing;
    tft.setCursor(col1X, cursorY); tft.printf("FS Type:");
    tft.setCursor(col2X, cursorY); tft.printf("%s", sdType);

    cursorY += rowSpacing;
    tft.setCursor(col1X, cursorY); tft.printf("Total Size:");
    tft.setCursor(col2X, cursorY); tft.printf("%llu MB", sdTotalSize / (1024ULL * 1024ULL));

    cursorY += rowSpacing;
    tft.setCursor(col1X, cursorY); tft.printf("Used Space:");
    tft.setCursor(col2X, cursorY); tft.printf("%llu MB", sdUsedSpace / (1024ULL * 1024ULL));
  } else {
    cursorY += rowSpacing;
    tft.setCursor(col1X, cursorY); tft.printf("FFat:");
    tft.setCursor(col2X, cursorY); tft.printf("Not mounted");

    cursorY += rowSpacing;
    tft.setTextColor(TFT_BLUE, TFT_WHITE);
    tft.setCursor(centerX - tft.textWidth("<..!--SYSTEM FAIL--!..>") / 2, cursorY);
    tft.println("<..!--SYSTEM FAIL--!..>");
  }

  int tableWidth = tft.width() - 20;
  int tableHeight = rowSpacing * numRows;
  int startX = 5;
  int startY = rowStartY - 5;

  tft.drawRect(startX, startY, tableWidth, tableHeight, TFT_WHITE);
  for (int i = 1; i < numRows; ++i)
    tft.drawLine(startX, startY + i * rowSpacing, startX + tableWidth, startY + i * rowSpacing, TFT_WHITE);
  tft.drawLine(col2X - 5, startY, col2X - 5, startY + tableHeight, TFT_WHITE);

  if (!sdMounted) {
    tft.fillScreen(TFT_RED);
    tft.setCursor(40, 80);
    tft.setTextColor(TFT_BLUE, TFT_WHITE);
    tft.setTextSize(2);
    tft.println("SD FAIL :-(");
  }
}

// ---------------- JPEG to TFT ----------------
void TFT_GUI::drawJpeg(const char *filename, int xpos, int ypos){
  File jpegFile = FFat.open(filename, FILE_READ);
  if (!jpegFile) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println("\" not found!");
    return;
  }
  if (JpegDec.decodeFsFile(jpegFile)) {
    jpegInfo();
    jpegRender(xpos, ypos);
  } else {
    Serial.println("Jpeg file format not supported!");
  }
}

void TFT_GUI::jpegRender(int xpos, int ypos){
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);

  uint32_t min_w = (max_x % mcu_w) ? (max_x % mcu_w) : mcu_w;
  uint32_t min_h = (max_y % mcu_h) ? (max_y % mcu_h) : mcu_h;

  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  uint32_t drawTime = millis();
  max_x += xpos;
  max_y += ypos;

  while (JpegDec.read()) {
    pImg = JpegDec.pImage;

    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    win_w = ((mcu_x + mcu_w) <= (int)max_x) ? mcu_w : min_w;
    win_h = ((mcu_y + mcu_h) <= (int)max_y) ? mcu_h : min_h;

    if (win_w != mcu_w) {
      uint16_t *cImg = pImg + win_w;
      int p = 0;
      for (int h = 1; h < (int)win_h; h++) {
        p += mcu_w;
        for (int w = 0; w < (int)win_w; w++) {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    if ((mcu_x + (int)win_w) <= tft.width() && (mcu_y + (int)win_h) <= tft.height())
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ((mcu_y + (int)win_h) >= tft.height())
      JpegDec.abort();
  }

  tft.setSwapBytes(swapBytes);
  showTime(millis() - drawTime);
}

// ---------------- JPEG to Sprite ----------------
bool TFT_GUI::drawJpegToSprite(TFT_eSprite* dst, const char* filename, int x, int y) {
  if (!dst) return false;

  File jpegFile = FFat.open(filename, FILE_READ);
  if (!jpegFile) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println("\" not found!");
    return false;
  }
  if (!JpegDec.decodeFsFile(jpegFile)) {
    Serial.println("Jpeg file format not supported!");
    return false;
  }
  return jpegRenderToSprite(dst, x, y);
}

bool TFT_GUI::jpegRenderToSprite(TFT_eSprite* dst, int xpos, int ypos) {
  if (!dst) return false;

  uint16_t *pImg;
  const uint16_t mcu_w = JpegDec.MCUWidth;
  const uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  dst->setColorDepth(16);
  bool oldSwap = dst->getSwapBytes();
  dst->setSwapBytes(true);

  const uint32_t min_w = (max_x % mcu_w) ? (max_x % mcu_w) : mcu_w;
  const uint32_t min_h = (max_y % mcu_h) ? (max_y % mcu_h) : mcu_h;

  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  while (JpegDec.read()) {
    pImg = JpegDec.pImage;

    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    win_w = ((mcu_x + mcu_w) <= (int)(xpos + max_x)) ? mcu_w : min_w;
    win_h = ((mcu_y + mcu_h) <= (int)(ypos + max_y)) ? mcu_h : min_h;

    if (mcu_x >= 0 && mcu_y >= 0 &&
        (mcu_x + (int)win_w) <= dst->width() &&
        (mcu_y + (int)win_h) <= dst->height()) {

      if (win_w != mcu_w) {
        uint16_t *cImg = pImg + win_w;
        int p = 0;
        for (int h = 1; h < (int)win_h; h++) {
          p += mcu_w;
          for (int w = 0; w < (int)win_w; w++) {
            *cImg = *(pImg + w + p);
            cImg++;
          }
        }
      }
      dst->pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    } else if ((mcu_y + (int)win_h) >= dst->height()) {
      JpegDec.abort();
    }
  }

  dst->setSwapBytes(oldSwap);
  return true;
}

// ---------------- Boot screens ----------------
void TFT_GUI::bootUP(){
  drawJpeg(BOOT0_PATH, 0, 0);
  fadeUP();
  delay(4000);
  fadeDown();
  drawJpeg(BOOT1_PATH, 0, 0);
  fadeUP();
  delay(4000);
  fadeDown();
}

// ---------------- UI builder ----------------
void TFT_GUI::renderUI() {
  spr.fillSprite(TFT_BLACK);

  if (!drawJpegToSprite(&spr, BG_PATH, 0, 0)) {
    Serial.println("renderUI: failed to load BG_PATH");
    spr.fillSprite(TFT_DARKGREY);
  }

  if (person.width() == 0 || person.height() == 0) {
    person.setColorDepth(16);
    person.createSprite(96, 96);
    person.setSwapBytes(true);
  }
  person.fillSprite(TFT_BLACK);
  person.pushToSprite(&spr, SCREEN_WIDTH - 96 - 8, 8, TFT_BLACK);

  spr.pushSprite(0, 0);
}

// ---------------- Convenience wrappers ----------------
bool TFT_GUI::drawsprjpeg(const char* filename, int x, int y) {
  bool ok = drawJpegToSprite(&spr, filename, x, y);
  if (ok) spr.pushSprite(0, 0);
  return ok;
}

bool TFT_GUI::drawsprjpeg(TFT_eSprite* dst, const char* filename, int x, int y) {
  return drawJpegToSprite(dst, filename, x, y);
}

// ---------------- Debug helpers ----------------
void TFT_GUI::jpegInfo(){
  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print("Width      :");   Serial.println(JpegDec.width);
  Serial.print("Height     :");   Serial.println(JpegDec.height);
  Serial.print("Components :");   Serial.println(JpegDec.comps);
  Serial.print("MCU / row  :");   Serial.println(JpegDec.MCUSPerRow);
  Serial.print("MCU / col  :");   Serial.println(JpegDec.MCUSPerCol);
  Serial.print("Scan type  :");   Serial.println(JpegDec.scanType);
  Serial.print("MCU width  :");   Serial.println(JpegDec.MCUWidth);
  Serial.print("MCU height :");   Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}

void TFT_GUI::showTime(uint32_t msTime){
  Serial.print(F(" JPEG drawn in "));
  Serial.print(msTime);
  Serial.println(F(" ms "));
}