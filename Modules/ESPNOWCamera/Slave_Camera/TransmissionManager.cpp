// TransmissionManager.cpp
#include "TransmissionManager.h"

TransmissionManager* TransmissionManager::instance = nullptr;

TransmissionManager::TransmissionManager() {
  instance = this;
  framesSent = 0;
  sendFailures = 0;
  totalTransmitTime = 0;
}

bool TransmissionManager::begin() {
  Serial.println("Initializing transmission...");
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);  // Enable WiFi sleep to reduce power
  Serial.printf("Slave MAC: %s\n", WiFi.macAddress().c_str());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return false;
  }
  
  esp_now_register_send_cb(onDataSent);
  
  // Add master as peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, masterMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer!");
    return false;
  }
  
  Serial.println("Transmission initialized successfully");
  return true;
}

void TransmissionManager::setMasterMac(uint8_t* mac) {
  memcpy(masterMac, mac, 6);
  Serial.printf("Master MAC set to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void TransmissionManager::onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    // Packet sent successfully (comment out to reduce serial spam)
    // Serial.print(".");
  } else {
    Serial.println("\n[Slave] Packet send FAILED!");
    if (instance) {
      instance->sendFailures++;
    }
  }
}

bool TransmissionManager::sendFrame(camera_fb_t* fb) {
  if (!fb) {
    Serial.println("[Slave] Cannot send null frame!");
    return false;
  }
  
  unsigned long startTime = millis();
  
  Serial.printf("[Slave] Sending frame: %d bytes, %d packets\n", 
                fb->len, (fb->len + 239) / 240);
  
  // Send header first
  ImageHeader header;
  header.imageSize = fb->len;
  header.width = fb->width;
  header.height = fb->height;
  header.format = 0;
  header.quality = 4;
  header.resolutionMode = 1;
  
  esp_err_t result = esp_now_send(masterMac, (uint8_t*)&header, sizeof(header));
  if (result != ESP_OK) {
    Serial.println("[Slave] Header send failed!");
    sendFailures++;
    return false;
  }
  
  delay(10);  // Increased delay for header to be processed
  
  // Send image data in packets
  uint16_t totalPackets = (fb->len + 239) / 240;
  int failedPackets = 0;
  
  for (uint16_t i = 0; i < totalPackets; i++) {
    ImagePacket packet;
    packet.packetNum = i;
    packet.totalPackets = totalPackets;
    
    uint32_t startIdx = i * 240;
    uint32_t endIdx = min(startIdx + 240, (uint32_t)fb->len);
    packet.dataSize = endIdx - startIdx;
    
    memcpy(packet.data, &fb->buf[startIdx], packet.dataSize);
    
    result = esp_now_send(masterMac, (uint8_t*)&packet, sizeof(packet));
    if (result != ESP_OK) {
      failedPackets++;
    }
    
    delay(2);  // Small delay between packets
  }
  
  unsigned long transmitTime = millis() - startTime;
  totalTransmitTime += transmitTime;
  
  if (failedPackets > 0) {
    Serial.printf("[Slave] Transmission completed with %d failed packets\n", failedPackets);
    sendFailures++;
    return false;
  }
  
  framesSent++;
  Serial.printf("[Slave] Frame sent successfully: %d packets in %lu ms\n", 
                totalPackets, transmitTime);
  
  return true;
}

bool TransmissionManager::sendTextMessage(const char* message) {
  TextMessagePacket msg;
  strncpy(msg.message, message, sizeof(msg.message) - 1);
  msg.message[sizeof(msg.message) - 1] = '\0';
  msg.timestamp = millis();
  msg.fromMaster = 0;  // From slave
  
  esp_err_t result = esp_now_send(masterMac, (uint8_t*)&msg, sizeof(msg));
  
  if (result == ESP_OK) {
    Serial.printf("📤 [Message sent to Master]: %s\n", message);
    return true;
  } else {
    Serial.printf("❌ Message send failed: %s\n", message);
    return false;
  }
}

unsigned long TransmissionManager::getAvgTransmitTime() {
  if (framesSent == 0) return 0;
  return totalTransmitTime / framesSent;
}

void TransmissionManager::resetStats() {
  framesSent = 0;
  sendFailures = 0;
  totalTransmitTime = 0;
}