// Existing (no change)
#define I2S_DOUT 11
#define I2S_BCLK 12
#define I2S_LRC  13
#define MIC_SD   18
#define MIC_SCK  17
#define MIC_WS   16
#define BUZZER   14
#define TFT_MISO 3
#define TFT_MOSI 7
#define TFT_SCLK 15
#define TFT_CS   4
#define TFT_DC   6
#define TFT_RST  5
#define TFT_BL   8

// New (safe for ESP32-S3 WROOM-1 N16R8 with Octal PSRAM)
#define BTN1  19
#define BTN2  20
#define BTN3  21
#define BTN4  10
#define BTN5  38

// Rotary encoder (use interrupt-capable GPIOs)
#define ENC_CLK 39
#define ENC_DT  40
#define ENC_SW  41

// I2C RTC (standard I2C pins; these are free and portable)
#define I2C_SDA 22
#define I2C_SCL 23
