/**
 * @file main.cpp
 * @brief Application composition root and startup entry point.
 *
 * Architecture Role:
 * - Adheres strictly to the "Composition Root" pattern.
 * - Instantiates concrete platform drivers (`ZephyrAuxdisplayAdapter`) and injects
 *   them into application modules (`DisplayTask`).
 * - Contains NO business logic or hardware register manipulation.
 * - Statically allocates all dependencies to avoid dynamic memory allocation (heap).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "common/app_status.hpp"
#include "display/display_task.hpp"
#include "platform/zephyr_auxdisplay_adapter.hpp"

// Register the root application logging module
LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

namespace {
/**
 * Static singletons allocated in BSS.
 * They persist for the entire lifetime of the embedded system without
 * risk of stack overflow or heap fragmentation.
 */

/// Concrete hardware driver managing I2C communication with HD44780 LCD
app::platform::ZephyrAuxdisplayAdapter g_display_adapter{};

/// Background RTOS task that coordinates display updates (Dependency Injection)
app::display::DisplayTask g_display_task{g_display_adapter};
}  // namespace

/**
 * @brief Zephyr RTOS application entry point.
 *
 * Called after kernel initialization. Initializes the display subsystem
 * and spawns the background worker thread.
 *
 * @return 0 on success, or a non-zero error status code on failure.
 */
int main()
{
    LOG_INF("Starting ESP32 HD44780 LCD application...");

    // Initialize hardware and launch the background update thread
    const app::Status status = g_display_task.Start();
    if (!app::IsOk(status)) {
        LOG_ERR("Failed to start display task: %d", static_cast<int>(status));
        return static_cast<int>(status);
    }

    LOG_INF("Application initialization complete, display task running in background");
    
    // The main thread can return here; the background display thread continues running autonomously.
    return 0;
}
