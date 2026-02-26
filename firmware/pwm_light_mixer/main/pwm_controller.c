/**
 * @file pwm_controller.c
 * @brief PWM LED Controller Implementation
 * 
 * Manages LEDC configuration and provides high-level PWM control API.
 */

#include "pwm_controller.h"
#include "config.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "PWM_CTRL";

/** Current brightness state for each channel */
static uint32_t current_duty_ch1 = 0;
static uint32_t current_duty_ch2 = 0;

/**
 * @brief Clamp duty value to valid range
 * 
 * @param duty Input duty value
 * @return Clamped duty value (0-255)
 */
static uint32_t pwm_clamp_duty(uint32_t duty)
{
    if (duty > CONFIG_PWM_MAX_DUTY) {
        return CONFIG_PWM_MAX_DUTY;
    }
    return duty;
}

/**
 * @brief Initialize PWM controller
 */
int pwm_controller_init(void)
{
    ESP_LOGI(TAG, "Initializing PWM controller");
    
    // Configure LEDC timer
    ledc_timer_config_t timer_config = {
        .speed_mode = CONFIG_LEDC_MODE,
        .timer_num = CONFIG_LEDC_TIMER,
        .duty_resolution = CONFIG_LEDC_DUTY_RES,
        .freq_hz = CONFIG_LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    if (ledc_timer_config(&timer_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer");
        return -1;
    }
    
    // Configure LED 1 channel
    ledc_channel_config_t channel1_config = {
        .speed_mode = CONFIG_LEDC_MODE,
        .channel = CONFIG_LEDC_CHANNEL_1,
        .timer_sel = CONFIG_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = CONFIG_LED_PIN_1,
        .duty = CONFIG_PWM_MIN_DUTY,
        .hpoint = 0
    };
    
    if (ledc_channel_config(&channel1_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LED 1 channel");
        return -1;
    }
    
    // Configure LED 2 channel
    ledc_channel_config_t channel2_config = {
        .speed_mode = CONFIG_LEDC_MODE,
        .channel = CONFIG_LEDC_CHANNEL_2,
        .timer_sel = CONFIG_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = CONFIG_LED_PIN_2,
        .duty = CONFIG_PWM_MIN_DUTY,
        .hpoint = 0
    };
    
    if (ledc_channel_config(&channel2_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LED 2 channel");
        return -1;
    }
    
    current_duty_ch1 = CONFIG_PWM_MIN_DUTY;
    current_duty_ch2 = CONFIG_PWM_MIN_DUTY;
    
    ESP_LOGI(TAG, "PWM controller initialized successfully");
    ESP_LOGI(TAG, "  Pins: LED1=%d, LED2=%d", CONFIG_LED_PIN_1, CONFIG_LED_PIN_2);
    ESP_LOGI(TAG, "  Frequency: %d Hz, Resolution: 8-bit", CONFIG_LEDC_FREQUENCY);
    
    return 0;
}

/**
 * @brief Set brightness on both LED channels synchronously
 */
int pwm_controller_set_brightness(uint32_t duty)
{
    duty = pwm_clamp_duty(duty);
    
    // Update channel 1
    if (ledc_set_duty(CONFIG_LEDC_MODE, CONFIG_LEDC_CHANNEL_1, duty) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty on channel 1");
        return -1;
    }
    
    if (ledc_update_duty(CONFIG_LEDC_MODE, CONFIG_LEDC_CHANNEL_1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty on channel 1");
        return -1;
    }
    
    // Update channel 2
    if (ledc_set_duty(CONFIG_LEDC_MODE, CONFIG_LEDC_CHANNEL_2, duty) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty on channel 2");
        return -1;
    }
    
    if (ledc_update_duty(CONFIG_LEDC_MODE, CONFIG_LEDC_CHANNEL_2) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty on channel 2");
        return -1;
    }
    
    current_duty_ch1 = duty;
    current_duty_ch2 = duty;
    
    return 0;
}

/**
 * @brief Set brightness on first LED channel
 */
int pwm_controller_set_brightness_ch1(uint32_t duty)
{
    duty = pwm_clamp_duty(duty);
    
    if (ledc_set_duty(CONFIG_LEDC_MODE, CONFIG_LEDC_CHANNEL_1, duty) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty on channel 1");
        return -1;
    }
    
    if (ledc_update_duty(CONFIG_LEDC_MODE, CONFIG_LEDC_CHANNEL_1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty on channel 1");
        return -1;
    }
    
    current_duty_ch1 = duty;
    return 0;
}

/**
 * @brief Set brightness on second LED channel
 */
int pwm_controller_set_brightness_ch2(uint32_t duty)
{
    duty = pwm_clamp_duty(duty);
    
    if (ledc_set_duty(CONFIG_LEDC_MODE, CONFIG_LEDC_CHANNEL_2, duty) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty on channel 2");
        return -1;
    }
    
    if (ledc_update_duty(CONFIG_LEDC_MODE, CONFIG_LEDC_CHANNEL_2) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty on channel 2");
        return -1;
    }
    
    current_duty_ch2 = duty;
    return 0;
}

/**
 * @brief Get current brightness of LED 1
 */
uint32_t pwm_controller_get_brightness_ch1(void)
{
    return current_duty_ch1;
}

/**
 * @brief Get current brightness of LED 2
 */
uint32_t pwm_controller_get_brightness_ch2(void)
{
    return current_duty_ch2;
}

/**
 * @brief Check if PWM channels are enabled
 */
bool pwm_controller_is_enabled(void)
{
    return (current_duty_ch1 > CONFIG_PWM_MIN_DUTY) || 
           (current_duty_ch2 > CONFIG_PWM_MIN_DUTY);
}
