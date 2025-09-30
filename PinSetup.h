// ================== I2S Speaker ==================
#define I2S_DOUT 11
#define I2S_BCLK 12
#define I2S_LRC  13
// ================== I2S Microphone ==================
#define MIC_SD   18
#define MIC_SCK  17
#define MIC_WS   16
// ================== BUZZER ==================
#define BUZZER   14
// ================== TFT Display ==================
#define TFT_MISO 3
#define TFT_MOSI 7
#define TFT_SCLK 15
#define TFT_CS   4
#define TFT_DC   6
#define TFT_RST  5
#define TFT_BL   8
// ================== Rotary Encoder Pins ==================
#define ENC_CLK 19   // Rotation A (wake source)
#define ENC_DT  20   // Rotation B (for direction, not wake)
#define ENC_SW  21   // Button press (wake source)


// New (safe for ESP32-S3 WROOM-1 N16R8 with Octal PSRAM)
#define BTN1  39
#define BTN2  40
#define BTN3  41
#define BTN4  10
#define BTN5  38

// I2C RTC (standard I2C pins; these are free and portable)
#define I2C_SDA 22
#define I2C_SCL 23
