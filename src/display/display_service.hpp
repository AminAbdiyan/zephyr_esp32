#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/app_status.hpp"

namespace app::display {

/**
 * @brief Public interface for alphanumeric character display services.
 */
class IDisplayService {
public:
    virtual ~IDisplayService() = default;

    /**
     * @brief Initialize and configure the display hardware.
     * @return Status::kOk on success, error code otherwise.
     */
    [[nodiscard]] virtual Status Initialize() noexcept = 0;

    /**
     * @brief Clear all characters on the screen and return cursor to (0, 0).
     * @return Status::kOk on success, error code otherwise.
     */
    [[nodiscard]] virtual Status Clear() noexcept = 0;

    /**
     * @brief Position the cursor at the specified column and row.
     * @param col 0-indexed column position.
     * @param row 0-indexed row position.
     * @return Status::kOk on success, Status::kInvalidArgument if out of range.
     */
    [[nodiscard]] virtual Status SetCursor(uint16_t col, uint16_t row) noexcept = 0;

    /**
     * @brief Write null-terminated string starting at the current cursor position.
     * @param text Null-terminated C string to write.
     * @return Status::kOk on success, error code otherwise.
     */
    [[nodiscard]] virtual Status Print(const char* text) noexcept = 0;

    /**
     * @brief Write buffer with explicit length starting at current cursor position.
     * @param text Pointer to characters to write.
     * @param len Number of characters to write.
     * @return Status::kOk on success, error code otherwise.
     */
    [[nodiscard]] virtual Status Print(const char* text, size_t len) noexcept = 0;

    /**
     * @brief Enable or disable display backlight.
     * @param enable True to turn on backlight, false to turn off.
     * @return Status::kOk on success, error code otherwise.
     */
    [[nodiscard]] virtual Status SetBacklight(bool enable) noexcept = 0;
};

}  // namespace app::display

