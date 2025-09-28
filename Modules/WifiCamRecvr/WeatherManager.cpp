// WeatherManager.cpp
#include "WeatherManager.h"
#include <WiFi.h>

WeatherManager::WeatherManager() 
    : temperature(0), feelsLike(0), humidity(0), windSpeed(0), 
      dataValid(false), lastFetchTime(0) {}

void WeatherManager::begin(const String& key, const String& cityName) {
    apiKey = key;
    city = cityName;
}

void WeatherManager::fetchWeather() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - skipping weather update");
        return;
    }
    
    String endpoint = "http://api.openweathermap.org/data/2.5/weather?q=" + 
                     city + "&APPID=" + apiKey + "&units=metric";
    
    HTTPClient http;
    http.begin(endpoint);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        String payload = http.getString();
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            temperature = doc["main"]["temp"];
            feelsLike = doc["main"]["feels_like"];
            description = doc["weather"][0]["description"].as<String>();
            humidity = doc["main"]["humidity"];
            windSpeed = doc["wind"]["speed"];
            
            // Capitalize first letter
            if (description.length() > 0) {
                description[0] = toupper(description[0]);
            }
            
            dataValid = true;
            lastFetchTime = millis();
            Serial.println("Weather: " + String((int)temperature) + "°C - " + description);
        } else {
            Serial.println("Weather JSON parse error");
            dataValid = false;
        }
    } else {
        Serial.println("Weather HTTP error: " + String(httpCode));
        dataValid = false;
    }
    
    http.end();
}

void WeatherManager::update() {
    if (millis() - lastFetchTime > WEATHER_UPDATE_INTERVAL) {
        fetchWeather();
    }
}