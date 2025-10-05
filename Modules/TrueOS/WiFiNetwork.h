// WiFiNetwork.h - WiFi Network Data Structure
#ifndef WIFINETWORK_H
#define WIFINETWORK_H

#include <Arduino.h>

struct WiFiNetwork {
    String ssid;
    String bssid;
    int rssi;
    uint8_t channel;
    uint8_t encryptionType;
    bool isHidden;
    
    WiFiNetwork() : 
        ssid(""), 
        bssid(""),
        rssi(-100), 
        channel(0), 
        encryptionType(0),
        isHidden(false) {}
    
    bool isEncrypted() const {
        return encryptionType != 0; // 0 = WIFI_AUTH_OPEN
    }
    
    String getEncryptionName() const {
        switch(encryptionType) {
            case 2: return "WPA";
            case 3: return "WPA2";
            case 4: return "WPA/WPA2";
            case 5: return "WPA2/WPA3";
            case 8: return "WPA3";
            default: return isEncrypted() ? "SEC" : "OPEN";
        }
    }
};

#endif // WIFINETWORK_H