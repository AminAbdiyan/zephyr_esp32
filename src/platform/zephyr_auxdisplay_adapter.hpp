#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#include <stddef.h>
#include <stdint.h>

#include "common/app_status.hpp"
#include "display/display_service.hpp"

namespace app::platform {

/**
 * @brief Platform adapter connecting IDisplayService to HD44780 LCD via PCF8574 I2C backpack.
 *        Supports auto-detection of both 0x27 (PCF8574T) and 0x3F (PCF8574AT) addresses.
 */
class ZephyrAuxdisplayAdapter final : public display::IDisplayService {
public:
    ZephyrAuxdisplayAdapter() noexcept;
    explicit ZephyrAuxdisplayAdapter(const struct device* i2c_dev) noexcept;
    ~ZephyrAuxdisplayAdapter() override = default;

    ZephyrAuxdisplayAdapter(const ZephyrAuxdisplayAdapter&) = delete;
    ZephyrAuxdisplayAdapter& operator=(const ZephyrAuxdisplayAdapter&) = delete;
    ZephyrAuxdisplayAdapter(ZephyrAuxdisplayAdapter&&) = delete;
    ZephyrAuxdisplayAdapter& operator=(ZephyrAuxdisplayAdapter&&) = delete;

    [[nodiscard]] Status Initialize() noexcept override;
    [[nodiscard]] Status Clear() noexcept override;
    [[nodiscard]] Status SetCursor(uint16_t col, uint16_t row) noexcept override;
    [[nodiscard]] Status Print(const char* text) noexcept override;
    [[nodiscard]] Status Print(const char* text, size_t len) noexcept override;
    [[nodiscard]] Status SetBacklight(bool enable) noexcept override;

private:
    [[nodiscard]] Status WritePcf8574(uint8_t value) noexcept;
    [[nodiscard]] Status PulseEnable(uint8_t data) noexcept;
    [[nodiscard]] Status SendNibble(uint8_t nibble, uint8_t rs_flag) noexcept;
    [[nodiscard]] Status SendByte(uint8_t val, uint8_t rs_flag) noexcept;
    [[nodiscard]] Status SendCommand(uint8_t cmd) noexcept;
    [[nodiscard]] Status SendData(uint8_t data) noexcept;
    [[nodiscard]] bool ProbeAddress(uint8_t addr) noexcept;
    void ScanI2cBus() noexcept;

    const struct device* const i2c_dev_{nullptr};
    uint8_t i2c_addr_{0x27U};
    uint8_t backlight_mask_{0x08U};
    bool is_initialized_{false};
};

}  // namespace app::platform
