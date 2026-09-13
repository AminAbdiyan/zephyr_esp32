# ESP32 Zephyr MPU6050 IMU & HD44780 1602 LCD Application

This application reads 6-DOF motion data (3-axis accelerometer, 3-axis gyroscope, and die temperature) from an **ICQUANZX GY-521 MPU-6050** sensor at 1 Hz and displays the real-time measurements on an **AZDelivery HD44780 1602 LCD** module via I2C using Zephyr RTOS on an **ESP32 WROOM DevKit** (e.g. uPesy ESP32 Wroom).

It follows embedded safety, clean separation of concerns, and MISRA C++ principles outlined in `AGENTS.md`.

---

## Hardware Pinout & Wiring

Both the **GY-521 MPU6050** and the **HD44780 LCD backpack** connect to the **same I2C bus** (`&i2c0`):
- **SDA** -> **GPIO 21**
- **SCL** -> **GPIO 22**
- **VCC** -> **5V** or **3V3** (LCD backpack requires 5V for contrast; GY-521 can accept 3.3V or 5V thanks to its onboard regulator)
- **GND** -> **GND**

> [!IMPORTANT]
> Hold your uPesy board with the **USB port at the BOTTOM** and the **Wi-Fi antenna / metal shield at the TOP**.

```text
                  uPesy ESP32 Wroom DevKit
                     +----------------+
      (Antenna)      |     [WiFi]     |      (Antenna)
             EN  [ ] | 1            1 | [ ]  23
             36  [ ] | 2            2 | [ ]  22  <--- Shared I2C SCL (LCD Pin 2 & MPU6050 SCL)
             39  [ ] | 3            3 | [ ]  TX0
             34  [ ] | 4            4 | [ ]  RX0
             35  [ ] | 5            5 | [ ]  21  <--- Shared I2C SDA (LCD Pin 5 & MPU6050 SDA)
             32  [ ] | 6            6 | [ ]  19
             33  [ ] | 7            7 | [ ]  18
             25  [ ] | 8            8 | [ ]  5
             26  [ ] | 9            9 | [ ]  17
             27  [ ] | 10          10 | [ ]  16
             14  [ ] | 11          11 | [ ]  4
             12  [ ] | 12          12 | [ ]  0
             13  [ ] | 13          13 | [ ]  2
(DO NOT USE) VIN [ ] | 14          14 | [ ]  15
VCC ---> 5V      [ ] | 15          15 | [ ]  3V3 <--- MPU6050 VCC (or 5V)
GND ---> GND     [ ] | 16          16 | [ ]  GND <--- Shared GND
                     +----------------+
                        [USB Port]
```

### Complete Wiring Table

| Device | Device Pin | ESP32 Header | Physical Pin | Silkscreen Label | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **LCD Backpack** | **GND** | Left Header | Pin 16 | `GND` | Ground |
| **LCD Backpack** | **VCC** | Left Header | Pin 15 | `5V` | Must be 5V for readable contrast (do NOT use `VIN`) |
| **LCD Backpack** | **SDA** | Right Header | Pin 5 | `D21` | Shared I2C SDA bus line |
| **LCD Backpack** | **SCL** | Right Header | Pin 2 | `D22` | Shared I2C SCL bus line |
| **GY-521 MPU6050** | **VCC** | Right Header | Pin 15 (or Left Pin 15) | `3V3` or `5V` | Onboard LDO supports both |
| **GY-521 MPU6050** | **GND** | Left / Right | Pin 16 | `GND` | Common ground |
| **GY-521 MPU6050** | **SDA** | Right Header | Pin 5 | `D21` | Shared I2C SDA bus line |
| **GY-521 MPU6050** | **SCL** | Right Header | Pin 2 | `D22` | Shared I2C SCL bus line |
| **GY-521 MPU6050** | **AD0** | — | — | — | Leave unconnected or to GND (Address = `0x68`) |

---

## Architecture & Multi-Task Design

```
                     ┌──────────────────┐
                     │     main.cpp     │  (Composition Root)
                     └────────┬─────────┘
            ┌─────────────────┴─────────────────┐
            ▼                                   ▼
   ┌─────────────────┐                 ┌─────────────────┐
   │   SensorTask    │                 │   DisplayTask   │
   │  (Priority 6)   │                 │  (Priority 7)   │
   │   Period: 1s    │                 │  Period: 500ms  │
   └────────┬────────┘                 └────────▲────────┘
            │                                   │
   1. Polls MPU6050                    3. Reads latest data
   2. Terminal LOG_INF                          │
   3. Updates hub                               │
            │                                   │
            ▼                                   │
   ┌────────────────────────────────────────────┴────────┐
   │            app::sensor::SensorDataHub               │
   │          (Thread-Safe Mutex Repository)             │
   └─────────────────────────────────────────────────────┘
```

- **Devicetree** (`overlay/esp32_devkitc_procpu.overlay`):
  - Configures `&i2c0` on GPIO 21 (SDA) and GPIO 22 (SCL).
  - Instantiates `mpu6050@68` node compatible with `invensense,mpu6050`.
- **Sensor Module** (`src/sensor/`):
  - `sensor_data.hpp`: `ImuMeasurement` data structure and thread-safe `SensorDataHub` protected by `k_mutex`.
  - `sensor_service.hpp`: Pure abstract interface `ISensorService`.
  - `sensor_task.hpp` / `.cpp`: Dedicated RTOS thread running at 1 Hz, sampling MPU6050 and logging readings to console (`LOG_INF`).
- **Display Module** (`src/display/`):
  - `display_service.hpp`: Pure abstract interface `IDisplayService`.
  - `display_task.hpp` / `.cpp`: Dedicated RTOS thread reading from `SensorDataHub` and rendering real-time Accel / Gyro / Temp onto LCD.
- **Platform Adapters** (`src/platform/`):
  - `zephyr_auxdisplay_adapter.hpp` / `.cpp`: HD44780 LCD backpack driver with auto-probing (0x27/0x3F) and I2C bus scanner.
  - `zephyr_mpu6050_adapter.hpp` / `.cpp`: Integrates upstream Zephyr `sensor.h` driver for MPU-6050.
- **Application Startup** (`src/main.cpp`):
  - Ultra-lean dependency injection wiring.

---

## Building and Flashing

```powershell
# Build project with Ninja
$env:PATH = "C:\Users\amin_\.zinstaller\.venv\Scripts;" + $env:PATH
ninja -C build/primary

# Or build with West
west build -b esp32_devkitc/esp32/procpu -p auto

# Flash to board
west flash --esp-device COM5 --esp-baud-rate 921600
```

---

## Important changes

- 2026-09-13: Integrated **ICQUANZX GY-521 MPU6050** 6-axis IMU sensor. Implemented multi-task architecture with dedicated `SensorTask` (1 Hz polling + terminal logging) and `DisplayTask` (real-time LCD rendering via mutex-protected `SensorDataHub`), completely replacing the previous static welcome text and counter.
- 2026-09-13: Reorganized folder structure to co-locate `.hpp` and `.cpp` in `src/`, moved board overlay to `overlay/esp32_devkitc_procpu.overlay`, and thoroughly commented the entire codebase.
- 2026-09-12: Added HD44780 1602 LCD support via PCF8574 I2C backpack. Enabled C++17 support.
