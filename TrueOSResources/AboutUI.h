#ifndef ABOUTUI_H
#define ABOUTUI_H

#include <TFT_eSPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <JPEGDecoder.h>
#include <FFat.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"

struct TeamMember {
    String name;
    String role;
    String imagePath;
    String description;
};

class AboutUI {
private:
    TFT_eSprite* sprite;
    AiEsp32RotaryEncoder* encoder;
    TFT_eSPI* tft;   // now comes from outside
    
    TeamMember team[3] = {
        {"Rakib Hasan", "CEO of N.A.M.O.R. Inc.", "/team/rakib.jpg", 
         "Lead innovator & Developer"},
        {"Sahadat Sani", "CTO of N.A.M.O.R. Inc.", "/team/sani.jpg",
         "Co-innovator & Lead Designer"},
        {"A B M Shawkat Ali", "Director of N.A.M.O.R. Inc.", "/team/shawkat.jpg",
         "Co-innovator & core visionary"}
    };
    
    int currentMember = 0;
    
    void drawCircularPhoto(int centerX, int centerY, int radius, const char* imagePath) {
        TFT_eSprite photo = TFT_eSprite(tft);
        photo.setColorDepth(16);
        photo.createSprite(radius * 2, radius * 2);
        photo.fillSprite(TFT_BLACK);
        
        // Try to load JPEG
        if (FFat.exists(imagePath)) {
            File jpegFile = FFat.open(imagePath, FILE_READ);
            if (jpegFile && JpegDec.decodeFsFile(jpegFile)) {
                uint16_t *pImg;
                while (JpegDec.read()) {
                    pImg = JpegDec.pImage;
                    int mcu_x = JpegDec.MCUx * JpegDec.MCUWidth;
                    int mcu_y = JpegDec.MCUy * JpegDec.MCUHeight;
                    if (mcu_x < radius * 2 && mcu_y < radius * 2) {
                        photo.pushImage(mcu_x, mcu_y, JpegDec.MCUWidth, JpegDec.MCUHeight, pImg);
                    }
                }
            }
            jpegFile.close();
        } else {
            // Fallback: colored circle with initials
            uint16_t colors[] = {COLOR_ACCENT, COLOR_SUCCESS, COLOR_WARNING};
            photo.fillCircle(radius, radius, radius - 5, colors[currentMember]);
            photo.setTextColor(TFT_WHITE);
            photo.setTextDatum(MC_DATUM);
            photo.setTextSize(3);
            String initials = String(team[currentMember].name[0]);
            int spacePos = team[currentMember].name.indexOf(' ');
            if (spacePos > 0) {
                initials += String(team[currentMember].name[spacePos + 1]);
            }
            photo.drawString(initials, radius, radius);
        }
        
        // Blit circular photo
        const uint16_t* srcPtr = (const uint16_t*)photo.getPointer();
        for (int y = 0; y < radius * 2; y++) {
            for (int x = 0; x < radius * 2; x++) {
                int dx = x - radius;
                int dy = y - radius;
                if (dx*dx + dy*dy <= (radius-2)*(radius-2)) {
                    sprite->drawPixel(centerX - radius + x, centerY - radius + y, 
                                    srcPtr[y * radius * 2 + x]);
                }
            }
        }
        
        // Border
        sprite->drawCircle(centerX, centerY, radius, COLOR_ACCENT);
        sprite->drawCircle(centerX, centerY, radius + 1, COLOR_ACCENT);
        
        photo.deleteSprite();
    }
    
public:
    AboutUI(TFT_eSprite* spr, AiEsp32RotaryEncoder* enc, TFT_eSPI* tftRef)
        : sprite(spr), encoder(enc), tft(tftRef){}
    
void show() {
        encoder->reset();
        encoder->setBoundaries(0, 2, true);
        encoder->setEncoderValue(currentMember);
        
        bool done = false;
        
        while (!done) {
            // Check for navigation
            if (encoder->encoderChanged()) {
                currentMember = encoder->readEncoder();
            }
            
            // Check for exit
            if (encoder->isEncoderButtonClicked() || digitalRead(BTN_SPECIAL) == LOW) {
                done = true;
                delay(300);
            }
            
            // Draw UI - Business Card Style
            sprite->fillSprite(COLOR_BG);
            
            char subtitle[10];
            sprintf(subtitle, "%d/3", currentMember + 1);
            UIHelper::drawHeader(sprite, "About / Credits", subtitle);
            
            TeamMember* member = &team[currentMember];
            
            // Card background
            int cardY = 70;
            sprite->fillRoundRect(100, cardY, 300, 100, 12, COLOR_CARD);
            sprite->drawRoundRect(100, cardY, 300, 100, 12, COLOR_ACCENT);
            
            // Photo on left (60x60 circle)
            int photoX = 70;
            int photoY = cardY + 50;
            drawCircularPhoto(photoX, photoY, 60, member->imagePath.c_str());
            
            // Text on right
            int textX = 120;
            
            sprite->setTextColor(COLOR_TEXT);
            sprite->setTextDatum(TL_DATUM);
            sprite->setTextSize(1);
            sprite->drawString(member->name, textX+50, cardY + 20, 2);
            
            sprite->setTextColor(COLOR_ACCENT);
            sprite->drawString(member->role, textX+25, cardY + 45, 2);
            
            sprite->setTextColor(COLOR_TEXT_DIM);
            sprite->drawString(member->description, textX+20, cardY + 70, 1);
            
            // Navigation hint
            UIHelper::drawFooter(sprite, "Rotate: Next  |  Press: Exit");
            
            sprite->pushSprite(0, 0);
            delay(10);
        }
    }
};

#endif