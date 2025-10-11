// CameraModule.cpp
#include "CameraModule.h"

CameraModule::CameraModule() {
  framesCaptured = 0;
  lastCaptureTime = 0;
}

bool CameraModule::begin() {
  Serial.println("Initializing camera...");
  
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
  
  // QVGA 320x240 with high quality
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 4;  // High quality
  config.fb_count = 1;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  
  // Configure sensor settings
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_sharpness(s, 0);
    s->set_denoise(s, 0);
    s->set_gainceiling(s, GAINCEILING_4X);
    s->set_quality(s, 4);
    s->set_colorbar(s, 0);
    s->set_whitebal(s, 0);
    s->set_gain_ctrl(s, 1);
    s->set_exposure_ctrl(s, 0);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    s->set_awb_gain(s, 0);
    s->set_agc_gain(s, 0);
    s->set_aec_value(s, 300);
    s->set_aec2(s, 0);
    s->set_special_effect(s, 0);
  }
  
  Serial.println("Camera initialized: QVGA 320x240, Quality 4");
  return true;
}

camera_fb_t* CameraModule::captureFrame() {
  camera_fb_t* fb = esp_camera_fb_get();
  
  if (fb) {
    framesCaptured++;
    lastCaptureTime = millis();
    Serial.printf("Frame captured: %d bytes\n", fb->len);
  } else {
    Serial.println("Frame capture failed!");
  }
  
  return fb;
}

void CameraModule::returnFrame(camera_fb_t* fb) {
  if (fb) {
    esp_camera_fb_return(fb);
  }
}

void CameraModule::resetStats() {
  framesCaptured = 0;
  lastCaptureTime = millis();
}