#ifndef TOUCH_SENSOR_H
#define TOUCH_SENSOR_H

#include "driver/gpio.h"
#include <stdbool.h>

// Touch sensor GPIO pin
#define TOUCH_SENSOR_PIN GPIO_NUM_15

typedef struct {
    bool is_touched;
    uint32_t touch_count;
} touch_sensor_state_t;

/**
 * Initialize the touch sensor
 */
void touch_sensor_init(void);

/**
 * Check if touch sensor is currently touched
 * @return true if sensor is touched, false otherwise
 */
bool touch_sensor_is_touched(void);

/**
 * Get the number of touch events detected
 * @return Touch event count
 */
uint32_t touch_sensor_get_touch_count(void);

/**
 * Reset touch event count
 */
void touch_sensor_reset_touch_count(void);

/**
 * Get complete touch sensor state
 * @param state Pointer to state structure to fill
 */
void touch_sensor_get_state(touch_sensor_state_t *state);

#endif // TOUCH_SENSOR_H
