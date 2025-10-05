// Config.h - Hardware Configuration with AI Assistant
#ifndef CONFIG_H
#define CONFIG_H

// ================== Display Pins (TFT_eSPI) ==================
#define TFT_MISO    3
#define TFT_MOSI    7
#define TFT_SCLK    15
#define TFT_CS      4
#define TFT_DC      6
#define TFT_RST     5
#define TFT_BL      8

// ================== Display Settings ==================
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   240
#define DISPLAY_ROTATION 3

// ================== Rotary Encoder Pins ==================
#define ENC_CLK     19
#define ENC_DT      20
#define ENC_SW      21

// ================== Physical Button Pins ==================
#define BTN_SELECT   10  // CAST - Select/Enter
#define BTN_SHIFT    39  // MIC - AI Assistant trigger
#define BTN_DEL      40  // CAMERA - Delete/Back
#define BTN_SPECIAL  41  // HOME - Menu/Exit
#define BTN_SCAN     38  // POWER - WiFi Scan

#define BUTTON_DEBOUNCE_MS  20

// ================== I2C RTC ==================
#define I2C_SDA     1
#define I2C_SCL     2
#define SQW         14

// ================== I2S Audio Output (Speaker/TTS) ==================
#define I2S_DOUT    11
#define I2S_BCLK    12
#define I2S_LRC     13

// ================== I2S Microphone (STT) ==================
#define MIC_SD      18  // Microphone data
#define MIC_SCK     17  // Microphone clock
#define MIC_WS      16  // Microphone word select

// ================== Rotary Encoder Settings ==================
#define ROTARY_STEPS     4
#define ENCODER_VCC_PIN -1

// ================== UI Constants ==================
#define HEADER_HEIGHT    40
#define FOOTER_HEIGHT    36
#define ITEM_HEIGHT      48
#define VISIBLE_ITEMS    4
#define CARD_MARGIN      3
#define CARD_RADIUS      6

// ================== WiFi Scanner Settings ==================
#define MAX_NETWORKS     50
#define SCAN_INTERVAL    0
#define CONNECT_TIMEOUT  30000

// ================== Keyboard Settings ==================
#define MAX_PASSWORD_LENGTH  63
#define KEYBOARD_ROWS        3
#define KEYBOARD_COLS        10
#define KEY_WIDTH           30
#define KEY_HEIGHT          36
#define KEY_SPACING         2
#define KEYBOARD_PAGES      2

// ================== AI Assistant Settings ==================
#define MAX_CHAT_MESSAGES    50
#define MAX_MESSAGE_LENGTH   500
#define AI_RESPONSE_TIMEOUT  30000  // 30 seconds

// STT Server URL
#define STT_SERVER_URL "https://espeechserver-iukg.onrender.com/uploadAudio"

// Claude API (set your key in code)
#define AI_API_URL "https://api.anthropic.com/v1/messages"
#define AI_MODEL "claude-3-5-sonnet-20241022"
#define AI_MAX_TOKENS 500

#endif // CONFIG_H