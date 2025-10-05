// WiFiScanner.h - Using BLOCKING scan (more reliable)
#ifndef WIFISCANNER_H
#define WIFISCANNER_H

#include <WiFi.h>
#include "WiFiNetwork.h"
#include "Config.h"

class WiFiScanner {
private:
  WiFiNetwork networks[MAX_NETWORKS];
  int networkCount;
  bool isScanning;
  unsigned long lastScanTime;

  void processResults() {
    Serial.printf("processResults() with %d networks\n", networkCount);
    
    int beforeDedupe = networkCount;
    for (int i = 0; i < networkCount; i++) {
      for (int j = i + 1; j < networkCount; j++) {
        if (networks[i].bssid == networks[j].bssid) {
          if (networks[j].rssi > networks[i].rssi) {
            networks[i] = networks[j];
          }
          for (int k = j; k < networkCount - 1; k++) {
            networks[k] = networks[k + 1];
          }
          networkCount--;
          j--;
        }
      }
    }
    Serial.printf("  After dedupe: %d -> %d\n", beforeDedupe, networkCount);

    // Sort by RSSI
    for (int i = 0; i < networkCount - 1; i++) {
      for (int j = i + 1; j < networkCount; j++) {
        if (networks[j].rssi > networks[i].rssi) {
          WiFiNetwork temp = networks[i];
          networks[i] = networks[j];
          networks[j] = temp;
        }
      }
    }
  }

public:
  WiFiScanner() : networkCount(0), isScanning(false), lastScanTime(0) {}

  void begin() {
    Serial.println("WiFiScanner::begin()");
    
    // Complete reset
    WiFi.mode(WIFI_OFF);
    delay(500);
    
    WiFi.mode(WIFI_STA);
    delay(500);
    
    Serial.printf("  WiFi mode: %d (should be 1 for STA)\n", WiFi.getMode());
  }

  // Blocking scan - more reliable
  bool startScanBlocking() {
    Serial.println("\n>>> startScanBlocking()");
    
    if (isScanning) {
      Serial.println("  Already scanning!");
      return false;
    }
    
    isScanning = true;
    
    Serial.println("  Calling WiFi.scanNetworks(false, true)...");
    int n = WiFi.scanNetworks(false, true); // blocking=false means sync, show_hidden=true
    
    Serial.printf("  scanNetworks() returned: %d\n", n);
    
    if (n < 0) {
      Serial.printf("  ERROR: Scan failed with code %d\n", n);
      isScanning = false;
      return false;
    }
    
    if (n == 0) {
      Serial.println("  No networks found");
      networkCount = 0;
      isScanning = false;
      return true;
    }

    Serial.printf("  Found %d networks, processing...\n", n);
    networkCount = min(n, MAX_NETWORKS);

    for (int i = 0; i < networkCount; i++) {
      networks[i].ssid = WiFi.SSID(i);
      networks[i].bssid = WiFi.BSSIDstr(i);
      networks[i].rssi = WiFi.RSSI(i);
      networks[i].channel = WiFi.channel(i);
      networks[i].encryptionType = WiFi.encryptionType(i);
      networks[i].isHidden = (networks[i].ssid.length() == 0);

      if (networks[i].isHidden) {
        networks[i].ssid = "<Hidden Network>";
      }
      
      Serial.printf("    [%d] '%s' RSSI=%d Ch=%d\n",
        i, networks[i].ssid.c_str(), networks[i].rssi, networks[i].channel);
    }

    processResults();
    lastScanTime = millis();
    isScanning = false;
    
    Serial.printf("  SUCCESS: %d networks ready\n", networkCount);
    return true;
  }

  int getNetworkCount() const {
    return networkCount;
  }

  WiFiNetwork* getNetwork(int index) {
    if (index >= 0 && index < networkCount) {
      return &networks[index];
    }
    return nullptr;
  }

  WiFiNetwork* getNetworks() {
    return networks;
  }

  bool getIsScanning() const {
    return isScanning;
  }

  void reset() {
    Serial.println("WiFiScanner::reset()");
    networkCount = 0;
    isScanning = false;
  }
};

#endif