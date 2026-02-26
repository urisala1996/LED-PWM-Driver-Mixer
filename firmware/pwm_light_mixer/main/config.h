/**
 * @file config.h
 * @brief Central configuration for LED PWM Driver Mixer
 * 
 * This header consolidates all hardware pins, constants, and configuration
 * parameters in a single location for easy maintenance and modification.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "driver/ledc.h"

/**
 * ============================================================================
 * HARDWARE PIN DEFINITIONS
 * ============================================================================
 */

/** @defgroup GPIO_Pins GPIO Pin Assignments
 * @{
 */
#define CONFIG_LED_PIN_1              GPIO_NUM_2     ///< First LED PWM output
#define CONFIG_LED_PIN_2              GPIO_NUM_33    ///< Second LED PWM output

#define CONFIG_ENCODER_CLK_PIN        GPIO_NUM_34    ///< Encoder clock pin
#define CONFIG_ENCODER_DT_PIN         GPIO_NUM_35    ///< Encoder data pin
#define CONFIG_ENCODER_SW_PIN         GPIO_NUM_32    ///< Encoder button pin

#define CONFIG_TOUCH_SENSOR_PIN       GPIO_NUM_15    ///< Touch sensor input pin
/** @} */

/**
 * ============================================================================
 * PWM CONFIGURATION
 * ============================================================================
 */

/** @defgroup PWM_Config PWM Hardware Configuration
 * @{
 */
#define CONFIG_LEDC_TIMER             LEDC_TIMER_0
#define CONFIG_LEDC_MODE              LEDC_HIGH_SPEED_MODE
#define CONFIG_LEDC_CHANNEL_1         LEDC_CHANNEL_0
#define CONFIG_LEDC_CHANNEL_2         LEDC_CHANNEL_1
#define CONFIG_LEDC_DUTY_RES          LEDC_TIMER_8_BIT   ///< 8-bit resolution (0-255)
#define CONFIG_LEDC_FREQUENCY         5000               ///< PWM frequency in Hz
#define CONFIG_PWM_MIN_DUTY           0                  ///< Minimum PWM duty
#define CONFIG_PWM_MAX_DUTY           255                ///< Maximum PWM duty (8-bit)
/** @} */

/**
 * ============================================================================
 * ENCODER CONFIGURATION
 * ============================================================================
 */

/** @defgroup Encoder_Config Encoder Behavior Configuration
 * @{
 */
#define CONFIG_ENCODER_INITIAL_POS    155                ///< Initial encoder position
#define CONFIG_ENCODER_SCALE_FACTORS  {1, 2, 5}          ///< Available scale factors
#define CONFIG_ENCODER_NUM_SCALES     3                  ///< Number of scale factors
#define CONFIG_ENCODER_POLL_INTERVAL  10                 ///< Polling interval in ms
/** @} */

/**
 * ============================================================================
 * TOUCH SENSOR CONFIGURATION
 * ============================================================================
 */

/** @defgroup Touch_Config Touch Sensor Behavior Configuration
 * @{
 */
#define CONFIG_TOUCH_DEBOUNCE_MS      50                 ///< Touch debounce time in ms
#define CONFIG_TOUCH_DEBOUNCE_COUNT   5                  ///< Debounce counter threshold
#define CONFIG_TOUCH_POLL_INTERVAL    10                 ///< Polling interval in ms
/** @} */

/**
 * ============================================================================
 * NVS (NON-VOLATILE STORAGE) CONFIGURATION
 * ============================================================================
 */

/** @defgroup NVS_Config Flash Storage Configuration
 * @{
 */
#define CONFIG_NVS_NAMESPACE          "led_ctrl"         ///< NVS namespace for LED state
#define CONFIG_NVS_KEY_PWM_ENABLED    "pwm_en"           ///< Key for PWM enabled flag
#define CONFIG_NVS_KEY_PWM_VALUE      "pwm_val"          ///< Key for PWM value
#define CONFIG_FLASH_WRITE_DEBOUNCE   5000               ///< Flash write debounce in ms
#define CONFIG_NVS_DEFAULT_PWM_VALUE  CONFIG_ENCODER_INITIAL_POS ///< Default PWM value
#define CONFIG_NVS_DEFAULT_PWM_ENABLE true               ///< Default PWM enabled state
/** @} */

/**
 * ============================================================================
 * TASK CONFIGURATION
 * ============================================================================
 */

/** @defgroup Task_Config RTOS Task Configuration
 * @{
 */
#define CONFIG_ENCODER_TASK_STACK     2048               ///< Encoder task stack size
#define CONFIG_ENCODER_TASK_PRIORITY  5                  ///< Encoder task priority
#define CONFIG_TOUCH_TASK_STACK       2048               ///< Touch sensor task stack size
#define CONFIG_TOUCH_TASK_PRIORITY    5                  ///< Touch sensor task priority
#define CONFIG_MAIN_LOOP_INTERVAL     50                 ///< Main loop polling in ms
/** @} */

/**
 * ============================================================================
 * NVS CHECK INTERVAL CONFIGURATION
 * ============================================================================
 */

/** @defgroup NVS_Check NVS Check Interval Calculation
 * @{
 */
#define CONFIG_NVS_CHECK_INTERVAL     200                ///< NVS pending write check in ms
#define CONFIG_NVS_CHECK_COUNT        (CONFIG_NVS_CHECK_INTERVAL / CONFIG_MAIN_LOOP_INTERVAL)
/** @} */

/**
 * ============================================================================
 * FEATURE FLAGS
 * ============================================================================
 */

/** @defgroup Feature_Flags Feature Enable/Disable Flags
 * @{
 */
#define CONFIG_ENABLE_NVS_STORAGE     1                  ///< Enable persistent storage
#define CONFIG_ENABLE_TOUCH_TOGGLE    1                  ///< Enable touch sensor toggle
/** @} */

#endif // CONFIG_H
