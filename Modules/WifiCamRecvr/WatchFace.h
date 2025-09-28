// WatchFace.h
#ifndef WATCHFACE_H
#define WATCHFACE_H

#include <time.h>
#include "Config.h"
#include "DisplayManager.h"
#include "WeatherManager.h"

class WatchFace {
private:
    DisplayManager* display;
    WeatherManager* weather;
    struct tm timeInfo;
    
    void drawBackground();
    void drawTimePanel();
    void drawDateDisplay();
    void drawWeatherDisplay();
    void drawSystemIndicators();
    
public:
    WatchFace();
    void begin(DisplayManager* disp, WeatherManager* wth);
    void draw();
};

#endif // WATCHFACE_H