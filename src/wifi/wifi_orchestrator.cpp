/**
 * @file wifi_orchestrator.cpp
 * @brief Implementation of the Wi-Fi orchestration service.
 *
 * Architecture & Safety:
 * - Implements interactive Wi-Fi credential provisioning, flash storage persistence,
 *   connection retry policies, and UI mirror callbacks onto the LCD and serial terminal.
 * - Adheres strictly to the Single Responsibility Principle (SRP) by isolating network
 *   setup workflows away from the Composition Root (main.cpp).
 * - Compliant with MISRA C++ safety guidelines (no dynamic heap allocations, noexcept methods,
 *   static buffer guarantees, defensive pointer checks).
 */

#include "wifi/wifi_orchestrator.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(wifi_orchestrator, LOG_LEVEL_INF);

namespace app::wifi {

WifiOrchestrator::WifiOrchestrator(
    storage::CredentialsStorage& storage,
    WifiManager& wifi_mgr,
    terminal::TerminalInput& terminal,
    display::DisplayTask& display) noexcept
    : storage_(storage),
      wifi_mgr_(wifi_mgr),
      terminal_(terminal),
      display_(display)
{
}

void WifiOrchestrator::ProgressCallbackTrampoline(
    const char* const line1,
    const char* const line2,
    void* const user_data) noexcept
{
    auto* const self = static_cast<WifiOrchestrator*>(user_data);
    if (self != nullptr) {
        (void)self->display_.ShowMessage(line1, line2);
    }
}

Status WifiOrchestrator::PromptCredentials() noexcept
{
    // Initialize Zephyr console subsystem so console_getchar() can read from UART
    const Status term_status = terminal_.Initialize();
    if (!IsOk(term_status)) {
        LOG_ERR("Failed to initialize serial console input subsystem: %d", static_cast<int>(term_status));
    }

    LOG_INF("Prompting for Wi-Fi credentials via serial terminal...");
    (void)display_.ShowMessage("Enter WiFi Creds", "via Serial Term ");

    printk("\r\n========================================\r\n");
    printk("ESP32 Wi-Fi Setup\r\n");
    printk("========================================\r\n");

    (void)terminal_.Prompt("Enter Wi-Fi SSID: ", ssid_buf_, sizeof(ssid_buf_), false);
    (void)terminal_.Prompt("Enter Wi-Fi Password: ", password_buf_, sizeof(password_buf_), true);

    if (strlen(ssid_buf_) == 0U) {
        printk("Error: SSID cannot be empty! Please try again.\r\n");
        return Status::kInvalidArgument;
    }

    // Persist newly entered credentials to flash NVS
    printk("\r\n[NVS] Saving credentials to internal flash memory...\r\n");
    const Status save_status = storage_.SaveCredentials(ssid_buf_, password_buf_);
    if (IsOk(save_status)) {
        printk("[NVS] Credentials saved to flash memory successfully.\r\n");
    } else {
        printk("[NVS] Warning: Failed to save credentials to flash (%d).\r\n", static_cast<int>(save_status));
    }

    return Status::kOk;
}

Status WifiOrchestrator::SetupNetwork(char* const ip_str_out, const size_t max_len) noexcept
{
    if ((ip_str_out == nullptr) || (max_len < WifiManager::kMaxIpStringLength)) {
        return Status::kInvalidArgument;
    }

    // 1. Check if credentials already exist in flash NVS
    const bool has_creds = storage_.HasCredentials();
    if (has_creds) {
        LOG_INF("Found existing Wi-Fi credentials in flash storage");
        const Status load_status = storage_.LoadCredentials(
            ssid_buf_,
            sizeof(ssid_buf_),
            password_buf_,
            sizeof(password_buf_));

        if (!IsOk(load_status)) {
            LOG_WRN("Failed to read credentials despite flag; prompting via serial");
        }
    }

    // 2. Connection loop: if credentials missing or connection fails repeatedly, prompt via serial terminal
    while (!wifi_mgr_.IsConnected()) {
        if (strlen(ssid_buf_) == 0U) {
            const Status prompt_status = PromptCredentials();
            if (!IsOk(prompt_status)) {
                // Empty SSID entered; re-prompt in loop
                continue;
            }
        }

        // 3. Connect to Wi-Fi with retries (up to kMaxRetries attempts to tolerate weak signal or distance)
        bool connected = false;

        for (uint32_t attempt = 1U; attempt <= kMaxRetries; ++attempt) {
            printk("\r\n>>> Wi-Fi Connection Attempt %u of %u <<<\r\n", attempt, kMaxRetries);
            char attempt_buf[17] = {};
            (void)snprintf(attempt_buf, sizeof(attempt_buf), "Attempt %u of %u", attempt, kMaxRetries);
            (void)display_.ShowMessage("Connecting WiFi", attempt_buf);

            const Status wifi_status = wifi_mgr_.Connect(
                ssid_buf_,
                password_buf_,
                &ProgressCallbackTrampoline,
                this);

            if (IsOk(wifi_status)) {
                connected = true;
                break;
            }

            const char* const reason = wifi_mgr_.GetLastFailureReason();
            printk("\r\n[Wi-Fi] Attempt %u failed: %s\r\n", attempt, reason);

            if (attempt < kMaxRetries) {
                printk("[Wi-Fi] Retrying in %u seconds...\r\n", kRetryDelaySec);
                (void)display_.ShowMessage("WiFi Retry...", "Wait 3 sec...");
                k_sleep(K_SECONDS(kRetryDelaySec));
            }
        }

        if (!connected) {
            printk("\r\n========================================\r\n");
            printk("[Wi-Fi] ERROR: Could not connect to '%s' after %u attempts.\r\n", ssid_buf_, kMaxRetries);
            printk("[Wi-Fi] Clearing invalid credentials from flash memory.\r\n");
            printk("[Wi-Fi] Please re-enter your Wi-Fi SSID and Password.\r\n");
            printk("========================================\r\n\r\n");

            (void)display_.ShowMessage("WiFi Failed!", "Re-enter Creds");
            (void)storage_.ClearCredentials();
            ssid_buf_[0] = '\0';
            password_buf_[0] = '\0';

            k_sleep(K_SECONDS(3));
        }
    }

    // 4. Retrieve assigned IPv4 address
    (void)wifi_mgr_.GetIpAddress(ip_str_out, max_len);
    LOG_INF("==================================================");
    LOG_INF("WiFi Connected! Board IP: %s", ip_str_out);
    LOG_INF("Web Dashboard available at: http://%s/", ip_str_out);
    LOG_INF("==================================================");

    // Display IP address on LCD
    char ip_line[17] = {};
    (void)snprintf(ip_line, sizeof(ip_line), "IP:%-13.13s", ip_str_out);
    (void)display_.ShowMessage("WiFi Connected!", ip_line);

    return Status::kOk;
}

}  // namespace app::wifi

