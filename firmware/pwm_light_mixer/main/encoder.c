#include "encoder.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ENCODER";

/**
 * ============================================================================
 * ENCODER STATE AND CONFIGURATION
 * ============================================================================
 */

// Encoder position bounds
#define ENCODER_POS_MIN               0
#define ENCODER_POS_MAX               255
#define ENCODER_INITIAL_POS           CONFIG_ENCODER_INITIAL_POS

// Scale factors for acceleration
static const uint32_t SCALE_FACTORS[] = CONFIG_ENCODER_SCALE_FACTORS;
static const uint32_t NUM_SCALES = CONFIG_ENCODER_NUM_SCALES;

// Encoder state variables
static volatile int32_t encoder_position = ENCODER_INITIAL_POS;
static volatile uint32_t button_press_count = 0;
static volatile bool button_pressed = false;
static volatile int last_clk_state = 0;
static volatile int last_dt_state = 0;
static volatile uint32_t current_scale_index = 0;

// Thread synchronization
static SemaphoreHandle_t encoder_mutex = NULL;

/**
 * @brief Read current button state (active low)
 */
static bool encoder_read_button(void)
{
    return gpio_get_level(CONFIG_ENCODER_SW_PIN) == 0;
}

/**
 * @brief Read current CLK pin state
 */
static int encoder_read_clk(void)
{
    return gpio_get_level(CONFIG_ENCODER_CLK_PIN);
}

/**
 * @brief Read current DT pin state
 */
static int encoder_read_dt(void)
{
    return gpio_get_level(CONFIG_ENCODER_DT_PIN);
}

/**
 * @brief Update encoder position based on rotation
 * 
 * Uses quadrature decoding to detect all state transitions reliably.
 * Position is clamped between ENCODER_POS_MIN and ENCODER_POS_MAX.
 * 
 * Supports standard full-step quadrature:
 * CW:  00->01->11->10->00
 * CCW: 00->10->11->01->00
 */
static void encoder_update_position(void)
{
    int clk_state = encoder_read_clk();
    int dt_state = encoder_read_dt();
    
    // Check for state change
    bool clk_changed = (clk_state != last_clk_state);
    bool dt_changed = (dt_state != last_dt_state);
    
    if (clk_changed || dt_changed) {
        int prev_state = (last_clk_state << 1) | last_dt_state;
        int curr_state = (clk_state << 1) | dt_state;
        
        uint32_t scale = SCALE_FACTORS[current_scale_index];
        int direction = 0;
        
        // Standard quadrature decoding
        // CW transitions: 00->01, 01->11, 11->10, 10->00
        if ((prev_state == 0 && curr_state == 1) ||
            (prev_state == 1 && curr_state == 3) ||
            (prev_state == 3 && curr_state == 2) ||
            (prev_state == 2 && curr_state == 0)) {
            direction = 1;  // Clockwise
        }
        // CCW transitions: 00->10, 10->11, 11->01, 01->00
        else if ((prev_state == 0 && curr_state == 2) ||
                 (prev_state == 2 && curr_state == 3) ||
                 (prev_state == 3 && curr_state == 1) ||
                 (prev_state == 1 && curr_state == 0)) {
            direction = -1;  // Counter-clockwise
        }
        else {
            // Could also try inverted interpretation or other encoder types
            // For now, treat as invalid transition
            direction = 0;
        }
        
        // Update position with mutex protection
        xSemaphoreTake(encoder_mutex, portMAX_DELAY);
        
        int32_t old_pos = encoder_position;
        
        if (direction == 1) {
            if (encoder_position + (int32_t)scale <= ENCODER_POS_MAX) {
                encoder_position += scale;
            } else {
                encoder_position = ENCODER_POS_MAX;
            }
        } else if (direction == -1) {
            if (encoder_position - (int32_t)scale >= ENCODER_POS_MIN) {
                encoder_position -= scale;
            } else {
                encoder_position = ENCODER_POS_MIN;
            }
        }
        
        if (encoder_position != old_pos) {
            ESP_LOGI(TAG, "%s: %d->%d | pos %ld->%ld (scale=%lu)", 
                     direction > 0 ? "CW" : "CCW", 
                     prev_state, curr_state, old_pos, encoder_position, scale);
        } else if (direction == 0) {
            ESP_LOGD(TAG, "Invalid: %d->%d", prev_state, curr_state);
        }
        
        xSemaphoreGive(encoder_mutex);
    }
    
    last_clk_state = clk_state;
    last_dt_state = dt_state;
}

/**
 * @brief RTOS task for encoder reading
 * 
 * Monitors encoder CLK/DT pins and button state. Updates position on
 * quadrature state changes and counts button press events.
 * 
 * @param pvParameters Task parameters (unused)
 */
static void encoder_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Encoder task started");
    
    bool last_button_state = false;
    int32_t last_logged_position = encoder_position;
    uint32_t log_counter = 0;
    
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
        
        // Log position periodically if it changes
        if (++log_counter >= 50) {  // Log every 500ms (50 * 10ms)
            log_counter = 0;
            int32_t current_pos = encoder_get_position();
            if (current_pos != last_logged_position) {
                ESP_LOGI(TAG, "Position changed: %ld", current_pos);
                last_logged_position = current_pos;
            }
        }
        
        // Polling interval (set in config, typically 10ms)
        vTaskDelay(CONFIG_ENCODER_POLL_INTERVAL / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Initialize the rotary encoder
 * 
 * Configures GPIO pins for CLK, DT, and SW with pull-ups.
 * Creates synchronization mutex for thread-safe state access.
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
        .pin_bit_mask = (1ULL << CONFIG_ENCODER_CLK_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&clk_conf);
    
    // Configure GPIO for DT pin
    gpio_config_t dt_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CONFIG_ENCODER_DT_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&dt_conf);
    
    // Configure GPIO for Switch pin
    gpio_config_t sw_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CONFIG_ENCODER_SW_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&sw_conf);
    
    // Wait a brief moment for GPIO stabilization
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    // Initialize last CLK and DT states (CRITICAL: both must be set!)
    last_clk_state = encoder_read_clk();
    last_dt_state = encoder_read_dt();
    
    ESP_LOGI(TAG, "Encoder pins configured: CLK=%d, DT=%d, SW=%d",
             CONFIG_ENCODER_CLK_PIN, CONFIG_ENCODER_DT_PIN, CONFIG_ENCODER_SW_PIN);
    ESP_LOGI(TAG, "Encoder initial state: CLK=%d, DT=%d", last_clk_state, last_dt_state);
    ESP_LOGI(TAG, "Encoder polling interval: %d ms", CONFIG_ENCODER_POLL_INTERVAL);
    ESP_LOGI(TAG, "Starting position: %ld / 255", encoder_position);
}

/**
 * @brief Start the encoder RTOS task
 * 
 * Creates and starts the background task that continuously monitors
 * encoder position and button events.
 */
void encoder_task_start(void)
{
    xTaskCreate(
        encoder_task,                      // Task function
        "encoder_task",                    // Task name
        CONFIG_ENCODER_TASK_STACK,         // Stack size
        NULL,                              // Task parameters
        CONFIG_ENCODER_TASK_PRIORITY,      // Priority
        NULL                               // Task handle
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
 * @brief Diagnostic: Get raw pin states
 * @return Packed as 0xCB00 where C=CLK, B=button, D=DT
 */
uint32_t encoder_get_raw_pins(void)
{
    int clk = encoder_read_clk();
    int dt = encoder_read_dt();
    int btn = encoder_read_button();
    return (clk << 2) | (btn << 1) | dt;
}

/**
 * @brief Diagnostic: Log current pin states
 */
void encoder_log_diagnostic(void)
{
    int clk = encoder_read_clk();
    int dt = encoder_read_dt();
    int btn = encoder_read_button();
    int32_t pos = encoder_get_position();
    
    ESP_LOGI(TAG, "DIAG: CLK=%d DT=%d BTN=%d POS=%ld SCALE=%lu", 
             clk, dt, btn, pos, SCALE_FACTORS[current_scale_index]);
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
