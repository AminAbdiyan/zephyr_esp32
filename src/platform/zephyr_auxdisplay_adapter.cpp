#include "platform/zephyr_auxdisplay_adapter.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

// Register logging module for LCD hardware adapter
LOG_MODULE_REGISTER(auxdisplay_adapter, LOG_LEVEL_INF);

namespace app::platform {

namespace {
// Standard 7-bit I2C addresses used by PCF8574 backpack chips:
// - PCF8574T (standard variant): default 0x27 (when A0=A1=A2 are pulled high)
// - PCF8574AT ('A' variant):     default 0x3F (when A0=A1=A2 are pulled high)
constexpr uint8_t kDefaultAddr1 = 0x27U; // PCF8574T
constexpr uint8_t kDefaultAddr2 = 0x3FU; // PCF8574AT

/**
 * PCF8574 I2C Expander Pin Assignment to HD44780 LCD:
 *
 *  Bit 0 (P0) -> RS  (Register Select: 0 = Instruction/Command, 1 = Character Data)
 *  Bit 1 (P1) -> RW  (Read / Write: 0 = Write, 1 = Read; held 0 since we only write)
 *  Bit 2 (P2) -> EN  (Enable strobe: data latches into HD44780 on High->Low falling edge)
 *  Bit 3 (P3) -> BL  (Backlight: 1 = NPN transistor turns LED backlight ON, 0 = OFF)
 *  Bit 4 (P4) -> DB4 (Data bus bit 4)
 *  Bit 5 (P5) -> DB5 (Data bus bit 5)
 *  Bit 6 (P6) -> DB6 (Data bus bit 6)
 *  Bit 7 (P7) -> DB7 (Data bus bit 7)
 */
constexpr uint8_t kBitRs        = 0x01U; // 0b00000001: Register Select
constexpr uint8_t kBitRw        = 0x02U; // 0b00000010: Read/Write (always 0)
constexpr uint8_t kBitEn        = 0x04U; // 0b00000100: Enable clock strobe
constexpr uint8_t kBitBacklight = 0x08U; // 0b00001000: Backlight control bit

/**
 * HD44780 Instruction Set Commands (from Hitachi HD44780U datasheet):
 * - Clear display: clears DDRAM and resets cursor to home address 0x00.
 * - Return home: resets cursor to address 0x00 without clearing DDRAM contents.
 * - Entry mode: specifies cursor move direction (increment/decrement) after each write.
 * - Display control: toggles display ON/OFF, cursor line ON/OFF, and cursor blink ON/OFF.
 * - Function set: sets interface data length (8-bit vs 4-bit), line count (1 vs 2), font (5x8 vs 5x10).
 * - Set DDRAM address: positions the cursor to a specific character memory location.
 */
constexpr uint8_t kCmdClearDisplay = 0x01U; // Clears entire display
constexpr uint8_t kCmdReturnHome   = 0x02U; // Moves cursor to (0, 0)
constexpr uint8_t kCmdEntryMode    = 0x04U; // Base opcode for Entry Mode Set
constexpr uint8_t kCmdDisplayCtrl  = 0x08U; // Base opcode for Display ON/OFF Control
constexpr uint8_t kCmdFunctionSet  = 0x20U; // Base opcode for Function Set
constexpr uint8_t kCmdSetDdramAddr = 0x80U; // Base opcode for Setting DDRAM Cursor Address
}  // namespace

ZephyrAuxdisplayAdapter::ZephyrAuxdisplayAdapter() noexcept
    : i2c_dev_(DEVICE_DT_GET(DT_NODELABEL(i2c0)))
{
}

ZephyrAuxdisplayAdapter::ZephyrAuxdisplayAdapter(const struct device* const i2c_dev) noexcept
    : i2c_dev_(i2c_dev)
{
}

bool ZephyrAuxdisplayAdapter::ProbeAddress(const uint8_t addr) noexcept
{
    // A 0-byte I2C write sends only the device address + write bit.
    // If a slave exists at this address, it pulls SDA low (ACK), returning 0.
    // If no slave responds, the line stays high (NACK), returning an error.
    uint8_t dummy = 0U;
    const int ret = i2c_write(i2c_dev_, &dummy, 0U, addr);
    return (ret == 0);
}

void ZephyrAuxdisplayAdapter::ScanI2cBus() noexcept
{
    LOG_INF("Scanning I2C bus for available devices (0x08 - 0x77)...");
    uint8_t found_count = 0U;

    // Scan standard 7-bit address space excluding reserved ranges
    for (uint8_t addr = 0x08U; addr <= 0x77U; ++addr) {
        if (ProbeAddress(addr)) {
            LOG_INF("  [+] I2C device detected at 0x%02X", addr);
            found_count++;
        }
    }

    if (found_count == 0U) {
        LOG_ERR("  [-] No I2C devices responded! Check wiring (SDA->GPIO21, SCL->GPIO22, 5V power).");
    }
}

Status ZephyrAuxdisplayAdapter::WritePcf8574(const uint8_t value) noexcept
{
    uint8_t data = value;
    // Transmit 1 byte over I2C to the PCF8574 GPIO port
    const int ret = i2c_write(i2c_dev_, &data, 1U, i2c_addr_);
    if (ret != 0) {
        LOG_ERR("I2C write failed! addr: 0x%02X, val: 0x%02X, err: %d", i2c_addr_, value, ret);
        return Status::kIoError;
    }
    return Status::kOk;
}

Status ZephyrAuxdisplayAdapter::PulseEnable(const uint8_t data) noexcept
{
    // Step 1: Set Enable (EN) pin HIGH while holding data bits and backlight stable
    Status status = WritePcf8574(static_cast<uint8_t>(data | kBitEn));
    if (!IsOk(status)) { return status; }

    // HD44780 requires EN pulse width >= 450 nanoseconds (PWEH timing).
    // We wait 5 microseconds to comfortably exceed the minimum across all operating voltages.
    k_busy_wait(5U);

    // Step 2: Pull Enable (EN) pin LOW. The HD44780 samples and latches data on this FALLING edge.
    status = WritePcf8574(static_cast<uint8_t>(data & static_cast<uint8_t>(~kBitEn)));
    if (!IsOk(status)) { return status; }

    // HD44780 standard command execution time is typically 37 microseconds.
    // Waiting 100 microseconds guarantees completion before sending the next nibble.
    k_busy_wait(100U);

    return Status::kOk;
}

Status ZephyrAuxdisplayAdapter::Initialize() noexcept
{
    // Ensure the Zephyr I2C driver device is ready
    if (i2c_dev_ == nullptr || !device_is_ready(i2c_dev_)) {
        LOG_ERR("I2C bus is not ready");
        return Status::kNotReady;
    }

    LOG_INF("Probing LCD I2C backpack on bus '%s'...", i2c_dev_->name);

    // Auto-detect whether PCF8574 (0x27) or PCF8574A (0x3F) is connected
    if (ProbeAddress(kDefaultAddr1)) {
        i2c_addr_ = kDefaultAddr1;
        LOG_INF("Detected LCD at I2C address 0x27 (PCF8574T)");
    } else if (ProbeAddress(kDefaultAddr2)) {
        i2c_addr_ = kDefaultAddr2;
        LOG_INF("Detected LCD at I2C address 0x3F (PCF8574AT)");
    } else {
        LOG_ERR("LCD backpack not found at 0x27 or 0x3F!");
        ScanI2cBus();
        return Status::kIoError;
    }

    LOG_INF("Starting HD44780 4-bit initialization sequence...");

    // Step 0: Ensure all control and data lines start in a known state (backlight enabled)
    Status status = WritePcf8574(backlight_mask_);
    if (!IsOk(status)) { return status; }

    // Datasheet requirement: Wait at least 40 ms after VDD rises to 4.5V.
    // We wait 100 ms to ensure complete power rail stabilization.
    LOG_DBG("Waiting 100ms for power stabilization...");
    k_sleep(K_MSEC(100));

    /**
     * Official HD44780 "Initialization by Instruction" Sequence:
     * When powered on, the LCD controller starts in an unknown state (it could think it's in 8-bit mode).
     * To synchronize reliably without relying on the busy flag:
     * 1. Send 0x03 as an 8-bit command, wait > 4.1 ms.
     * 2. Send 0x03 again, wait > 100 us.
     * 3. Send 0x03 a third time.
     * 4. Send 0x02 to switch controller interface to 4-bit mode.
     */
    LOG_DBG("Init Step 1.1: Sending 0x03");
    status = SendNibble(0x03U, 0U);
    if (!IsOk(status)) { return status; }
    k_busy_wait(5000U); // Wait > 4.1 ms

    LOG_DBG("Init Step 1.2: Sending 0x03");
    status = SendNibble(0x03U, 0U);
    if (!IsOk(status)) { return status; }
    k_busy_wait(200U);  // Wait > 100 us

    LOG_DBG("Init Step 1.3: Sending 0x03");
    status = SendNibble(0x03U, 0U);
    if (!IsOk(status)) { return status; }
    k_busy_wait(200U);

    // Switch to 4-bit mode: send nibble 0x02
    LOG_DBG("Init Step 2: Sending 0x02 (Switch to 4-bit mode)");
    status = SendNibble(0x02U, 0U);
    if (!IsOk(status)) { return status; }
    k_busy_wait(200U);

    // Step 3: Function Set: 4-bit mode, 2 display lines, 5x8 character dots (0x28)
    LOG_DBG("Init Step 3: Function Set (0x28: 2 lines, 5x8 dots)");
    status = SendCommand(kCmdFunctionSet | 0x08U);
    if (!IsOk(status)) { return status; }

    // Step 4: Display OFF (0x08): turn off display before clearing memory
    LOG_DBG("Init Step 4: Display OFF (0x08)");
    status = SendCommand(kCmdDisplayCtrl);
    if (!IsOk(status)) { return status; }

    // Step 5: Clear Display (0x01): wipe all characters and return cursor home
    LOG_DBG("Init Step 5: Clear Display (0x01)");
    status = Clear();
    if (!IsOk(status)) { return status; }

    // Step 6: Entry Mode Set (0x06): increment cursor to the right after write, no auto display scroll
    LOG_DBG("Init Step 6: Entry Mode Set (0x06: auto-increment cursor)");
    status = SendCommand(kCmdEntryMode | 0x02U);
    if (!IsOk(status)) { return status; }

    // Step 7: Display ON (0x0C): display ON, cursor line OFF, blinking OFF
    LOG_DBG("Init Step 7: Display ON (0x0C: screen ON, cursor hidden)");
    status = SendCommand(kCmdDisplayCtrl | 0x04U);
    if (!IsOk(status)) { return status; }

    is_initialized_ = true;
    LOG_INF("HD44780 1602 LCD initialization complete");
    return Status::kOk;
}

Status ZephyrAuxdisplayAdapter::SendNibble(const uint8_t nibble, const uint8_t rs_flag) noexcept
{
    // The LCD data lines D4-D7 are mapped to PCF8574 outputs P4-P7 (upper 4 bits).
    // Shift the 4-bit nibble into bits 4-7, combined with the Register Select (RS) flag
    // and the backlight control bit.
    const uint8_t data = static_cast<uint8_t>(
        ((nibble & 0x0FU) << 4U) | rs_flag | backlight_mask_);

    // Write the pin pattern with Enable LOW
    Status status = WritePcf8574(data);
    if (!IsOk(status)) { return status; }

    // Pulse Enable HIGH then LOW to latch the 4 bits into the LCD
    return PulseEnable(data);
}

Status ZephyrAuxdisplayAdapter::SendByte(const uint8_t val, const uint8_t rs_flag) noexcept
{
    // In 4-bit mode, an 8-bit byte is transmitted as two consecutive 4-bit transfers:
    // 1. High nibble (bits 7..4)
    Status status = SendNibble(static_cast<uint8_t>((val >> 4U) & 0x0FU), rs_flag);
    if (!IsOk(status)) { return status; }

    // 2. Low nibble (bits 3..0)
    return SendNibble(static_cast<uint8_t>(val & 0x0FU), rs_flag);
}

Status ZephyrAuxdisplayAdapter::SendCommand(const uint8_t cmd) noexcept
{
    // RS = 0: Write to Instruction / Command Register
    return SendByte(cmd, 0U);
}

Status ZephyrAuxdisplayAdapter::SendData(const uint8_t data) noexcept
{
    // RS = 1: Write to Data Register (character rendered on display)
    return SendByte(data, kBitRs);
}

Status ZephyrAuxdisplayAdapter::Clear() noexcept
{
    const Status status = SendCommand(kCmdClearDisplay);
    // HD44780 Clear command requires ~1.52 ms to complete inside the LCD controller.
    // Using k_busy_wait instead of k_msleep ensures exact timing independent of the OS tick rate.
    k_busy_wait(2500U);
    return status;
}

Status ZephyrAuxdisplayAdapter::SetCursor(const uint16_t col, const uint16_t row) noexcept
{
    // Bounds check: 16 columns (0-15) and 2 rows (0-1)
    if (col >= 16U || row >= 2U) {
        return Status::kInvalidArgument;
    }

    // HD44780 DDRAM memory mapping:
    // - Row 0 starts at address 0x00
    // - Row 1 starts at address 0x40
    static constexpr uint8_t kRowOffsets[2] = {0x00U, 0x40U};
    return SendCommand(static_cast<uint8_t>(kCmdSetDdramAddr | (col + kRowOffsets[row])));
}

Status ZephyrAuxdisplayAdapter::Print(const char* const text) noexcept
{
    if (text == nullptr) {
        return Status::kInvalidArgument;
    }
    return Print(text, strlen(text));
}

Status ZephyrAuxdisplayAdapter::Print(const char* const text, const size_t len) noexcept
{
    if (!is_initialized_) { return Status::kNotReady; }
    if (text == nullptr) { return Status::kInvalidArgument; }

    // Send each ASCII character byte to LCD Data Register
    for (size_t i = 0U; i < len; ++i) {
        const Status status = SendData(static_cast<uint8_t>(text[i]));
        if (!IsOk(status)) { return status; }
    }

    return Status::kOk;
}

Status ZephyrAuxdisplayAdapter::SetBacklight(const bool enable) noexcept
{
    backlight_mask_ = enable ? kBitBacklight : 0x00U;
    return WritePcf8574(backlight_mask_);
}

}  // namespace app::platform
