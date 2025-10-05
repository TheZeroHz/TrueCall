#include "GeminiESP32.h"
#include "GeminiConfig.h"

// Constructor - accepts token and model name
GeminiESP32::GeminiESP32(const char* token, const char* modelName)
    : token(token), modelName(modelName), maxTokens(MAX_RESPONSE_TOKENS) {}

// Helper function to extract text from JSON part object
String getTextFromPart(JsonObject part) {
    if (part.containsKey("text")) {
        return part["text"].as<String>();
    }
    return "";
}

String GeminiESP32::askQuestion(String question, int Max_Token) {
    // Override default maxTokens if a new value is passed
    if (Max_Token > 0) {
        maxTokens = Max_Token;
    }
    
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Gemini] WiFi not connected");
        return "WiFi not connected";
    }
    
    HTTPClient https;
    https.setTimeout(15000);  // 15 second timeout
    
    // Construct the Gemini API endpoint URL
    // Format: https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent?key={api_key}
    String url = "https://generativelanguage.googleapis.com/v1beta/models/";
    url += modelName;
    url += ":generateContent?key=";
    url += token;
    
    Serial.println("[Gemini] Connecting to API...");
    Serial.printf("[Gemini] Model: %s\n", modelName);
    
    if (!https.begin(url)) {
        Serial.println("[Gemini] Failed to begin HTTPS connection");
        return "Connection failed";
    }
    
    https.addHeader("Content-Type", "application/json");
    
    // Build JSON payload with proper structure
    DynamicJsonDocument requestDoc(4096); // 4KB buffer for request
    
    // Add generation configuration
    JsonObject generationConfig = requestDoc.createNestedObject("generationConfig");
    generationConfig["maxOutputTokens"] = maxTokens;
    generationConfig["temperature"] = 0.7;
    
    // Add safety settings (optional - set to BLOCK_NONE for minimal filtering)
    JsonArray safetySettings = requestDoc.createNestedArray("safetySettings");
    
    const char* categories[] = {
        "HARM_CATEGORY_HARASSMENT",
        "HARM_CATEGORY_HATE_SPEECH",
        "HARM_CATEGORY_SEXUALLY_EXPLICIT",
        "HARM_CATEGORY_DANGEROUS_CONTENT"
    };
    
    for (int i = 0; i < 4; i++) {
        JsonObject safety = safetySettings.createNestedObject();
        safety["category"] = categories[i];
        safety["threshold"] = "BLOCK_NONE";
    }
    
    // Add the user's question to contents array
    JsonArray contents = requestDoc.createNestedArray("contents");
    JsonObject userContent = contents.createNestedObject();
    userContent["role"] = "user";
    
    JsonArray parts = userContent.createNestedArray("parts");
    JsonObject textPart = parts.createNestedObject();
    textPart["text"] = question + " " + SYSTEM_PROMPT;
    
    // Serialize and send request
    String requestBody;
    serializeJson(requestDoc, requestBody);
    
    Serial.println("[Gemini] Sending request...");
    Serial.printf("[Gemini] Payload size: %d bytes\n", requestBody.length());
    
    int httpCode = https.POST(requestBody);
    
    Serial.printf("[Gemini] HTTP Response Code: %d\n", httpCode);
    
    // Handle successful response
    if (httpCode == HTTP_CODE_OK || httpCode == 200) {
        String response = https.getString();
        Serial.println("[Gemini] Response received");
        
        // Parse JSON response
        DynamicJsonDocument responseDoc(4096); // 4KB buffer for response
        DeserializationError error = deserializeJson(responseDoc, response);
        
        if (error) {
            Serial.printf("[Gemini] JSON parsing failed: %s\n", error.c_str());
            https.end();
            return "JSON parsing failed";
        }
        
        // Extract text from candidates array
        if (responseDoc.containsKey("candidates") && responseDoc["candidates"].size() > 0) {
            JsonObject candidate = responseDoc["candidates"][0];
            
            // Check for safety blocks
            if (candidate.containsKey("finishReason")) {
                String finishReason = candidate["finishReason"].as<String>();
                if (finishReason == "SAFETY") {
                    Serial.println("[Gemini] Response blocked due to safety settings");
                    https.end();
                    return "Response blocked due to safety settings";
                }
            }
            
            // Extract content
            if (candidate.containsKey("content")) {
                JsonObject content = candidate["content"];
                if (content.containsKey("parts") && content["parts"].size() > 0) {
                    String answer = getTextFromPart(content["parts"][0]);
                    
                    if (answer.length() > 0) {
                        answer.trim();
                        Serial.printf("[Gemini] Success: %s\n", answer.c_str());
                        https.end();
                        return answer;
                    }
                }
            }
        }
        
        // Check for API error in response
        if (responseDoc.containsKey("error")) {
            String errorMsg = responseDoc["error"]["message"].as<String>();
            Serial.printf("[Gemini] API Error: %s\n", errorMsg.c_str());
            https.end();
            return "API Error: " + errorMsg;
        }
        
        https.end();
        return "Invalid response format or empty answer";
        
    // Handle HTTP error codes
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
        Serial.printf("[Gemini] Unknown HTTP ERROR %d\n", httpCode);
        Serial.printf("[Gemini] Response: %s\n", response.c_str());
        https.end();
        return "Unknown HTTP error: " + String(httpCode);
    }
}
