# ESP32 Zephyr MPU6050 IMU, HD44780 LCD & Wi-Fi Web Dashboard

This application reads 6-DOF motion data (3-axis accelerometer, 3-axis gyroscope, and die temperature) from an **ICQUANZX GY-521 MPU-6050** sensor at 1 Hz, displays real-time measurements on an **AZDelivery HD44780 1602 LCD** via I2C, and serves a live web dashboard over **Wi-Fi** on an **ESP32 WROOM DevKit** (e.g. uPesy ESP32 Wroom).

It adheres strictly to embedded safety, clean separation of concerns, and MISRA C++ principles outlined in `AGENTS.md`.

---

## Key Features

1. **Interactive Wi-Fi Setup & NVS Storage**:
   - **First Boot**: If credentials are not in flash, the LCD prompts `Enter WiFi Creds` / `via Serial Term`. Enter your Wi-Fi SSID and Password interactively in the serial console (115200 baud).
   - **Flash Persistence**: Credentials are saved to internal SPI flash via Zephyr NVS (`storage_partition`).
   - **Subsequent Boots**: Automatically bypasses the serial prompt and connects directly using stored credentials.
2. **Sequenced Boot Feedback**:
   - LCD displays connection states: `Enter WiFi Creds` &rarr; `Connecting WiFi` &rarr; `WiFi Connected!` with assigned IP address.
   - HTTP server starts immediately upon IP acquisition.
   - Waits **5 seconds** so the user can comfortably view the IP address on the LCD before periodic tasks start.
3. **Live Web Dashboard (Port 80)**:
   - Access `http://<board-ip>` in any web browser.
   - Displays real-time acceleration ($m/s^2$), angular velocity ($rad/s$), and temperature ($^\circ\text{C}$) updating every second via AJAX.
4. **Dual Display Architecture**:
   - Mutex-protected `SensorDataHub` safely delivers data concurrently to both the periodic LCD task and the HTTP web server.

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
        ┌───────────────────────────────┼───────────────────────────────┐
        ▼                               ▼                               ▼
 ┌─────────────┐                 ┌─────────────┐                 ┌─────────────┐
 │ SensorTask  │                 │ DisplayTask │                 │  HttpServer │
 │(Priority 6) │                 │(Priority 7) │                 │(Priority 8) │
 │ Period: 1s  │                 │Period: 500ms│                 │   Port 80   │
 └──────┬──────┘                 └──────▲──────┘                 └──────▲──────┘
        │                               │                               │
 1. Polls MPU6050                3. Reads data                   4. Serves Web
 2. Terminal LOG_INF                    │                           Dashboard
 3. Updates hub                         │                               │
        │                               │                               │
        ▼                               │                               │
 ┌──────────────────────────────────────┴───────────────────────────────┴─┐
 │                       app::sensor::SensorDataHub                       │
 │                     (Thread-Safe Mutex Repository)                     │
 └────────────────────────────────────────────────────────────────────────┘
```

- **Storage Module** (`src/storage/`):
  - `credentials_storage.hpp` / `.cpp`: Reads/writes Wi-Fi credentials to flash via Zephyr NVS (`storage_partition`).
- **Terminal Module** (`src/terminal/`):
  - `terminal_input.hpp` / `.cpp`: Serial console reader supporting prompt, backspace, echo, and password masking.
- **Wi-Fi Module** (`src/wifi/`):
  - `wifi_manager.hpp` / `.cpp`: Manages station association and DHCP lease using Zephyr `net_mgmt` callbacks.
- **HTTP Server Module** (`src/http/`):
  - `http_server_task.hpp` / `.cpp`: BSD socket server serving styled HTML dashboard and `/api/data` JSON endpoint.
- **Sensor Module** (`src/sensor/`):
  - `sensor_data.hpp`: `ImuMeasurement` data structure and thread-safe `SensorDataHub` protected by `k_mutex`.
  - `sensor_task.hpp` / `.cpp`: Dedicated RTOS thread running at 1 Hz, sampling MPU6050 and logging readings to console.
- **Display Module** (`src/display/`):
  - `display_service.hpp`: Pure abstract interface `IDisplayService`.
  - `display_task.hpp` / `.cpp`: Autonomous thread rendering IMU data; provides `ShowMessage` for boot status.
- **Platform Adapters** (`src/platform/`):
  - `zephyr_auxdisplay_adapter.hpp` / `.cpp`: HD44780 LCD backpack driver with auto-probing.
  - `zephyr_mpu6050_adapter.hpp` / `.cpp`: Integrates upstream Zephyr MPU6050 driver.

---

## Building and Flashing

```powershell
# Build project with Ninja
$env:PATH = "C:\Users\amin_\.zinstaller\.venv\Scripts;" + $env:PATH
ninja -C build/primary

# Flash to board
west flash --esp-device COM5 --esp-baud-rate 921600
```

---

## Important changes

- 2026-09-14: Added **Wi-Fi Connectivity, Persistent NVS Storage, and HTTP Web Dashboard**:
  - Interactive Wi-Fi SSID and Password prompt via serial console on first boot.
  - Persistent credential storage in internal flash via Zephyr NVS (`storage_partition`).
  - Sequenced LCD and serial feedback (`Enter WiFi Creds`, `Connecting WiFi`, `WiFi Connected!` + IP).
  - Embedded socket HTTP server on port 80 serving real-time telemetry dashboard.
  - 5-second countdown allowing user to view assigned IP address before live sensor loop starts.
- 2026-09-13: Integrated **ICQUANZX GY-521 MPU6050** 6-axis IMU sensor with `SensorTask` and `DisplayTask`.
- 2026-09-13: Reorganized folder structure to co-locate `.hpp` and `.cpp` in `src/`, moved board overlay to `overlay/esp32_devkitc_procpu.overlay`.
- 2026-09-12: Added HD44780 1602 LCD support via PCF8574 I2C backpack. Enabled C++17 support.
