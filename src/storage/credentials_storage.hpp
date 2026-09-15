/**
 * @file credentials_storage.hpp
 * @brief Thread-safe persistent flash storage for Wi-Fi credentials using Zephyr NVS.
 *
 * Architecture & Safety:
 * - Emulates EEPROM non-volatile storage using Zephyr's Non-Volatile Storage (NVS) subsystem.
 * - Targets the predefined flash partition labeled "storage" (storage_partition: partition@3b0000).
 * - Stores SSID (Key ID 1) and Password / PSK (Key ID 2) with fixed maximum buffer lengths.
 * - Adheres to safety-oriented embedded C++ principles:
 *   - No heap allocations (fixed-size stack and static buffers).
 *   - Clear error codes mapped to app::Status.
 *   - All methods fully documented with Doxygen comments and rationale.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <zephyr/kvss/nvs.h>

#include "common/app_status.hpp"

namespace app::storage {

/**
 * @brief Manages read and write access to Wi-Fi credentials stored in flash via NVS.
 */
class CredentialsStorage final {
public:
    /// Maximum allowable length for Wi-Fi SSID according to IEEE 802.11 standards (32 chars + null terminator).
    static constexpr size_t kMaxSsidLength = 33U;

    /// Maximum allowable length for WPA2/WPA3 Pre-Shared Key (63 ASCII chars or 64 hex + null terminator).
    static constexpr size_t kMaxPasswordLength = 65U;

    /// NVS Key identifier for Wi-Fi SSID.
    static constexpr uint16_t kKeySsid = 1U;

    /// NVS Key identifier for Wi-Fi Password.
    static constexpr uint16_t kKeyPassword = 2U;

    CredentialsStorage() noexcept = default;
    ~CredentialsStorage() = default;

    // Disallow copying and moving to prevent multiple controller handles over flash sector
    CredentialsStorage(const CredentialsStorage&) = delete;
    CredentialsStorage& operator=(const CredentialsStorage&) = delete;
    CredentialsStorage(CredentialsStorage&&) = delete;
    CredentialsStorage& operator=(CredentialsStorage&&) = delete;

    /**
     * @brief Mounts and initializes the NVS file system on the flash storage partition.
     *
     * @return Status::kOk on successful mount.
     * @return Status::kNotReady if the flash device is not ready.
     * @return Status::kIoError if NVS mounting or sector geometry detection fails.
     */
    [[nodiscard]] Status Initialize() noexcept;

    /**
     * @brief Checks whether valid SSID and password records exist in NVS storage.
     *
     * @return True if both SSID and password keys exist with non-zero length; false otherwise.
     */
    [[nodiscard]] bool HasCredentials() const noexcept;

    /**
     * @brief Reads the stored Wi-Fi SSID and Password from NVS.
     *
     * @param[out] ssid_out Buffer to receive the null-terminated SSID string.
     * @param[in]  ssid_max Maximum capacity of the `ssid_out` buffer (must be at least kMaxSsidLength).
     * @param[out] password_out Buffer to receive the null-terminated Password string.
     * @param[in]  password_max Maximum capacity of the `password_out` buffer (must be at least kMaxPasswordLength).
     * @return Status::kOk if both credentials were successfully loaded.
     * @return Status::kInvalidArgument if null pointers or insufficient buffer sizes are passed.
     * @return Status::kNotFound if credentials have not been stored yet.
     * @return Status::kIoError if reading flash memory fails.
     */
    [[nodiscard]] Status LoadCredentials(
        char* ssid_out,
        size_t ssid_max,
        char* password_out,
        size_t password_max) const noexcept;

    /**
     * @brief Writes and persists Wi-Fi SSID and Password to internal flash NVS.
     *
     * @param[in] ssid Null-terminated SSID string to persist.
     * @param[in] password Null-terminated Password string to persist.
     * @return Status::kOk if both records were written successfully.
     * @return Status::kInvalidArgument if pointers are null or lengths exceed maximum bounds.
     * @return Status::kIoError if flash write fails.
     */
    [[nodiscard]] Status SaveCredentials(const char* ssid, const char* password) noexcept;

    /**
     * @brief Deletes stored credentials from NVS (useful for factory reset).
     *
     * @return Status::kOk if credentials were removed successfully.
     * @return Status::kIoError if NVS deletion failed.
     */
    [[nodiscard]] Status ClearCredentials() noexcept;

private:
    /// Zephyr NVS file system management structure (mutable because nvs_read requires non-const struct nvs_fs*).
    mutable struct nvs_fs fs_ {};

    /// True if Initialize() has successfully mounted the flash partition.
    bool is_initialized_{false};
};

}  // namespace app::storage

