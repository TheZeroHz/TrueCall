#include "esp_camera.h"
#include <WiFi.h>
#include <esp_now.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Resolution control - change this value (0-4) to adjust quality/speed
#define RESOLUTION_MODE 0

// Automatic resolution adjustment based on performance
#define AUTO_RESOLUTION_ADJUST 0

// Performance thresholds for auto-adjustment
#define TARGET_FPS 5.0f
#define MIN_FPS_THRESHOLD 4.0f
#define MAX_FPS_THRESHOLD 6.0f

// Camera pins for Freenove ESP32-S3
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

uint8_t receiverMac[] = {0x84, 0xFC, 0xE6, 0x50, 0xA2, 0x2C}; // UPDATE WITH YOUR RECEIVER MAC

// Common packet structures
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

// Resolution configurations (using proper ESP32 frame sizes)
typedef struct {
  framesize_t frameSize;
  uint8_t quality;
  const char* name;
  uint16_t width;
  uint16_t height;
  uint16_t maxExpectedSize;
  uint16_t displayX;  // X offset for centering on 240x240 display
  uint16_t displayY;  // Y offset for centering on 240x240 display
} ResolutionConfig;

const ResolutionConfig resolutionConfigs[] = {
  {FRAMESIZE_HQVGA,   14,  "HQVGA",   240, 176, 25000, 0,  32},   // 240x176, best fit for 240x240
  {FRAMESIZE_QVGA,    18, "QVGA",    320, 240, 30000, 0,  0},    // 320x240 (will be cropped)
  {FRAMESIZE_CIF,     12, "CIF",     400, 296, 35000, 0,  0},    // 400x296 (will be cropped)
  {FRAMESIZE_QQVGA,   15, "QQVGA",   160, 120, 12000, 40, 60},   // 160x120, centered
  {FRAMESIZE_96X96,   18, "96x96",   96,  96,  6000,  72, 72}    // 96x96, centered
};

const int NUM_RESOLUTION_MODES = sizeof(resolutionConfigs) / sizeof(resolutionConfigs[0]);

typedef struct {
  camera_fb_t* fb;
  uint32_t timestamp;
} FrameBuffer;

// RTOS components
QueueHandle_t frameQueue;
QueueHandle_t transmitQueue;
SemaphoreHandle_t wifiSemaphore;
TaskHandle_t captureTaskHandle;
TaskHandle_t transmitTaskHandle;
TaskHandle_t packetTaskHandle;

// Performance tracking
volatile unsigned long lastFPSTime = 0;
volatile int capturedFrames = 0;
volatile int transmittedFrames = 0;
volatile int droppedFrames = 0;
volatile int currentResolutionMode = RESOLUTION_MODE;
volatile float currentFPS = 0.0f;
volatile unsigned long lastResolutionAdjust = 0;

void adjustResolution() {
  if (!AUTO_RESOLUTION_ADJUST) return;
  if (millis() - lastResolutionAdjust < 5000) return;
  
  int newMode = currentResolutionMode;
  
  if (currentFPS < MIN_FPS_THRESHOLD && currentResolutionMode < NUM_RESOLUTION_MODES - 1) {
    newMode = currentResolutionMode + 1;
    Serial.printf("FPS too low (%.1f), reducing to mode %d (%s)\n", 
                  currentFPS, newMode, resolutionConfigs[newMode].name);
  } else if (currentFPS > MAX_FPS_THRESHOLD && currentResolutionMode > 0) {
    newMode = currentResolutionMode - 1;
    Serial.printf("FPS good (%.1f), increasing to mode %d (%s)\n", 
                  currentFPS, newMode, resolutionConfigs[newMode].name);
  }
  
  if (newMode != currentResolutionMode) {
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
      s->set_framesize(s, resolutionConfigs[newMode].frameSize);
      s->set_quality(s, resolutionConfigs[newMode].quality);
      currentResolutionMode = newMode;
      Serial.printf("Resolution: %s (%dx%d), Quality: %d\n", 
                    resolutionConfigs[newMode].name,
                    resolutionConfigs[newMode].width,
                    resolutionConfigs[newMode].height,
                    resolutionConfigs[newMode].quality);
    }
    lastResolutionAdjust = millis();
  }
}

void captureTask(void* parameter) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  
  for(;;) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1));
      continue;
    }
    
    FrameBuffer frameWrapper;
    frameWrapper.fb = fb;
    frameWrapper.timestamp = millis();
    
    if (xQueueSend(frameQueue, &frameWrapper, 0) != pdTRUE) {
      FrameBuffer oldFrame;
      if (xQueueReceive(frameQueue, &oldFrame, 0) == pdTRUE) {
        esp_camera_fb_return(oldFrame.fb);
        droppedFrames++;
      }
      if (xQueueSend(frameQueue, &frameWrapper, 0) != pdTRUE) {
        esp_camera_fb_return(fb);
        droppedFrames++;
      }
    }
    
    capturedFrames++;
    
    // Adaptive frame rate based on resolution
    int delayMs = (currentResolutionMode == 0) ? 120 : // HQVGA: 8 FPS
                  (currentResolutionMode == 1) ? 100 : // QVGA: 10 FPS  
                  (currentResolutionMode == 2) ? 80  : // CIF: 12 FPS
                  (currentResolutionMode == 3) ? 50  : // QQVGA: 20 FPS
                                                33;    // 96x96: 30 FPS
    
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(delayMs));
  }
}

void packetTransmissionTask(void* parameter) {
  ImagePacket packet;
  
  for(;;) {
    if (xQueueReceive(transmitQueue, &packet, portMAX_DELAY) == pdTRUE) {
      xSemaphoreTake(wifiSemaphore, portMAX_DELAY);
      esp_now_send(receiverMac, (uint8_t*)&packet, sizeof(packet));
      vTaskDelay(pdMS_TO_TICKS(1));
      xSemaphoreGive(wifiSemaphore);
    }
  }
}

void transmitTask(void* parameter) {
  FrameBuffer frameWrapper;
  
  for(;;) {
    if (xQueueReceive(frameQueue, &frameWrapper, portMAX_DELAY) == pdTRUE) {
      camera_fb_t* fb = frameWrapper.fb;
      
      xSemaphoreTake(wifiSemaphore, portMAX_DELAY);
      
      ImageHeader header;
      header.imageSize = fb->len;
      header.width = fb->width;
      header.height = fb->height;
      header.format = 0;
      header.quality = resolutionConfigs[currentResolutionMode].quality;
      header.resolutionMode = currentResolutionMode;
      
      esp_now_send(receiverMac, (uint8_t*)&header, sizeof(header));
      vTaskDelay(pdMS_TO_TICKS(3));
      xSemaphoreGive(wifiSemaphore);
      
      uint16_t totalPackets = (fb->len + 239) / 240;
      
      for (uint16_t i = 0; i < totalPackets; i++) {
        ImagePacket packet;
        packet.packetNum = i;
        packet.totalPackets = totalPackets;
        
        uint32_t startIdx = i * 240;
        uint32_t endIdx = min(startIdx + 240, (uint32_t)fb->len);
        packet.dataSize = endIdx - startIdx;
        
        memcpy(packet.data, &fb->buf[startIdx], packet.dataSize);
        xQueueSend(transmitQueue, &packet, portMAX_DELAY);
      }
      
      esp_camera_fb_return(fb);
      transmittedFrames++;
      
      // Calculate FPS and adjust resolution
      if (millis() - lastFPSTime > 3000) {
        float captureFPS = capturedFrames / ((millis() - lastFPSTime) / 1000.0);
        float transmitFPS = transmittedFrames / ((millis() - lastFPSTime) / 1000.0);
        currentFPS = transmitFPS;
        
        Serial.printf("Mode %d (%s): Cap %.1f FPS, Tx %.1f FPS, Drop %d, Q %d\n", 
                      currentResolutionMode, resolutionConfigs[currentResolutionMode].name,
                      captureFPS, transmitFPS, droppedFrames, 
                      uxQueueMessagesWaiting(frameQueue));
        
        adjustResolution();
        
        capturedFrames = 0;
        transmittedFrames = 0;
        droppedFrames = 0;
        lastFPSTime = millis();
      }
    }
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Minimal callback
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Starting RTOS Camera Sender - Resolution Mode %d (%s)\n", 
                RESOLUTION_MODE, resolutionConfigs[RESOLUTION_MODE].name);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  
  config.frame_size = resolutionConfigs[RESOLUTION_MODE].frameSize;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = resolutionConfigs[RESOLUTION_MODE].quality;
  config.fb_count = 2;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x", err);
    return;
  }
  
  WiFi.mode(WIFI_STA);
  Serial.printf("Sender MAC: %s\n", WiFi.macAddress().c_str());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  
  esp_now_register_send_cb(OnDataSent);
  
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  
  frameQueue = xQueueCreate(3, sizeof(FrameBuffer));
  transmitQueue = xQueueCreate(100, sizeof(ImagePacket));
  wifiSemaphore = xSemaphoreCreateMutex();
  
  if (!frameQueue || !transmitQueue || !wifiSemaphore) {
    Serial.println("Failed to create RTOS components");
    return;
  }
  pinMode(48,OUTPUT);
  digitalWrite(48,LOW);
  xTaskCreatePinnedToCore(captureTask, "CaptureTask", 4096, NULL, 3, &captureTaskHandle, 0);
  xTaskCreatePinnedToCore(transmitTask, "TransmitTask", 4096, NULL, 2, &transmitTaskHandle, 1);
  xTaskCreatePinnedToCore(packetTransmissionTask, "PacketTask", 2048, NULL, 4, &packetTaskHandle, 1);
  
  Serial.println("RTOS Camera Sender Ready!");
  Serial.printf("Resolution: %s (%dx%d), Quality: %d\n", 
                resolutionConfigs[RESOLUTION_MODE].name,
                resolutionConfigs[RESOLUTION_MODE].width,
                resolutionConfigs[RESOLUTION_MODE].height,
                resolutionConfigs[RESOLUTION_MODE].quality);
  
  lastFPSTime = millis();
  
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
  Serial.printf("Free heap: %d bytes\n", esp_get_free_heap_size());
}