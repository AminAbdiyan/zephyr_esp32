#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/app_status.hpp"

namespace app::display {

/**
 * @brief Abstract interface (pure virtual) for character displays (e.g., HD44780 1602 LCD).
 *
 * Design Pattern:
 * - Follows the Dependency Inversion Principle (DIP). Higher-level business logic
 *   (such as `DisplayTask`) depends solely on this interface, completely decoupled
 *   from any Zephyr device drivers or hardware buses (e.g. I2C, SPI, GPIO).
 * - This design allows easy unit-testing using mock implementations on host machines.
 */
class IDisplayService {
public:
    virtual ~IDisplayService() = default;

    /**
     * @brief Probes and initializes the display hardware controller.
     *
     * Should be called once during system startup before invoking any drawing methods.
     *
     * @return Status::kOk on successful initialization.
     * @return Status::kNotReady if the underlying communication bus is unready.
     * @return Status::kIoError if the display fails to respond or acknowledge commands.
     */
    [[nodiscard]] virtual Status Initialize() noexcept = 0;

    /**
     * @brief Clears all characters from the display and moves the cursor to (0, 0).
     *
     * @return Status::kOk on success, Status::kIoError on transmission failure.
     */
    [[nodiscard]] virtual Status Clear() noexcept = 0;

    /**
     * @brief Sets the text cursor position.
     *
     * @param col 0-based column index (e.g., 0 to 15 on a 16x2 display).
     * @param row 0-based row index (e.g., 0 or 1 on a 16x2 display).
     * @return Status::kOk on success.
     * @return Status::kInvalidArgument if column or row exceeds display dimensions.
     * @return Status::kIoError on transmission failure.
     */
    [[nodiscard]] virtual Status SetCursor(uint16_t col, uint16_t row) noexcept = 0;

    /**
     * @brief Writes a null-terminated C string starting at the current cursor position.
     *
     * @param text Pointer to null-terminated ASCII string. Must not be nullptr.
     * @return Status::kOk on success.
     * @return Status::kInvalidArgument if `text` is nullptr.
     * @return Status::kIoError on transmission failure.
     */
    [[nodiscard]] virtual Status Print(const char* text) noexcept = 0;

    /**
     * @brief Writes a buffer of explicit length starting at the current cursor position.
     *
     * @param text Pointer to character buffer.
     * @param len Number of characters to write from buffer.
     * @return Status::kOk on success.
     * @return Status::kInvalidArgument if `text` is nullptr and len > 0.
     * @return Status::kIoError on transmission failure.
     */
    [[nodiscard]] virtual Status Print(const char* text, size_t len) noexcept = 0;

    /**
     * @brief Enables or disables the LCD backlight.
     *
     * @param enable True to turn backlight on, false to turn backlight off.
     * @return Status::kOk on success, Status::kIoError on communication failure.
     */
    [[nodiscard]] virtual Status SetBacklight(bool enable) noexcept = 0;
};

}  // namespace app::display
