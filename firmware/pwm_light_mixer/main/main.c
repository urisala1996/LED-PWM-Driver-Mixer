#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "encoder.h"
#include "touch_sensor.h"
#include "nvs_manager.h"

#define LED_PIN GPIO_NUM_2           // Built-in LED on GPIO 2
#define LED_PIN_2 GPIO_NUM_33        // Second LED on GPIO 33
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_CHANNEL_2 LEDC_CHANNEL_1
#define LEDC_DUTY_RES LEDC_TIMER_8_BIT  // 8-bit resolution (0-255)
#define LEDC_FREQUENCY 5000             // 5kHz frequency

void pwm_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    
    // Configure first PWM channel (LED on GPIO 2)
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LED_PIN,
        .duty = 0,  // Will be set to encoder position in app_main
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
    
    // Configure second PWM channel (LED on GPIO 33)
    ledc_channel_config_t ledc_channel_2 = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_2,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LED_PIN_2,
        .duty = 0,  // Will be set to encoder position in app_main
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_2);
    
    ESP_LOGI("PWM", "PWM initialized on GPIO %d and GPIO %d, Frequency: %d Hz", 
             LED_PIN, LED_PIN_2, LEDC_FREQUENCY);
}

void app_main(void)
{
    ESP_LOGI("MAIN", "Hello World! LED PWM Control via Encoder + Touch Sensor");
    ESP_LOGI("MAIN", "Starting PWM LED control with touch sensor toggle");
    
    // Initialize NVS for persistent storage
    if (nvs_manager_init() != 0) {
        ESP_LOGE("MAIN", "Failed to initialize NVS");
    }
    
    // Load saved state from flash
    nvs_led_state_t saved_state;
    nvs_manager_load_led_state(&saved_state);
    
    // Initialize encoder module
    encoder_init();
    
    // Sync encoder position with saved PWM value
    encoder_set_position((int32_t)saved_state.pwm_value);
    
    // Start encoder RTOS task
    encoder_task_start();
    
    // Initialize touch sensor
    touch_sensor_init();
    
    // Initialize PWM for LED
    pwm_init();
    
    // Set LED to initial state (from saved state or defaults)
    int32_t initial_position = saved_state.pwm_value;
    bool initial_enabled = saved_state.pwm_enabled;
    
    if (initial_enabled) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (uint32_t)initial_position);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, (uint32_t)initial_position);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
        ESP_LOGI("MAIN", "LEDs enabled (restored) at PWM: %lu/255", saved_state.pwm_value);
    } else {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
        ESP_LOGI("MAIN", "LEDs disabled (restored)");
    }
    
    ESP_LOGI("MAIN", "Encoder, touch sensor, and PWM initialized");
    
    // Main task: Control LED brightness via encoder position and toggle via touch sensor
    int32_t last_position = initial_position;
    bool pwm_enabled = initial_enabled;
    uint32_t last_touch_count = 0;
    uint32_t nvs_check_counter = 0;  // Counter to check pending writes every 200ms
    uint32_t last_scale_factor = 1;   // Track scale factor changes
    
    // Prepare state for saving
    nvs_led_state_t current_state;
    current_state.pwm_enabled = pwm_enabled;
    current_state.pwm_value = (uint32_t)last_position;
    
    while (1) {
        // Check for touch events (bi-stable toggle)
        uint32_t touch_count = touch_sensor_get_touch_count();
        if (touch_count != last_touch_count) {
            // Touch event detected, toggle PWM state
            pwm_enabled = !pwm_enabled;
            last_touch_count = touch_count;
            
            if (pwm_enabled) {
                ESP_LOGI("MAIN", "Touch detected - LEDs ON at encoder position %ld", last_position);
                // Restore to last set position
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (uint32_t)last_position);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, (uint32_t)last_position);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
            } else {
                ESP_LOGI("MAIN", "Touch detected - LEDs OFF");
                // Turn LEDs completely off
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, 0);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
            }
            
            // Save updated state to NVS
            current_state.pwm_enabled = pwm_enabled;
            current_state.pwm_value = (uint32_t)last_position;
            nvs_manager_save_led_state(&current_state);
        }
        
        // Get encoder position and control brightness if PWM is enabled
        int32_t position = encoder_get_position();
        uint32_t current_scale = encoder_get_scale_factor();
        
        // Check if scale factor changed
        if (current_scale != last_scale_factor) {
            ESP_LOGI("MAIN", "Scale factor changed: %lu", current_scale);
            last_scale_factor = current_scale;
        }
        
        if (position != last_position && pwm_enabled) {
            // Encoder position is already clamped to 0-255, use directly
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (uint32_t)position);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, (uint32_t)position);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
            ESP_LOGI("MAIN", "Encoder Position: %ld | PWM Duty: %ld/255 | Scale: %lu | State: ON", 
                     position, position, current_scale);
            last_position = position;
            
            // Save updated PWM value to NVS
            current_state.pwm_enabled = pwm_enabled;
            current_state.pwm_value = (uint32_t)position;
            nvs_manager_save_led_state(&current_state);
        }
        
        // Check pending NVS writes every 200ms
        if (++nvs_check_counter >= 4) {  // 4 * 50ms = 200ms
            nvs_check_counter = 0;
            nvs_manager_check_pending_write();
        }
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
