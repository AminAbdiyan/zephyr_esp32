#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "common/app_status.hpp"
#include "display/display_task.hpp"
#include "platform/zephyr_auxdisplay_adapter.hpp"

LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

namespace {
app::platform::ZephyrAuxdisplayAdapter g_display_adapter{};
app::display::DisplayTask g_display_task{g_display_adapter};
}  // namespace

int main()
{
    LOG_INF("Starting ESP32 HD44780 LCD application...");

    const app::Status status = g_display_task.Start();
    if (!app::IsOk(status)) {
        LOG_ERR("Failed to start display task: %d", static_cast<int>(status));
        return static_cast<int>(status);
    }

    LOG_INF("Application initialization complete, display task running");
    return 0;
}
