/**
 * @file wifi_manager.cpp
 * @brief Implementation of Wi-Fi station management using Zephyr net_mgmt API.
 */

#include "wifi/wifi_manager.hpp"

#include <zephyr/logging/log.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/wifi.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <string.h>

// Register logging module for Wi-Fi management
LOG_MODULE_REGISTER(wifi_manager, LOG_LEVEL_INF);

namespace app::wifi {

namespace {
/// Pointer to singleton instance for callback dispatching
WifiManager* g_wifi_manager_instance = nullptr;
}  // namespace

WifiManager::WifiManager() noexcept
{
    g_wifi_manager_instance = this;
    // Initialize binary semaphore with 0 permits (blocks caller until DHCP acquired)
    k_sem_init(&dhcp_sem_, 0, 1);
}

WifiManager::~WifiManager()
{
    if (is_initialized_) {
        net_mgmt_del_event_callback(&wifi_cb_);
        net_mgmt_del_event_callback(&ipv4_cb_);
    }
    if (g_wifi_manager_instance == this) {
        g_wifi_manager_instance = nullptr;
    }
}

Status WifiManager::Initialize() noexcept
{
    if (is_initialized_) {
        return Status::kOk;
    }

    // 1. Obtain default network interface
    iface_ = net_if_get_default();
    if (iface_ == nullptr) {
        LOG_ERR("No default network interface found for Wi-Fi");
        return Status::kNotReady;
    }

    // 2. Set up Wi-Fi L2 event callbacks (NET_EVENT_WIFI_CONNECT_RESULT)
    net_mgmt_init_event_callback(
        &wifi_cb_,
        &WifiManager::NetworkEventHandler,
        NET_EVENT_WIFI_CONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb_);

    // 3. Set up IPv4 DHCP event callbacks (NET_EVENT_IPV4_DHCP_BOUND)
    net_mgmt_init_event_callback(
        &ipv4_cb_,
        &WifiManager::NetworkEventHandler,
        NET_EVENT_IPV4_DHCP_BOUND);
    net_mgmt_add_event_callback(&ipv4_cb_);

    LOG_INF("Wi-Fi manager initialized successfully on iface: %p", iface_);
    is_initialized_ = true;
    return Status::kOk;
}

Status WifiManager::Connect(
    const char* const ssid,
    const char* const password,
    const ProgressCallback progress_cb,
    void* const user_data) noexcept
{
    if ((ssid == nullptr) || (strlen(ssid) == 0U)) {
        LOG_ERR("Connect: invalid SSID parameter");
        return Status::kInvalidArgument;
    }

    const Status init_status = Initialize();
    if (!IsOk(init_status)) {
        return init_status;
    }

    progress_cb_ = progress_cb;
    user_data_ = user_data;
    last_conn_status_ = 0;

    // Reset semaphore in case of previous attempts
    k_sem_reset(&dhcp_sem_);
    is_connected_ = false;
    ip_address_[0] = '\0';

    // Prepare Zephyr Wi-Fi connection parameters
    struct wifi_connect_req_params cparams {};
    cparams.ssid = reinterpret_cast<const uint8_t*>(ssid);
    cparams.ssid_length = static_cast<uint8_t>(strlen(ssid));
    cparams.channel = WIFI_CHANNEL_ANY;
    cparams.security = (password != nullptr && strlen(password) > 0U)
                           ? WIFI_SECURITY_TYPE_PSK
                           : WIFI_SECURITY_TYPE_NONE;

    if (cparams.security == WIFI_SECURITY_TYPE_PSK) {
        cparams.psk = reinterpret_cast<const uint8_t*>(password);
        cparams.psk_length = static_cast<uint8_t>(strlen(password));
    }

    // Print clear visual banner to serial terminal immediately
    printk("\r\n========================================\r\n");
    printk("[Wi-Fi] Connecting to SSID: '%s'\r\n", ssid);
    printk("[Wi-Fi] Security Mode: %s\r\n", (cparams.security == WIFI_SECURITY_TYPE_PSK) ? "WPA2-PSK" : "Open");
    printk("[Wi-Fi] Timeout: %u seconds\r\n", kConnectTimeoutMs / 1000U);
    printk("========================================\r\n");

    LOG_INF("Connecting to Wi-Fi SSID: '%s' ...", ssid);

    if (progress_cb_ != nullptr) {
        progress_cb_("Connecting WiFi", ssid, user_data_);
    }

    // Request L2 Wi-Fi connection through net_mgmt
    const int req_err = net_mgmt(
        NET_REQUEST_WIFI_CONNECT,
        iface_,
        &cparams,
        sizeof(struct wifi_connect_req_params));

    if (req_err != 0) {
        LOG_ERR("net_mgmt(NET_REQUEST_WIFI_CONNECT) failed: %d", req_err);
        printk("[Wi-Fi] Error: net_mgmt request failed (%d)\r\n", req_err);
        if (progress_cb_ != nullptr) {
            progress_cb_("WiFi Error", "net_mgmt failed", user_data_);
        }
        return Status::kIoError;
    }

    // Loop in 1-second ticks so we can print live progress and update LCD
    const uint32_t start_time = k_uptime_get_32();
    const uint32_t max_sec = kConnectTimeoutMs / 1000U;

    while (true) {
        // Wait up to 1000 ms for event signal (semaphore give from NetworkEventHandler)
        const int sem_res = k_sem_take(&dhcp_sem_, K_MSEC(1000));
        if (sem_res == 0) {
            // Signal received! Either L2 failure or DHCP acquisition complete
            break;
        }

        const uint32_t elapsed_s = (k_uptime_get_32() - start_time) / 1000U;
        if (elapsed_s >= max_sec) {
            printk("\r\n[Wi-Fi] Connection timed out after %u seconds!\r\n", elapsed_s);
            printk("[Wi-Fi] Hint: The router may be out of range, or SSID/password is incorrect.\r\n");
            LOG_ERR("Wi-Fi connection / DHCP lease timed out");
            if (progress_cb_ != nullptr) {
                progress_cb_("WiFi Timed Out", "Check Router/Dist", user_data_);
            }
            return Status::kTimeout;
        }

        // Output progress tick to serial terminal
        printk(".");
        if ((elapsed_s % 5U) == 0U) {
            printk(" [%us/%us] ", elapsed_s, max_sec);
        }

        // Update LCD progress line with elapsed seconds
        if (progress_cb_ != nullptr) {
            char prog_buf[17] = {};
            (void)snprintf(prog_buf, sizeof(prog_buf), "Connecting (%us)", elapsed_s);
            progress_cb_("Connecting WiFi", prog_buf, user_data_);
        }
    }

    if (!is_connected_) {
        const char* const err_reason = GetLastFailureReason();
        printk("\r\n[Wi-Fi] Association failed: %s\r\n", err_reason);
        LOG_ERR("Wi-Fi association failed: %s", err_reason);
        if (progress_cb_ != nullptr) {
            progress_cb_("WiFi Conn Failed", err_reason, user_data_);
        }
        return Status::kIoError;
    }

    printk("\r\n[Wi-Fi] Connection successful! Assigned IP: %s\r\n", ip_address_);
    LOG_INF("Wi-Fi connection successful! IP: %s", ip_address_);
    return Status::kOk;
}

bool WifiManager::IsConnected() const noexcept
{
    return is_connected_;
}

Status WifiManager::GetIpAddress(char* const ip_str, const size_t max_len) const noexcept
{
    if ((ip_str == nullptr) || (max_len < kMaxIpStringLength)) {
        return Status::kInvalidArgument;
    }

    if (!is_connected_ || (ip_address_[0] == '\0')) {
        return Status::kNotReady;
    }

    strncpy(ip_str, ip_address_, max_len - 1U);
    ip_str[max_len - 1U] = '\0';
    return Status::kOk;
}

const char* WifiManager::GetLastFailureReason() const noexcept
{
    if (last_conn_status_ == 0) {
        return "Unknown or Timeout";
    }
    const char* const txt = wifi_conn_status_txt(static_cast<enum wifi_conn_status>(last_conn_status_));
    if (txt != nullptr) {
        return txt;
    }
    return "Connection failed";
}

void WifiManager::NetworkEventHandler(
    struct net_mgmt_event_callback* const cb,
    const uint64_t mgmt_event,
    struct net_if* const iface) noexcept
{
    (void)cb;
    if (g_wifi_manager_instance == nullptr) {
        return;
    }

    // Handle Wi-Fi Connection Result Event
    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        const struct wifi_status* const status =
            static_cast<const struct wifi_status*>(cb->info);
        if ((status != nullptr) && (status->status == 0)) {
            g_wifi_manager_instance->last_conn_status_ = 0;
            printk("\r\n[Wi-Fi] L2 station connected! Requesting DHCP IPv4 lease...\r\n");
            LOG_INF("Wi-Fi L2 station connected, waiting for DHCP IPv4 lease...");
            if (g_wifi_manager_instance->progress_cb_ != nullptr) {
                g_wifi_manager_instance->progress_cb_(
                    "WiFi Associated",
                    "Waiting DHCP...",
                    g_wifi_manager_instance->user_data_);
            }
        } else {
            const int err_code = (status != nullptr) ? status->status : -1;
            g_wifi_manager_instance->last_conn_status_ = err_code;
            const char* const reason_txt = wifi_conn_status_txt(static_cast<enum wifi_conn_status>(err_code));

            printk("\r\n[Wi-Fi] L2 connection failed with code %d (%s)\r\n",
                   err_code, (reason_txt != nullptr) ? reason_txt : "Unknown");
            LOG_ERR("Wi-Fi L2 connection failed with status code: %d (%s)",
                    err_code, (reason_txt != nullptr) ? reason_txt : "Unknown");

            g_wifi_manager_instance->is_connected_ = false;
            // Wake up waiter on failure so it doesn't hang until full timeout
            k_sem_give(&g_wifi_manager_instance->dhcp_sem_);
        }
        return;
    }

    // Handle IPv4 DHCP Bound Event
    if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
        char buf[NET_IPV4_ADDR_LEN] = {};
        // Retrieve the bound IPv4 address from interface
        const struct net_if_addr_ipv4* const if_addr = &iface->config.ip.ipv4->unicast[0];
        if ((if_addr != nullptr) && (if_addr->ipv4.is_used)) {
            (void)net_addr_ntop(AF_INET, &if_addr->ipv4.address.in_addr, buf, sizeof(buf));
            strncpy(g_wifi_manager_instance->ip_address_, buf, sizeof(g_wifi_manager_instance->ip_address_) - 1U);
            g_wifi_manager_instance->ip_address_[sizeof(g_wifi_manager_instance->ip_address_) - 1U] = '\0';
            g_wifi_manager_instance->is_connected_ = true;

            printk("\r\n[Wi-Fi] DHCP IPv4 address bound: %s\r\n", g_wifi_manager_instance->ip_address_);
            LOG_INF("DHCP IPv4 address bound: %s", g_wifi_manager_instance->ip_address_);

            // Signal caller that connection & IP acquisition is complete
            k_sem_give(&g_wifi_manager_instance->dhcp_sem_);
        }
    }
}

}  // namespace app::wifi

