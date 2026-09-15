/**
 * @file main.cpp
 * @brief Application composition root and startup entry point.
 *
 * Architecture Role:
 * - Adheres strictly to the "Composition Root" pattern.
 * - Instantiates concrete platform drivers (ZephyrAuxdisplayAdapter, ZephyrMpu6050Adapter),
 *   persistent storage (CredentialsStorage), Wi-Fi manager (WifiManager), serial terminal reader,
 *   Wi-Fi orchestrator (WifiOrchestrator), HTTP server (HttpServerTask), the shared thread-safe
 *   SensorDataHub, and application worker tasks (SensorTask, DisplayTask).
 * - Statically allocates all objects in BSS to prevent dynamic memory fragmentation (MISRA C++).
 * - Controls the multi-stage startup sequencing:
 *   1. Hardware & LCD initialization.
 *   2. Persistent NVS flash initialization.
 *   3. Wi-Fi setup via WifiOrchestrator (credential loading, terminal prompt, retries, LCD updates).
 *   4. Embedded HTTP web server launch on port 80 (instantly browsable).
 *   5. 5-second pause so the user can view the assigned IP on the LCD.
 *   6. Periodic SensorTask (1 Hz polling) and DisplayTask (IMU data rendering on LCD) launch.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "common/app_status.hpp"
#include "display/display_task.hpp"
#include "http/http_server_task.hpp"
#include "platform/zephyr_auxdisplay_adapter.hpp"
#include "platform/zephyr_mpu6050_adapter.hpp"
#include "sensor/sensor_data.hpp"
#include "sensor/sensor_task.hpp"
#include "storage/credentials_storage.hpp"
#include "terminal/terminal_input.hpp"
#include "wifi/wifi_manager.hpp"
#include "wifi/wifi_orchestrator.hpp"

// Register the root application logging module
LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

namespace {
/**
 * Statically allocated hardware adapters and tasks.
 * Persist for the entire lifetime of the embedded device in BSS (zero dynamic heap allocation).
 */

/// I2C adapter driving the HD44780 1602 LCD via PCF8574 backpack
app::platform::ZephyrAuxdisplayAdapter g_display_adapter{};

/// I2C adapter communicating with the MPU6050 6-DOF IMU sensor
app::platform::ZephyrMpu6050Adapter g_mpu6050_adapter{};

/// Thread-safe data hub transferring IMU readings from SensorTask to DisplayTask and HttpServerTask
app::sensor::SensorDataHub g_sensor_data_hub{};

/// Periodic task sampling MPU6050 at 1 Hz and logging readings to console
app::sensor::SensorTask g_sensor_task{g_mpu6050_adapter, g_sensor_data_hub};

/// Periodic task updating the LCD with the latest IMU readings
app::display::DisplayTask g_display_task{g_display_adapter, g_sensor_data_hub};

/// Persistent flash storage for Wi-Fi credentials using NVS
app::storage::CredentialsStorage g_credentials_storage{};

/// Serial console input reader for interactive credentials entry
app::terminal::TerminalInput g_terminal_input{};

/// Wi-Fi station connection manager
app::wifi::WifiManager g_wifi_manager{};

/// High-level Wi-Fi setup orchestrator (coordinates storage, terminal input, retries, and display)
app::wifi::WifiOrchestrator g_wifi_orchestrator{
    g_credentials_storage,
    g_wifi_manager,
    g_terminal_input,
    g_display_task};

/// Embedded HTTP server listening on port 80 for web dashboard
app::http::HttpServerTask g_http_server_task{g_sensor_data_hub};

/// Buffer holding the acquired IPv4 address string
char g_ip_str[app::wifi::WifiManager::kMaxIpStringLength] = {};

}  // namespace

/**
 * @brief Zephyr RTOS application entry point.
 *
 * Implements clean, top-level composition and lifecycle sequencing.
 *
 * @return 0 on success, or non-zero status code on failure.
 */
int main()
{
    LOG_INF("==================================================");
    LOG_INF("Starting ESP32 MPU6050 IMU & HD44780 LCD Application");
    LOG_INF("==================================================");

    // 1. Initialize LCD hardware display controller first so boot progress can be displayed
    const app::Status lcd_init_status = g_display_task.InitHardware();
    if (!app::IsOk(lcd_init_status)) {
        LOG_ERR("Failed to initialize LCD hardware: %d", static_cast<int>(lcd_init_status));
        return static_cast<int>(lcd_init_status);
    }

    (void)g_display_task.ShowMessage("System Starting", "Initializing...");

    // 2. Initialize persistent flash storage (NVS)
    const app::Status storage_status = g_credentials_storage.Initialize();
    if (!app::IsOk(storage_status)) {
        LOG_ERR("Failed to initialize NVS flash storage: %d", static_cast<int>(storage_status));
        (void)g_display_task.ShowMessage("Storage Error", "NVS Mount Failed");
        return static_cast<int>(storage_status);
    }

    // 3. Coordinate Wi-Fi provisioning, persistent loading, connection attempts, and LCD feedback
    const app::Status wifi_status = g_wifi_orchestrator.SetupNetwork(g_ip_str, sizeof(g_ip_str));
    if (!app::IsOk(wifi_status)) {
        LOG_ERR("Failed to configure Wi-Fi network: %d", static_cast<int>(wifi_status));
        return static_cast<int>(wifi_status);
    }

    // 4. Start HTTP Server Task immediately so web dashboard is accessible right after connection
    const app::Status http_status = g_http_server_task.Start();
    if (!app::IsOk(http_status)) {
        LOG_ERR("Failed to start HTTP server task: %d", static_cast<int>(http_status));
    }

    // 5. Wait 5 seconds so the user can comfortably view the assigned IP on the LCD
    LOG_INF("Waiting 5 seconds before starting sensor acquisition and display updates...");
    k_sleep(K_SECONDS(5));

    // 6. Start Sensor Task (MPU6050 1 Hz polling)
    const app::Status sensor_status = g_sensor_task.Start();
    if (!app::IsOk(sensor_status)) {
        LOG_ERR("Failed to start sensor task: %d", static_cast<int>(sensor_status));
        return static_cast<int>(sensor_status);
    }

    // 7. Start Display Task (IMU display update loop)
    const app::Status display_status = g_display_task.Start();
    if (!app::IsOk(display_status)) {
        LOG_ERR("Failed to start display task: %d", static_cast<int>(display_status));
        return static_cast<int>(display_status);
    }

    LOG_INF("System initialization complete: All tasks (Sensor, Display, HTTP) active.");

    return 0;
}
