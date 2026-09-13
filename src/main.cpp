/**
 * @file main.cpp
 * @brief Application composition root and startup entry point.
 *
 * Architecture Role:
 * - Adheres strictly to the "Composition Root" pattern.
 * - Instantiates concrete platform drivers (ZephyrAuxdisplayAdapter, ZephyrMpu6050Adapter),
 *   the shared thread-safe SensorDataHub, and application worker tasks (SensorTask, DisplayTask).
 * - Statically allocates all objects in BSS to prevent dynamic memory fragmentation.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "common/app_status.hpp"
#include "display/display_task.hpp"
#include "platform/zephyr_auxdisplay_adapter.hpp"
#include "platform/zephyr_mpu6050_adapter.hpp"
#include "sensor/sensor_data.hpp"
#include "sensor/sensor_task.hpp"

// Register the root application logging module
LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

namespace {
/**
 * Statically allocated hardware adapters and tasks.
 * Persist for the entire lifetime of the embedded device.
 */

/// I2C adapter driving the HD44780 1602 LCD via PCF8574 backpack
app::platform::ZephyrAuxdisplayAdapter g_display_adapter{};

/// I2C adapter communicating with the MPU6050 6-DOF IMU sensor
app::platform::ZephyrMpu6050Adapter g_mpu6050_adapter{};

/// Thread-safe data hub transferring IMU readings from SensorTask to DisplayTask
app::sensor::SensorDataHub g_sensor_data_hub{};

/// Periodic task sampling MPU6050 at 1 Hz and logging readings to console
app::sensor::SensorTask g_sensor_task{g_mpu6050_adapter, g_sensor_data_hub};

/// Periodic task updating the LCD with the latest IMU readings
app::display::DisplayTask g_display_task{g_display_adapter, g_sensor_data_hub};
}  // namespace

/**
 * @brief Zephyr RTOS application entry point.
 *
 * Starts the display and sensor tasks.
 *
 * @return 0 on success, or non-zero status code on failure.
 */
int main()
{
    LOG_INF("==================================================");
    LOG_INF("Starting ESP32 MPU6050 IMU & HD44780 LCD Application");
    LOG_INF("==================================================");

    // 1. Start Display Task
    const app::Status display_status = g_display_task.Start();
    if (!app::IsOk(display_status)) {
        LOG_ERR("Failed to start display task: %d", static_cast<int>(display_status));
        return static_cast<int>(display_status);
    }

    // 2. Start Sensor Task (MPU6050 1 Hz polling)
    const app::Status sensor_status = g_sensor_task.Start();
    if (!app::IsOk(sensor_status)) {
        LOG_ERR("Failed to start sensor task: %d", static_cast<int>(sensor_status));
        return static_cast<int>(sensor_status);
    }

    LOG_INF("System initialization complete: SensorTask (1 Hz) and DisplayTask running");

    return 0;
}
