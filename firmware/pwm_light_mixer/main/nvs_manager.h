#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool pwm_enabled;
    uint32_t pwm_value;
} nvs_led_state_t;

/**
 * Initialize NVS (Non-Volatile Storage)
 * @return 0 on success, -1 on failure
 */
int nvs_manager_init(void);

/**
 * Load LED state from flash
 * @param state Pointer to state structure to fill
 * @return 0 on success, -1 on failure
 */
int nvs_manager_load_led_state(nvs_led_state_t *state);

/**
 * Save LED state to flash (queues for debounced write with 5s delay)
 * Actual flash write only happens if value is stable for 5 seconds
 * @param state Pointer to state structure to save
 * @return 0 on success, -1 on failure
 */
int nvs_manager_save_led_state(const nvs_led_state_t *state);

/**
 * Get the last saved state without writing
 * @param state Pointer to state structure to fill
 * @return 0 on success, -1 on failure
 */
int nvs_manager_get_last_state(nvs_led_state_t *state);

/**
 * Check and commit pending writes if debounce time (5s) has elapsed
 * Should be called periodically from main task loop
 * @return 0 if no write occurred or write succeeded, -1 on error
 */
int nvs_manager_check_pending_write(void);

#endif // NVS_MANAGER_H
