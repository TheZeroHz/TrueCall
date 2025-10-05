// ButtonHandle.h - Non-blocking, lightweight button handler
#ifndef BUTTONHANDLE_H
#define BUTTONHANDLE_H

#include <Arduino.h>
#include "Config.h"

class ButtonHandle {
private:
    // Simple state tracking - no blocking
    struct ButtonState {
        bool currentState = HIGH;
        bool lastState = HIGH;
        unsigned long lastChangeTime = 0;
        unsigned long pressStartTime = 0;
        bool wasPressed = false;
    };
    
    ButtonState scan, shift, del, special, select;
    const unsigned long DEBOUNCE_TIME = 30; // Reduced to 30ms for faster response
    
    // Fast non-blocking check
    inline void updateButton(int pin, ButtonState &btn) {
        bool reading = digitalRead(pin);
        
        // Simple debounce: only update if stable for DEBOUNCE_TIME
        if (reading != btn.lastState) {
            btn.lastChangeTime = millis();
            btn.lastState = reading;
        }
        
        if ((millis() - btn.lastChangeTime) > DEBOUNCE_TIME) {
            if (reading != btn.currentState) {
                btn.currentState = reading;
                
                if (btn.currentState == LOW) {
                    btn.pressStartTime = millis();
                    btn.wasPressed = true;
                }
            }
        }
    }
    
public:
    ButtonHandle() {}
    
    void begin() {
        pinMode(BTN_SCAN, INPUT_PULLUP);
        pinMode(BTN_SHIFT, INPUT_PULLUP);
        pinMode(BTN_DEL, INPUT_PULLUP);
        pinMode(BTN_SPECIAL, INPUT_PULLUP);
        pinMode(BTN_SELECT, INPUT_PULLUP);
        Serial.println("[ButtonHandle] Non-blocking initialized");
    }
    
    // MUST call this frequently - but it's super fast (no blocking)
    inline void update() {
        updateButton(BTN_SCAN, scan);
        updateButton(BTN_SHIFT, shift);
        updateButton(BTN_DEL, del);
        updateButton(BTN_SPECIAL, special);
        updateButton(BTN_SELECT, select);
    }
    
    // Fast inline functions for instant response
    inline bool scanPressed() { 
        if (scan.wasPressed && scan.currentState == LOW) {
            scan.wasPressed = false;
            return true;
        }
        return false;
    }
    
    inline bool shiftPressed() { 
        if (shift.wasPressed && shift.currentState == LOW) {
            shift.wasPressed = false;
            return true;
        }
        return false;
    }
    
    inline bool delPressed() { 
        if (del.wasPressed && del.currentState == LOW) {
            del.wasPressed = false;
            return true;
        }
        return false;
    }
    
    inline bool specialPressed() { 
        if (special.wasPressed && special.currentState == LOW) {
            special.wasPressed = false;
            return true;
        }
        return false;
    }
    
    inline bool selectPressed() { 
        if (select.wasPressed && select.currentState == LOW) {
            select.wasPressed = false;
            return true;
        }
        return false;
    }
    
    // Instant state check (no delay)
    inline bool scanHeld() { return scan.currentState == LOW; }
    inline bool shiftHeld() { return shift.currentState == LOW; }
    inline bool delHeld() { return del.currentState == LOW; }
    inline bool specialHeld() { return special.currentState == LOW; }
    inline bool selectHeld() { return select.currentState == LOW; }
    
    // Duration tracking
    inline unsigned long scanDuration() { 
        return scan.currentState == LOW ? (millis() - scan.pressStartTime) : 0; 
    }
    
    inline unsigned long shiftDuration() { 
        return shift.currentState == LOW ? (millis() - shift.pressStartTime) : 0; 
    }
    
    inline unsigned long delDuration() { 
        return del.currentState == LOW ? (millis() - del.pressStartTime) : 0; 
    }
    
    inline unsigned long specialDuration() { 
        return special.currentState == LOW ? (millis() - special.pressStartTime) : 0; 
    }
    
    inline unsigned long selectDuration() { 
        return select.currentState == LOW ? (millis() - select.pressStartTime) : 0; 
    }
};

#endif