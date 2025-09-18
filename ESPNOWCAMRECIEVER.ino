#include <WiFi.h>
#include <esp_now.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Resolution control - change this value (0-4) to adjust quality/speed
#define RESOLUTION_MODE 0

// Common packet structures (must match sender)
typedef struct {
  uint16_t packetNum;
  uint16_t totalPackets;
  uint16_t dataSize;
  uint8_t data[240];
} ImagePacket;

typedef struct {
  uint32_t imageSize;
  uint16_t width;
  uint16_t height;
  uint8_t format;
  uint8_t quality;
  uint8_t resolutionMode;
} ImageHeader;

// Resolution display configurations
typedef struct {
  const char* name;
  uint16_t width;
  uint16_t height;
  uint16_t displayX;  // X offset for centering on 240x240 display
  uint16_t displayY;  // Y offset for centering on 240x240 display
} DisplayConfig;

const DisplayConfig displayConfigs[] = {
  {"HQVGA",   240, 176, 0,  32},   // 240x176, centered vertically
  {"QVGA",    320, 240, 0,  0},    // 320x240 (will be cropped to center)
  {"CIF",     400, 296, 0,  0},    // 400x296 (will be cropped to center)
  {"QQVGA",   160, 120, 40, 60},   // 160x120, centered
  {"96x96",   96,  96,  72, 72}    // 96x96, centered
};

const int NUM_DISPLAY_MODES = sizeof(displayConfigs) / sizeof(displayConfigs[0]);

TFT_eSPI tft = TFT_eSPI();

typedef struct {
  uint8_t* imageData;
  uint32_t imageSize;
  uint16_t width;
  uint16_t height;
  uint8_t resolutionMode;
  uint32_t timestamp;
} CompleteImage;

// RTOS components
QueueHandle_t imageQueue;
QueueHandle_t packetQueue;
SemaphoreHandle_t displaySemaphore;
TaskHandle_t packetProcessingTaskHandle;
TaskHandle_t displayTaskHandle;

// Image reconstruction
uint8_t* imageBuffer = nullptr;
uint32_t expectedImageSize = 0;
uint16_t expectedTotalPackets = 0;
bool* packetReceived = nullptr;
uint32_t packetsReceivedCount = 0;
unsigned long lastPacketTime = 0;
bool receivingImage = false;
ImageHeader currentHeader;

// Performance tracking
volatile unsigned long lastDisplayTime = 0;
volatile int imagesReceived = 0;
volatile int imagesDisplayed = 0;
volatile int packetsProcessed = 0;

// TJpg callback with dynamic positioning
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (currentHeader.resolutionMode < NUM_DISPLAY_MODES) {
    const DisplayConfig& config = displayConfigs[currentHeader.resolutionMode];
    
    if (config.width <= 240 && config.height <= 240) {
      // Image fits, center it
      tft.pushImage(x + config.displayX, y + config.displayY, w, h, bitmap);
    } else {
      // Image larger than display, crop center portion
      int16_t cropX = (config.width - 240) / 2;
      int16_t cropY = (config.height - 240) / 2;
      
      if (x >= cropX && y >= cropY && 
          x + w <= cropX + 240 && y + h <= cropY + 240) {
        tft.pushImage(x - cropX, y - cropY, w, h, bitmap);
      }
    }
  } else {
    // Fallback - center on display
    int16_t centerX = (240 - currentHeader.width) / 2;
    int16_t centerY = (240 - currentHeader.height) / 2;
    if (centerX < 0) centerX = 0;
    if (centerY < 0) centerY = 0;
    tft.pushImage(x + centerX, y + centerY, w, h, bitmap);
  }
  return 1;
}

void packetProcessingTask(void* parameter) {
  ImagePacket packet;
  
  for(;;) {
    if (xQueueReceive(packetQueue, &packet, portMAX_DELAY) == pdTRUE) {
      packetsProcessed++;
      
      if (!receivingImage || packet.packetNum >= expectedTotalPackets) {
        continue;
      }
      
      if (packetReceived[packet.packetNum]) {
        continue;
      }
      
      uint32_t bufferPos = packet.packetNum * 240;
      uint32_t copySize = min((uint32_t)packet.dataSize, expectedImageSize - bufferPos);
      
      if (bufferPos + copySize <= expectedImageSize) {
        memcpy(&imageBuffer[bufferPos], packet.data, copySize);
        packetReceived[packet.packetNum] = true;
        packetsReceivedCount++;
        
        if (packetsReceivedCount >= expectedTotalPackets) {
          // Verify JPEG header
          if (imageBuffer[0] == 0xFF && imageBuffer[1] == 0xD8) {
            CompleteImage completeImg;
            completeImg.imageData = (uint8_t*)malloc(expectedImageSize);
            if (completeImg.imageData) {
              memcpy(completeImg.imageData, imageBuffer, expectedImageSize);
              completeImg.imageSize = expectedImageSize;
              completeImg.width = currentHeader.width;
              completeImg.height = currentHeader.height;
              completeImg.resolutionMode = currentHeader.resolutionMode;
              completeImg.timestamp = millis();
              
              if (xQueueSend(imageQueue, &completeImg, 0) != pdTRUE) {
                CompleteImage oldImg;
                if (xQueueReceive(imageQueue, &oldImg, 0) == pdTRUE) {
                  free(oldImg.imageData);
                }
                xQueueSend(imageQueue, &completeImg, 0);
              }
              
              imagesReceived++;
            }
          }
          resetImageReception();
        }
      }
      
      lastPacketTime = millis();
    }
  }
}

void displayTask(void* parameter) {
  CompleteImage image;
  
  for(;;) {
    if (xQueueReceive(imageQueue, &image, portMAX_DELAY) == pdTRUE) {
      xSemaphoreTake(displaySemaphore, portMAX_DELAY);
      
      unsigned long displayStart = millis();
      
      // Clear screen for new resolution mode
      static uint8_t lastResolutionMode = 255;
      if (image.resolutionMode != lastResolutionMode) {
        tft.fillScreen(TFT_BLUE);
        lastResolutionMode = image.resolutionMode;
      }
      
      // Update current header for callback
      currentHeader.width = image.width;
      currentHeader.height = image.height;
      currentHeader.resolutionMode = image.resolutionMode;
      
      if (TJpgDec.drawJpg(0, 0, image.imageData, image.imageSize) != JDR_OK) {
        tft.setTextColor(TFT_RED, TFT_BLUE);
        tft.setTextSize(1);
        tft.drawString("DECODE ERROR", 80, 110);
        Serial.printf("JPEG decode failed for %d bytes\n", image.imageSize);
      }
      
      xSemaphoreGive(displaySemaphore);
      
      free(image.imageData);
      imagesDisplayed++;
      
      unsigned long displayTime = millis() - displayStart;
      
      if (millis() - lastDisplayTime > 3000) {
        float receiveFPS = imagesReceived / ((millis() - lastDisplayTime) / 1000.0);
        float displayFPS = imagesDisplayed / ((millis() - lastDisplayTime) / 1000.0);
        
        if (image.resolutionMode < NUM_DISPLAY_MODES) {
          Serial.printf("Mode %d (%s): Rx %.1f FPS, Display %.1f FPS, %lums\n", 
                        image.resolutionMode, displayConfigs[image.resolutionMode].name,
                        receiveFPS, displayFPS, displayTime);
        }
        
        imagesReceived = 0;
        imagesDisplayed = 0;
        packetsProcessed = 0;
        lastDisplayTime = millis();
      }
    }
  }
}

void resetImageReception() {
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

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(ImageHeader)) {
    ImageHeader* header = (ImageHeader*)incomingData;
    
    resetImageReception();
    
    expectedImageSize = header->imageSize;
    expectedTotalPackets = (expectedImageSize + 239) / 240;
    currentHeader = *header;
    
    imageBuffer = (uint8_t*)malloc(expectedImageSize);
    packetReceived = (bool*)calloc(expectedTotalPackets, sizeof(bool));
    
    if (imageBuffer && packetReceived) {
      receivingImage = true;
      packetsReceivedCount = 0;
      lastPacketTime = millis();
    } else {
      resetImageReception();
    }
    return;
  }
  
  if (len == sizeof(ImagePacket) && receivingImage) {
    ImagePacket packet = *(ImagePacket*)incomingData;
    xQueueSendFromISR(packetQueue, &packet, NULL);
  }
}

void checkTimeout() {
  if (receivingImage && (millis() - lastPacketTime > 3000)) {
    resetImageReception();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("RTOS Camera Receiver Starting (240x240 Display)");
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  
  tft.drawString("Camera", 70, 100);
  tft.drawString("Ready", 80, 130);
  
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
  
  WiFi.mode(WIFI_STA);
  Serial.printf("Receiver MAC: %s\n", WiFi.macAddress().c_str());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  
  esp_now_register_recv_cb(OnDataRecv);
  
  imageQueue = xQueueCreate(2, sizeof(CompleteImage));
  packetQueue = xQueueCreate(150, sizeof(ImagePacket));
  displaySemaphore = xSemaphoreCreateMutex();
  
  if (!imageQueue || !packetQueue || !displaySemaphore) {
    Serial.println("Failed to create RTOS components");
    return;
  }
  
  xTaskCreatePinnedToCore(packetProcessingTask, "PacketProcessor", 4096, NULL, 2, &packetProcessingTaskHandle, 0);
  xTaskCreatePinnedToCore(displayTask, "DisplayTask", 8192, NULL, 3, &displayTaskHandle, 1);
  
  Serial.println("RTOS Camera Receiver Ready!");
  
  delay(2000);
  tft.fillScreen(TFT_BLUE);
  lastDisplayTime = millis();
}

void loop() {
  checkTimeout();
  vTaskDelay(pdMS_TO_TICKS(100));
}