// AIAssistant.h - AI Assistant with Smart TTS Chunking
// 
// Flow:
// 1. User speaks → STT (Speech-to-Text) → Get text from user
// 2. Send text to Gemini AI → Get text response from Gemini
// 3. Split Gemini's text response into chunks
// 4. Send each chunk to TTS (Text-to-Speech) → Speak Gemini's response
//
#ifndef AIASSISTANT_H
#define AIASSISTANT_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESpeech.h>
#include "GeminiESP32.h"
#include "GeminiConfig.h"
#include "Config.h"
#include "Colors.h"
#include "AudioManager.h"

#define MAX_CHAT_HISTORY 50
#define TTS_CHUNK_SIZE 50      // Max characters per TTS chunk
#define TTS_ENABLE_CHUNKING true // Set to false to disable chunking

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
    
    // Helper function to speak Gemini's text response in chunks
    void speakInChunks(const String& textFromGemini, const char* lang, int maxChunkSize = TTS_CHUNK_SIZE) {
        if (textFromGemini.length() == 0) return;
        
        // If chunking disabled or text is short, speak all at once
        if (!TTS_ENABLE_CHUNKING || textFromGemini.length() <= maxChunkSize) {
            Serial.printf("[TTS] Speaking full response (%d chars)\n", textFromGemini.length());
            audioConnecttoSpeech(textFromGemini.c_str(), lang);
            return;
        }
        
        int startPos = 0;
        int chunkNumber = 1;
        
        while (startPos < textFromGemini.length()) {
            int chunkSize = min(maxChunkSize, (int)(textFromGemini.length() - startPos));
            int endPos = startPos + chunkSize;
            
            // If not at end, find good break point
            if (endPos < textFromGemini.length()) {
                int lookbackDistance = min(50, chunkSize / 2);
                
                // Priority 1: Sentence endings (. ! ?)
                int sentenceEnd = -1;
                for (int i = endPos; i > startPos && i > endPos - lookbackDistance; i--) {
                    char c = textFromGemini[i];
                    if (c == '.' || c == '!' || c == '?') {
                        if (i + 1 >= textFromGemini.length() || textFromGemini[i + 1] == ' ') {
                            sentenceEnd = i + 1;
                            break;
                        }
                    }
                }
                
                if (sentenceEnd > startPos) {
                    endPos = sentenceEnd;
                } else {
                    // Priority 2: Clause breaks (, ; :)
                    int clauseEnd = -1;
                    for (int i = endPos; i > startPos && i > endPos - lookbackDistance; i--) {
                        char c = textFromGemini[i];
                        if (c == ',' || c == ';' || c == ':') {
                            clauseEnd = i + 1;
                            break;
                        }
                    }
                    
                    if (clauseEnd > startPos) {
                        endPos = clauseEnd;
                    } else {
                        // Priority 3: Word boundary (space)
                        int spacePos = -1;
                        for (int i = endPos; i > startPos && i > endPos - lookbackDistance; i--) {
                            if (textFromGemini[i] == ' ') {
                                spacePos = i + 1;
                                break;
                            }
                        }
                        
                        if (spacePos > startPos) {
                            endPos = spacePos;
                        }
                    }
                }
            }
            
            // Extract chunk and trim
            String textChunk = textFromGemini.substring(startPos, endPos);
            textChunk.trim();
            
            if (textChunk.length() > 0) {
                Serial.printf("[TTS] Chunk %d (%d chars): %s\n", 
                             chunkNumber++, textChunk.length(), textChunk.c_str());
                audioConnecttoSpeech(textChunk.c_str(), lang);
                delay(100);
            }
            
            startPos = endPos;
        }
        
        Serial.printf("[TTS] Finished speaking %d chunks\n", chunkNumber - 1);
    }
    
public:
    AIAssistant() : stt(nullptr), 
                    gemini(nullptr),
                    chatCount(0), 
                    isListening(false), 
                    isSpeaking(false) {
        gemini = new GeminiESP32(GEMINI_API_TOKEN, GEMINI_MODEL_NAME);
    }
    
    ~AIAssistant() {
        if (stt) delete stt;
        if (gemini) delete gemini;
    }
    
    String getSTT() {
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
        
        if (!gemini) {
            Serial.println("[AI] ERROR: Gemini not initialized!");
            return false;
        }
        
        // Test connection
        Serial.println("[AI] Testing Gemini connection...");
        String testResponse = gemini->askQuestion("Say OK", MAX_RESPONSE_TOKENS);
        
        if (testResponse.indexOf("Error") >= 0 || 
            testResponse.indexOf("error") >= 0 ||
            testResponse.indexOf("failed") >= 0 ||
            testResponse.indexOf("Failed") >= 0) {
            Serial.printf("[AI] Gemini test failed: %s\n", testResponse.c_str());
            addMessage("AI initialization warning: " + testResponse, false);
        } else {
            Serial.println("[AI] Gemini test successful");
            Serial.printf("[AI] Test response: %s\n", testResponse.c_str());

        }
        
        Serial.println("[AI] AI Assistant ready");
        addMessage("Hello! I'm your AI assistant. How can I help you?", false);
        speakInChunks("Hello! I'm your AI assistant. How can I help you?", "en");
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
        isListening = true;
        
        // Record audio from user
        stt->recordAudio();
        
        // Get transcription of user's speech
        String userText = stt->getTranscription();
        data = userText;
        isListening = false;
        
        if (userText.length() > 0) {
            Serial.printf("[AI] User said: '%s'\n", userText.c_str());
            // Add user's message (but DON'T speak it)
            addMessage(userText, true);
            
            // Get AI response from Gemini
            String aiResponse = getAIResponse(userText);
            
            if (aiResponse.length() > 0) {
                addMessage(aiResponse, false);
                
                
            } else {
                addMessage("Sorry, I couldn't generate a response", false);
                speakInChunks("Sorry, I couldn't generate a response", "en");
            }
        } else {
            Serial.println("[AI] No speech detected");
            addMessage("No speech detected. Try again.", false);
            speakInChunks("No speech detected. Try again.", "en");
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
        
        isSpeaking = true;
        
        Serial.println("[AI] Asking Gemini...");
        Serial.printf("[AI] Question: %s\n", userInput.c_str());
        
        unsigned long startTime = millis();
        String response = gemini->askQuestion(userInput, MAX_RESPONSE_TOKENS);
        unsigned long duration = millis() - startTime;
        
        isSpeaking = false;
        
        Serial.printf("[AI] Response time: %lu ms\n", duration);
        Serial.printf("[AI] Response: %s\n", response.c_str());
        speakInChunks(response.c_str(), "en");
        
        if (response.length() == 0) {
            return "No response received";
        }
        
        if (response.indexOf("Error") >= 0 || 
            response.indexOf("error") >= 0 ||
            response.indexOf("Failed") >= 0 ||
            response.indexOf("failed") >= 0) {
            Serial.printf("[AI] Error response: %s\n", response.c_str());
            return "Sorry, I encountered an error. Please try again.";
        }
        
        return response;
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