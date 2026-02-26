/**
 * @mainpage LED PWM Driver Mixer - Refactored Architecture
 * 
 * ## Overview
 * 
 * This codebase has been comprehensively refactored for production quality,
 * modularity, and maintainability. The system controls dual-channel LED
 * brightness via a rotary encoder with persistent state storage.
 * 
 * ## Architecture
 * 
 * ### Module Organization
 * 
 * ```
 * main.c
 *  ├── config.h (Centralized Constants)
 *  ├── pwm_controller.{h,c} (LED PWM Abstraction)
 *  ├── encoder.{h,c} (Rotary Encoder Interface)
 *  ├── touch_sensor.{h,c} (Touch Sensor Interface)
 *  └── nvs_manager.{h,c} (Flash Storage)
 * ```
 * 
 * ### Design Principles
 * 
 * 1. **Separation of Concerns**: Each module handles a single responsibility
 * 2. **Centralized Configuration**: All constants in config.h for easy customization
 * 3. **Clear APIs**: Public interfaces clearly defined in header files
 * 4. **Thread Safety**: Mutexes protect shared state in background tasks
 * 5. **Error Handling**: Proper return codes and validation throughout
 * 6. **Documentation**: Comprehensive Doxygen-style comments
 * 
 * ## Module Descriptions
 * 
 * ### config.h
 * 
 * **Purpose**: Centralized configuration for the entire application
 * **Key Contents**:
 * - Hardware pin definitions (GPIO assignments)
 * - PWM parameters (frequency, resolution, channels)
 * - Encoder configuration (scale factors, polling intervals)
 * - Touch sensor debounce settings
 * - NVS flash storage parameters
 * - RTOS task configuration
 * - Feature flags
 * 
 * **Benefits**:
 * - Single location to modify all hardware constants
 * - Easy to adapt to different hardware configurations
 * - Feature flags allow compile-time feature selection
 * 
 * ### pwm_controller.{h,c}
 * 
 * **Purpose**: High-level PWM LED control abstraction
 * **Public API**:
 * - `pwm_controller_init()`: Initialize LEDC timer and channels
 * - `pwm_controller_set_brightness()`: Set both LED channels
 * - `pwm_controller_set_brightness_ch1/2()`: Control individual channels
 * - `pwm_controller_get_brightness_ch1/2()`: Query current brightness
 * 
 * **Implementation Details**:
 * - Handles LEDC hardware configuration
 * - Value clamping (0-255 range)
 * - Error handling for hardware operations
 * - Tracks current state for status queries
 * 
 * **Why Separate**:
 * - Shields application from LEDC driver complexity
 * - Easy to add features (PWM fade, ramping, etc.)
 * - Can switch implementations without changing main.c
 * 
 * ### encoder.{h,c}
 * 
 * **Purpose**: Rotary encoder interface with acceleration scales
 * **Public API**:
 * - `encoder_init()`: Configure GPIO and initialize state
 * - `encoder_task_start()`: Start background monitoring task
 * - `encoder_get_position()`: Get current position (0-255)
 * - `encoder_get_scale_factor()`: Get current acceleration scale
 * - `encoder_get_button_press_count()`: Get button press events
 * - `encoder_cycle_scale_factor()`: Switch 1x/2x/5x acceleration
 * 
 * **Implementation Details**:
 * - Quadrature decoding for reliable position tracking
 * - Button edge detection with event counting
 * - Semaphore-protected state for thread safety
 * - Configurable polling interval (default 10ms)
 * 
 * **Acceleration Scales**:
 * - 1x: Fine control (1 position per tick)
 * - 2x: Medium control (2 positions per tick)
 * - 5x: Fast control (5 positions per tick)
 * - Cycled via button press
 * 
 * ### touch_sensor.{h,c}
 * 
 * **Purpose**: Debounced touch sensor input
 * **Public API**:
 * - `touch_sensor_init()`: Configure GPIO and start task
 * - `touch_sensor_is_touched()`: Query current touch state
 * - `touch_sensor_get_touch_count()`: Get number of touch events
 * - `touch_sensor_reset_touch_count()`: Reset event counter
 * 
 * **Implementation Details**:
 * - Configurable debounce window (default 50ms)
 * - Rising edge detection for event counting
 * - Semaphore-protected state
 * - Background task with configurable polling (default 10ms)
 * 
 * **Use Case**:
 * Primary application toggles LED on/off with each touch event
 * 
 * ### nvs_manager.{h,c}
 * 
 * **Purpose**: Persistent LED state storage in flash
 * **Features**:
 * - Load state on startup (with defaults if no saved state)
 * - Debounced writes (5 second default) to reduce flash wear
 * - State caching to avoid duplicate writes
 * - Full error handling
 * 
 * **Public API**:
 * - `nvs_manager_init()`: Initialize NVS partition
 * - `nvs_manager_load_led_state()`: Read state from flash
 * - `nvs_manager_save_led_state()`: Queue state for write
 * - `nvs_manager_check_pending_write()`: Commit pending writes
 * - `nvs_manager_get_last_state()`: Read last saved state
 * 
 * **Debouncing Strategy**:
 * - State changes are queued but not immediately written
 * - Only committed if state is stable for 5 seconds
 * - Reduces flash wear from rapid brightness adjustments
 * - Check called every 200ms from main loop
 * 
 * **Stored Data**:
 * - `pwm_enabled`: LED enable/disable state (bool)
 * - `pwm_value`: Brightness value (0-255)
 * 
 * ### main.c
 * 
 * **Purpose**: Application orchestration and event handling
 * **Structure**:
 * - `app_init_all_modules()`: Sequential initialization
 * - `app_restore_state()`: Load saved state from flash
 * - `app_handle_encoder_change()`: Process position updates
 * - `app_handle_touch_toggle()`: Process touch events
 * - `app_main()`: Main event loop
 * 
 * **Main Loop Design**:
 * 1. Check touch sensor for toggle events
 * 2. Check encoder for position changes
 * 3. Periodic NVS maintenance (every 200ms)
 * 4. Sleep for 50ms before next iteration
 * 
 * **Event Flow**:
 * ```
 * User rotates encoder
 *   ↓
 * encoder_task detects rotation
 *   ↓
 * app_main reads position
 *   ↓
 * pwm_controller updates brightness
 *   ↓
 * nvs_manager queues state for saving
 *   ↓
 * (after 5 seconds of stability)
 * nvs_manager commits to flash
 * ```
 * 
 * ## Task Structure
 * 
 * **Priority Levels**: (Lower number = higher priority)
 * - Priority 1: app_main (main loop)
 * - Priority 5: encoder_task (rotary encoder monitoring)
 * - Priority 5: touch_sensor_task (touch sensor monitoring)
 * 
 * **Background Tasks**:
 * - `encoder_task`: Continuous encoder position monitoring
 *   - Stack: 2048 bytes
 *   - Interval: 10ms polling
 *   - Updates position, detects button press
 * 
 * - `touch_sensor_task`: Continuous touch sensor monitoring
 *   - Stack: 2048 bytes
 *   - Interval: 10ms polling
 *   - Debounces for ~50ms before declaring edge
 * 
 * ## Configuration Customization
 * 
 * To adapt to different hardware:
 * 
 * 1. **Change GPIO Pins**: Update in config.h
 *    ```c
 *    #define CONFIG_LED_PIN_1 GPIO_NUM_X
 *    #define CONFIG_ENCODER_CLK_PIN GPIO_NUM_Y
 *    ```
 * 
 * 2. **Adjust PWM Frequency**: Update in config.h
 *    ```c
 *    #define CONFIG_LEDC_FREQUENCY 10000  // 10kHz
 *    ```
 * 
 * 3. **Change Encoder Scale Factors**: Update in config.h
 *    ```c
 *    #define CONFIG_ENCODER_SCALE_FACTORS {1, 4, 10}
 *    ```
 * 
 * 4. **Disable Features**: Use feature flags in config.h
 *    ```c
 *    #define CONFIG_ENABLE_NVS_STORAGE 0  // Disable flash savings
 *    #define CONFIG_ENABLE_TOUCH_TOGGLE 0 // Disable touch control
 *    ```
 * 
 * ## Building and Flashing
 * 
 * Using ESP-IDF toolchain:
 * ```bash
 * idf.py build
 * idf.py flash
 * idf.py monitor
 * ```
 * 
 * ## Thread Safety
 * 
 * The design uses FreeRTOS semaphores (`xSemaphoreMutex`) to protect
 * shared state in background tasks:
 * 
 * - **Encoder state**: Protected by `encoder_mutex`
 * - **Touch state**: Protected by `touch_mutex`
 * - **NVS operations**: Serialized (single flash access pattern)
 * 
 * ## Error Handling
 * 
 * All public APIs return error codes:
 * - `0`: Success
 * - `-1`: Failure (logs error message)
 * 
 * NVS operations gracefully degrade:
 * - If flash is empty: Uses default values
 * - If flash is corrupted: Re-initializes with defaults
 * 
 * ## Memory Usage
 * 
 * Approximate memory breakdown:
 * - Static state variables: ~100 bytes
 * - encoder_task stack: 2048 bytes
 * - touch_sensor_task stack: 2048 bytes
 * - Semaphores: ~100 bytes
 * - Total: ~4.3 KB from heap
 * 
 * ## Future Enhancement Ideas
 * 
 * 1. **PWM Transitions**: Add smooth brightness ramping
 * 2. **Preset Levels**: Button long-press cycles through brightness presets
 * 3. **Color Mixing**: Extend to RGB LED support with color mixing
 * 4. **Remote Control**: Add MQTT/WiFi for remote brightness control
 * 5. **Logging**: Add circular buffer for event logging
 * 6. **Animation Modes**: Add pre-programmed brightness patterns
 * 7. **Power Save**: Add sleep mode when idle
 * 8. **Temperature Monitoring**: Add thermal management
 * 
 * ## Code Quality Improvements
 * 
 * This refactor addresses:
 * - ✓ Single responsibility principle: Each module handles one concern
 * - ✓ Configuration centralization: All constants in one place
 * - ✓ Clear APIs: Well-defined public interfaces
 * - ✓ Documentation: Comprehensive Doxygen comments
 * - ✓ Error handling: Proper return codes and validation
 * - ✓ Thread safety: Semaphore protection for shared state
 * - ✓ Modularity: Easy to add/remove/replace components
 * - ✓ Readability: Clear variable names and code structure
 * - ✓ Maintainability: Changes localized to specific modules
 * - ✓ Testability: Can test modules independently
 * 
 * ## Notes
 * 
 * - The .gitignore has been updated with comprehensive entries
 * - Refactored code is fully backward compatible with existing hardware
 * - All original functionality preserved and improved
 * - Ready for production deployment
 * 
 */

/**
 * @file config.h
 * @brief Central application configuration
 * @see @ref LED PWM Driver Mixer - Refactored Architecture for module overview
 */

/**
 * @file pwm_controller.h
 * @brief LED PWM hardware abstraction
 */

/**
 * @file encoder.h
 * @brief Rotary encoder interface module
 */

/**
 * @file touch_sensor.h
 * @brief Debounced touch sensor interface
 */

/**
 * @file nvs_manager.h
 * @brief Non-volatile storage flash manager
 */

/**
 * @file main.c
 * @brief Main application and event orchestration layer
 */
