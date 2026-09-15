/**
 * @file wifi_manager.hpp
 * @brief Thread-safe Wi-Fi Station manager for Zephyr RTOS on ESP32.
 *
 * Architecture & Safety:
 * - Interacts with Zephyr's Wi-Fi L2 management subsystem (`net_mgmt`, `NET_REQUEST_WIFI_CONNECT`).
 * - Listens for asynchronous network events:
 *   - `NET_EVENT_WIFI_CONNECT_RESULT`: Confirms L2 802.11 association.
 *   - `NET_EVENT_IPV4_DHCP_BOUND`: Confirms IPv4 address assignment by DHCP server.
 * - Synchronizes with caller via a Zephyr semaphore (`k_sem`).
 * - Provides non-blocking inspection of the acquired IPv4 address as a string.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include "common/app_status.hpp"

namespace app::wifi {

/**
 * @brief Wi-Fi connection manager handling station association and DHCP lease acquisition.
 */
class WifiManager final {
public:
    /// Maximum buffer length for standard dotted-quad IPv4 string ("255.255.255.255" + null = 16 bytes).
    static constexpr size_t kMaxIpStringLength = 16U;

    /// Callback function type for reporting multi-line connection status (e.g. to LCD or console).
    using ProgressCallback = void (*)(const char* line1, const char* line2, void* user_data);

    /// Connection timeout in milliseconds (increased to 35 seconds to allow for weak RSSI or distant APs).
    static constexpr uint32_t kConnectTimeoutMs = 35000U;

    WifiManager() noexcept;
    ~WifiManager();

    // Prevent copying and moving
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;
    WifiManager(WifiManager&&) = delete;
    WifiManager& operator=(WifiManager&&) = delete;

    /**
     * @brief Initializes the Wi-Fi network interface and registers network management event callbacks.
     *
     * @return Status::kOk on success.
     * @return Status::kNotReady if no suitable Wi-Fi network interface is found in Zephyr.
     */
    [[nodiscard]] Status Initialize() noexcept;

    /**
     * @brief Initiates Wi-Fi connection with the provided SSID and Password, and blocks until DHCP bounds or timeout.
     *
     * @param[in] ssid Null-terminated SSID string.
     * @param[in] password Null-terminated WPA2/WPA3 password string (can be empty for open networks).
     * @param[in] progress_cb Optional callback invoked when connection state changes or ticks elapse.
     * @param[in] user_data Opaque pointer forwarded directly to progress_cb.
     * @return Status::kOk if connected and IPv4 address acquired via DHCP.
     * @return Status::kInvalidArgument if ssid is null or empty.
     * @return Status::kTimeout if connection or DHCP failed within timeout window.
     * @return Status::kIoError on network management request failure.
     */
    [[nodiscard]] Status Connect(
        const char* ssid,
        const char* password,
        ProgressCallback progress_cb = nullptr,
        void* user_data = nullptr) noexcept;

    /**
     * @brief Checks if Wi-Fi is connected and an IPv4 address is actively assigned.
     *
     * @return True if connected with valid IP; false otherwise.
     */
    [[nodiscard]] bool IsConnected() const noexcept;

    /**
     * @brief Retrieves the active assigned IPv4 address as a null-terminated dotted-quad string.
     *
     * @param[out] ip_str Output buffer.
     * @param[in]  max_len Capacity of output buffer (must be at least kMaxIpStringLength).
     * @return Status::kOk if IP was copied successfully.
     * @return Status::kNotReady if Wi-Fi is not connected or has no IP assigned yet.
     * @return Status::kInvalidArgument if buffer is null or too small.
     */
    [[nodiscard]] Status GetIpAddress(char* ip_str, size_t max_len) const noexcept;

    /**
     * @brief Returns a human-readable description of the last Wi-Fi connection failure.
     *
     * @return Pointer to static string explaining the failure reason.
     */
    [[nodiscard]] const char* GetLastFailureReason() const noexcept;

private:
    /**
     * @brief C-style callback invoked by Zephyr net_mgmt subsystem on network events.
     */
    static void NetworkEventHandler(struct net_mgmt_event_callback* cb, uint64_t mgmt_event, struct net_if* iface) noexcept;

    /// Pointer to the active network interface (Zephyr net_if for Wi-Fi STA).
    struct net_if* iface_{nullptr};

    /// Network event callback structure for Wi-Fi L2 events.
    struct net_mgmt_event_callback wifi_cb_ {};

    /// Network event callback structure for IPv4 DHCP events.
    struct net_mgmt_event_callback ipv4_cb_ {};

    /// Semaphore used to block Connect() until DHCP IP is bound or connection fails.
    struct k_sem dhcp_sem_ {};

    /// Assigned IPv4 address stored as string.
    char ip_address_[kMaxIpStringLength] = {};

    /// Optional progress callback invoked during connection steps.
    ProgressCallback progress_cb_{nullptr};

    /// User data passed to progress callback.
    void* user_data_{nullptr};

    /// Last L2 Wi-Fi connection result code.
    int last_conn_status_{0};

    /// True if connected and DHCP bound.
    bool is_connected_{false};

    /// True if Initialize() succeeded.
    bool is_initialized_{false};
};

}  // namespace app::wifi

