#include "WeatherClient.h"

WeatherClient::WeatherClient(const String &apiKey, const String &city)
    : apiKey(apiKey), city(city), lastUpdateTime(0), validData(false) {
    endpoint = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&APPID=" + apiKey + "&units=metric";
}

void WeatherClient::fetchWeather() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(endpoint);
        http.setTimeout(5000); // 5 second timeout
        
        int httpCode = http.GET();
        
        if (httpCode > 0) {
            payload = http.getString();
            parseWeatherData(payload);
            lastUpdateTime = millis();
            validData = true;
            Serial.println("Weather data updated successfully");
        } else {
            Serial.println("Error on HTTP request: " + String(httpCode));
            validData = false;
        }
        
        http.end();
    } else {
        Serial.println("WiFi not connected");
        validData = false;
    }
}

void WeatherClient::parseWeatherData(const String &jsonPayload) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, jsonPayload);

    if (!error) {
        temperature = doc["main"]["temp"];
        feelsLike = doc["main"]["feels_like"];
        weatherDescription = doc["weather"][0]["description"].as<String>();
        weatherIcon = doc["weather"][0]["icon"].as<String>();
        humidity = doc["main"]["humidity"];
        windSpeed = doc["wind"]["speed"];
        cloudiness = doc["clouds"]["all"];
        country = doc["sys"]["country"].as<String>();
        cityName = doc["name"].as<String>();
        
        // Capitalize first letter of description
        if (weatherDescription.length() > 0) {
            weatherDescription[0] = toupper(weatherDescription[0]);
        }
    } else {
        Serial.println("Error parsing weather JSON");
        validData = false;
    }
}

// Basic getters
float WeatherClient::getTemperature() { return temperature; }
float WeatherClient::getFeelsLike() { return feelsLike; }
String WeatherClient::getWeatherDescription() { return weatherDescription; }
String WeatherClient::getWeatherIcon() { return weatherIcon; }
int WeatherClient::getHumidity() { return humidity; }
float WeatherClient::getWindSpeed() { return windSpeed; }
int WeatherClient::getCloudiness() { return cloudiness; }
String WeatherClient::getCountry() { return country; }
String WeatherClient::getCityName() { return cityName; }

// Enhanced features
uint16_t WeatherClient::getWeatherColor() {
    // Combine temperature and weather type for color
    uint16_t tempColor = getColorFromTemperature(temperature);
    uint16_t weatherColor = getColorFromWeatherType(weatherIcon);
    
    // Blend the colors
    return tempColor; // For now, use temperature-based color
}

uint16_t WeatherClient::getColorFromTemperature(float temp) {
    if (temp < 0) return TFT_BLUE;           // Freezing
    else if (temp < 10) return TFT_CYAN;     // Cold
    else if (temp < 20) return TFT_GREEN;    // Cool
    else if (temp < 30) return TFT_YELLOW;   // Warm
    else if (temp < 35) return TFT_ORANGE;   // Hot
    else return TFT_RED;                     // Very hot
}

uint16_t WeatherClient::getColorFromWeatherType(const String &icon) {
    if (icon.startsWith("01")) return TFT_YELLOW;      // Clear sky
    else if (icon.startsWith("02")) return TFT_ORANGE;  // Few clouds
    else if (icon.startsWith("03")) return TFT_LIGHTGREY; // Scattered clouds
    else if (icon.startsWith("04")) return TFT_DARKGREY;  // Broken clouds
    else if (icon.startsWith("09")) return TFT_BLUE;      // Shower rain
    else if (icon.startsWith("10")) return TFT_CYAN;      // Rain
    else if (icon.startsWith("11")) return TFT_PURPLE;    // Thunderstorm
    else if (icon.startsWith("13")) return TFT_WHITE;     // Snow
    else if (icon.startsWith("50")) return TFT_LIGHTGREY; // Mist
    else return TFT_WHITE;
}

String WeatherClient::getWeatherEmoji() {
    String icon = weatherIcon;
    if (icon.startsWith("01")) return "☀";        // Clear sky
    else if (icon.startsWith("02")) return "⛅";   // Few clouds
    else if (icon.startsWith("03")) return "☁";   // Scattered clouds
    else if (icon.startsWith("04")) return "☁";   // Broken clouds
    else if (icon.startsWith("09")) return "🌦";   // Shower rain
    else if (icon.startsWith("10")) return "🌧";   // Rain
    else if (icon.startsWith("11")) return "⛈";   // Thunderstorm
    else if (icon.startsWith("13")) return "🌨";   // Snow
    else if (icon.startsWith("50")) return "🌫";   // Mist
    else return "🌤";
}

String WeatherClient::getTemperatureRange() {
    if (temperature < 0) return "FREEZING";
    else if (temperature < 10) return "COLD";
    else if (temperature < 20) return "COOL";
    else if (temperature < 25) return "COMFORTABLE";
    else if (temperature < 30) return "WARM";
    else if (temperature < 35) return "HOT";
    else return "VERY HOT";
}

String WeatherClient::getDressingSuggestion() {
    if (temperature < 0) return "Heavy coat, gloves, warm hat";
    else if (temperature < 10) return "Jacket, long pants, closed shoes";
    else if (temperature < 20) return "Light jacket or sweater";
    else if (temperature < 25) return "T-shirt and jeans";
    else if (temperature < 30) return "Light clothing, shorts";
    else if (temperature < 35) return "Minimal clothing, stay cool";
    else return "Light fabrics, avoid sun exposure";
}

String WeatherClient::getActivitySuggestion() {
    String weather = weatherIcon;
    if (weather.startsWith("01") || weather.startsWith("02")) {
        if (temperature > 15 && temperature < 30) return "Perfect for outdoor activities";
        else if (temperature >= 30) return "Stay hydrated, seek shade";
        else return "Bundle up for outdoor fun";
    }
    else if (weather.startsWith("10") || weather.startsWith("09")) return "Indoor activities recommended";
    else if (weather.startsWith("11")) return "Stay indoors, thunderstorms";
    else if (weather.startsWith("13")) return "Winter sports or cozy indoors";
    else if (weather.startsWith("50")) return "Limited visibility, be careful";
    else return "Check conditions before going out";
}

bool WeatherClient::isDataValid() {
    return validData && (millis() - lastUpdateTime < 300000); // Valid for 5 minutes
}

unsigned long WeatherClient::getLastUpdate() {
    return lastUpdateTime;
}

String WeatherClient::getFormattedTemperature() {
    return String((int)temperature) + "°C";
}

String WeatherClient::getFormattedFeelsLike() {
    return "Feels " + String((int)feelsLike) + "°C";
}

String WeatherClient::getFormattedWindSpeed() {
    return String(windSpeed, 1) + " m/s";
}

String WeatherClient::getFormattedHumidity() {
    return String(humidity) + "%";
}

void WeatherClient::drawWeatherIcon(TFT_eSprite& sprite, int x, int y, int size) {
    uint16_t color = getWeatherColor();
    String emoji = getWeatherEmoji();
    
    // Draw a simple weather representation
    if (weatherIcon.startsWith("01")) {
        // Sun
        sprite.fillCircle(x + size/2, y + size/2, size/3, TFT_YELLOW);
        for (int i = 0; i < 8; i++) {
            float angle = i * PI / 4;
            int x1 = x + size/2 + cos(angle) * (size/2 - 5);
            int y1 = y + size/2 + sin(angle) * (size/2 - 5);
            int x2 = x + size/2 + cos(angle) * (size/2);
            int y2 = y + size/2 + sin(angle) * (size/2);
            sprite.drawLine(x1, y1, x2, y2, TFT_YELLOW);
        }
    }
    else if (weatherIcon.startsWith("02") || weatherIcon.startsWith("03")) {
        // Partly cloudy
        sprite.fillCircle(x + size/3, y + size/3, size/4, TFT_YELLOW);
        sprite.fillCircle(x + size/2, y + size/2, size/3, TFT_LIGHTGREY);
        sprite.drawCircle(x + size/2, y + size/2, size/3, TFT_DARKGREY);
    }
    else if (weatherIcon.startsWith("04")) {
        // Cloudy
        sprite.fillCircle(x + size/3, y + size/2, size/4, TFT_LIGHTGREY);
        sprite.fillCircle(x + 2*size/3, y + size/2, size/4, TFT_LIGHTGREY);
        sprite.drawCircle(x + size/3, y + size/2, size/4, TFT_DARKGREY);
        sprite.drawCircle(x + 2*size/3, y + size/2, size/4, TFT_DARKGREY);
    }
    else if (weatherIcon.startsWith("09") || weatherIcon.startsWith("10")) {
        // Rain
        sprite.fillCircle(x + size/2, y + size/3, size/3, TFT_DARKGREY);
        sprite.drawCircle(x + size/2, y + size/3, size/3, TFT_LIGHTGREY);
        for (int i = 0; i < 5; i++) {
            int rainX = x + size/4 + (i * size/8);
            sprite.drawLine(rainX, y + 2*size/3, rainX, y + size - 5, TFT_CYAN);
        }
    }
    else if (weatherIcon.startsWith("11")) {
        // Thunderstorm
        sprite.fillCircle(x + size/2, y + size/3, size/3, TFT_DARKGREY);
        sprite.drawCircle(x + size/2, y + size/3, size/3, TFT_WHITE);
        // Lightning bolt
        sprite.drawLine(x + size/2, y + size/2, x + size/2 - 5, y + 3*size/4, TFT_YELLOW);
        sprite.drawLine(x + size/2 - 5, y + 3*size/4, x + size/2 + 3, y + size - 8, TFT_YELLOW);
    }
    else if (weatherIcon.startsWith("13")) {
        // Snow
        sprite.fillCircle(x + size/2, y + size/2, size/3, TFT_WHITE);
        sprite.drawCircle(x + size/2, y + size/2, size/3, TFT_LIGHTGREY);
        // Snowflakes
        for (int i = 0; i < 6; i++) {
            float angle = i * PI / 3;
            int x1 = x + size/2 + cos(angle) * (size/6);
            int y1 = y + size/2 + sin(angle) * (size/6);
            sprite.drawPixel(x1, y1, TFT_WHITE);
        }
    }
    else if (weatherIcon.startsWith("50")) {
        // Mist/Fog
        for (int i = 0; i < 4; i++) {
            int fogY = y + size/4 + (i * size/8);
            sprite.drawLine(x + 5, fogY, x + size - 5, fogY, TFT_LIGHTGREY);
        }
    }
}