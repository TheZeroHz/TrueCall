#ifndef WEATHERCLIENT_H
#define WEATHERCLIENT_H

#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>

class WeatherClient {
public:
    WeatherClient(const String &apiKey, const String &city);
    void fetchWeather();
    
    // Basic weather data
    float getTemperature();
    float getFeelsLike();
    String getWeatherDescription();
    int getHumidity();
    float getWindSpeed();
    int getCloudiness();
    String getCountry();
    String getCityName();
    String getWeatherIcon();
    
    // Enhanced features for aesthetic display
    uint16_t getWeatherColor();           // Get color based on weather
    String getWeatherEmoji();             // Get emoji representation
    String getTemperatureRange();         // Get temperature category
    String getDressingSuggestion();       // Get clothing suggestion
    String getActivitySuggestion();       // Get activity suggestion
    bool isDataValid();                   // Check if weather data is current
    unsigned long getLastUpdate();        // Get timestamp of last update
    
    // Display helpers
    void drawWeatherIcon(TFT_eSprite& sprite, int x, int y, int size);
    String getFormattedTemperature();     // Returns formatted temp with symbol
    String getFormattedFeelsLike();       // Returns formatted feels-like temp
    String getFormattedWindSpeed();       // Returns formatted wind speed
    String getFormattedHumidity();        // Returns formatted humidity
    
private:
    String apiKey;
    String city;
    String endpoint;
    String payload;
    
    // Weather data
    float temperature;
    float feelsLike;
    String weatherDescription;
    String weatherIcon;
    int humidity;
    float windSpeed;
    int cloudiness;
    String country;
    String cityName;
    
    // Enhanced data
    unsigned long lastUpdateTime;
    bool validData;
    
    void parseWeatherData(const String &jsonPayload);
    uint16_t getColorFromTemperature(float temp);
    uint16_t getColorFromWeatherType(const String &icon);
};

#endif