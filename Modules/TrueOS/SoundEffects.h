// SoundEffects.h - INSTANT tone-based feedback with multiple sound types
#ifndef SOUNDEFFECTS_H
#define SOUNDEFFECTS_H
#include <driver/i2s.h>

#define I2S_DOUT 11
#define I2S_BCLK 12
#define I2S_LRC  13
#define SAMPLE_RATE 16000

// Sound definitions - carefully tuned frequencies and durations
#define TICK_FREQ 2200
#define TICK_DURATION_MS 12

#define CLICK_FREQ 2600
#define CLICK_DURATION_MS 18

#define SELECT_FREQ1 2000
#define SELECT_FREQ2 2600
#define SELECT_DURATION_MS 30

#define BACK_FREQ 1600
#define BACK_DURATION_MS 20

#define ERROR_FREQ 700
#define ERROR_DURATION_MS 100

#define SUCCESS_FREQ1 2200
#define SUCCESS_FREQ2 3000
#define SUCCESS_DURATION_MS 40

#define WARN_FREQ 1400
#define WARN_DURATION_MS 60

class SoundEffects {
private:
    i2s_port_t i2s_num;
    uint8_t volume;
    
    // Generate tone with envelope
    void generateTone(int16_t* buffer, size_t samples, float frequency, float duration_ms) {
        float amplitude = (volume / 21.0) * 10000;
        
        for (size_t i = 0; i < samples; i++) {
            float t = (float)i / SAMPLE_RATE;
            float progress = t / (duration_ms / 1000.0);
            
            // Envelope: quick attack, sustain, quick release
            float envelope = 1.0;
            if (progress < 0.1) {
                envelope = progress / 0.1; // Attack
            } else if (progress > 0.7) {
                envelope = (1.0 - progress) / 0.3; // Release
            }
            
            float sample = amplitude * envelope * sin(2.0 * PI * frequency * t);
            buffer[i] = (int16_t)sample;
        }
    }
    
    // Play single tone
    void playToneInternal(float frequency, float duration_ms, float amp_scale = 1.0) {
        size_t num_samples = (SAMPLE_RATE * duration_ms) / 1000;
        int16_t* samples = (int16_t*)malloc(num_samples * sizeof(int16_t));
        if (!samples) return;
        
        float old_volume = volume;
        volume = (uint8_t)(volume * amp_scale);
        
        generateTone(samples, num_samples, frequency, duration_ms);
        
        size_t bytes_written;
        i2s_write(i2s_num, samples, num_samples * sizeof(int16_t), &bytes_written, 0);
        
        volume = old_volume;
        free(samples);
    }
    
public:
    SoundEffects() : i2s_num(I2S_NUM_0), volume(10) {}
    
    bool begin() {
        Serial.println("[Audio] Initializing I2S tone generator...");
        
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = 64,
            .use_apll = false,
            .tx_desc_auto_clear = true,
            .fixed_mclk = 0
        };
        
        i2s_pin_config_t pin_config = {
            .bck_io_num = I2S_BCLK,
            .ws_io_num = I2S_LRC,
            .data_out_num = I2S_DOUT,
            .data_in_num = I2S_PIN_NO_CHANGE
        };
        
        if (i2s_driver_install(i2s_num, &i2s_config, 0, NULL) != ESP_OK) {
            Serial.println("[Audio] ERROR: I2S driver install failed");
            return false;
        }
        
        if (i2s_set_pin(i2s_num, &pin_config) != ESP_OK) {
            Serial.println("[Audio] ERROR: I2S pin setup failed");
            return false;
        }
        
        Serial.println("[Audio] Tone generator ready");
        return true;
    }
    
    // Individual sound effects
    void playTick() {
        playToneInternal(TICK_FREQ, TICK_DURATION_MS, 0.8);
    }
    
    void playClick() {
        playToneInternal(CLICK_FREQ, CLICK_DURATION_MS, 1.0);
    }
    
    void playSelect() {
        playToneInternal(SELECT_FREQ1, SELECT_DURATION_MS / 2, 0.9);
        vTaskDelay(2 / portTICK_PERIOD_MS);
        playToneInternal(SELECT_FREQ2, SELECT_DURATION_MS / 2, 0.9);
    }
    
    void playBack() {
        playToneInternal(BACK_FREQ, BACK_DURATION_MS, 0.9);
    }
    
    void playError() {
        // Two harsh buzzes
        for (int i = 0; i < 2; i++) {
            playToneInternal(ERROR_FREQ, ERROR_DURATION_MS / 2, 1.2);
            vTaskDelay(40 / portTICK_PERIOD_MS);
        }
    }
    
    void playSuccess() {
        playToneInternal(SUCCESS_FREQ1, SUCCESS_DURATION_MS / 2, 0.9);
        vTaskDelay(2 / portTICK_PERIOD_MS);
        playToneInternal(SUCCESS_FREQ2, SUCCESS_DURATION_MS / 2, 0.9);
    }
    
    void playWarning() {
        playToneInternal(WARN_FREQ, WARN_DURATION_MS, 1.0);
    }
    
    void playStartup() {
        playToneInternal(1500, 20, 0.8);
        vTaskDelay(15 / portTICK_PERIOD_MS);
        playToneInternal(2000, 20, 0.9);
        vTaskDelay(15 / portTICK_PERIOD_MS);
        playToneInternal(2500, 25, 1.0);
    }
    
    void playShutdown() {
        playToneInternal(2500, 20, 1.0);
        vTaskDelay(15 / portTICK_PERIOD_MS);
        playToneInternal(2000, 20, 0.9);
        vTaskDelay(15 / portTICK_PERIOD_MS);
        playToneInternal(1500, 30, 0.8);
    }
    
    void setVolume(uint8_t vol) {
        volume = constrain(vol, 0, 21);
        Serial.printf("[Audio] Volume set to %d\n", volume);
    }
    
    uint8_t getVolume() {
        return volume;
    }
    
    void stop() {
        i2s_zero_dma_buffer(i2s_num);
    }
};

// Global instance
SoundEffects Effect;

#endif