#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool pwm_enabled;
    uint32_t pwm_value;
} nvs_led_state_t;

int nvs_manager_init(void);

int nvs_manager_load_led_state(nvs_led_state_t *state);

int nvs_manager_save_led_state(const nvs_led_state_t *state);

int nvs_manager_get_last_state(nvs_led_state_t *state);

int nvs_manager_check_pending_write(void);

#endif
