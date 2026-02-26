#ifndef PWM_CONTROLLER_H
#define PWM_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

int pwm_controller_init(void);

int pwm_controller_set_brightness(uint32_t duty);

int pwm_controller_set_brightness_ch1(uint32_t duty);

int pwm_controller_set_brightness_ch2(uint32_t duty);

uint32_t pwm_controller_get_brightness_ch1(void);

uint32_t pwm_controller_get_brightness_ch2(void);

bool pwm_controller_is_enabled(void);

#endif
