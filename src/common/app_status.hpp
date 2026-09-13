#pragma once

#include <stdint.h>

namespace app {

/**
 * @brief Application-wide return status / error codes.
 *
 * Underlying type is explicitly defined as `int32_t` for:
 * 1. Fixed size (always 4 bytes) across all compilers and CPU architectures.
 * 2. Binary compatibility with Zephyr RTOS C APIs, which return signed 32-bit error numbers (e.g. -EIO).
 * 3. MISRA C++ compliance by avoiding implementation-defined enum sizes.
 */
enum class Status : int32_t {
    kOk = 0,             ///< Operation completed successfully.
    kInvalidArgument,    ///< An invalid argument was passed (e.g., coordinates out of screen bounds).
    kNotReady,           ///< Hardware or subsystem is not ready (e.g., I2C bus not found/ready).
    kTimeout,            ///< Operation timed out waiting for hardware or resource.
    kBusy,               ///< Subsystem or task is already running / busy.
    kIoError,            ///< Communication error with external hardware (e.g., I2C NACK or bus fault).
    kInternalError,      ///< Unrecoverable internal failure.
};

/**
 * @brief Helper function to quickly check if a Status represents success.
 *
 * Marked `[[nodiscard]]` to prevent silently ignoring error checks,
 * and `constexpr` to allow evaluation at compile time.
 *
 * @param status The status code to evaluate.
 * @return true if status is Status::kOk, false otherwise.
 */
[[nodiscard]] constexpr bool IsOk(const Status status) noexcept {
    return status == Status::kOk;
}

}  // namespace app
