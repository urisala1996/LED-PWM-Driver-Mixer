# PWM Light Mixer Firmware

A sophisticated dual-channel PWM LED brightness control system for ESP32 WROOM-32 DevKit with rotary encoder, capacitive touch sensor, and non-volatile persistent storage.

## Features

✨ **Core Features:**
- Dual-channel PWM LED control (GPIO 2 and GPIO 33)
- Rotary encoder with 3-scale factor modes (1x, 2x, 5x) for variable speed control
- Capacitive touch sensor for on/off toggle
- Persistent state storage (NVS Flash) with 5-second debounce
- Real-time brightness adjustment (0-255 PWM levels)
- Scale factor cycling via encoder button press
- Intelligent flash write optimization to minimize wear

## Hardware Requirements

### ESP32 WROOM-32 DevKit

**Connections:**

| Component | GPIO Pin | Description |
|-----------|----------|-------------|
| **LED 1** | GPIO 2 | Built-in LED (PWM) |
| **LED 2** | GPIO 33 | External LED output (PWM) |
| **Encoder CLK** | GPIO 34 | Rotary encoder clock signal |
| **Encoder DT** | GPIO 35 | Rotary encoder data signal |
| **Encoder SW** | GPIO 32 | Encoder push button (active low) |
| **Touch Sensor** | GPIO 15 | Capacitive touch input (active high) |

**PWM Configuration:**
- Frequency: 5 kHz
- Resolution: 8-bit (0-255)
- Timer: LEDC_TIMER_0
- Channels: LEDC_CHANNEL_0 (GPIO 2), LEDC_CHANNEL_1 (GPIO 33)

## Firmware Architecture

### Module Overview

#### 1. **Main Application** (`main.c`)
- Initializes all peripherals (encoder, touch sensor, NVS, PWM)
- Implements main control loop for brightness and mode management
- Handles state synchronization between encoder and PWM
- Manages touch sensor toggling and NVS writes

#### 2. **Encoder Module** (`encoder.c` / `encoder.h`)
- Quadrature decoding with robust Gray code detection
- Position tracking (0-255) with clamping
- Button press detection with debouncing
- Scale factor management (1, 2, 5)
- Thread-safe mutex-protected access

**Functions:**
- `encoder_init()` - Initialize encoder
- `encoder_task_start()` - Start RTOS task
- `encoder_get_position()` - Get current position
- `encoder_get_scale_factor()` - Get active scale (1, 2, or 5)
- `encoder_set_position()` - Sync encoder to saved value
- `encoder_cycle_scale_factor()` - Cycle through scales

#### 3. **Touch Sensor Module** (`touch_sensor.c` / `touch_sensor.h`)
- GPIO input monitoring for capacitive touch circuit
- Touch event detection with debouncing (~50ms)
- Event counting and state tracking
- Separate RTOS task for non-blocking monitoring

**Functions:**
- `touch_sensor_init()` - Initialize sensor
- `touch_sensor_is_touched()` - Get current touch state
- `touch_sensor_get_touch_count()` - Get total touch events

#### 4. **NVS Manager** (`nvs_manager.c` / `nvs_manager.h`)
- Persistent storage of LED state and brightness
- Optimized flash write with 5-second stability window
- Duplicate write prevention
- Automatic NVS partition initialization

**Key Features:**
- Only writes to flash when values change
- 5-second debounce timer ensures stable writes
- Prevents duplicate writes for same values
- Console message on actual flash write

**Functions:**
- `nvs_manager_init()` - Initialize NVS
- `nvs_manager_load_led_state()` - Load from flash
- `nvs_manager_save_led_state()` - Queue write
- `nvs_manager_check_pending_write()` - Check debounce timer

## Usage Guide

### Controls

**Encoder Rotation:**
- Rotate encoder to adjust LED brightness (0-255)
- Speed: 1 unit per detent with current scale factor
  - Scale 1x: 1 unit per step
  - Scale 2x: 2 units per step
  - Scale 5x: 5 units per step

**Encoder Button Press:**
- Press to cycle through scale factors: 1x → 2x → 5x → 1x
- Useful for quick adjustments without too many rotations

**Touch Sensor:**
- Short touch: Toggle LEDs ON/OFF
- When OFF: Turning encoder remembers last brightness
- When ON: Adjust brightness with encoder

### Brightness Control Flow

1. **Startup**: Loads saved state from flash (default: enabled at 155/255)
2. **Adjustment**: Rotate encoder to change brightness
3. **Toggle**: Touch sensor to enable/disable
4. **Persistence**: State automatically saved to flash after 5 seconds of stability
5. **Recovery**: On reset, previous state is restored

## Software Architecture

### RTOS Tasks

| Task | Priority | Function |
|------|----------|----------|
| **app_main** | 1 | Main control loop, LED updates, NVS management |
| **encoder_task** | 5 | Quadrature decoding, button detection (10ms polling) |
| **touch_sensor_task** | 5 | Touch input monitoring (10ms polling) |

### Data Flow

```
┌─────────────┐         ┌──────────────┐
│   Encoder   │         │ Touch Sensor │
│   Task      │         │   Task       │
└──────┬──────┘         └──────┬───────┘
       │                       │
       └───────┬───────────────┘
               │
        ┌──────▼──────┐
        │  app_main   │
        │   Loop      │
        └──────┬──────┘
               │
       ┌───────┴────────┐
       │                │
    ┌──▼──┐        ┌────▼────┐
    │ PWM │        │   NVS    │
    │(LEDs)       │ Manager  │
    └─────┘       └──────────┘
```

### State Management

LED and brightness state remains in memory and is synced to NVS flash with optimized writes:
- Changes trigger a 5-second debounce timer
- Only writes if value actually differs from last saved
- Prevents duplicate writes for same state values
- Reduces flash wear through intelligent batching

## Building and Flashing

### Prerequisites

- ESP-IDF v5.0 or later
- ESP32 WROOM-32 DevKit board

### Build

```bash
cd firmware/pwm_light_mixer
idf.py build
```

### Flash

```bash
idf.py flash
```

### Monitor Serial Output

```bash
idf.py monitor
```

Press `Ctrl+]` to exit monitor.

## Project Structure

```
pwm_light_mixer/
├── CMakeLists.txt           # Root CMake config
├── README.md               # This file
└── main/
    ├── CMakeLists.txt      # Component CMake config
    ├── main.c              # Main application
    ├── encoder.c           # Encoder implementation
    ├── encoder.h           # Encoder API
    ├── touch_sensor.c      # Touch sensor implementation
    ├── touch_sensor.h      # Touch sensor API
    ├── nvs_manager.c       # NVS storage implementation
    └── nvs_manager.h       # NVS storage API
```

## Technical Details

### Encoder Quadrature Decoding

Uses robust Gray code state machine detection:
- Monitors both CLK and DT signal transitions
- Detects valid encoder states (00, 01, 10, 11)
- Differentiates clockwise from counter-clockwise rotation
- Applies scale factor to each detected step

**Polling Rate:** 10ms (100 Hz) - sufficient for fast rotations without CPU overhead

### Touch Sensor Debouncing

- 50ms software debounce filter
- Eliminates noise from capacitive sensing
- Reports state change only after stable detection
- Counts total press events for state cycling

### PWM Specifications

- **Frequency:** 5 kHz (smooth visual appearance, low audible noise)
- **Resolution:** 8-bit (256 levels, 0-255)
- **Mode:** High-speed PWM
- **Duty Range:** 0% (LED off) to 100% (LED fully on)

### NVS Storage

- **Namespace:** "led_ctrl"
- **Keys:**
  - `pwm_en` (uint8): LED enabled state (0 or 1)
  - `pwm_val` (uint32): Brightness value (0-255)
- **Write Strategy:** Optimized with state change detection
- **Debounce Window:** 5 seconds (prevents excessive writes)

## Logging

All messages use ESP-IDF logging with appropriate log levels:

**Log Tags:**
- `MAIN` - Main application messages
- `ENCODER` - Encoder task messages
- `TOUCH_SENSOR` - Touch sensor messages
- `NVS_MANAGER` - Flash storage messages
- `PWM` - PWM initialization

**Console Output Examples:**

```
I (15000) MAIN: Touch detected - LEDs ON at encoder position 155
I (25000) ENCODER: Button pressed! Count: 1 | Scale factor changed to: 2
I (30000) NVS_MANAGER: *** FLASH WRITE: saved LED state - enabled=1, pwm=200 ***
E (35000) MAIN: Encoder Position: 200 | PWM Duty: 200/255 | Scale: 2 | State: ON
```

## Performance Characteristics

- **Encoder Response Time:** <20ms
- **Touch Detection:** <50ms (with debounce)
- **PWM Frequency:** 5 kHz
- **Flash Write Latency:** ~100-200ms (occurs every 5+ seconds)
- **CPU Usage:** Minimal (tasks yield frequently)
- **Memory Usage:** ~30KB dynamic RAM for application

## Troubleshooting

### LED Not Responding to Encoder
- Check GPIO pin connections (CLK: 34, DT: 35)
- Verify encoder task is running (check serial output)
- Ensure encoder position is synced on startup

### Touch Sensor Not Working
- Verify GPIO 15 is connected to capacitive sensor circuit
- Check if sensor output is properly pulled up
- Verify debounce settings if sensor is very noisy

### State Not Persisting
- Check NVS partition is erased if upgrading firmware
- Monitor serial output for NVS write messages
- Verify GPIO 0 not held low during reset (NVS erase condition)

### PWM Flickering
- Ensure stable power supply to ESP32
- Check for loose wire connections
- Verify PWM frequency setting (5kHz recommended)

## Future Enhancements

Potential improvements:
- [ ] Add color mixing with 3-channel RGB control
- [ ] Implement preset brightness levels
- [ ] Add motion sensor for auto-off
- [ ] Web interface for remote control
- [ ] MQTT integration for smart home
- [ ] Multiple scene/profile storage
- [ ] Smooth brightness transitions

## License

This project is provided as-is for educational and personal use.

## Support

For issues or questions:
1. Check the Troubleshooting section
2. Review ESP-IDF documentation
3. Inspect serial monitor output for error messages
4. Verify hardware connections match the pinout table
