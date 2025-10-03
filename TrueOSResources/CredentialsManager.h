// CredentialsManager.h - Save and load WiFi credentials
#ifndef CREDENTIALSMANAGER_H
#define CREDENTIALSMANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>

#define CREDENTIALS_FILE "/wifi_creds.txt"
#define MAX_SAVED_NETWORKS 10

struct SavedNetwork {
    String ssid;
    String password;
    int rssi;
    unsigned long lastConnected;
    
    SavedNetwork() : ssid(""), password(""), rssi(-100), lastConnected(0) {}
};

class CredentialsManager {
private:
    SavedNetwork networks[MAX_SAVED_NETWORKS];
    int networkCount;
    
public:
    CredentialsManager() : networkCount(0) {}
    
    bool begin() {
        if (!FFat.begin()) {
            Serial.println("FFat mount failed for credentials");
            return false;
        }
        
        load();
        return true;
    }
    
    bool save(const String& ssid, const String& password, int rssi = -50) {
        Serial.printf("\n>>> Saving credentials for '%s'\n", ssid.c_str());
        
        // Check if network already exists
        int existingIndex = -1;
        for (int i = 0; i < networkCount; i++) {
            if (networks[i].ssid == ssid) {
                existingIndex = i;
                break;
            }
        }
        
        if (existingIndex >= 0) {
            // Update existing
            networks[existingIndex].password = password;
            networks[existingIndex].rssi = rssi;
            networks[existingIndex].lastConnected = millis();
            Serial.println("  Updated existing network");
        } else {
            // Add new
            if (networkCount < MAX_SAVED_NETWORKS) {
                networks[networkCount].ssid = ssid;
                networks[networkCount].password = password;
                networks[networkCount].rssi = rssi;
                networks[networkCount].lastConnected = millis();
                networkCount++;
                Serial.printf("  Added new network (total: %d)\n", networkCount);
            } else {
                // Replace oldest
                int oldestIndex = 0;
                unsigned long oldestTime = networks[0].lastConnected;
                
                for (int i = 1; i < networkCount; i++) {
                    if (networks[i].lastConnected < oldestTime) {
                        oldestTime = networks[i].lastConnected;
                        oldestIndex = i;
                    }
                }
                
                networks[oldestIndex].ssid = ssid;
                networks[oldestIndex].password = password;
                networks[oldestIndex].rssi = rssi;
                networks[oldestIndex].lastConnected = millis();
                Serial.printf("  Replaced oldest network at index %d\n", oldestIndex);
            }
        }
        
        return writeToFile();
    }
    
    bool load() {
        Serial.println("\n>>> Loading saved credentials");
        
        if (!FFat.exists(CREDENTIALS_FILE)) {
            Serial.println("  No saved credentials found");
            return false;
        }
        
        fs::File file = FFat.open(CREDENTIALS_FILE, "r");
        if (!file) {
            Serial.println("  Failed to open credentials file");
            return false;
        }
        
        networkCount = 0;
        
        while (file.available() && networkCount < MAX_SAVED_NETWORKS) {
            String line = file.readStringUntil('\n');
            line.trim();
            
            if (line.length() == 0) continue;
            
            // Format: SSID|PASSWORD|RSSI|TIMESTAMP
            int sep1 = line.indexOf('|');
            int sep2 = line.indexOf('|', sep1 + 1);
            int sep3 = line.indexOf('|', sep2 + 1);
            
            if (sep1 > 0 && sep2 > sep1 && sep3 > sep2) {
                networks[networkCount].ssid = line.substring(0, sep1);
                networks[networkCount].password = line.substring(sep1 + 1, sep2);
                networks[networkCount].rssi = line.substring(sep2 + 1, sep3).toInt();
                networks[networkCount].lastConnected = line.substring(sep3 + 1).toInt();
                
                Serial.printf("  [%d] %s (RSSI: %d)\n", 
                    networkCount, 
                    networks[networkCount].ssid.c_str(),
                    networks[networkCount].rssi);
                
                networkCount++;
            }
        }
        
        file.close();
        Serial.printf("  Loaded %d saved networks\n", networkCount);
        return true;
    }
    
    bool writeToFile() {
        Serial.println("  Writing to file...");
        
        fs::File file = FFat.open(CREDENTIALS_FILE, "w");
        if (!file) {
            Serial.println("  ERROR: Failed to open file for writing");
            return false;
        }
        
        for (int i = 0; i < networkCount; i++) {
            file.print(networks[i].ssid);
            file.print('|');
            file.print(networks[i].password);
            file.print('|');
            file.print(networks[i].rssi);
            file.print('|');
            file.println(networks[i].lastConnected);
        }
        
        file.close();
        Serial.println("  Credentials saved successfully");
        return true;
    }
    
    bool getCredentials(const String& ssid, String& password) {
        for (int i = 0; i < networkCount; i++) {
            if (networks[i].ssid == ssid) {
                password = networks[i].password;
                Serial.printf("Found saved password for '%s'\n", ssid.c_str());
                return true;
            }
        }
        return false;
    }
    
    bool hasCredentials(const String& ssid) {
        for (int i = 0; i < networkCount; i++) {
            if (networks[i].ssid == ssid) {
                return true;
            }
        }
        return false;
    }
    
    void remove(const String& ssid) {
        for (int i = 0; i < networkCount; i++) {
            if (networks[i].ssid == ssid) {
                // Shift remaining networks
                for (int j = i; j < networkCount - 1; j++) {
                    networks[j] = networks[j + 1];
                }
                networkCount--;
                writeToFile();
                Serial.printf("Removed credentials for '%s'\n", ssid.c_str());
                return;
            }
        }
    }
    
    int getCount() const {
        return networkCount;
    }
    
    SavedNetwork* getNetwork(int index) {
        if (index >= 0 && index < networkCount) {
            return &networks[index];
        }
        return nullptr;
    }
    
    // Get most recently connected network
    SavedNetwork* getMostRecent() {
        if (networkCount == 0) return nullptr;
        
        int mostRecentIndex = 0;
        unsigned long mostRecentTime = networks[0].lastConnected;
        
        for (int i = 1; i < networkCount; i++) {
            if (networks[i].lastConnected > mostRecentTime) {
                mostRecentTime = networks[i].lastConnected;
                mostRecentIndex = i;
            }
        }
        
        return &networks[mostRecentIndex];
    }
    
    void clear() {
        networkCount = 0;
        FFat.remove(CREDENTIALS_FILE);
        Serial.println("All credentials cleared");
    }
};

#endif