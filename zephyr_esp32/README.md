# ESP32 Zephyr HD44780 1602 LCD Application

This application drives an AZDelivery HD44780 1602 LCD module with PCF8574 I2C backpack using Zephyr RTOS on a **uPesy ESP32 WROOM DevKit**.

It follows the embedded safety and architecture guidelines specified in `AGENTS.md`.

---

## Hardware Pinout & Wiring for ESP32 WROOM DevKit

> [!IMPORTANT]
> Hold your uPesy board with the **USB port at the BOTTOM** and the **Wi-Fi antenna / metal shield at the TOP**.

```text
                  uPesy ESP32 Wroom DevKit
                     +----------------+
      (Antenna)      |     [WiFi]     |      (Antenna)
             EN  [ ] | 1            1 | [ ]  23
             36  [ ] | 2            2 | [ ]  22  <--- LCD SCL (Pin 2)
             39  [ ] | 3            3 | [ ]  TX0
             34  [ ] | 4            4 | [ ]  RX0
             35  [ ] | 5            5 | [ ]  21  <--- LCD SDA (Pin 5)
             32  [ ] | 6            6 | [ ]  19
             33  [ ] | 7            7 | [ ]  18
             25  [ ] | 8            8 | [ ]  5
             26  [ ] | 9            9 | [ ]  17
             27  [ ] | 10          10 | [ ]  16
             14  [ ] | 11          11 | [ ]  4
             12  [ ] | 12          12 | [ ]  0
             13  [ ] | 13          13 | [ ]  2
(DO NOT USE) VIN [ ] | 14          14 | [ ]  15
LCD VCC ---> 5V  [ ] | 15          15 | [ ]  3V3
LCD GND ---> GND [ ] | 16          16 | [ ]  GND
                     +----------------+
                        [USB Port]
```

### Exact Wiring Table

| LCD Backpack Pin | uPesy Board Header | Physical Pin Position | Board Silkscreen Label |
| :--- | :--- | :--- | :--- |
| **GND** | **Left Header** | **Pin 16** (bottom-most pin near EN button) | **`GND`** |
| **VCC** | **Left Header** | **Pin 15** (1 pin above GND) | **`5V`** *(Do NOT use `VIN`!)* |
| **SDA** | **Right Header** | **Pin 5** (5th pin down from antenna) | **`D21`** |
| **SCL** | **Right Header** | **Pin 2** (2nd pin down from antenna) | **`D22`** |

### Common Pitfalls on the uPesy Board
1. **`5V` vs `VIN`**: On uPesy boards, `VIN` (Pin 14) is an input for external power supplies. When powered by USB, **`VIN` does NOT supply 5V**. You **must** connect LCD VCC to the pin labeled **`5V`** (Pin 15)!
2. **SDA & SCL are NOT Adjacent**: On the uPesy board, GPIO 22 (SCL) is **Pin 2**, and GPIO 21 (SDA) is **Pin 5**. Between them are TX0 and RX0!
3. **Contrast Potentiometer**: Turn the small blue trimmer potentiometer on the back of the LCD backpack with a screwdriver until characters appear.
4. **Backlight Jumper**: Ensure the black jumper cap is firmly seated on the 2-pin header of the backpack.
5. **Auto-Detecting I2C Address**: The firmware automatically detects whether your module uses **`0x27` (PCF8574T)** or **`0x3F` (PCF8574AT)**.

---

## Architecture

- **Devicetree** (`app.overlay` / `boards/esp32_devkitc_procpu.overlay`):
  - Configures `&i2c0` at 100 kHz on GPIO 21 (SDA) and GPIO 22 (SCL).
- **Public Interfaces** (`include/app/`):
  - `app_status.hpp`: Standardized `app::Status` enum and `IsOk()` predicate.
  - `display/display_service.hpp`: Pure abstract interface `IDisplayService`.
  - `display/display_task.hpp`: Background worker task interface.
- **Platform Adapter** (`src/platform/`):
  - `zephyr_auxdisplay_adapter.hpp` / `.cpp`: Auto-detects I2C addresses `0x27` and `0x3F`, provides I2C bus diagnostic scanning, and controls the HD44780 4-bit protocol.
- **Application Logic** (`src/display/`):
  - `display_task.cpp`: Dedicated Zephyr thread updating the LCD (Row 0: "Hello world", Row 1: "Count: <n>" incrementing every 1 second).
- **Application Startup** (`src/main.cpp`):
  - Ultra-lean dependency wiring with zero business logic.

---

## Building and Flashing

```powershell
# Build project with Ninja
ninja -C build/primary

# Or using West
west build -b esp32_devkitc/esp32/procpu -p auto

# Flash to board (specify COM port if needed)
west flash --esp-baud-rate 921600
```

---

## Important changes

- 2026-09-13: Added dedicated uPesy ESP32 Wroom DevKit pinout diagram and wiring instructions. Added automatic I2C address auto-detection (supporting both `0x27` PCF8574T and `0x3F` PCF8574AT) with automatic I2C bus diagnostic scanner on boot.
- 2026-09-12: Added modular `DisplayTask` background thread incrementing a 1-second counter on line 1 while line 0 displays "Hello world", keeping `main.cpp` lean and modular.
- 2026-09-12: Added HD44780 1602 LCD support via PCF8574 I2C backpack. Enabled C++17 support.
