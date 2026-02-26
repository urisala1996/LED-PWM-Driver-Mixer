#include "nvs_manager.h"
#include "config.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "NVS_MANAGER";

/**
 * ============================================================================
 * NVS CONFIGURATION - Uses centralized config.h
 * ============================================================================
 */

static const char *NVS_NAMESPACE = CONFIG_NVS_NAMESPACE;
static const char *NVS_KEY_PWM_ENABLED = CONFIG_NVS_KEY_PWM_ENABLED;
static const char *NVS_KEY_PWM_VALUE = CONFIG_NVS_KEY_PWM_VALUE;
static const uint32_t FLASH_WRITE_DEBOUNCE_MS = CONFIG_FLASH_WRITE_DEBOUNCE;

// Cache for last saved state to optimize write cycles
static nvs_led_state_t last_saved_state = {
    .pwm_enabled = CONFIG_NVS_DEFAULT_PWM_ENABLE,
    .pwm_value = CONFIG_NVS_DEFAULT_PWM_VALUE
};
static bool state_cached = false;

// Pending write tracking
static nvs_led_state_t pending_state = {
    .pwm_enabled = CONFIG_NVS_DEFAULT_PWM_ENABLE,
    .pwm_value = CONFIG_NVS_DEFAULT_PWM_VALUE
};
static uint32_t pending_timestamp = 0;
static bool pending_write = false;

/**
 * Initialize NVS (Non-Volatile Storage)
 */
int nvs_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition erased, reinitializing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: 0x%x", ret);
        return -1;
    }
    
    ESP_LOGI(TAG, "NVS initialized successfully");
    
    // Load initial state from flash
    if (nvs_manager_load_led_state(&last_saved_state) == 0) {
        state_cached = true;
        ESP_LOGI(TAG, "Loaded state from NVS: enabled=%d, pwm=%lu",
                 last_saved_state.pwm_enabled, last_saved_state.pwm_value);
    } else {
        // Use defaults if load fails
        state_cached = true;
        ESP_LOGW(TAG, "Using default state: enabled=true, pwm=155");
    }
    
    return 0;
}

/**
 * Load LED state from flash
 */
int nvs_manager_load_led_state(nvs_led_state_t *state)
{
    if (state == NULL) {
        ESP_LOGE(TAG, "Invalid state pointer");
        return -1;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No NVS data found, using defaults");
        state->pwm_enabled = CONFIG_NVS_DEFAULT_PWM_ENABLE;
        state->pwm_value = CONFIG_NVS_DEFAULT_PWM_VALUE;
        return 0;  // Return success with defaults
    }
    
    // Read PWM enabled state
    uint8_t pwm_enabled = (uint8_t)CONFIG_NVS_DEFAULT_PWM_ENABLE;
    ret = nvs_get_u8(nvs_handle, NVS_KEY_PWM_ENABLED, &pwm_enabled);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to read pwm_enabled from NVS: 0x%x", ret);
        nvs_close(nvs_handle);
        return -1;
    }
    state->pwm_enabled = (bool)pwm_enabled;
    
    // Read PWM value
    uint32_t pwm_value = CONFIG_NVS_DEFAULT_PWM_VALUE;
    ret = nvs_get_u32(nvs_handle, NVS_KEY_PWM_VALUE, &pwm_value);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to read pwm_value from NVS: 0x%x", ret);
        nvs_close(nvs_handle);
        return -1;
    }
    state->pwm_value = pwm_value;
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Loaded from NVS: enabled=%d, pwm=%lu", state->pwm_enabled, state->pwm_value);
    
    return 0;
}

/**
 * Save LED state to flash (queues for debounced write)
 */
int nvs_manager_save_led_state(const nvs_led_state_t *state)
{
    if (state == NULL) {
        ESP_LOGE(TAG, "Invalid state pointer");
        return -1;
    }
    
    // Check if state has changed - optimize write cycles
    if (state_cached && 
        last_saved_state.pwm_enabled == state->pwm_enabled &&
        last_saved_state.pwm_value == state->pwm_value) {
        // No change from last saved, skip write
        return 0;
    }
    
    // Check against pending write to avoid duplicate queuing
    if (pending_write &&
        pending_state.pwm_enabled == state->pwm_enabled &&
        pending_state.pwm_value == state->pwm_value) {
        // Already queued with same value, skip
        return 0;
    }
    
    // Queue a pending write
    pending_state = *state;
    pending_timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    pending_write = true;
    
    ESP_LOGD(TAG, "Queued state change: enabled=%d, pwm=%lu (will write in 5s if stable)",
             state->pwm_enabled, state->pwm_value);
    
    return 0;
}

/**
 * Internal function to perform actual flash write
 */
static int nvs_manager_commit_write(const nvs_led_state_t *state)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: 0x%x", ret);
        return -1;
    }
    
    // Write PWM enabled state
    ret = nvs_set_u8(nvs_handle, NVS_KEY_PWM_ENABLED, (uint8_t)state->pwm_enabled);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write pwm_enabled to NVS: 0x%x", ret);
        nvs_close(nvs_handle);
        return -1;
    }
    
    // Write PWM value
    ret = nvs_set_u32(nvs_handle, NVS_KEY_PWM_VALUE, state->pwm_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write pwm_value to NVS: 0x%x", ret);
        nvs_close(nvs_handle);
        return -1;
    }
    
    // Commit changes to flash
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: 0x%x", ret);
        nvs_close(nvs_handle);
        return -1;
    }
    
    nvs_close(nvs_handle);
    
    // Update cache with new values
    last_saved_state = *state;
    state_cached = true;
    
    ESP_LOGI(TAG, "*** FLASH WRITE: saved LED state - enabled=%d, pwm=%lu ***",
           state->pwm_enabled, state->pwm_value);
    ESP_LOGI(TAG, "Committed to flash: enabled=%d, pwm=%lu", state->pwm_enabled, state->pwm_value);
    
    return 0;
}

/**
 * Check and commit pending writes if debounce time has elapsed
 * Should be called periodically (e.g., every 100ms) from main task
 */
int nvs_manager_check_pending_write(void)
{
    if (!pending_write) {
        return 0;  // No pending write
    }
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed_time = current_time - pending_timestamp;
    
    if (elapsed_time >= FLASH_WRITE_DEBOUNCE_MS) {
        // Debounce time has elapsed, commit the pending write
        int result = nvs_manager_commit_write(&pending_state);
        pending_write = false;
        return result;
    }
    
    // Debounce timer still running
    return 0;
}

/**
 * Get the last saved state without writing
 */
int nvs_manager_get_last_state(nvs_led_state_t *state)
{
    if (state == NULL) {
        ESP_LOGE(TAG, "Invalid state pointer");
        return -1;
    }
    
    if (!state_cached) {
        ESP_LOGW(TAG, "State not cached, returning defaults");
        state->pwm_enabled = CONFIG_NVS_DEFAULT_PWM_ENABLE;
        state->pwm_value = CONFIG_NVS_DEFAULT_PWM_VALUE;
        return 0;
    }
    
    *state = last_saved_state;
    return 0;
}
