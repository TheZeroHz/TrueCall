#include "GeminiESP32.h"

GeminiESP32::GeminiESP32(const char* token)
    : token(token), maxTokens(200) {}

String GeminiESP32::askQuestion(String question, int Max_Token) {
    maxTokens = Max_Token;
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Gemini] WiFi not connected");
        return "WiFi not connected";
    }
    
    HTTPClient https;
    https.setTimeout(15000);  // 15 second timeout
    
    // Use correct Gemini API endpoint
    String url = String("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash-exp:generateContent?key=") + token;
    
    Serial.println("[Gemini] Connecting to API...");
    Serial.printf("[Gemini] URL: %s\n", url.c_str());
    
    if (!https.begin(url)) {
        Serial.println("[Gemini] Failed to begin HTTPS connection");
        return "Connection failed";
    }
    
    https.addHeader("Content-Type", "application/json");
    
    // Build proper JSON payload
    DynamicJsonDocument requestDoc(2048);
    JsonArray contents = requestDoc.createNestedArray("contents");
    JsonObject content = contents.createNestedObject();
    JsonArray parts = content.createNestedArray("parts");
    JsonObject part = parts.createNestedObject();
    part["text"] = question + " (Keep response under 50 words)";
    
    JsonObject generationConfig = requestDoc.createNestedObject("generationConfig");
    generationConfig["maxOutputTokens"] = maxTokens;
    generationConfig["temperature"] = 0.7;
    
    String payload;
    serializeJson(requestDoc, payload);
    
    Serial.println("[Gemini] Sending request...");
    Serial.printf("[Gemini] Payload size: %d bytes\n", payload.length());
    
    int httpCode = https.POST(payload);
    
    Serial.printf("[Gemini] HTTP Response Code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK || httpCode == 200) {
        String response = https.getString();
        Serial.println("[Gemini] Response received");
        
        // Parse response
        DynamicJsonDocument responseDoc(4096);
        DeserializationError error = deserializeJson(responseDoc, response);
        
        if (error) {
            Serial.printf("[Gemini] JSON parse error: %s\n", error.c_str());
            https.end();
            return "Failed to parse response";
        }
        
        // Extract text from response
        if (responseDoc.containsKey("candidates")) {
            JsonArray candidates = responseDoc["candidates"];
            if (candidates.size() > 0) {
                JsonObject candidate = candidates[0];
                if (candidate.containsKey("content")) {
                    JsonObject content = candidate["content"];
                    if (content.containsKey("parts")) {
                        JsonArray parts = content["parts"];
                        if (parts.size() > 0) {
                            String answer = parts[0]["text"].as<String>();
                            answer.trim();
                            
                            // Clean up the answer
                            String filteredAnswer = "";
                            for (size_t i = 0; i < answer.length(); i++) {
                                char c = answer[i];
                                if (isalnum(c) || isspace(c) || ispunct(c)) {
                                    filteredAnswer += c;
                                }
                            }
                            
                            https.end();
                            Serial.printf("[Gemini] Success: %s\n", filteredAnswer.c_str());
                            return filteredAnswer;
                        }
                    }
                }
            }
        }
        
        // Check for error in response
        if (responseDoc.containsKey("error")) {
            String errorMsg = responseDoc["error"]["message"].as<String>();
            Serial.printf("[Gemini] API Error: %s\n", errorMsg.c_str());
            https.end();
            return "API Error: " + errorMsg;
        }
        
        https.end();
        return "Invalid response format";
        
    } else if (httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == 401) {
        Serial.println("[Gemini] ERROR: Invalid API key");
        https.end();
        return "Invalid API key";
        
    } else if (httpCode == HTTP_CODE_TOO_MANY_REQUESTS || httpCode == 429) {
        Serial.println("[Gemini] ERROR: Rate limit exceeded");
        https.end();
        return "Rate limit exceeded";
        
    } else if (httpCode < 0) {
        Serial.printf("[Gemini] Connection error: %s\n", https.errorToString(httpCode).c_str());
        https.end();
        return "Connection error";
        
    } else {
        String response = https.getString();
        Serial.printf("[Gemini] HTTP Error %d: %s\n", httpCode, response.c_str());
        https.end();
        return "HTTP Error " + String(httpCode);
    }
}