// MenuManager.h
#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include "Config.h"
#include "DisplayManager.h"

class MenuManager {
private:
    DisplayManager* display;
    int selectedIndex;
    
    struct MenuItem {
        String label;
        SystemMode mode;
    };
    
    static const int MENU_COUNT = 6;
    MenuItem items[MENU_COUNT];
    
public:
    MenuManager();
    void begin();
    void draw();
    void navigateNext();
    void navigatePrev();
    SystemMode selectCurrent();
    String getCurrentLabel() const;
};

#endif




