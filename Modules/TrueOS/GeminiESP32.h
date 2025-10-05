#ifndef GEMINI_ESP32_H
#define GEMINI_ESP32_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // Dependency: Requires ArduinoJson library (v6 or v7)

class GeminiESP32 {
public:
    /**
     * @brief Constructor - Initializes the Gemini ESP32 client
     * @param token Your Gemini API key
     * @param modelName The Gemini model to use (e.g., "gemini-2.0-flash-exp")
     */
    GeminiESP32(const char* token, const char* modelName);
    
    /**
     * @brief Sends a question to the Gemini API and returns the text response
     * @param question The user's prompt string
     * @param Max_Token (Optional) Override the max response tokens set in GeminiConfig.h
     * @return String The text response from the Gemini model
     */
    String askQuestion(String question, int Max_Token = 300);

private:
    const char* token;
    const char* modelName;
    int maxTokens;
};

#endif
