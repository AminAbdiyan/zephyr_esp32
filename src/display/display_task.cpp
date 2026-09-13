#include "display/display_task.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

// Register logging module for display task
LOG_MODULE_REGISTER(display_task, LOG_LEVEL_INF);

namespace app::display {

namespace {
// Statically allocate stack and thread control block in BSS (zero heap malloc)
K_THREAD_STACK_DEFINE(g_display_task_stack, DisplayTask::kStackSizeBytes);
struct k_thread g_display_task_thread {};
}  // namespace

DisplayTask::DisplayTask(IDisplayService& display_service, const sensor::SensorDataHub& data_hub) noexcept
    : display_service_(display_service)
    , data_hub_(data_hub)
{
}

Status DisplayTask::Start() noexcept
{
    if (is_started_) {
        return Status::kBusy;
    }

    // 1. Initialize LCD hardware controller
    const Status init_status = display_service_.Initialize();
    if (!IsOk(init_status)) {
        LOG_ERR("Failed to initialize display service: %d", static_cast<int>(init_status));
        return init_status;
    }

    // 2. Enable LCD backlight
    const Status bl_status = display_service_.SetBacklight(true);
    if (!IsOk(bl_status)) {
        LOG_WRN("Failed to enable backlight: %d", static_cast<int>(bl_status));
    }

    // 3. Spawn the background display update thread
    const k_tid_t tid = k_thread_create(
        &g_display_task_thread,                      // Pointer to thread control block (struct k_thread)
        g_display_task_stack,                        // Pointer to allocated stack memory buffer
        K_THREAD_STACK_SIZEOF(g_display_task_stack), // Usable stack size in bytes (2048U)
        &DisplayTask::ThreadEntry,                   // Static C-compatible entry point trampoline function
        this,                                        // User parameter 1 (p1): pointer to this DisplayTask instance
        nullptr,                                     // User parameter 2 (p2): unused
        nullptr,                                     // User parameter 3 (p3): unused
        kThreadPriority,                             // Preemptive thread priority (7: UI background task)
        0U,                                          // Thread options (0: standard preemptible thread)
        K_NO_WAIT);                                  // Scheduling delay (start thread immediately)

    if (tid == nullptr) {
        LOG_ERR("Failed to create display task thread");
        return Status::kInternalError;
    }

    k_thread_name_set(tid, "display_task");
    is_started_ = true;
    return Status::kOk;
}

void DisplayTask::ThreadEntry(void* const p1, void* const /*p2*/, void* const /*p3*/) noexcept
{
    if (p1 == nullptr) {
        return;
    }
    auto* const self = static_cast<DisplayTask*>(p1);
    self->Run();
}

void DisplayTask::Run() noexcept
{
    LOG_INF("Display update task started");

    // Clear any previous text on startup
    (void)display_service_.Clear();

    // Sized for 16 characters + null terminator
    char line0[17] = {};
    char line1[17] = {};
    uint32_t page_tick = 0U;

    while (true) {
        if (!data_hub_.HasValidData()) {
            // Display waiting message while sensor task acquires initial sample
            (void)display_service_.SetCursor(0U, 0U);
            (void)display_service_.Print("MPU-6050 Sensor ");
            (void)display_service_.SetCursor(0U, 1U);
            (void)display_service_.Print("Waiting data... ");
        } else {
            // Retrieve latest atomic snapshot from sensor task
            const sensor::ImuMeasurement data = data_hub_.GetLatest();

            // Alternate view every 6 ticks (3 seconds with 500ms period)
            const uint32_t page = (page_tick / 6U) % 2U;

            if (page == 0U) {
                // Page 0: Linear Acceleration (Row 0) and Angular Velocity (Row 1)
                // Exactly 16 chars: "A:%+4.1f %+4.1f %+4.1f"
                (void)snprintf(
                    line0,
                    sizeof(line0),
                    "A:%+4.1f %+4.1f %+4.1f",
                    data.accel_x,
                    data.accel_y,
                    data.accel_z);

                // Exactly 16 chars: "G:%+4.1f %+4.1f %+4.1f"
                (void)snprintf(
                    line1,
                    sizeof(line1),
                    "G:%+4.1f %+4.1f %+4.1f",
                    data.gyro_x,
                    data.gyro_y,
                    data.gyro_z);
            } else {
                // Page 1: Module Info and Die Temperature (strictly <= 16 characters)
                (void)snprintf(line0, sizeof(line0), "MPU6050 6DOF IMU");
                (void)snprintf(line1, sizeof(line1), "Temp:   %4.1f C  ", data.temperature);
            }

            // Render Row 0 (16 chars overwrite entirely, no flicker)
            (void)display_service_.SetCursor(0U, 0U);
            (void)display_service_.Print(line0);

            // Render Row 1
            (void)display_service_.SetCursor(0U, 1U);
            (void)display_service_.Print(line1);

            page_tick++;
        }

        // Sleep 500 ms between display refreshes
        k_sleep(K_MSEC(kPeriodMs));
    }
}

}  // namespace app::display
