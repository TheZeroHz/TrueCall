#ifndef AIASSISTANT_H
#define AIASSISTANT_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESpeech.h>
#include "GeminiESP32.h"
#include "Config.h"
#include "Colors.h"

// Gemini Configuration
#define GEMINI_API_TOKEN "AIzaSyDm7GHqLe7UlT_Z9fPl9nGj1OLAuX0aL2k"
#define MAX_CHAT_HISTORY 50  // Increased from 20
#define MAX_RESPONSE_TOKENS 150

struct ChatMessage {
    String text;
    bool isUser;
    unsigned long timestamp;
    
    ChatMessage() : text(""), isUser(true), timestamp(0) {}
    ChatMessage(String t, bool u) : text(t), isUser(u), timestamp(millis()) {}
};

class AIAssistant {
private:
    String data;
    ESpeech* stt;
    GeminiESP32* gemini;
    ChatMessage chatHistory[MAX_CHAT_HISTORY];
    int chatCount;
    bool isListening;
    bool isSpeaking;
    
public:
    AIAssistant() : stt(nullptr), gemini(nullptr), 
                    chatCount(0), isListening(false), isSpeaking(false) {}
    String getSTT(){
        return data;
    }
    
    bool begin() {
        Serial.println("[AI] Initializing AI Assistant...");
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[AI] ERROR: WiFi not connected!");
            return false;
        }
        
        // Initialize Speech-to-Text
        Serial.println("[AI] Initializing ESpeech...");
        stt = new ESpeech(I2S_NUM_1, MIC_SCK, MIC_WS, MIC_SD);
        stt->serverURL(STT_SERVER_URL);
        
        // Initialize Gemini AI
        Serial.println("[AI] Initializing Gemini...");
        gemini = new GeminiESP32(GEMINI_API_TOKEN);
        
        // Test connection
        Serial.println("[AI] Testing Gemini connection...");
        String testResponse = gemini->askQuestion("Say 'OK'", 10);
        
        if (testResponse.indexOf("Error") >= 0 || testResponse.indexOf("error") >= 0) {
            Serial.printf("[AI] Gemini test failed: %s\n", testResponse.c_str());
            addMessage("AI initialization warning: " + testResponse, false);
        } else {
            Serial.println("[AI] Gemini test successful");
        }
        
        Serial.println("[AI] AI Assistant ready");
        addMessage("Hello! I'm your AI assistant. How can I help you?", false);
        return true;
    }
    
    void startListening() {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[AI] ERROR: WiFi not connected!");
            addMessage("WiFi connection required", false);
            return;
        }
        
        if (!stt) {
            Serial.println("[AI] ERROR: STT not initialized!");
            return;
        }
        
        Serial.println("\n[AI] >>> Starting voice recording...");
        addMessage("Listening...", true);
        isListening = true;
        
        // Record audio
        stt->recordAudio();
        
        // Get transcription
        String userText = stt->getTranscription();
        data=userText;
        isListening = false;
        
        // Remove "Listening..." placeholder
        if (chatCount > 0 && chatHistory[chatCount - 1].text == "Listening...") {
            chatCount--;
        }
        
        if (userText.length() > 0) {
            Serial.printf("[AI] User said: '%s'\n", userText.c_str());
            addMessage(userText, true);
            
            // Get AI response
            addMessage("Thinking...", false);
            String aiResponse = getAIResponse(userText);
            
            // Remove "Thinking..." placeholder
            if (chatCount > 0 && chatHistory[chatCount - 1].text == "Thinking...") {
                chatCount--;
            }
            
            if (aiResponse.length() > 0) {
                addMessage(aiResponse, false);
            } else {
                addMessage("Sorry, I couldn't generate a response", false);
            }
        } else {
            Serial.println("[AI] No speech detected");
            addMessage("No speech detected. Try again.", false);
        }
    }
    
    String getAIResponse(const String& userInput) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[AI] WiFi connection lost");
            return "WiFi connection lost";
        }
        
        if (!gemini) {
            Serial.println("[AI] Gemini not initialized");
            return "AI not initialized";
        }
        
        Serial.println("[AI] Asking Gemini...");
        Serial.printf("[AI] Question: %s\n", userInput.c_str());
        
        // Get response with timeout protection
        unsigned long startTime = millis();
        String response = gemini->askQuestion(userInput, MAX_RESPONSE_TOKENS);
        unsigned long duration = millis() - startTime;
        
        Serial.printf("[AI] Response time: %lu ms\n", duration);
        Serial.printf("[AI] Response: %s\n", response.c_str());
        
        // Check for errors
        if (response.length() == 0) {
            return "No response received";
        }
        
        if (response.indexOf("Error") >= 0 || 
            response.indexOf("error") >= 0 ||
            response.indexOf("Failed") >= 0) {
            Serial.printf("[AI] Error response: %s\n", response.c_str());
            return "Sorry, I encountered an error: " + response;
        }
        
        // Limit response length for display
        if (response.length() > 300) {
            response = response.substring(0, 297) + "...";
        }
        
        return response;
    }
    
    void addMessage(const String& text, bool isUser) {
        if (chatCount >= MAX_CHAT_HISTORY) {
            // Shift messages to make room
            for (int i = 0; i < MAX_CHAT_HISTORY - 1; i++) {
                chatHistory[i] = chatHistory[i + 1];
            }
            chatCount = MAX_CHAT_HISTORY - 1;
        }
        
        chatHistory[chatCount] = ChatMessage(text, isUser);
        chatCount++;
        
        Serial.printf("[AI] Message added (count: %d): %s %s\n", 
                     chatCount, isUser ? "USER:" : "AI:", text.c_str());
    }
    
    int getChatCount() const { return chatCount; }
    
    ChatMessage* getMessage(int index) {
        if (index >= 0 && index < chatCount) {
            return &chatHistory[index];
        }
        return nullptr;
    }
    
    bool getIsListening() const { return isListening; }
    bool getIsSpeaking() const { return isSpeaking; }
    
    void clearHistory() {
        chatCount = 0;
        Serial.println("[AI] Chat history cleared");
        addMessage("Chat cleared. How can I help you?", false);
    }
};

#endif