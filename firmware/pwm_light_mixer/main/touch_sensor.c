#include "touch_sensor.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "TOUCH_SENSOR";

/**
 * ============================================================================
 * TOUCH SENSOR STATE
 * ============================================================================
 */

// Static variables for touch sensor state
static volatile bool sensor_touched = false;
static volatile uint32_t touch_event_count = 0;
static volatile bool last_sensor_state = false;

// Mutex for thread-safe access
static SemaphoreHandle_t touch_mutex = NULL;

/**
 * @brief Read current sensor state
 * @return true if sensor is high (touched), false otherwise
 */
static bool touch_sensor_read(void)
{
    return gpio_get_level(CONFIG_TOUCH_SENSOR_PIN) == 1;
}

/**
 * @brief RTOS task for touch sensor reading with debouncing
 * 
 * Monitors sensor pin with debouncing logic to filter noise and
 * properly detect touch edges.
 * 
 * @param pvParameters Task parameters (unused)
 */
static void touch_sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Touch sensor task started");
    
    uint32_t debounce_counter = 0;
    const uint32_t debounce_threshold = CONFIG_TOUCH_DEBOUNCE_COUNT;
    
    while (1) {
        bool current_state = touch_sensor_read();
        
        // Debouncing logic
        if (current_state == last_sensor_state) {
            debounce_counter = 0;
        } else {
            debounce_counter++;
            
            // State confirmed after debounce threshold
            if (debounce_counter >= debounce_threshold) {
                // State has changed
                xSemaphoreTake(touch_mutex, portMAX_DELAY);
                
                if (current_state && !sensor_touched) {
                    // Touch detected (transition to touched)
                    sensor_touched = true;
                    touch_event_count++;
                    ESP_LOGI(TAG, "Touch detected! Event count: %lu", touch_event_count);
                } else if (!current_state && sensor_touched) {
                    // Touch released
                    sensor_touched = false;
                    ESP_LOGI(TAG, "Touch released");
                }
                
                last_sensor_state = current_state;
                debounce_counter = 0;
                
                xSemaphoreGive(touch_mutex);
            }
        }
        
        vTaskDelay(CONFIG_TOUCH_POLL_INTERVAL / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Initialize the touch sensor
 * 
 * Configures GPIO pin and creates background task for debounced edge detection.
 */
void touch_sensor_init(void)
{
    ESP_LOGI(TAG, "Initializing touch sensor on GPIO %d", CONFIG_TOUCH_SENSOR_PIN);
    
    // Create mutex for thread-safe access
    touch_mutex = xSemaphoreCreateMutex();
    if (touch_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }
    
    // Configure GPIO for touch sensor input
    gpio_config_t sensor_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CONFIG_TOUCH_SENSOR_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&sensor_conf);
    
    // Initialize last sensor state
    last_sensor_state = touch_sensor_read();
    
    ESP_LOGI(TAG, "Touch sensor initialized on GPIO %d", CONFIG_TOUCH_SENSOR_PIN);
    
    // Create and start the touch sensor task
    xTaskCreate(
        touch_sensor_task,           // Task function
        "touch_sensor_task",         // Task name
        CONFIG_TOUCH_TASK_STACK,     // Stack size
        NULL,                        // Task parameters
        CONFIG_TOUCH_TASK_PRIORITY,  // Priority
        NULL                         // Task handle
    );
    
    ESP_LOGI(TAG, "Touch sensor RTOS task created");
}

/**
 * Check if touch sensor is currently touched
 */
bool touch_sensor_is_touched(void)
{
    bool touched;
    xSemaphoreTake(touch_mutex, portMAX_DELAY);
    touched = sensor_touched;
    xSemaphoreGive(touch_mutex);
    return touched;
}

/**
 * Get the number of touch events detected
 */
uint32_t touch_sensor_get_touch_count(void)
{
    uint32_t count;
    xSemaphoreTake(touch_mutex, portMAX_DELAY);
    count = touch_event_count;
    xSemaphoreGive(touch_mutex);
    return count;
}

/**
 * Reset touch event count
 */
void touch_sensor_reset_touch_count(void)
{
    xSemaphoreTake(touch_mutex, portMAX_DELAY);
    touch_event_count = 0;
    ESP_LOGI(TAG, "Touch event count reset");
    xSemaphoreGive(touch_mutex);
}

/**
 * Get complete touch sensor state
 */
void touch_sensor_get_state(touch_sensor_state_t *state)
{
    xSemaphoreTake(touch_mutex, portMAX_DELAY);
    state->is_touched = sensor_touched;
    state->touch_count = touch_event_count;
    xSemaphoreGive(touch_mutex);
}
