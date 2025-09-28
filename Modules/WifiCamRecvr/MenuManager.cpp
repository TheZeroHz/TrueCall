// MenuManager.cpp
#include "MenuManager.h"

MenuManager::MenuManager() : display(nullptr), selectedIndex(0) {
    items[0] = {"Watch", MODE_WATCH};
    items[1] = {"Camera", MODE_CAMERA};
    items[2] = {"Music", MODE_WATCH};
    items[3] = {"Gallery", MODE_WATCH};
    items[4] = {"Settings", MODE_WATCH};
    items[5] = {"Games", MODE_WATCH};
}

void MenuManager::begin() {
    // Menu initialization if needed
}

void MenuManager::draw() {
    if (!display) return;
    
    TFT_eSprite* sprite = display->getMainSprite();
    sprite->fillSprite(COLOR_BG_DARK);
    
    sprite->setTextDatum(MC_DATUM);
    sprite->setTextColor(COLOR_ACCENT);
    sprite->setTextSize(2);
    sprite->drawString("MENU", DISPLAY_WIDTH/2, 30);
    
    int itemY = 70;
    for (int i = 0; i < MENU_COUNT; i++) {
        if (i == selectedIndex) {
            display->drawRoundRect(sprite, 60, itemY - 15, 200, 30, 5, COLOR_ACCENT);
            sprite->setTextColor(COLOR_BG_DARK);
        } else {
            sprite->setTextColor(COLOR_TEXT_PRIMARY);
        }
        
        sprite->setTextSize(1);
        sprite->drawString(items[i].label, DISPLAY_WIDTH/2, itemY);
        itemY += 35;
        
        if (itemY > DISPLAY_HEIGHT - 20) break;
    }
    
    sprite->setTextColor(COLOR_TEXT_SECONDARY);
    sprite->setTextSize(1);
    sprite->drawString("Use: next/prev/select", DISPLAY_WIDTH/2, DISPLAY_HEIGHT - 10);
    
    display->pushSprite();
}

void MenuManager::navigateNext() {
    selectedIndex = (selectedIndex + 1) % MENU_COUNT;
    Serial.println("Menu: " + items[selectedIndex].label);
}

void MenuManager::navigatePrev() {
    selectedIndex--;
    if (selectedIndex < 0) selectedIndex = MENU_COUNT - 1;
    Serial.println("Menu: " + items[selectedIndex].label);
}

SystemMode MenuManager::selectCurrent() {
    Serial.println("Selected: " + items[selectedIndex].label);
    return items[selectedIndex].mode;
}

String MenuManager::getCurrentLabel() const {
    return items[selectedIndex].label;
}