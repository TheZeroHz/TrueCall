#ifndef AIASSISTANT_H
#define AIASSISTANT_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESpeech.h>
#include "GeminiESP32.h"
#include "Config.h"
#include "Colors.h"

// Gemini Configuration
#define GEMINI_API_TOKEN "AIzaSyDPNTBZEBFmlZBIStC-ExslDAMQPudkuOE"
#define MAX_CHAT_HISTORY 20
#define MAX_RESPONSE_TOKENS 200

struct ChatMessage {
    String text;
    bool isUser;
    unsigned long timestamp;
    
    ChatMessage() : text(""), isUser(true), timestamp(0) {}
    ChatMessage(String t, bool u) : text(t), isUser(u), timestamp(millis()) {}
};
class AIAssistant {
private:
    ESpeech* stt;
    GeminiESP32* gemini;
    ChatMessage chatHistory[MAX_CHAT_HISTORY];
    int chatCount;
    bool isListening;
    bool isSpeaking;
    bool audioInitialized;

    
public:
    AIAssistant() : stt(nullptr), gemini(nullptr), 
                    chatCount(0), isListening(false), isSpeaking(false){}
    
    bool begin() {
        Serial.println("Initializing AI Assistant...");
        
        // Initialize Speech-to-Text
        stt = new ESpeech(I2S_NUM_1, MIC_SCK, MIC_WS, MIC_SD);
        stt->serverURL("https://espeechserver-iukg.onrender.com/uploadAudio");
        
        // Initialize Gemini AI
        gemini = new GeminiESP32(GEMINI_API_TOKEN);
        
        Serial.println("AI Assistant ready (TTS lazy-loaded)");
        return true;
    }
    
    void startListening() {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi not connected!");
            return;
        }
        
        
        Serial.println("\n>>> Starting voice recording...");
        isListening = true;
        stt->recordAudio();
        String userText = stt->getTranscription();
        isListening = false;
        if (userText.length() > 0) {
            Serial.printf("User said: %s\n", userText.c_str());
            addMessage(userText, true);
            
            String aiResponse = getAIResponse(userText);
            
            if (aiResponse.length() > 0) {
                addMessage(aiResponse, false);
            }
        } else {
            Serial.println("No speech detected");
        }
    }
    
    String getAIResponse(const String& userInput) {
        if (WiFi.status() != WL_CONNECTED) {
            return "WiFi connection lost";
        }
        
        if (!gemini) {
            return "AI not initialized";
        }
        
        Serial.println("Asking Gemini...");
        
        String response = gemini->askQuestion(userInput, MAX_RESPONSE_TOKENS);
        
        if (response.length() > 0) {
            Serial.printf("Gemini: %s\n", response.c_str());
            return response;
        } else {
            return "Sorry, I couldn't generate a response";
        }
    }
    
    void addMessage(const String& text, bool isUser) {
        if (chatCount >= MAX_CHAT_HISTORY) {
            for (int i = 0; i < MAX_CHAT_HISTORY - 1; i++) {
                chatHistory[i] = chatHistory[i + 1];
            }
            chatCount = MAX_CHAT_HISTORY - 1;
        }
        
        chatHistory[chatCount] = ChatMessage(text, isUser);
        chatCount++;
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
        Serial.println("Chat history cleared");
    }
};

#endif