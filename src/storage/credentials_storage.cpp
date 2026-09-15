/**
 * @file credentials_storage.cpp
 * @brief Implementation of persistent flash storage for Wi-Fi credentials via Zephyr NVS.
 *
 * Technical Details:
 * - Uses FLASH_AREA_OFFSET(storage_partition) and FLASH_AREA_SIZE(storage_partition)
 *   provided by Zephyr's `<zephyr/storage/flash_map.h>`.
 * - Sector size is determined via flash_get_page_info_by_offs() to match the hardware's
 *   underlying 4KB flash erase sectors on the ESP32 SPI flash.
 * - String storage includes the null terminator byte to simplify string retrieval.
 */

#include "storage/credentials_storage.hpp"

#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <string.h>

// Register logging module for persistent storage
LOG_MODULE_REGISTER(credentials_storage, LOG_LEVEL_INF);

namespace app::storage {

namespace {
/// Partition identifiers for the "storage" partition defined in Devicetree
#define STORAGE_PARTITION_ID PARTITION_ID(storage_partition)
#define STORAGE_PARTITION_OFFSET PARTITION_OFFSET(storage_partition)
#define STORAGE_PARTITION_DEVICE PARTITION_DEVICE(storage_partition)
}  // namespace

Status CredentialsStorage::Initialize() noexcept
{
    if (is_initialized_) {
        return Status::kOk;
    }

    // 1. Retrieve the flash device associated with the storage partition
    const struct device* const flash_dev = STORAGE_PARTITION_DEVICE;
    if (!device_is_ready(flash_dev)) {
        LOG_ERR("Flash device for storage partition is not ready");
        return Status::kNotReady;
    }

    // 2. Query flash page/sector geometry
    struct flash_pages_info info {};
    const int page_err = flash_get_page_info_by_offs(flash_dev, STORAGE_PARTITION_OFFSET, &info);
    if (page_err != 0) {
        LOG_ERR("Failed to get flash page info: %d", page_err);
        return Status::kIoError;
    }

    // 3. Configure the NVS file system parameters
    fs_.flash_device = flash_dev;
    fs_.offset = STORAGE_PARTITION_OFFSET;
    fs_.sector_size = static_cast<uint16_t>(info.size);
    // Use at least 3 sectors for wear leveling and garbage collection
    fs_.sector_count = 3U;

    // 4. Mount the NVS file system
    const int mount_err = nvs_mount(&fs_);
    if (mount_err != 0) {
        LOG_ERR("Failed to mount NVS file system: %d", mount_err);
        return Status::kIoError;
    }

    LOG_INF("NVS storage mounted successfully (sector size: %u bytes, count: %u)",
            fs_.sector_size, fs_.sector_count);
    is_initialized_ = true;
    return Status::kOk;
}

bool CredentialsStorage::HasCredentials() const noexcept
{
    if (!is_initialized_) {
        return false;
    }

    // Check if both keys exist in NVS
    char temp_buf[4] = {};
    const ssize_t ssid_len = nvs_read(&fs_, kKeySsid, temp_buf, sizeof(temp_buf));
    const ssize_t pass_len = nvs_read(&fs_, kKeyPassword, temp_buf, sizeof(temp_buf));

    // Valid if both have non-negative lengths and non-zero bytes
    return (ssid_len > 0) && (pass_len >= 0);
}

Status CredentialsStorage::LoadCredentials(
    char* const ssid_out,
    const size_t ssid_max,
    char* const password_out,
    const size_t password_max) const noexcept
{
    if ((ssid_out == nullptr) || (password_out == nullptr)) {
        LOG_ERR("LoadCredentials: output buffer pointer is null");
        return Status::kInvalidArgument;
    }

    if ((ssid_max < kMaxSsidLength) || (password_max < kMaxPasswordLength)) {
        LOG_ERR("LoadCredentials: buffer size too small");
        return Status::kInvalidArgument;
    }

    if (!is_initialized_) {
        LOG_ERR("LoadCredentials: NVS storage not initialized");
        return Status::kNotReady;
    }

    // Read SSID
    const ssize_t ssid_bytes = nvs_read(&fs_, kKeySsid, ssid_out, ssid_max - 1U);
    if (ssid_bytes <= 0) {
        LOG_WRN("No SSID stored in NVS (err: %d)", static_cast<int>(ssid_bytes));
        return Status::kNotFound;
    }
    ssid_out[ssid_bytes] = '\0';

    // Read Password
    const ssize_t pass_bytes = nvs_read(&fs_, kKeyPassword, password_out, password_max - 1U);
    if (pass_bytes < 0) {
        LOG_WRN("No Password stored in NVS (err: %d)", static_cast<int>(pass_bytes));
        return Status::kNotFound;
    }
    password_out[pass_bytes] = '\0';

    LOG_INF("Successfully loaded Wi-Fi credentials from NVS (SSID: %s)", ssid_out);
    return Status::kOk;
}

Status CredentialsStorage::SaveCredentials(const char* const ssid, const char* const password) noexcept
{
    if ((ssid == nullptr) || (password == nullptr)) {
        LOG_ERR("SaveCredentials: input credential pointer is null");
        return Status::kInvalidArgument;
    }

    const size_t ssid_len = strlen(ssid);
    const size_t pass_len = strlen(password);

    if ((ssid_len == 0U) || (ssid_len >= kMaxSsidLength) || (pass_len >= kMaxPasswordLength)) {
        LOG_ERR("SaveCredentials: invalid credential lengths (SSID len: %u, Pass len: %u)",
                static_cast<unsigned int>(ssid_len), static_cast<unsigned int>(pass_len));
        return Status::kInvalidArgument;
    }

    if (!is_initialized_) {
        LOG_ERR("SaveCredentials: NVS storage not initialized");
        return Status::kNotReady;
    }

    // Write SSID (write exact string length without null terminator in NVS, null is appended on read)
    const ssize_t write_ssid = nvs_write(&fs_, kKeySsid, ssid, ssid_len);
    if (write_ssid < 0) {
        LOG_ERR("Failed to write SSID to NVS: %d", static_cast<int>(write_ssid));
        return Status::kIoError;
    }

    // Write Password
    const ssize_t write_pass = nvs_write(&fs_, kKeyPassword, password, pass_len);
    if (write_pass < 0) {
        LOG_ERR("Failed to write Password to NVS: %d", static_cast<int>(write_pass));
        return Status::kIoError;
    }

    LOG_INF("Wi-Fi credentials saved successfully to NVS (SSID: %s)", ssid);
    return Status::kOk;
}

Status CredentialsStorage::ClearCredentials() noexcept
{
    if (!is_initialized_) {
        return Status::kNotReady;
    }

    // Delete both records
    (void)nvs_delete(&fs_, kKeySsid);
    (void)nvs_delete(&fs_, kKeyPassword);

    LOG_INF("Wi-Fi credentials cleared from NVS");
    return Status::kOk;
}

}  // namespace app::storage

