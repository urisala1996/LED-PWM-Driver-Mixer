#include "encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ENCODER";

// Encoder position bounds (0-255 for PWM 8-bit resolution)
#define ENCODER_POS_MIN 0
#define ENCODER_POS_MAX 255
#define ENCODER_INITIAL_POS 155

// Static variables for encoder state
static volatile int32_t encoder_position = ENCODER_INITIAL_POS;
static volatile uint32_t button_press_count = 0;
static volatile bool button_pressed = false;
static volatile int last_clk_state = 0;
static volatile int last_dt_state = 0;

// Scale factor for faster adjustments (1, 2, or 5)
static const uint32_t SCALE_FACTORS[] = {1, 2, 5};
static const uint32_t NUM_SCALES = 3;
static volatile uint32_t current_scale_index = 0;  // Start with scale 1

// Mutex for thread-safe access
static SemaphoreHandle_t encoder_mutex = NULL;

/**
 * Read current button state
 */
static bool encoder_read_button(void)
{
    return gpio_get_level(ENCODER_SW_PIN) == 0;  // Button is active low
}

/**
 * Read current CLK state
 */
static int encoder_read_clk(void)
{
    return gpio_get_level(ENCODER_CLK_PIN);
}

/**
 * Read current DT state
 */
static int encoder_read_dt(void)
{
    return gpio_get_level(ENCODER_DT_PIN);
}

/**
 * Update encoder position based on rotation
 * Uses robust quadrature decoding to detect all state transitions
 * Clamps position between ENCODER_POS_MIN and ENCODER_POS_MAX
 */
static void encoder_update_position(void)
{
    int clk_state = encoder_read_clk();
    int dt_state = encoder_read_dt();
    
    // Check for state change (detect both CLK and DT transitions)
    int state_changed = (clk_state != last_clk_state) || (dt_state != last_dt_state);
    
    if (state_changed) {
        // Quadrature state machine: detect direction from state transitions
        // The encoder produces four states: 00, 01, 10, 11
        // Rotating one direction: 00 -> 10 -> 11 -> 01 -> 00 (clockwise)
        // Rotating other direction: 00 -> 01 -> 11 -> 10 -> 00 (counter-clockwise)
        
        // Create a 2-bit number from previous and current state
        int prev_state = (last_clk_state << 1) | last_dt_state;
        int curr_state = (clk_state << 1) | dt_state;
        
        // Detect valid encoder transitions (Gray code)
        // Valid clockwise: 0->1 or 1->3 or 3->2 or 2->0
        // Valid counter-clockwise: 0->2 or 2->3 or 3->1 or 1->0
        uint32_t scale = SCALE_FACTORS[current_scale_index];
        
        if ((prev_state == 0 && curr_state == 1) ||
            (prev_state == 1 && curr_state == 3) ||
            (prev_state == 3 && curr_state == 2) ||
            (prev_state == 2 && curr_state == 0)) {
            // Clockwise rotation with scale factor
            if (encoder_position + (int32_t)scale <= ENCODER_POS_MAX) {
                encoder_position += scale;
            } else {
                encoder_position = ENCODER_POS_MAX;
            }
        } else if ((prev_state == 0 && curr_state == 2) ||
                   (prev_state == 2 && curr_state == 3) ||
                   (prev_state == 3 && curr_state == 1) ||
                   (prev_state == 1 && curr_state == 0)) {
            // Counter-clockwise rotation with scale factor
            if (encoder_position - (int32_t)scale >= ENCODER_POS_MIN) {
                encoder_position -= scale;
            } else {
                encoder_position = ENCODER_POS_MIN;
            }
        }
    }
    
    last_clk_state = clk_state;
    last_dt_state = dt_state;
}

/**
 * RTOS task for encoder reading
 */
static void encoder_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Encoder task started");
    
    bool last_button_state = false;
    
    while (1) {
        // Read button state
        bool current_button_state = encoder_read_button();
        
        if (current_button_state && !last_button_state) {
            // Button pressed (falling edge)
            xSemaphoreTake(encoder_mutex, portMAX_DELAY);
            button_press_count++;
            button_pressed = true;
            
            // Cycle scale factor
            current_scale_index = (current_scale_index + 1) % NUM_SCALES;
            uint32_t new_scale = SCALE_FACTORS[current_scale_index];
            
            ESP_LOGI(TAG, "Button pressed! Count: %lu | Scale factor changed to: %lu", 
                     button_press_count, new_scale);
            xSemaphoreGive(encoder_mutex);
        } else if (!current_button_state && last_button_state) {
            // Button released
            xSemaphoreTake(encoder_mutex, portMAX_DELAY);
            button_pressed = false;
            ESP_LOGI(TAG, "Button released");
            xSemaphoreGive(encoder_mutex);
        }
        
        last_button_state = current_button_state;
        
        // Update encoder position
        encoder_update_position();
        
        // Polling interval (10ms) - sufficient for reliable encoder detection
        // while allowing other tasks time to run
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

/**
 * Initialize the rotary encoder
 */
void encoder_init(void)
{
    ESP_LOGI(TAG, "Initializing rotary encoder");
    
    // Create mutex for thread-safe access
    encoder_mutex = xSemaphoreCreateMutex();
    if (encoder_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }
    
    // Configure GPIO for CLK pin
    gpio_config_t clk_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << ENCODER_CLK_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&clk_conf);
    
    // Configure GPIO for DT pin
    gpio_config_t dt_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << ENCODER_DT_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&dt_conf);
    
    // Configure GPIO for Switch pin
    gpio_config_t sw_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << ENCODER_SW_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&sw_conf);
    
    // Initialize last CLK state
    last_clk_state = encoder_read_clk();
    
    ESP_LOGI(TAG, "Encoder pins configured: CLK=%d, DT=%d, SW=%d",
             ENCODER_CLK_PIN, ENCODER_DT_PIN, ENCODER_SW_PIN);
}

/**
 * Start the encoder RTOS task
 */
void encoder_task_start(void)
{
    xTaskCreate(
        encoder_task,           // Task function
        "encoder_task",         // Task name
        2048,                   // Stack size
        NULL,                   // Task parameters
        5,                      // Priority (higher than main)
        NULL                    // Task handle
    );
    ESP_LOGI(TAG, "Encoder RTOS task created");
}

/**
 * Get the current encoder position
 */
int32_t encoder_get_position(void)
{
    int32_t pos;
    xSemaphoreTake(encoder_mutex, portMAX_DELAY);
    pos = encoder_position;
    xSemaphoreGive(encoder_mutex);
    return pos;
}

/**
 * Reset encoder position to zero
 */
void encoder_reset_position(void)
{
    xSemaphoreTake(encoder_mutex, portMAX_DELAY);
    encoder_position = 0;
    ESP_LOGI(TAG, "Encoder position reset to 0");
    xSemaphoreGive(encoder_mutex);
}

/**
 * Get button press count
 */
uint32_t encoder_get_button_press_count(void)
{
    uint32_t count;
    xSemaphoreTake(encoder_mutex, portMAX_DELAY);
    count = button_press_count;
    xSemaphoreGive(encoder_mutex);
    return count;
}

/**
 * Check if button is currently pressed
 */
bool encoder_is_button_pressed(void)
{
    bool pressed;
    xSemaphoreTake(encoder_mutex, portMAX_DELAY);
    pressed = button_pressed;
    xSemaphoreGive(encoder_mutex);
    return pressed;
}

/**
 * Reset button press count
 */
void encoder_reset_button_count(void)
{
    xSemaphoreTake(encoder_mutex, portMAX_DELAY);
    button_press_count = 0;
    ESP_LOGI(TAG, "Button press count reset");
    xSemaphoreGive(encoder_mutex);
}

/**
 * Set encoder position directly
 */
void encoder_set_position(int32_t position)
{
    xSemaphoreTake(encoder_mutex, portMAX_DELAY);
    
    // Clamp position to valid range
    if (position < ENCODER_POS_MIN) {
        encoder_position = ENCODER_POS_MIN;
    } else if (position > ENCODER_POS_MAX) {
        encoder_position = ENCODER_POS_MAX;
    } else {
        encoder_position = position;
    }
    
    ESP_LOGI(TAG, "Encoder position set to %ld", encoder_position);
    xSemaphoreGive(encoder_mutex);
}

/**
 * Get complete encoder state
 */
void encoder_get_state(encoder_state_t *state)
{
    xSemaphoreTake(encoder_mutex, portMAX_DELAY);
    state->position = encoder_position;
    state->button_press_count = button_press_count;
    state->button_pressed = button_pressed;
    state->scale_factor = SCALE_FACTORS[current_scale_index];
    xSemaphoreGive(encoder_mutex);
}

/**
 * Get current scale factor
 */
uint32_t encoder_get_scale_factor(void)
{
    uint32_t scale;
    xSemaphoreTake(encoder_mutex, portMAX_DELAY);
    scale = SCALE_FACTORS[current_scale_index];
    xSemaphoreGive(encoder_mutex);
    return scale;
}

/**
 * Cycle to next scale factor (1 -> 2 -> 5 -> 1)
 */
void encoder_cycle_scale_factor(void)
{
    xSemaphoreTake(encoder_mutex, portMAX_DELAY);
    current_scale_index = (current_scale_index + 1) % NUM_SCALES;
    uint32_t new_scale = SCALE_FACTORS[current_scale_index];
    ESP_LOGI(TAG, "Scale factor cycled to: %lu", new_scale);
    xSemaphoreGive(encoder_mutex);
}
