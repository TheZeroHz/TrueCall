// KeyboardUI.h - Fixed button mappings and text input
#ifndef KEYBOARDUI_H
#define KEYBOARDUI_H

#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"

class KeyboardUI {
private:
    TFT_eSprite* sprite;
    AiEsp32RotaryEncoder* encoder;
    int lastEncoderPos;
    
    // 3 pages of keyboard layouts
    const char* keysPage0[3][10] = {
        {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
        {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
        {"a", "s", "d", "f", "g", "h", "j", "k", "l", "-"}
    };
    
    const char* keysPage1[3][10] = {
        {"z", "x", "c", "v", "b", "n", "m", ".", "_", " "},
        {"@", "#", "$", "%", "&", "*", "(", ")", "=", "+"},
        {"[", "]", "{", "}", "<", ">", "/", "\\", "|", ":"}
    };
    
    const char* keysPage2[3][10] = {
        {"!", "^", "~", "`", ";", "'", "\"", ",", "?", " "},
        {" ", " ", " ", " ", " ", " ", " ", " ", " ", " "},
        {" ", " ", " ", " ", " ", " ", " ", " ", " ", " "}
    };
    
    int selectedRow;
    int selectedCol;
    int currentPage;
    bool shiftMode;
    String inputText;
    
    // Button debouncing
    unsigned long lastShiftPress = 0;
    unsigned long lastDelPress = 0;
    unsigned long lastSpecialPress = 0;
    unsigned long lastPagePress = 0;
    unsigned long lastSelectPress = 0;
    
    bool lastShiftState = HIGH;
    bool lastDelState = HIGH;
    bool lastSpecialState = HIGH;
    bool lastPageState = HIGH;
    bool lastSelectState = HIGH;
    
    const unsigned long DEBOUNCE_MS = BUTTON_DEBOUNCE_MS;
    
    int keyboardStartY = 85;
    int inputFieldY = 45;
    
    void drawHeader(const String& title) {
        // Transparent gradient header
        for (int i = 0; i < HEADER_HEIGHT; i++) {
            uint8_t brightness = 8 + (i / 3);
            uint16_t color = sprite->color565(brightness, brightness * 2, brightness * 4);
            sprite->drawFastHLine(0, i, SCREEN_WIDTH, color);
        }
        
        sprite->setTextColor(COLOR_TEXT);
        sprite->setTextDatum(MC_DATUM);
        sprite->drawString(title, SCREEN_WIDTH / 2, HEADER_HEIGHT / 2, 2);
        sprite->drawFastHLine(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, COLOR_ACCENT);
    }
    
    void drawInputField() {
        sprite->fillRoundRect(5, inputFieldY, SCREEN_WIDTH - 10, 32, 6, COLOR_CARD);
        sprite->drawRoundRect(5, inputFieldY, SCREEN_WIDTH - 10, 32, 6, COLOR_ACCENT);
        
        sprite->setTextColor(COLOR_TEXT, COLOR_CARD);
        sprite->setTextDatum(ML_DATUM);
        
        String displayText = inputText;
        if (displayText.length() > 30) {
            displayText = displayText.substring(displayText.length() - 30);
        }
        
        sprite->drawString(displayText, 10, inputFieldY + 16, 2);
        
        // Cursor blink
        if (millis() % 1000 < 500) {
            int cursorX = 10 + sprite->textWidth(displayText, 2);
            sprite->drawFastVLine(cursorX + 2, inputFieldY + 8, 16, COLOR_ACCENT);
        }
    }
    
    void drawKey(int row, int col, bool selected) {
        const char* key = nullptr;
        
        // Get correct key based on page
        if (currentPage == 0) {
            key = keysPage0[row][col];
        } else if (currentPage == 1) {
            key = keysPage1[row][col];
        } else {
            key = keysPage2[row][col];
        }
        
        if (strlen(key) == 0 || strcmp(key, " ") == 0) {
            return;
        }
        
        int x = 5 + (col * (KEY_WIDTH + KEY_SPACING));
        int y = keyboardStartY + (row * (KEY_HEIGHT + KEY_SPACING));
        
        uint16_t bgColor = selected ? COLOR_KEY_ACTIVE : COLOR_KEY_BG;
        sprite->fillRoundRect(x, y, KEY_WIDTH, KEY_HEIGHT, 4, bgColor);
        
        if (selected) {
            sprite->drawRoundRect(x, y, KEY_WIDTH, KEY_HEIGHT, 4, COLOR_ACCENT);
            sprite->drawRoundRect(x + 1, y + 1, KEY_WIDTH - 2, KEY_HEIGHT - 2, 4, COLOR_ACCENT);
        } else {
            sprite->drawRoundRect(x, y, KEY_WIDTH, KEY_HEIGHT, 4, COLOR_KEY_BORDER);
        }
        
        sprite->setTextColor(COLOR_TEXT, bgColor);
        sprite->setTextDatum(MC_DATUM);
        
        String displayKey = String(key);
        
        // Apply shift to letters
        if (shiftMode && displayKey.length() == 1) {
            char c = displayKey[0];
            if (c >= 'a' && c <= 'z') {
                displayKey = String((char)(c - 32));
            }
        }
        
        sprite->drawString(displayKey, x + KEY_WIDTH / 2, y + KEY_HEIGHT / 2, 2);
    }
    
    void drawButtonHints() {
        int hintY = SCREEN_HEIGHT - 40;
        sprite->fillRect(0, hintY, SCREEN_WIDTH, 40, COLOR_HEADER);
        sprite->drawFastHLine(0, hintY, SCREEN_WIDTH, COLOR_ACCENT);
        
        sprite->setTextColor(COLOR_TEXT_DIM, COLOR_HEADER);
        sprite->setTextDatum(TL_DATUM);
        
        int x = 3;
        
        // POWER = Shift
        if (shiftMode) {
            sprite->fillCircle(x + 3, hintY + 9, 3, COLOR_ACCENT);
        }
        sprite->drawString("PWR", x, hintY + 3, 1);
        sprite->drawString("Shift", x, hintY + 14, 1);
        
        x += 45;
        // CAMERA = Del
        sprite->drawString("CAM", x, hintY + 3, 1);
        sprite->drawString("Del", x, hintY + 14, 1);
        
        x += 45;
        // MIC = Special chars
        sprite->drawString("MIC", x, hintY + 3, 1);
        sprite->drawString("Spec", x, hintY + 14, 1);
        
        x += 45;
        // HOME = Page
        sprite->setTextColor(COLOR_ACCENT, COLOR_HEADER);
        sprite->drawString("HOME", x, hintY + 3, 1);
        sprite->setTextColor(COLOR_TEXT_DIM, COLOR_HEADER);
        char pageStr[8];
        snprintf(pageStr, sizeof(pageStr), "Pg %d/3", currentPage + 1);
        sprite->drawString(pageStr, x, hintY + 14, 1);
        
        x += 55;
        // CAST = Select char
        sprite->drawString("CAST", x, hintY + 3, 1);
        sprite->drawString("Type", x, hintY + 14, 1);
        
        x += 50;
        // ENCODER = Done
        sprite->setTextColor(COLOR_ACCENT, COLOR_HEADER);
        sprite->drawString("PUSH", x, hintY + 3, 1);
        sprite->setTextColor(COLOR_TEXT_DIM, COLOR_HEADER);
        sprite->drawString("Done", x, hintY + 14, 1);
    }
    
    bool checkButtonPress(int pin, bool& lastState, unsigned long& lastPress) {
        bool currentState = digitalRead(pin);
        
        if (currentState == LOW && lastState == HIGH) {
            if (millis() - lastPress > DEBOUNCE_MS) {
                lastPress = millis();
                lastState = currentState;
                return true;
            }
        }
        
        lastState = currentState;
        return false;
    }
    
public:
    KeyboardUI(TFT_eSprite* spr, AiEsp32RotaryEncoder* enc) 
        : sprite(spr), encoder(enc),
          selectedRow(0), selectedCol(0), currentPage(0), shiftMode(false),
          inputText(""), lastEncoderPos(0) {}
    
    void begin() {
        pinMode(BTN_SCAN, INPUT_PULLUP);    // POWER = Shift
        pinMode(BTN_DEL, INPUT_PULLUP);     // CAMERA = Del
        pinMode(BTN_SHIFT, INPUT_PULLUP);   // MIC = Special
        pinMode(BTN_SPECIAL, INPUT_PULLUP); // HOME = Page
        pinMode(BTN_SELECT, INPUT_PULLUP);  // CAST = Select char
    }
    
    String show(const String& title) {
        inputText = "";
        selectedRow = 1;
        selectedCol = 0;
        currentPage = 0;
        shiftMode = false;
        
        encoder->reset();
        encoder->setBoundaries(0, 29, true);
        encoder->setAcceleration(50);
        
        bool done = false;
        unsigned long lastUpdate = 0;
        
        Serial.println("\n=== Keyboard Active ===");
        Serial.println("Controls:");
        Serial.println("  POWER: Shift/Caps");
        Serial.println("  CAMERA: Delete");
        Serial.println("  MIC: Special chars");
        Serial.println("  HOME: Change page");
        Serial.println("  CAST: Type character");
        Serial.println("  ENCODER PUSH: Done");
        
        while (!done) {
            // POWER button = Shift
            if (checkButtonPress(BTN_SCAN, lastShiftState, lastShiftPress)) {
                shiftMode = !shiftMode;
                Serial.printf("Shift: %s\n", shiftMode ? "ON" : "OFF");
            }
            
            // CAMERA button = Delete
            if (checkButtonPress(BTN_DEL, lastDelState, lastDelPress)) {
                if (inputText.length() > 0) {
                    inputText = inputText.substring(0, inputText.length() - 1);
                    Serial.printf("Delete (text now: '%s')\n", inputText.c_str());
                }
            }
            
            // MIC button = Special chars (page 2)
            if (checkButtonPress(BTN_SHIFT, lastSpecialState, lastSpecialPress)) {
                currentPage = 2;
                Serial.println("Switched to special chars (page 3)");
            }
            
            // HOME button = Cycle pages
            if (checkButtonPress(BTN_SPECIAL, lastPageState, lastPagePress)) {
                currentPage = (currentPage + 1) % 3;
                Serial.printf("Page changed to %d\n", currentPage + 1);
            }
            
            // CAST button = Type selected character
            if (checkButtonPress(BTN_SELECT, lastSelectState, lastSelectPress)) {
                const char* key = nullptr;
                
                if (currentPage == 0) {
                    key = keysPage0[selectedRow][selectedCol];
                } else if (currentPage == 1) {
                    key = keysPage1[selectedRow][selectedCol];
                } else {
                    key = keysPage2[selectedRow][selectedCol];
                }
                
                if (strlen(key) > 0 && strcmp(key, " ") != 0) {
                    if (inputText.length() < MAX_PASSWORD_LENGTH) {
                        String keyStr = String(key);
                        
                        if (shiftMode && keyStr.length() == 1) {
                            char c = keyStr[0];
                            if (c >= 'a' && c <= 'z') {
                                keyStr = String((char)(c - 32));
                                shiftMode = false;
                            }
                        }
                        
                        inputText += keyStr;
                        Serial.printf("Typed '%s' (text: '%s')\n", keyStr.c_str(), inputText.c_str());
                    }
                }
            }
            
            // Encoder rotation
            if (encoder->encoderChanged()) {
                int pos = encoder->readEncoder();
                
                // Play tick on rotation (safe null check)
                if (pos != lastEncoderPos) {
                    //play tick sound
                    lastEncoderPos = pos;
                }
                
                selectedRow = pos / KEYBOARD_COLS;
                selectedCol = pos % KEYBOARD_COLS;
            }
            
            // Encoder button = Done
            if (encoder->isEncoderButtonClicked()) {
                done = true;
                Serial.printf("Done! Password: '%s' (%d chars)\n", 
                    inputText.c_str(), inputText.length());
                delay(200);
            }
            
            // Redraw at 30fps
            if (millis() - lastUpdate > 33) {
                sprite->fillSprite(COLOR_BG);
                
                drawHeader(title);
                drawInputField();
                
                for (int row = 0; row < KEYBOARD_ROWS; row++) {
                    for (int col = 0; col < KEYBOARD_COLS; col++) {
                        bool selected = (row == selectedRow && col == selectedCol);
                        drawKey(row, col, selected);
                    }
                }
                
                drawButtonHints();
                
                sprite->pushSprite(0, 0);
                lastUpdate = millis();
            }
            
            delay(1);
        }
        
        return inputText;
    }
};

#endif