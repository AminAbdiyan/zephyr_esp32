/**
 * @file wifi_orchestrator.hpp
 * @brief Orchestrator service coordinating Wi-Fi provisioning, persistent credentials, and UI feedback.
 *
 * Architecture & Safety:
 * - Decouples networking workflows, retry policies, and terminal prompting from the Composition Root (`main.cpp`).
 * - Adheres strictly to the Single Responsibility Principle (SRP).
 * - Manages the multi-stage Wi-Fi lifecycle:
 *   1. Check persistent NVS flash storage for existing credentials.
 *   2. Prompt user via serial terminal if credentials are missing or connection repeatedly fails.
 *   3. Coordinate connection attempts via `WifiManager` with automatic retries.
 *   4. Mirror progress and connection milestones onto the HD44780 1602 LCD (`DisplayTask`).
 *   5. Retrieve and format assigned IPv4 address for the system.
 * - Statically allocated in BSS (zero dynamic heap allocation, compliant with MISRA C++).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "common/app_status.hpp"
#include "display/display_task.hpp"
#include "storage/credentials_storage.hpp"
#include "terminal/terminal_input.hpp"
#include "wifi/wifi_manager.hpp"

namespace app::wifi {

/**
 * @brief Coordinates Wi-Fi setup, interactive credential entry, retries, and UI updates.
 */
class WifiOrchestrator final {
public:
    /// Maximum number of connection retry attempts before discarding credentials and re-prompting.
    static constexpr uint32_t kMaxRetries = 3U;

    /// Delay in seconds between failed connection attempts.
    static constexpr uint32_t kRetryDelaySec = 3U;

    /**
     * @brief Constructs the Wi-Fi orchestrator with injected service dependencies.
     *
     * @param[in] storage Reference to persistent NVS credentials storage.
     * @param[in] wifi_mgr Reference to low-level Wi-Fi station manager.
     * @param[in] terminal Reference to interactive serial terminal reader.
     * @param[in] display Reference to LCD display task for user-facing progress updates.
     */
    WifiOrchestrator(
        storage::CredentialsStorage& storage,
        WifiManager& wifi_mgr,
        terminal::TerminalInput& terminal,
        display::DisplayTask& display) noexcept;

    ~WifiOrchestrator() = default;

    // Disallow copy and move operations to protect injected hardware references (MISRA C++ Rule)
    WifiOrchestrator(const WifiOrchestrator&) = delete;
    WifiOrchestrator& operator=(const WifiOrchestrator&) = delete;
    WifiOrchestrator(WifiOrchestrator&&) = delete;
    WifiOrchestrator& operator=(WifiOrchestrator&&) = delete;

    /**
     * @brief Executes the complete network setup workflow until an IPv4 address is assigned.
     *
     * Workflow:
     * - Loads saved SSID and Password from NVS flash.
     * - Prompts via serial console if credentials are empty.
     * - Attempts Wi-Fi connection up to `kMaxRetries` times.
     * - Dispatches live progress ticks to both LCD and serial console.
     * - Clears credentials from flash and prompts again if all retries fail.
     * - Copies assigned IPv4 string into output buffer upon success.
     *
     * @param[out] ip_str_out Destination buffer receiving the dotted-quad IPv4 string.
     * @param[in]  max_len Capacity of destination buffer (must be at least WifiManager::kMaxIpStringLength).
     * @return Status::kOk if Wi-Fi connected and IPv4 acquired.
     * @return Status::kInvalidArgument if ip_str_out is null or too small.
     */
    [[nodiscard]] Status SetupNetwork(char* ip_str_out, size_t max_len) noexcept;

private:
    /**
     * @brief Callback invoked by WifiManager to update the HD44780 1602 LCD in real time.
     *
     * @param[in] line1 First line of LCD text (16 characters max).
     * @param[in] line2 Second line of LCD text (16 characters max).
     * @param[in] user_data Pointer to the enclosing WifiOrchestrator instance.
     */
    static void ProgressCallbackTrampoline(
        const char* line1,
        const char* line2,
        void* user_data) noexcept;

    /**
     * @brief Prompts user interactively via serial terminal for SSID and Password.
     *
     * @return Status::kOk when non-empty SSID is entered and stored to flash.
     */
    [[nodiscard]] Status PromptCredentials() noexcept;

    /// Injected reference to persistent storage
    storage::CredentialsStorage& storage_;

    /// Injected reference to Wi-Fi station manager
    WifiManager& wifi_mgr_;

    /// Injected reference to serial terminal input
    terminal::TerminalInput& terminal_;

    /// Injected reference to LCD display task
    display::DisplayTask& display_;

    /// Buffer holding active Wi-Fi SSID
    char ssid_buf_[storage::CredentialsStorage::kMaxSsidLength] = {};

    /// Buffer holding active Wi-Fi Password
    char password_buf_[storage::CredentialsStorage::kMaxPasswordLength] = {};
};

}  // namespace app::wifi

