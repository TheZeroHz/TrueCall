// CommunicationManager.cpp
#include "CommunicationManager.h"

CommunicationManager* CommunicationManager::instance = nullptr;

CommunicationManager::CommunicationManager() {
  instance = this;
  imageBuffer = nullptr;
  expectedImageSize = 0;
  expectedTotalPackets = 0;
  packetReceived = nullptr;
  packetsReceivedCount = 0;
  receivingImage = false;
  lastPacketTime = 0;
  totalReceived = 0;
  totalLost = 0;
}

static void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("[Master] Command sent successfully");
  } else {
    Serial.println("[Master] Command send FAILED!");
  }
}

bool CommunicationManager::begin() {
  Serial.println("Initializing communication...");
  
  WiFi.mode(WIFI_STA);
  Serial.printf("Master MAC: %s\n", WiFi.macAddress().c_str());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return false;
  }
  
  esp_now_register_recv_cb(onDataReceived);
  esp_now_register_send_cb(onDataSent);
  
  // Add peer (slave camera)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, slaveMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer!");
    return false;
  }
  
  // Create RTOS components
  imageQueue = xQueueCreate(2, sizeof(CompleteImage));
  packetQueue = xQueueCreate(150, sizeof(ImagePacket));
  rxMutex = xSemaphoreCreateMutex();
  
  if (!imageQueue || !packetQueue || !rxMutex) {
    Serial.println("Failed to create RTOS components!");
    return false;
  }
  
  // Start packet processing task
  xTaskCreatePinnedToCore(
    packetProcessingTask,
    "PacketProc",
    4096,
    this,
    2,
    &packetTaskHandle,
    0
  );
  
  Serial.println("Communication initialized successfully");
  return true;
}

void CommunicationManager::setSlaveMac(uint8_t* mac) {
  memcpy(slaveMac, mac, 6);
  Serial.printf("Slave MAC set to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool CommunicationManager::sendCommand(const char* command) {
  CommandPacket cmd;
  strncpy(cmd.command, command, sizeof(cmd.command) - 1);
  cmd.command[sizeof(cmd.command) - 1] = '\0';
  cmd.timestamp = millis();
  
  esp_err_t result = esp_now_send(slaveMac, (uint8_t*)&cmd, sizeof(cmd));
  
  if (result == ESP_OK) {
    Serial.printf("Command sent: %s\n", command);
    return true;
  } else {
    Serial.printf("Command send failed: %s\n", command);
    return false;
  }
}

bool CommunicationManager::sendTextMessage(const char* message) {
  TextMessagePacket msg;
  strncpy(msg.message, message, sizeof(msg.message) - 1);
  msg.message[sizeof(msg.message) - 1] = '\0';
  msg.timestamp = millis();
  msg.fromMaster = 1;  // From master
  
  esp_err_t result = esp_now_send(slaveMac, (uint8_t*)&msg, sizeof(msg));
  
  if (result == ESP_OK) {
    Serial.printf("📤 [Message sent to Slave]: %s\n", message);
    return true;
  } else {
    Serial.printf("❌ Message send failed: %s\n", message);
    return false;
  }
}

void CommunicationManager::onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
  if (!instance) return;
  
  // Check for image header
  if (len == sizeof(ImageHeader)) {
    ImageHeader* header = (ImageHeader*)data;
    
    Serial.printf("[Master] Receiving new image: %dx%d, %d bytes, %d packets\n",
                  header->width, header->height, header->imageSize,
                  (header->imageSize + 239) / 240);
    
    xSemaphoreTake(instance->rxMutex, portMAX_DELAY);
    instance->resetReception();
    
    instance->expectedImageSize = header->imageSize;
    instance->expectedTotalPackets = (header->imageSize + 239) / 240;
    instance->currentHeader = *header;
    
    instance->imageBuffer = (uint8_t*)malloc(header->imageSize);
    instance->packetReceived = (bool*)calloc(instance->expectedTotalPackets, sizeof(bool));
    
    if (instance->imageBuffer && instance->packetReceived) {
      instance->receivingImage = true;
      instance->packetsReceivedCount = 0;
      instance->lastPacketTime = millis();
      Serial.printf("Receiving image: %dx%d, %d bytes, %d packets\n",
                    header->width, header->height, header->imageSize, 
                    instance->expectedTotalPackets);
    } else {
      instance->resetReception();
      Serial.println("Memory allocation failed!");
    }
    
    xSemaphoreGive(instance->rxMutex);
    return;
  }
  
  // Check for image packet
  if (len == sizeof(ImagePacket) && instance->receivingImage) {
    ImagePacket packet = *(ImagePacket*)data;
    xQueueSendFromISR(instance->packetQueue, &packet, NULL);
    return;
  }
  
  // Check for text message
  if (len == sizeof(TextMessagePacket)) {
    TextMessagePacket* msg = (TextMessagePacket*)data;
    if (msg->fromMaster == 0) {  // Message from slave
      Serial.printf("\n📨 [Message from Slave]: %s\n\n", msg->message);
    }
    return;
  }
}

void CommunicationManager::packetProcessingTask(void* param) {
  CommunicationManager* self = (CommunicationManager*)param;
  ImagePacket packet;
  
  for(;;) {
    if (xQueueReceive(self->packetQueue, &packet, portMAX_DELAY) == pdTRUE) {
      self->processPacket(packet);
    }
  }
}

void CommunicationManager::processPacket(const ImagePacket& packet) {
  xSemaphoreTake(rxMutex, portMAX_DELAY);
  
  if (!receivingImage || packet.packetNum >= expectedTotalPackets) {
    xSemaphoreGive(rxMutex);
    return;
  }
  
  if (packetReceived[packet.packetNum]) {
    xSemaphoreGive(rxMutex);
    return;
  }
  
  uint32_t bufferPos = packet.packetNum * 240;
  uint32_t copySize = min((uint32_t)packet.dataSize, expectedImageSize - bufferPos);
  
  if (bufferPos + copySize <= expectedImageSize) {
    memcpy(&imageBuffer[bufferPos], packet.data, copySize);
    packetReceived[packet.packetNum] = true;
    packetsReceivedCount++;
    lastPacketTime = millis();
    
    // Check if image is complete
    if (packetsReceivedCount >= expectedTotalPackets) {
      // Verify JPEG header
      if (imageBuffer[0] == 0xFF && imageBuffer[1] == 0xD8) {
        CompleteImage img;
        img.imageData = (uint8_t*)malloc(expectedImageSize);
        
        if (img.imageData) {
          memcpy(img.imageData, imageBuffer, expectedImageSize);
          img.imageSize = expectedImageSize;
          img.width = currentHeader.width;
          img.height = currentHeader.height;
          img.resolutionMode = currentHeader.resolutionMode;
          img.timestamp = millis();
          
          if (xQueueSend(imageQueue, &img, 0) != pdTRUE) {
            // Queue full, remove old image
            CompleteImage oldImg;
            if (xQueueReceive(imageQueue, &oldImg, 0) == pdTRUE) {
              free(oldImg.imageData);
            }
            xQueueSend(imageQueue, &img, 0);
          }
          
          totalReceived++;
          Serial.printf("Image complete! (%d/%d packets)\n", 
                       packetsReceivedCount, expectedTotalPackets);
        }
      } else {
        Serial.println("Invalid JPEG header!");
        totalLost++;
      }
      
      resetReception();
    }
  }
  
  xSemaphoreGive(rxMutex);
}

void CommunicationManager::resetReception() {
  if (imageBuffer) {
    free(imageBuffer);
    imageBuffer = nullptr;
  }
  if (packetReceived) {
    free(packetReceived);
    packetReceived = nullptr;
  }
  expectedImageSize = 0;
  expectedTotalPackets = 0;
  packetsReceivedCount = 0;
  receivingImage = false;
}

void CommunicationManager::processIncomingData() {
  // Check for timeout - increased to 5 seconds for reliability
  if (receivingImage && (millis() - lastPacketTime > 5000)) {
    xSemaphoreTake(rxMutex, portMAX_DELAY);
    Serial.printf("[Master] Reception timeout! Got %d/%d packets\n", 
                  packetsReceivedCount, expectedTotalPackets);
    totalLost++;
    resetReception();
    xSemaphoreGive(rxMutex);
  }
}

bool CommunicationManager::hasCompleteImage() {
  return uxQueueMessagesWaiting(imageQueue) > 0;
}

CompleteImage CommunicationManager::getCompleteImage() {
  CompleteImage img;
  xQueueReceive(imageQueue, &img, 0);
  return img;
}

void CommunicationManager::freeImage(CompleteImage& img) {
  if (img.imageData) {
    free(img.imageData);
    img.imageData = nullptr;
  }
}