#ifndef TOUCH_SENSOR_H
#define TOUCH_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool is_touched;
    uint32_t touch_count;
} touch_sensor_state_t;

void touch_sensor_init(void);

bool touch_sensor_is_touched(void);

uint32_t touch_sensor_get_touch_count(void);

void touch_sensor_reset_touch_count(void);

void touch_sensor_get_state(touch_sensor_state_t *state);

#endif
