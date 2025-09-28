#include "esp_camera.h"
#include <WiFi.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "img_converters.h"

const char* ssid = "Rakib";
const char* password = "rakib@2024";
const uint16_t port = 8888;

// Capture settings
#define CAPTURE_RESOLUTION FRAMESIZE_XGA     // 1280x720 Full HD
#define TARGET_WIDTH 320
#define TARGET_HEIGHT 240
#define JPEG_QUALITY 63  // 1-63, lower = better quality, higher = smaller file

// Resolution options for CAPTURE_RESOLUTION:
// FRAMESIZE_HD       // 1280x720 (Full HD)
// FRAMESIZE_XGA      // 1024x768
// FRAMESIZE_SVGA     // 800x600
// FRAMESIZE_VGA      // 640x480
// FRAMESIZE_UXGA     // 1600x1200 (may be too memory intensive)

// ----------------- ESP32-S3 Camera Pinout -----------------
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y2_GPIO_NUM       11
#define Y3_GPIO_NUM       9
#define Y4_GPIO_NUM       8
#define Y5_GPIO_NUM       10
#define Y6_GPIO_NUM       12
#define Y7_GPIO_NUM       18
#define Y8_GPIO_NUM       17
#define Y9_GPIO_NUM       16
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

WiFiServer server(port);

// Performance monitoring
unsigned long frameCount = 0;
unsigned long totalJpegBytes = 0;
unsigned long totalRawBytes = 0;
unsigned long lastStatsTime = 0;

// Memory buffers for rescaling
uint8_t* rescaleBuffer = nullptr;
const size_t RESCALE_BUFFER_SIZE = TARGET_WIDTH * TARGET_HEIGHT * 2; // RGB565

// Simple bilinear rescaling function for RGB565
void rescaleRGB565(const uint8_t* src, int srcW, int srcH, 
                   uint8_t* dst, int dstW, int dstH) {
  
  float xRatio = (float)srcW / dstW;
  float yRatio = (float)srcH / dstH;
  
  uint16_t* srcPixels = (uint16_t*)src;
  uint16_t* dstPixels = (uint16_t*)dst;
  
  for (int y = 0; y < dstH; y++) {
    for (int x = 0; x < dstW; x++) {
      
      // Calculate source coordinates
      float srcX = x * xRatio;
      float srcY = y * yRatio;
      
      int x1 = (int)srcX;
      int y1 = (int)srcY;
      int x2 = min(x1 + 1, srcW - 1);
      int y2 = min(y1 + 1, srcH - 1);
      
      // Get source pixels
      uint16_t p1 = srcPixels[y1 * srcW + x1]; // Top-left
      uint16_t p2 = srcPixels[y1 * srcW + x2]; // Top-right
      uint16_t p3 = srcPixels[y2 * srcW + x1]; // Bottom-left
      uint16_t p4 = srcPixels[y2 * srcW + x2]; // Bottom-right
      
      // For simplicity, use nearest neighbor (you can implement bilinear if needed)
      // This is much faster and still gives good results
      dstPixels[y * dstW + x] = p1;
    }
    
    // Yield every few rows to prevent watchdog timeout
    if (y % 32 == 0) {
      yield();
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32-S3 Camera Server - Full HD to 320x240 Rescaling");
  Serial.printf("Capture: Full HD, Output: %dx%d, JPEG Quality: %d\n", 
                TARGET_WIDTH, TARGET_HEIGHT, JPEG_QUALITY);
  Serial.printf("Free heap: %d bytes, Free PSRAM: %d bytes\n", ESP.getFreeHeap(), ESP.getFreePsram());

  // Allocate rescale buffer in PSRAM
  if (ESP.getFreePsram() > RESCALE_BUFFER_SIZE + 500000) {
    rescaleBuffer = (uint8_t*)ps_malloc(RESCALE_BUFFER_SIZE);
    Serial.println("Rescale buffer allocated in PSRAM");
  } else {
    rescaleBuffer = (uint8_t*)heap_caps_malloc(RESCALE_BUFFER_SIZE, MALLOC_CAP_8BIT);
    Serial.println("Rescale buffer allocated in heap");
  }
  
  if (!rescaleBuffer) {
    Serial.println("FATAL: Failed to allocate rescale buffer!");
    ESP.restart();
  }

  // Camera configuration
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
  
  // Settings for Full HD capture
  config.xclk_freq_hz = 20000000; // 20MHz
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = CAPTURE_RESOLUTION; // Full HD
  config.jpeg_quality = 10; // Not used for RGB565 but set anyway
  config.fb_count = 1; // Single buffer to save memory with HD capture
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    ESP.restart();
  }
  
  // Optimize camera settings for performance
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 0);
    s->set_ae_level(s, 0);
    s->set_aec_value(s, 300);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 0);
    s->set_gainceiling(s, (gainceiling_t)0);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    s->set_dcw(s, 1);
    s->set_colorbar(s, 0);
  }

  Serial.println("Camera initialized successfully");
  
  // Get actual resolution info and test capture/rescale
  camera_fb_t * test_fb = esp_camera_fb_get();
  if (test_fb) {
    Serial.printf("Camera: %dx%d, Format: %d, Raw size: %d bytes\n", 
                  test_fb->width, test_fb->height, test_fb->format, test_fb->len);
    
    // Test rescaling
    unsigned long rescaleStart = millis();
    rescaleRGB565(test_fb->buf, test_fb->width, test_fb->height, 
                  rescaleBuffer, TARGET_WIDTH, TARGET_HEIGHT);
    unsigned long rescaleTime = millis() - rescaleStart;
    
    Serial.printf("Rescale: %dx%d -> %dx%d in %dms\n", 
                  test_fb->width, test_fb->height, TARGET_WIDTH, TARGET_HEIGHT, (int)rescaleTime);
    
    // Test JPEG conversion on rescaled image
    uint8_t* jpg_buf = NULL;
    size_t jpg_len = 0;
    
    // Create a temporary framebuffer structure for the rescaled data
    camera_fb_t rescaled_fb;
    rescaled_fb.buf = rescaleBuffer;
    rescaled_fb.len = RESCALE_BUFFER_SIZE;
    rescaled_fb.width = TARGET_WIDTH;
    rescaled_fb.height = TARGET_HEIGHT;
    rescaled_fb.format = PIXFORMAT_RGB565;
    
    unsigned long convStart = millis();
    bool jpegOk = frame2jpg(&rescaled_fb, JPEG_QUALITY, &jpg_buf, &jpg_len);
    unsigned long convTime = millis() - convStart;
    
    if (jpegOk) {
      float compression = (float)rescaled_fb.len / jpg_len;
      Serial.printf("JPEG conversion: %d -> %d bytes (%.1fx compression) in %dms\n", 
                    rescaled_fb.len, jpg_len, compression, (int)convTime);
      free(jpg_buf);
    } else {
      Serial.println("JPEG conversion test failed!");
    }
    
    esp_camera_fb_return(test_fb);
  }

  // WiFi setup
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi failed!");
    ESP.restart();
  }
  
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  Serial.printf("Server listening on port %d\n", port);
  
  // Configure TCP for performance
  server.setNoDelay(true);
  server.begin();
  
  lastStatsTime = millis();
}

void loop() {
  WiFiClient client = server.available();
  if (!client) {
    delay(1);
    return;
  }

  Serial.println("Client connected from: " + client.remoteIP().toString());
  
  // Set client socket options for performance
  client.setNoDelay(true);
  client.setTimeout(5000);
  
  unsigned long sessionStart = millis();
  unsigned long frameStartTime;
  
  while (client.connected()) {
    frameStartTime = millis();
    
    // Capture full HD frame
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Frame capture failed");
      break;
    }
    
    // Rescale to target resolution
    unsigned long rescaleStart = millis();
    rescaleRGB565(fb->buf, fb->width, fb->height, 
                  rescaleBuffer, TARGET_WIDTH, TARGET_HEIGHT);
    unsigned long rescaleTime = millis() - rescaleStart;
    
    // Create framebuffer structure for rescaled data
    camera_fb_t rescaled_fb;
    rescaled_fb.buf = rescaleBuffer;
    rescaled_fb.len = RESCALE_BUFFER_SIZE;
    rescaled_fb.width = TARGET_WIDTH;
    rescaled_fb.height = TARGET_HEIGHT;
    rescaled_fb.format = PIXFORMAT_RGB565;
    
    // Convert rescaled RGB565 to JPEG
    uint8_t* jpg_buf = NULL;
    size_t jpg_len = 0;
    
    unsigned long jpegStart = millis();
    bool jpegSuccess = frame2jpg(&rescaled_fb, JPEG_QUALITY, &jpg_buf, &jpg_len);
    unsigned long jpegTime = millis() - jpegStart;
    
    // Clean up original framebuffer
    esp_camera_fb_return(fb);
    
    if (!jpegSuccess) {
      Serial.println("JPEG conversion failed");
      break;
    }
    
    // Send JPEG with minimal header
    String header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Type: image/jpeg\r\n";
    header += "Content-Length: " + String(jpg_len) + "\r\n";
    header += "Connection: keep-alive\r\n\r\n";
    
    // Send header
    client.print(header);
    
    // Send JPEG data
    size_t bytesSent = client.write(jpg_buf, jpg_len);
    
    // Update statistics
    frameCount++;
    totalJpegBytes += bytesSent;
    totalRawBytes += fb->len;
    
    // Performance logging
    unsigned long frameTime = millis() - frameStartTime;
    if (frameTime > 300 || rescaleTime > 100 || jpegTime > 100) {
      Serial.printf("Frame: %dms (Rescale: %dms, JPEG: %dms), HD: %d -> JPEG: %d bytes\n", 
                    (int)frameTime, (int)rescaleTime, (int)jpegTime, fb->len, jpg_len);
    }
    
    // Clean up
    free(jpg_buf);
    
    // Check transmission success
    if (bytesSent != jpg_len) {
      Serial.println("Client write incomplete");
      break;
    }
    
    // Print periodic statistics
    if (millis() - lastStatsTime > 5000) {
      float avgJpegSize = (float)totalJpegBytes / frameCount;
      float sessionTime = (millis() - sessionStart) / 1000.0;
      float fps = frameCount / sessionTime;
      
      Serial.printf("Stats: %.1f FPS, JPEG: %.1f KB, Frames: %d\n", 
                    fps, avgJpegSize/1024, frameCount);
      
      lastStatsTime = millis();
    }
    
    // Frame rate control (adjusted for processing overhead)
    delay(100); // ~20 FPS max due to processing time
  }
  
  Serial.printf("Client disconnected after %d frames\n", frameCount);
  client.stop();
  
  // Reset counters for next session
  frameCount = 0;
  totalJpegBytes = 0;
  totalRawBytes = 0;
}