// AIChatUI.h - Fixed Chat with Proper Scrolling
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
    
    BubbleInfo bubbles[50];  // Match MAX_CHAT_HISTORY
    int bubbleCount;
    
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
            bubbleX = SCREEN_WIDTH - CHAT_BUBBLE_MARGIN - bubbleWidth;
            bubbleColor = COLOR_ACCENT;
            textColor = COLOR_BG;
        } else {
            bubbleX = CHAT_BUBBLE_MARGIN;
            bubbleColor = COLOR_CARD;
            textColor = COLOR_TEXT;
        }
        
        sprite->fillRoundRect(bubbleX + 2, y + 2, bubbleWidth, bubbleHeight, 12, COLOR_SHADOW);
        sprite->fillRoundRect(bubbleX, y, bubbleWidth, bubbleHeight, 12, bubbleColor);
        
        uint16_t borderColor = isUser ? COLOR_ACCENT_DIM : COLOR_BORDER;
        sprite->drawRoundRect(bubbleX, y, bubbleWidth, bubbleHeight, 12, borderColor);
        
        drawWrappedText(text, bubbleX + CHAT_BUBBLE_PADDING, 
                       y + CHAT_BUBBLE_PADDING, textWidth, 2, textColor, bubbleColor);
    }
    
    void calculateBubblePositions() {
        bubbleCount = assistant->getChatCount();
        int currentY = CHAT_AREA_TOP + 10;
        
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
            
            currentY += bubbleHeight + 12;
        }
        
        totalContentHeight = currentY - CHAT_AREA_TOP;
    }
    
    void drawListeningAnimation() {
        int centerX = SCREEN_WIDTH / 2;
        int centerY = CHAT_AREA_TOP + (CHAT_AREA_HEIGHT / 2);
        
        for (int i = 0; i < 5; i++) {
            int barX = centerX - 40 + (i * 20);
            int waveHeight = 10 + abs((int)(sin((waveformPhase + i * 30) * PI / 180.0) * 25));
            int barY = centerY - (waveHeight / 2);
            sprite->fillRoundRect(barX, barY, 12, waveHeight, 4, COLOR_ACCENT);
        }
        
        waveformPhase = (waveformPhase + 12) % 360;
        
        sprite->setTextColor(COLOR_TEXT);
        sprite->setTextDatum(MC_DATUM);
        sprite->drawString("Listening...", centerX, centerY + 50, 2);
        sprite->setTextColor(COLOR_TEXT_DIM);
        sprite->drawString("Speak now", centerX, centerY + 70, 1);
    }
    
public:
    AIChatUI(TFT_eSprite* spr, AIAssistant* ai) 
        : sprite(spr), assistant(ai), scrollOffset(0), 
          totalContentHeight(0), bubbleCount(0), waveformPhase(0) {}
    
void draw() {
        sprite->fillSprite(COLOR_BG);
        
        UIHelper::drawHeader(sprite, "AI Assistant", 
                           WiFi.status() == WL_CONNECTED ? "Online" : "Offline");
        
        // Create clipping area for chat content ONLY
        sprite->fillRect(0, CHAT_AREA_TOP, SCREEN_WIDTH, CHAT_AREA_HEIGHT, COLOR_BG);
        
        if (assistant->getIsListening()) {
            drawListeningAnimation();
        } else {
            calculateBubblePositions();
            
            if (bubbleCount == 0) {
                sprite->setTextColor(COLOR_TEXT_DIM);
                sprite->setTextDatum(MC_DATUM);
                sprite->drawString("Press MIC to start", SCREEN_WIDTH / 2, 
                                 CHAT_AREA_TOP + (CHAT_AREA_HEIGHT / 2) - 10, 2);
                sprite->drawString("Press HOME for menu", SCREEN_WIDTH / 2, 
                                 CHAT_AREA_TOP + (CHAT_AREA_HEIGHT / 2) + 15, 1);
            } else {
                // Draw bubbles ONLY in the chat area (clip at boundaries)
                for (int i = 0; i < bubbleCount; i++) {
                    int bubbleY = bubbles[i].y - scrollOffset;
                    
                    // Clip bubbles to chat area
                    if (bubbleY + bubbles[i].height >= CHAT_AREA_TOP && 
                        bubbleY <= CHAT_AREA_BOTTOM) {
                        
                        // If bubble extends above chat area, skip the part above
                        if (bubbleY < CHAT_AREA_TOP) {
                            // Don't draw - it would overlap header
                            continue;
                        }
                        
                        // If bubble extends below chat area, skip the part below
                        if (bubbleY + bubbles[i].height > CHAT_AREA_BOTTOM) {
                            // Partially visible at bottom - still draw it
                        }
                        
                        drawChatBubble(bubbles[i].text, bubbleY, bubbles[i].isUser);
                    }
                }
                
                // Scrollbar
                if (totalContentHeight > CHAT_AREA_HEIGHT) {
                    int scrollbarHeight = max(20, (CHAT_AREA_HEIGHT * CHAT_AREA_HEIGHT) / totalContentHeight);
                    int maxScroll = max(0, totalContentHeight - CHAT_AREA_HEIGHT);
                    
                    if (maxScroll > 0) {
                        int scrollbarY = CHAT_AREA_TOP + ((scrollOffset * (CHAT_AREA_HEIGHT - scrollbarHeight)) / maxScroll);
                        sprite->fillRoundRect(SCREEN_WIDTH - 6, CHAT_AREA_TOP, 4, CHAT_AREA_HEIGHT, 2, COLOR_CARD);
                        sprite->fillRoundRect(SCREEN_WIDTH - 6, scrollbarY, 4, scrollbarHeight, 2, COLOR_ACCENT);
                    }
                }
            }
        }
        
        UIHelper::drawFooter(sprite, "MIC: Talk  |  HOME: Menu  |  Rotate: Scroll");
        
        // Redraw header and footer to ensure they're on top
        UIHelper::drawHeader(sprite, "AI Assistant", 
                           WiFi.status() == WL_CONNECTED ? "Online" : "Offline");
        UIHelper::drawFooter(sprite, "MIC: Talk  |  HOME: Menu  |  Rotate: Scroll");
        
        sprite->pushSprite(0, 0);
    }
    
    void scroll(int delta) {
        calculateBubblePositions();
        scrollOffset += delta;
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