// AIChatUI.h - Professional AI Chat Interface with Scrolling
#ifndef AICHATUI_H
#define AICHATUI_H

#include <TFT_eSPI.h>
#include "Config.h"
#include "Colors.h"
#include "UIHelper.h"
#include "AIAssistant.h"

#define CHAT_BUBBLE_MARGIN 8
#define CHAT_BUBBLE_PADDING 10
#define MAX_BUBBLE_WIDTH 240
#define LINE_HEIGHT 20
#define CHAT_AREA_TOP (HEADER_HEIGHT + 4)
#define CHAT_AREA_BOTTOM (SCREEN_HEIGHT - FOOTER_HEIGHT - 4)
#define CHAT_AREA_HEIGHT (CHAT_AREA_BOTTOM - CHAT_AREA_TOP)

class AIChatUI {
private:
    TFT_eSprite* sprite;
    AIAssistant* assistant;
    int scrollOffset;
    int totalContentHeight;
    int waveformPhase;
    
    struct BubbleInfo {
        int y;
        int height;
        String text;
        bool isUser;
    };
    
    BubbleInfo bubbles[MAX_CHAT_HISTORY];
    int bubbleCount;
    
    // Word wrap helper
    int calculateTextHeight(const String& text, int maxWidth, int fontSize) {
        int lines = 1;
        int currentWidth = 0;
        String word = "";
        
        for (int i = 0; i < text.length(); i++) {
            char c = text[i];
            
            if (c == ' ' || c == '\n' || i == text.length() - 1) {
                if (i == text.length() - 1 && c != ' ' && c != '\n') {
                    word += c;
                }
                
                int wordWidth = sprite->textWidth(word, fontSize);
                
                if (currentWidth + wordWidth > maxWidth) {
                    lines++;
                    currentWidth = wordWidth;
                } else {
                    currentWidth += wordWidth;
                }
                
                if (c == ' ') {
                    currentWidth += sprite->textWidth(" ", fontSize);
                } else if (c == '\n') {
                    lines++;
                    currentWidth = 0;
                }
                
                word = "";
            } else {
                word += c;
            }
        }
        
        return lines * LINE_HEIGHT;
    }
    
    void drawWrappedText(const String& text, int x, int y, int maxWidth, 
                        int fontSize, uint16_t color, uint16_t bgColor) {
        sprite->setTextColor(color, bgColor);
        sprite->setTextDatum(TL_DATUM);
        
        int currentY = y;
        String currentLine = "";
        String word = "";
        
        for (int i = 0; i < text.length(); i++) {
            char c = text[i];
            
            if (c == ' ' || c == '\n' || i == text.length() - 1) {
                if (i == text.length() - 1 && c != ' ' && c != '\n') {
                    word += c;
                }
                
                String testLine = currentLine + word;
                int lineWidth = sprite->textWidth(testLine, fontSize);
                
                if (lineWidth > maxWidth && currentLine.length() > 0) {
                    sprite->drawString(currentLine, x, currentY, fontSize);
                    currentY += LINE_HEIGHT;
                    currentLine = word + (c == ' ' ? " " : "");
                } else {
                    currentLine = testLine;
                    if (c == ' ') currentLine += " ";
                }
                
                if (c == '\n') {
                    sprite->drawString(currentLine, x, currentY, fontSize);
                    currentY += LINE_HEIGHT;
                    currentLine = "";
                }
                
                word = "";
            } else {
                word += c;
            }
        }
        
        if (currentLine.length() > 0) {
            sprite->drawString(currentLine, x, currentY, fontSize);
        }
    }
    
    void drawChatBubble(const String& text, int y, bool isUser) {
        int bubbleWidth = min(MAX_BUBBLE_WIDTH, SCREEN_WIDTH - (CHAT_BUBBLE_MARGIN * 2));
        int textWidth = bubbleWidth - (CHAT_BUBBLE_PADDING * 2);
        int textHeight = calculateTextHeight(text, textWidth, 2);
        int bubbleHeight = textHeight + (CHAT_BUBBLE_PADDING * 2);
        
        int bubbleX;
        uint16_t bubbleColor, textColor;
        
        if (isUser) {
            // User bubble (right side, cyan)
            bubbleX = SCREEN_WIDTH - CHAT_BUBBLE_MARGIN - bubbleWidth;
            bubbleColor = COLOR_ACCENT;
            textColor = COLOR_BG;
        } else {
            // Assistant bubble (left side, card color)
            bubbleX = CHAT_BUBBLE_MARGIN;
            bubbleColor = COLOR_CARD;
            textColor = COLOR_TEXT;
        }
        
        // Draw bubble shadow
        sprite->fillRoundRect(bubbleX + 2, y + 2, bubbleWidth, bubbleHeight, 12, COLOR_SHADOW);
        
        // Draw bubble
        sprite->fillRoundRect(bubbleX, y, bubbleWidth, bubbleHeight, 12, bubbleColor);
        
        // Draw subtle border
        uint16_t borderColor = isUser ? COLOR_ACCENT_DIM : COLOR_BORDER;
        sprite->drawRoundRect(bubbleX, y, bubbleWidth, bubbleHeight, 12, borderColor);
        
        // Draw text
        drawWrappedText(text, bubbleX + CHAT_BUBBLE_PADDING, 
                       y + CHAT_BUBBLE_PADDING, textWidth, 2, textColor, bubbleColor);
    }
    
    void calculateBubblePositions() {
        bubbleCount = assistant->getChatCount();
        int currentY = CHAT_AREA_TOP;
        
        for (int i = 0; i < bubbleCount; i++) {
            ChatMessage* msg = assistant->getMessage(i);
            if (!msg) continue;
            
            int textWidth = min(MAX_BUBBLE_WIDTH, SCREEN_WIDTH - (CHAT_BUBBLE_MARGIN * 2)) 
                          - (CHAT_BUBBLE_PADDING * 2);
            int textHeight = calculateTextHeight(msg->text, textWidth, 2);
            int bubbleHeight = textHeight + (CHAT_BUBBLE_PADDING * 2);
            
            bubbles[i].y = currentY;
            bubbles[i].height = bubbleHeight;
            bubbles[i].text = msg->text;
            bubbles[i].isUser = msg->isUser;
            
            currentY += bubbleHeight + 12; // Gap between bubbles
        }
        
        totalContentHeight = currentY - CHAT_AREA_TOP;
    }
    
    void drawListeningAnimation() {
        int centerX = SCREEN_WIDTH / 2;
        int centerY = CHAT_AREA_TOP + (CHAT_AREA_HEIGHT / 2);
        
        // Animated waveform
        for (int i = 0; i < 5; i++) {
            int barX = centerX - 40 + (i * 20);
            int waveHeight = 10 + abs((int)(sin((waveformPhase + i * 30) * PI / 180.0) * 25));
            int barY = centerY - (waveHeight / 2);
            
            sprite->fillRoundRect(barX, barY, 12, waveHeight, 4, COLOR_ACCENT);
        }
        
        waveformPhase = (waveformPhase + 12) % 360;
        
        // Text
        sprite->setTextColor(COLOR_TEXT);
        sprite->setTextDatum(MC_DATUM);
        sprite->drawString("Listening...", centerX, centerY + 50, 2);
        
        sprite->setTextColor(COLOR_TEXT_DIM);
        sprite->drawString("Speak now", centerX, centerY + 70, 1);
    }
    
    void drawSpeakingAnimation() {
        int centerX = SCREEN_WIDTH / 2;
        int centerY = CHAT_AREA_TOP + (CHAT_AREA_HEIGHT / 2);
        
        // Pulsing circles
        for (int i = 0; i < 3; i++) {
            int radius = 15 + i * 8;
            int phase = (waveformPhase + i * 20) % 100;
            uint8_t alpha = 255 - (phase * 2);
            uint16_t color = sprite->color565(0, alpha / 2, alpha);
            
            sprite->drawCircle(centerX, centerY, radius + (phase / 10), color);
        }
        
        waveformPhase = (waveformPhase + 8) % 100;
        
        sprite->setTextColor(COLOR_ACCENT);
        sprite->setTextDatum(MC_DATUM);
        sprite->drawString("Speaking...", centerX, centerY + 50, 2);
    }
    
public:
    AIChatUI(TFT_eSprite* spr, AIAssistant* ai) 
        : sprite(spr), assistant(ai), scrollOffset(0), 
          totalContentHeight(0), bubbleCount(0), waveformPhase(0) {}
    
    void draw() {
        sprite->fillSprite(COLOR_BG);
        
        // Header
        UIHelper::drawHeader(sprite, "AI Assistant", 
                           WiFi.status() == WL_CONNECTED ? "Online" : "Offline");
        
        // Chat area background
        sprite->fillRect(0, CHAT_AREA_TOP, SCREEN_WIDTH, CHAT_AREA_HEIGHT, COLOR_BG);
        
        if (assistant->getIsListening()) {
            drawListeningAnimation();
        } else if (assistant->getIsSpeaking()) {
            drawSpeakingAnimation();
        } else {
            // Draw chat bubbles
            calculateBubblePositions();
            
            if (bubbleCount == 0) {
                // Empty state
                sprite->setTextColor(COLOR_TEXT_DIM);
                sprite->setTextDatum(MC_DATUM);
                sprite->drawString("Press MIC to start", SCREEN_WIDTH / 2, 
                                 CHAT_AREA_TOP + (CHAT_AREA_HEIGHT / 2) - 10, 2);
                sprite->drawString("Press HOME for menu", SCREEN_WIDTH / 2, 
                                 CHAT_AREA_TOP + (CHAT_AREA_HEIGHT / 2) + 15, 1);
            } else {
                // Draw visible bubbles
                for (int i = 0; i < bubbleCount; i++) {
                    int bubbleY = bubbles[i].y - scrollOffset;
                    
                    // Only draw if visible
                    if (bubbleY + bubbles[i].height >= CHAT_AREA_TOP && 
                        bubbleY <= CHAT_AREA_BOTTOM) {
                        drawChatBubble(bubbles[i].text, bubbleY, bubbles[i].isUser);
                    }
                }
                
                // Scrollbar
                if (totalContentHeight > CHAT_AREA_HEIGHT) {
                    int scrollbarHeight = max(20, (CHAT_AREA_HEIGHT * CHAT_AREA_HEIGHT) / totalContentHeight);
                    int maxScroll = totalContentHeight - CHAT_AREA_HEIGHT;
                    int scrollbarY = CHAT_AREA_TOP + ((scrollOffset * (CHAT_AREA_HEIGHT - scrollbarHeight)) / maxScroll);
                    
                    sprite->fillRoundRect(SCREEN_WIDTH - 6, CHAT_AREA_TOP, 4, CHAT_AREA_HEIGHT, 2, COLOR_CARD);
                    sprite->fillRoundRect(SCREEN_WIDTH - 6, scrollbarY, 4, scrollbarHeight, 2, COLOR_ACCENT);
                }
            }
        }
        
        // Footer
        UIHelper::drawFooter(sprite, "MIC: Talk  |  HOME: Menu  |  Rotate: Scroll");
        
        sprite->pushSprite(0, 0);
    }
    
    void scroll(int delta) {
        calculateBubblePositions();
        
        scrollOffset += delta * 20; // Scroll speed
        
        int maxScroll = max(0, totalContentHeight - CHAT_AREA_HEIGHT);
        scrollOffset = constrain(scrollOffset, 0, maxScroll);
    }
    
    void scrollToBottom() {
        calculateBubblePositions();
        scrollOffset = max(0, totalContentHeight - CHAT_AREA_HEIGHT);
    }
    
    void reset() {
        scrollOffset = 0;
        totalContentHeight = 0;
        bubbleCount = 0;
    }
};

#endif