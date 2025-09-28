// WeatherManager.h
#ifndef WEATHER_MANAGER_H
#define WEATHER_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Config.h"

class WeatherManager {
private:
    String apiKey;
    String city;
    float temperature;
    float feelsLike;
    String description;
    int humidity;
    float windSpeed;
    bool dataValid;
    unsigned long lastFetchTime;
    
public:
    WeatherManager();
    void begin(const String& key, const String& cityName);
    void fetchWeather();
    void update();
    
    bool isDataValid() const { return dataValid; }
    float getTemperature() const { return temperature; }
    float getFeelsLike() const { return feelsLike; }
    String getDescription() const { return description; }
    int getHumidity() const { return humidity; }
    float getWindSpeed() const { return windSpeed; }
};

#endif