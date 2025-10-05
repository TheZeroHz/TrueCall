// AIChatUI.h - Improved Chat with Listening Bubble and Long Text Support
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
    int listeningDots;
    unsigned long lastDotUpdate;
    
    struct BubbleInfo {
        int y;
        int height;
        String text;
        bool isUser;
    };
    
    BubbleInfo bubbles[51];  // MAX_CHAT_HISTORY + 1 for listening bubble
    int bubbleCount;
    
    int calculateTextHeight(const String& text, int maxWidth, int fontSize) {
        if (text.length() == 0) return LINE_HEIGHT;
        
        int lines = 1;
        int currentWidth = 0;
        String word = "";
        
        for (unsigned int i = 0; i < text.length(); i++) {
            char c = text[i];
            
            if (c == ' ' || c == '\n' || i == text.length() - 1) {
                if (i == text.length() - 1 && c != ' ' && c != '\n') {
                    word += c;
                }
                
                int wordWidth = sprite->textWidth(word, fontSize);
                
                if (currentWidth + wordWidth > maxWidth && currentWidth > 0) {
                    lines++;
                    currentWidth = wordWidth + sprite->textWidth(" ", fontSize);
                } else {
                    currentWidth += wordWidth;
                    if (c == ' ') {
                        currentWidth += sprite->textWidth(" ", fontSize);
                    }
                }
                
                if (c == '\n') {
                    lines++;
                    currentWidth = 0;
                }
                
                word = "";
            } else {
                word += c;
            }
        }
        
        // No height limit - bubble can be as tall as needed
        int totalHeight = lines * LINE_HEIGHT;
        return totalHeight;
    }
    
    void drawWrappedText(const String& text, int x, int y, int maxWidth, 
                        int fontSize, uint16_t color, uint16_t bgColor) {
        sprite->setTextColor(color, bgColor);
        sprite->setTextDatum(TL_DATUM);
        
        int currentY = y;
        String currentLine = "";
        String word = "";
        
        for (unsigned int i = 0; i < text.length(); i++) {
            char c = text[i];
            
            if (c == ' ' || c == '\n' || i == text.length() - 1) {
                if (i == text.length() - 1 && c != ' ' && c != '\n') {
                    word += c;
                }
                
                String testLine = currentLine.length() > 0 ? currentLine + word : word;
                int lineWidth = sprite->textWidth(testLine, fontSize);
                
                if (lineWidth > maxWidth && currentLine.length() > 0) {
                    // Draw current line and start new one
                    sprite->drawString(currentLine, x, currentY, fontSize);
                    currentY += LINE_HEIGHT;
                    currentLine = word;
                    if (c == ' ') currentLine += " ";
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
        
        // Draw remaining text
        if (currentLine.length() > 0) {
            sprite->drawString(currentLine, x, currentY, fontSize);
        }
    }
    
    void drawChatBubble(const String& text, int y, bool isUser, bool isListening = false) {
        int bubbleWidth = min(MAX_BUBBLE_WIDTH, SCREEN_WIDTH - (CHAT_BUBBLE_MARGIN * 2));
        int textWidth = bubbleWidth - (CHAT_BUBBLE_PADDING * 2);
        int textHeight = calculateTextHeight(text, textWidth, 2);
        int bubbleHeight = textHeight + (CHAT_BUBBLE_PADDING * 2);
        
        // NO height limit - bubble can be as tall as the content needs
        
        int bubbleX;
        uint16_t bubbleColor, textColor;
        
        if (isUser || isListening) {
            bubbleX = SCREEN_WIDTH - CHAT_BUBBLE_MARGIN - bubbleWidth;
            bubbleColor = isListening ? COLOR_ACCENT_DIM : COLOR_ACCENT;
            textColor = COLOR_BG;
        } else {
            bubbleX = CHAT_BUBBLE_MARGIN;
            bubbleColor = COLOR_CARD;
            textColor = COLOR_TEXT;
        }
        
        // Shadow
        sprite->fillRoundRect(bubbleX + 2, y + 2, bubbleWidth, bubbleHeight, 12, COLOR_SHADOW);
        
        // Bubble background
        sprite->fillRoundRect(bubbleX, y, bubbleWidth, bubbleHeight, 12, bubbleColor);
        
        // Border
        uint16_t borderColor = isUser || isListening ? COLOR_ACCENT : COLOR_BORDER;
        sprite->drawRoundRect(bubbleX, y, bubbleWidth, bubbleHeight, 12, borderColor);
        
        // Draw text
        String displayText = text;
        if (isListening) {
            // Add animated dots
            displayText = "Listening";
            for (int i = 0; i < listeningDots; i++) {
                displayText += ".";
            }
        }
        
        drawWrappedText(displayText, bubbleX + CHAT_BUBBLE_PADDING, 
                       y + CHAT_BUBBLE_PADDING, textWidth, 2, textColor, bubbleColor);
    }
    
    void calculateBubblePositions() {
        bubbleCount = 0;
        int currentY = CHAT_AREA_TOP + 10;
        
        // Add existing chat messages
        int msgCount = assistant->getChatCount();
        for (int i = 0; i < msgCount; i++) {
            ChatMessage* msg = assistant->getMessage(i);
            if (!msg) continue;
            
            int textWidth = min(MAX_BUBBLE_WIDTH, SCREEN_WIDTH - (CHAT_BUBBLE_MARGIN * 2)) 
                          - (CHAT_BUBBLE_PADDING * 2);
            int textHeight = calculateTextHeight(msg->text, textWidth, 2);
            int bubbleHeight = textHeight + (CHAT_BUBBLE_PADDING * 2);
            
            // NO minimum height - let bubble be as tall as needed
            
            bubbles[bubbleCount].y = currentY;
            bubbles[bubbleCount].height = bubbleHeight;
            bubbles[bubbleCount].text = msg->text;
            bubbles[bubbleCount].isUser = msg->isUser;
            
            currentY += bubbleHeight + 12;
            bubbleCount++;
        }
        
        // Add listening bubble if actively listening
        if (assistant->getIsListening()) {
            int textWidth = min(MAX_BUBBLE_WIDTH, SCREEN_WIDTH - (CHAT_BUBBLE_MARGIN * 2)) 
                          - (CHAT_BUBBLE_PADDING * 2);
            int bubbleHeight = 40;  // Fixed height for listening animation
            
            bubbles[bubbleCount].y = currentY;
            bubbles[bubbleCount].height = bubbleHeight;
            bubbles[bubbleCount].text = "Listening...";
            bubbles[bubbleCount].isUser = true;
            
            currentY += bubbleHeight + 12;
            bubbleCount++;
            
            // Update listening dots animation
            if (millis() - lastDotUpdate > 400) {
                listeningDots = (listeningDots + 1) % 4;
                lastDotUpdate = millis();
            }
        }
        
        // Add thinking bubble if AI is processing
        if (assistant->getIsSpeaking()) {
            int textWidth = min(MAX_BUBBLE_WIDTH, SCREEN_WIDTH - (CHAT_BUBBLE_MARGIN * 2)) 
                          - (CHAT_BUBBLE_PADDING * 2);
            int bubbleHeight = 40;  // Fixed height for thinking animation
            
            bubbles[bubbleCount].y = currentY;
            bubbles[bubbleCount].height = bubbleHeight;
            bubbles[bubbleCount].text = "Thinking...";
            bubbles[bubbleCount].isUser = false;
            
            currentY += bubbleHeight + 12;
            bubbleCount++;
        }
        
        totalContentHeight = currentY - CHAT_AREA_TOP;
    }
    
public:
    AIChatUI(TFT_eSprite* spr, AIAssistant* ai) 
        : sprite(spr), assistant(ai), scrollOffset(0), 
          totalContentHeight(0), bubbleCount(0), waveformPhase(0),
          listeningDots(0), lastDotUpdate(0) {}
    
    void draw() {
        sprite->fillSprite(COLOR_BG);
        
        // Draw header
        UIHelper::drawHeader(sprite, "AI Assistant", 
                           WiFi.status() == WL_CONNECTED ? "Online" : "Offline");
        
        // Create chat area
        sprite->fillRect(0, CHAT_AREA_TOP, SCREEN_WIDTH, CHAT_AREA_HEIGHT, COLOR_BG);
        
        calculateBubblePositions();
        
        if (bubbleCount == 0) {
            // Show welcome message
            sprite->setTextColor(COLOR_TEXT_DIM);
            sprite->setTextDatum(MC_DATUM);
            sprite->drawString("Press MIC to start", SCREEN_WIDTH / 2, 
                             CHAT_AREA_TOP + (CHAT_AREA_HEIGHT / 2) - 10, 2);
            sprite->drawString("Press HOME for menu", SCREEN_WIDTH / 2, 
                             CHAT_AREA_TOP + (CHAT_AREA_HEIGHT / 2) + 15, 1);
        } else {
            // Draw bubbles - IMPORTANT: Bubbles can be any height!
            // Tall bubbles can extend beyond the visible area (under footer)
            // Users scroll to see the full content - NO height limits applied
            for (int i = 0; i < bubbleCount; i++) {
                int bubbleY = bubbles[i].y - scrollOffset;
                int bubbleBottom = bubbleY + bubbles[i].height;
                
                // Draw if ANY part of bubble is in visible area
                // This allows tall bubbles to extend beyond the footer
                if (bubbleBottom > CHAT_AREA_TOP && bubbleY < CHAT_AREA_BOTTOM) {
                    bool isListeningBubble = (i == bubbleCount - 1 && assistant->getIsListening());
                    drawChatBubble(bubbles[i].text, bubbleY, bubbles[i].isUser, isListeningBubble);
                }
            }
            
            // Draw scrollbar if content is scrollable
            if (totalContentHeight > CHAT_AREA_HEIGHT) {
                int scrollbarHeight = max(20, (CHAT_AREA_HEIGHT * CHAT_AREA_HEIGHT) / totalContentHeight);
                int maxScroll = max(0, totalContentHeight - CHAT_AREA_HEIGHT);
                
                if (maxScroll > 0) {
                    int scrollbarY = CHAT_AREA_TOP + ((scrollOffset * (CHAT_AREA_HEIGHT - scrollbarHeight)) / maxScroll);
                    
                    // Scrollbar track
                    sprite->fillRoundRect(SCREEN_WIDTH - 6, CHAT_AREA_TOP, 4, CHAT_AREA_HEIGHT, 2, COLOR_CARD);
                    
                    // Scrollbar thumb
                    sprite->fillRoundRect(SCREEN_WIDTH - 6, scrollbarY, 4, scrollbarHeight, 2, COLOR_ACCENT);
                }
            }
        }
        
        // Draw footer on top to cover any bubbles that extend beyond
        UIHelper::drawFooter(sprite, "MIC: Talk  |  HOME: Menu  |  Rotate: Scroll");
        
        // Redraw header to ensure it stays on top
        UIHelper::drawHeader(sprite, "AI Assistant", 
                           WiFi.status() == WL_CONNECTED ? "Online" : "Offline");
        
        sprite->pushSprite(0, 0);
    }
    
    void scroll(int delta) {
        scrollOffset += delta;
        int maxScroll = max(0, totalContentHeight - CHAT_AREA_HEIGHT);
        scrollOffset = constrain(scrollOffset, 0, maxScroll);
    }
    
    void scrollToBottom() {
        calculateBubblePositions();
        scrollOffset = max(0, totalContentHeight - CHAT_AREA_HEIGHT);
    }
    
    void autoScrollToNewContent() {
        // Auto-scroll to bottom when new messages arrive
        calculateBubblePositions();
        int maxScroll = max(0, totalContentHeight - CHAT_AREA_HEIGHT);
        
        // If we're near the bottom (within 50 pixels), auto-scroll to new content
        if (scrollOffset >= maxScroll - 50 || scrollOffset == 0) {
            scrollOffset = maxScroll;
        }
    }
    
    void reset() {
        scrollOffset = 0;
        totalContentHeight = 0;
        bubbleCount = 0;
        listeningDots = 0;
    }
};


#endif