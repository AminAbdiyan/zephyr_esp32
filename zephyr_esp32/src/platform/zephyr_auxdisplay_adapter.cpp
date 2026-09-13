#include "platform/zephyr_auxdisplay_adapter.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(auxdisplay_adapter, LOG_LEVEL_INF);

namespace app::platform {

namespace {
constexpr uint8_t kDefaultAddr1 = 0x27U; // PCF8574T
constexpr uint8_t kDefaultAddr2 = 0x3FU; // PCF8574AT

// PCF8574 I2C backpack bit mapping (active-high)
constexpr uint8_t kBitRs        = 0x01U; // Register Select
constexpr uint8_t kBitRw        = 0x02U; // Read/Write (always 0 for write)
constexpr uint8_t kBitEn        = 0x04U; // Enable strobe
constexpr uint8_t kBitBacklight = 0x08U; // Backlight control
// Bits 4-7 = D4-D7

// HD44780 commands
constexpr uint8_t kCmdClearDisplay = 0x01U;
constexpr uint8_t kCmdReturnHome   = 0x02U;
constexpr uint8_t kCmdEntryMode    = 0x04U;
constexpr uint8_t kCmdDisplayCtrl  = 0x08U;
constexpr uint8_t kCmdFunctionSet  = 0x20U;
constexpr uint8_t kCmdSetDdramAddr = 0x80U;
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
    uint8_t dummy = 0U;
    const int ret = i2c_write(i2c_dev_, &dummy, 0U, addr);
    return (ret == 0);
}

void ZephyrAuxdisplayAdapter::ScanI2cBus() noexcept
{
    LOG_INF("Scanning I2C bus for available devices (0x08 - 0x77)...");
    uint8_t found_count = 0U;

    for (uint8_t addr = 0x08U; addr <= 0x77U; ++addr) {
        if (ProbeAddress(addr)) {
            LOG_INF("  [+] I2C device detected at 0x%02X", addr);
            found_count++;
        }
    }

    if (found_count == 0U) {
        LOG_ERR("  [-] No I2C devices responded! Check wiring.");
    }
}

Status ZephyrAuxdisplayAdapter::WritePcf8574(const uint8_t value) noexcept
{
    uint8_t data = value;
    const int ret = i2c_write(i2c_dev_, &data, 1U, i2c_addr_);
    if (ret != 0) {
        LOG_ERR("I2C write failed! addr: 0x%02X, val: 0x%02X, err: %d", i2c_addr_, value, ret);
        return Status::kIoError;
    }
    return Status::kOk;
}

Status ZephyrAuxdisplayAdapter::PulseEnable(const uint8_t data) noexcept
{
    // EN high — hold data lines stable, raise EN
    Status status = WritePcf8574(static_cast<uint8_t>(data | kBitEn));
    if (!IsOk(status)) { return status; }
    
    // EN pulse width: min 450ns. We use 5us to be extremely safe.
    k_busy_wait(5U);

    // EN low — latch data into HD44780 on falling edge
    status = WritePcf8574(static_cast<uint8_t>(data & static_cast<uint8_t>(~kBitEn)));
    if (!IsOk(status)) { return status; }
    
    // Command execution time: > 37us typical. We use 100us to be safe.
    k_busy_wait(100U);

    return Status::kOk;
}

Status ZephyrAuxdisplayAdapter::Initialize() noexcept
{
    if (i2c_dev_ == nullptr || !device_is_ready(i2c_dev_)) {
        LOG_ERR("I2C bus is not ready");
        return Status::kNotReady;
    }

    LOG_INF("Probing LCD I2C backpack on bus '%s'...", i2c_dev_->name);

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

    LOG_INF("Starting HD44780 initialization sequence...");

    // Step 0: Set PCF8574 outputs to known state (all low + backlight on)
    Status status = WritePcf8574(backlight_mask_);
    if (!IsOk(status)) { return status; }

    // Power-on delay: wait > 40ms. Use 100ms.
    LOG_DBG("Waiting 100ms for power stabilization...");
    k_sleep(K_MSEC(100));

    // Step 1: Function set — send 0x03 (8-bit mode) three times
    LOG_DBG("Init Step 1.1: Sending 0x03");
    status = SendNibble(0x03U, 0U);
    if (!IsOk(status)) { return status; }
    k_busy_wait(5000U); // > 4.1ms wait

    LOG_DBG("Init Step 1.2: Sending 0x03");
    status = SendNibble(0x03U, 0U);
    if (!IsOk(status)) { return status; }
    k_busy_wait(200U); // > 100us wait

    LOG_DBG("Init Step 1.3: Sending 0x03");
    status = SendNibble(0x03U, 0U);
    if (!IsOk(status)) { return status; }
    k_busy_wait(200U);

    // Step 2: Set interface to 4-bit mode (send 0x02 as nibble)
    LOG_DBG("Init Step 2: Sending 0x02 (Switch to 4-bit mode)");
    status = SendNibble(0x02U, 0U);
    if (!IsOk(status)) { return status; }
    k_busy_wait(200U);

    // Step 3: Function set (0x28)
    LOG_DBG("Init Step 3: Function Set (0x28)");
    status = SendCommand(kCmdFunctionSet | 0x08U);
    if (!IsOk(status)) { return status; }

    // Step 4: Display OFF (0x08)
    LOG_DBG("Init Step 4: Display OFF (0x08)");
    status = SendCommand(kCmdDisplayCtrl);
    if (!IsOk(status)) { return status; }

    // Step 5: Clear Display (0x01)
    LOG_DBG("Init Step 5: Clear Display (0x01)");
    status = Clear();
    if (!IsOk(status)) { return status; }

    // Step 6: Entry Mode Set (0x06)
    LOG_DBG("Init Step 6: Entry Mode Set (0x06)");
    status = SendCommand(kCmdEntryMode | 0x02U);
    if (!IsOk(status)) { return status; }

    // Step 7: Display ON (0x0C)
    LOG_DBG("Init Step 7: Display ON (0x0C)");
    status = SendCommand(kCmdDisplayCtrl | 0x04U);
    if (!IsOk(status)) { return status; }

    is_initialized_ = true;
    LOG_INF("HD44780 1602 LCD initialization complete");
    return Status::kOk;
}

Status ZephyrAuxdisplayAdapter::SendNibble(const uint8_t nibble, const uint8_t rs_flag) noexcept
{
    // LOG_DBG("  SendNibble: 0x%X, rs: %u", nibble, rs_flag);
    
    // Place the 4-bit value into bits 4-7 of the PCF8574 output byte
    const uint8_t data = static_cast<uint8_t>(
        ((nibble & 0x0FU) << 4U) | rs_flag | backlight_mask_);

    // Write data lines first (EN low), then pulse EN
    Status status = WritePcf8574(data);
    if (!IsOk(status)) { return status; }

    return PulseEnable(data);
}

Status ZephyrAuxdisplayAdapter::SendByte(const uint8_t val, const uint8_t rs_flag) noexcept
{
    LOG_DBG("SendByte: 0x%02X, rs: %u", val, rs_flag);
    
    // High nibble first
    Status status = SendNibble(static_cast<uint8_t>((val >> 4U) & 0x0FU), rs_flag);
    if (!IsOk(status)) { return status; }
    
    // Low nibble second
    return SendNibble(static_cast<uint8_t>(val & 0x0FU), rs_flag);
}

Status ZephyrAuxdisplayAdapter::SendCommand(const uint8_t cmd) noexcept
{
    return SendByte(cmd, 0U);
}

Status ZephyrAuxdisplayAdapter::SendData(const uint8_t data) noexcept
{
    return SendByte(data, kBitRs);
}

Status ZephyrAuxdisplayAdapter::Clear() noexcept
{
    const Status status = SendCommand(kCmdClearDisplay);
    // Clear command execution time is ~1.52ms. 
    // Using k_busy_wait instead of k_msleep to avoid RTOS tick inaccuracies.
    k_busy_wait(2500U); 
    return status;
}

Status ZephyrAuxdisplayAdapter::SetCursor(const uint16_t col, const uint16_t row) noexcept
{
    if (col >= 16U || row >= 2U) {
        return Status::kInvalidArgument;
    }

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
