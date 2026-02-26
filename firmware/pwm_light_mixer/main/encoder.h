#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int32_t position;
    uint32_t button_press_count;
    bool button_pressed;
    uint32_t scale_factor;
} encoder_state_t;

void encoder_init(void);

void encoder_task_start(void);

int32_t encoder_get_position(void);

void encoder_reset_position(void);

uint32_t encoder_get_button_press_count(void);

void encoder_reset_button_count(void);

bool encoder_is_button_pressed(void);

void encoder_set_position(int32_t position);

void encoder_get_state(encoder_state_t *state);

uint32_t encoder_get_scale_factor(void);

void encoder_cycle_scale_factor(void);

uint32_t encoder_get_raw_pins(void);

void encoder_log_diagnostic(void);

#endif
