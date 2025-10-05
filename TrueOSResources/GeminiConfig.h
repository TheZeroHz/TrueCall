// GeminiConfig.h - Configuration for Gemini AI
#ifndef GEMINICONFIG_H
#define GEMINICONFIG_H

// Your Gemini API Token
#define GEMINI_API_TOKEN "AIzaSyAnJZIBIHXWP-6iZrb07CM_ziEgmc7hAKc"

// --- Gemini Model Configuration ---
// Using the current stable model: "gemini-2.0-flash-exp" (experimental but free)
// Alternative: "gemini-1.5-flash" for production stability
#define GEMINI_MODEL_NAME "gemini-2.0-flash-exp"

// Response settings
#define MAX_RESPONSE_TOKENS 30  // Shorter for faster response on ESP32
#define SYSTEM_PROMPT "You are a helpful AI assistant. Keep responses concise and clear, under 50 words when possible."

#endif
