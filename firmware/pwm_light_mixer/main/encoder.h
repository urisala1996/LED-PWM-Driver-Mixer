#ifndef ENCODER_H
#define ENCODER_H

#include "driver/gpio.h"

// Encoder GPIO pins
#define ENCODER_CLK_PIN GPIO_NUM_34   // Clock pin
#define ENCODER_DT_PIN GPIO_NUM_35    // Data pin
#define ENCODER_SW_PIN GPIO_NUM_32    // Switch pin

typedef struct {
    int32_t position;
    uint32_t button_press_count;
    bool button_pressed;
    uint32_t scale_factor;
} encoder_state_t;

/**
 * Initialize the rotary encoder
 */
void encoder_init(void);

/**
 * Start the encoder RTOS task
 */
void encoder_task_start(void);

/**
 * Get the current encoder position
 * @return Current position value
 */
int32_t encoder_get_position(void);

/**
 * Reset encoder position to zero
 */
void encoder_reset_position(void);

/**
 * Get button press count
 * @return Number of button presses
 */
uint32_t encoder_get_button_press_count(void);

/**
 * Check if button is currently pressed
 * @return true if button is pressed, false otherwise
 */
bool encoder_is_button_pressed(void);

/**
 * Set encoder position directly (useful for syncing with saved state)
 * @param position New position (will be clamped to 0-255)
 */
void encoder_set_position(int32_t position);

/**
 * Get complete encoder state
 * @param state Pointer to state structure to fill
 */
void encoder_get_state(encoder_state_t *state);

/**
 * Get current scale factor
 * @return Current scale factor (1, 2, or 5)
 */
uint32_t encoder_get_scale_factor(void);

/**
 * Cycle to next scale factor (1 -> 2 -> 5 -> 1)
 */
void encoder_cycle_scale_factor(void);

#endif // ENCODER_H
