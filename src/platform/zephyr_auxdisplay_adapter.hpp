#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#include <stddef.h>
#include <stdint.h>

#include "common/app_status.hpp"
#include "display/display_service.hpp"

namespace app::platform {

/**
 * @brief Zephyr hardware adapter driving an HD44780 LCD via a PCF8574 I2C backpack.
 *
 * Hardware Overview:
 * - The PCF8574 is an 8-bit I2C I/O expander soldered onto standard LCD backpacks.
 * - Pin assignments from PCF8574 port to LCD:
 *     Bit 0 (P0) -> RS (Register Select: 0 = Instruction, 1 = Data)
 *     Bit 1 (P1) -> RW (Read/Write: tied low for write-only)
 *     Bit 2 (P2) -> EN (Enable clock strobe: latch on falling edge)
 *     Bit 3 (P3) -> Backlight transistor control (1 = ON, 0 = OFF)
 *     Bit 4 (P4) -> D4 (LCD Data bit 4)
 *     Bit 5 (P5) -> D5 (LCD Data bit 5)
 *     Bit 6 (P6) -> D6 (LCD Data bit 6)
 *     Bit 7 (P7) -> D7 (LCD Data bit 7)
 * - Commands and data are sent in 4-bit mode (upper 4 bits first, then lower 4 bits).
 * - Auto-detects standard PCF8574 (0x27) and PCF8574A (0x3F) I2C addresses on boot.
 */
class ZephyrAuxdisplayAdapter final : public display::IDisplayService {
public:
    /**
     * @brief Constructs adapter using the default Devicetree I2C device (`DEVICE_DT_GET(DT_NODELABEL(i2c0))`).
     */
    ZephyrAuxdisplayAdapter() noexcept;

    /**
     * @brief Constructs adapter with a custom Zephyr I2C device pointer (useful for dependency injection).
     * @param i2c_dev Pointer to Zephyr I2C device struct.
     */
    explicit ZephyrAuxdisplayAdapter(const struct device* i2c_dev) noexcept;
    ~ZephyrAuxdisplayAdapter() override = default;

    // Disallow copying and moving to prevent hardware resource aliasing
    ZephyrAuxdisplayAdapter(const ZephyrAuxdisplayAdapter&) = delete;
    ZephyrAuxdisplayAdapter& operator=(const ZephyrAuxdisplayAdapter&) = delete;
    ZephyrAuxdisplayAdapter(ZephyrAuxdisplayAdapter&&) = delete;
    ZephyrAuxdisplayAdapter& operator=(ZephyrAuxdisplayAdapter&&) = delete;

    /// @name IDisplayService Implementation
    /// @{
    [[nodiscard]] Status Initialize() noexcept override;
    [[nodiscard]] Status Clear() noexcept override;
    [[nodiscard]] Status SetCursor(uint16_t col, uint16_t row) noexcept override;
    [[nodiscard]] Status Print(const char* text) noexcept override;
    [[nodiscard]] Status Print(const char* text, size_t len) noexcept override;
    [[nodiscard]] Status SetBacklight(bool enable) noexcept override;
    /// @}

private:
    /**
     * @brief Sends a raw 8-bit byte over I2C to the PCF8574 expander.
     * @param value Byte containing pin states (D7..D4, Backlight, EN, RW, RS).
     */
    [[nodiscard]] Status WritePcf8574(uint8_t value) noexcept;

    /**
     * @brief Pulses the EN (Enable) pin high then low to latch 4 bits of data into the LCD controller.
     * @param data Byte with current data nibble and control bits.
     */
    [[nodiscard]] Status PulseEnable(uint8_t data) noexcept;

    /**
     * @brief Formats a 4-bit nibble into the upper 4 bits of PCF8574 and pulses EN.
     * @param nibble Lower 4 bits contain the nibble to send.
     * @param rs_flag 0 for instruction register, kBitRs (0x01) for data register.
     */
    [[nodiscard]] Status SendNibble(uint8_t nibble, uint8_t rs_flag) noexcept;

    /**
     * @brief Splits an 8-bit command or data byte into two 4-bit nibbles and sends them.
     * @param val 8-bit payload to send.
     * @param rs_flag 0 for instruction, kBitRs for data.
     */
    [[nodiscard]] Status SendByte(uint8_t val, uint8_t rs_flag) noexcept;

    /**
     * @brief Sends an HD44780 instruction command (RS = 0).
     * @param cmd Command byte.
     */
    [[nodiscard]] Status SendCommand(uint8_t cmd) noexcept;

    /**
     * @brief Sends an ASCII character or custom glyph data byte (RS = 1).
     * @param data Character code to display.
     */
    [[nodiscard]] Status SendData(uint8_t data) noexcept;

    /**
     * @brief Tests if an I2C slave acknowledges on a given 7-bit address.
     * @param addr 7-bit I2C address.
     * @return true if ACK received, false if NACK/timeout.
     */
    [[nodiscard]] bool ProbeAddress(uint8_t addr) noexcept;

    /**
     * @brief Diagnostics helper: scans all 127 I2C addresses and prints detected devices to Zephyr log.
     */
    void ScanI2cBus() noexcept;

    const struct device* const i2c_dev_{nullptr}; ///< Pointer to Zephyr I2C device driver.
    uint8_t i2c_addr_{0x27U};                     ///< Active 7-bit I2C slave address (0x27 or 0x3F).
    uint8_t backlight_mask_{0x08U};               ///< Bit mask for backlight: 0x08 = ON, 0x00 = OFF.
    bool is_initialized_{false};                  ///< Tracks initialization state.
};

}  // namespace app::platform
