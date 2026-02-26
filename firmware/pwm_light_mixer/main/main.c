#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "config.h"
#include "pwm_controller.h"
#include "encoder.h"
#include "touch_sensor.h"
#include "nvs_manager.h"

static const char *TAG = "MAIN";

typedef struct {
    bool pwm_enabled;
    int32_t current_position;
    uint32_t last_touch_count;
    uint32_t nvs_check_counter;
} app_state_t;

static int app_init_all_modules(void)
{
    ESP_LOGI(TAG, "Init modules");

    if (CONFIG_ENABLE_NVS_STORAGE) {
        if (nvs_manager_init() != 0) {
            ESP_LOGE(TAG, "NVS init failed");
            return -1;
        }
    }

    encoder_init();
    encoder_task_start();

    if (CONFIG_ENABLE_TOUCH_TOGGLE) {
        touch_sensor_init();
    }

    if (pwm_controller_init() != 0) {
        return -1;
    }

    return 0;
}

static int app_restore_state(app_state_t *state)
{
    nvs_led_state_t saved_state;

    if (nvs_manager_load_led_state(&saved_state) != 0) {
        return -1;
    }

    state->pwm_enabled = saved_state.pwm_enabled;
    state->current_position = saved_state.pwm_value;
    state->last_touch_count = 0;
    state->nvs_check_counter = 0;

    encoder_set_position((int32_t)saved_state.pwm_value);

    if (saved_state.pwm_enabled) {
        pwm_controller_set_brightness(saved_state.pwm_value);
    } else {
        pwm_controller_set_brightness(0);
    }

    return 0;
}

static void app_handle_encoder_change(int32_t position, app_state_t *state)
{
    state->current_position = position;

    if (state->pwm_enabled) {
        pwm_controller_set_brightness((uint32_t)position);

        nvs_led_state_t current_state = {
            .pwm_enabled = state->pwm_enabled,
            .pwm_value = (uint32_t)position
        };
        nvs_manager_save_led_state(&current_state);
    }
}

static void app_handle_touch_toggle(app_state_t *state)
{
    state->pwm_enabled = !state->pwm_enabled;

    if (state->pwm_enabled) {
        pwm_controller_set_brightness((uint32_t)state->current_position);
    } else {
        pwm_controller_set_brightness(0);
    }

    nvs_led_state_t current_state = {
        .pwm_enabled = state->pwm_enabled,
        .pwm_value = (uint32_t)state->current_position
    };
    nvs_manager_save_led_state(&current_state);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting LED PWM Driver");

    if (app_init_all_modules() != 0) {
        return;
    }

    app_state_t state = {0};
    if (app_restore_state(&state) != 0) {
        state.pwm_enabled = CONFIG_NVS_DEFAULT_PWM_ENABLE;
        state.current_position = CONFIG_NVS_DEFAULT_PWM_VALUE;
    }

    ESP_LOGI(TAG, "Entering main loop");

    while (1) {
        if (CONFIG_ENABLE_TOUCH_TOGGLE) {
            uint32_t touch_count = touch_sensor_get_touch_count();
            if (touch_count != state.last_touch_count) {
                state.last_touch_count = touch_count;
                app_handle_touch_toggle(&state);
            }
        }

        int32_t position = encoder_get_position();
        if (position != state.current_position) {
            app_handle_encoder_change(position, &state);
        }

        if (++state.nvs_check_counter >= CONFIG_NVS_CHECK_COUNT) {
            state.nvs_check_counter = 0;
            nvs_manager_check_pending_write();
        }

        vTaskDelay(CONFIG_MAIN_LOOP_INTERVAL / portTICK_PERIOD_MS);
    }
}
