#include "display/display_task.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

// Register logging module for this translation unit
LOG_MODULE_REGISTER(display_task, LOG_LEVEL_INF);

namespace app::display {

namespace {
/**
 * Statically allocate the RTOS thread stack and thread control block in BSS.
 * This guarantees zero dynamic heap allocation (malloc) at runtime,
 * strictly complying with embedded safety principles (MISRA).
 */
K_THREAD_STACK_DEFINE(g_display_task_stack, DisplayTask::kStackSizeBytes);
struct k_thread g_display_task_thread {};
}  // namespace

DisplayTask::DisplayTask(IDisplayService& display_service) noexcept
    : display_service_(display_service)
{
}

Status DisplayTask::Start() noexcept
{
    // Prevent starting the task multiple times
    if (is_started_) {
        return Status::kBusy;
    }

    // 1. Initialize the display hardware first before launching the worker thread
    const Status init_status = display_service_.Initialize();
    if (!IsOk(init_status)) {
        LOG_ERR("Failed to initialize display service: %d", static_cast<int>(init_status));
        return init_status;
    }

    // 2. Turn on the LCD backlight
    const Status bl_status = display_service_.SetBacklight(true);
    if (!IsOk(bl_status)) {
        LOG_WRN("Failed to enable backlight: %d", static_cast<int>(bl_status));
    }

    // 3. Spawn the Zephyr background thread
    // Pass 'this' pointer as the first user parameter (p1) to ThreadEntry
    const k_tid_t tid = k_thread_create(
        &g_display_task_thread,                      // Thread control block
        g_display_task_stack,                        // Stack memory pointer
        K_THREAD_STACK_SIZEOF(g_display_task_stack), // Stack size in bytes
        &DisplayTask::ThreadEntry,                   // Static entry point function
        this,                                        // p1: pointer to this instance
        nullptr,                                     // p2: unused
        nullptr,                                     // p3: unused
        kThreadPriority,                             // Preemptive thread priority (7)
        0U,                                          // Thread options (0 = standard preemptible)
        K_NO_WAIT);                                  // Delay before starting (start immediately)

    if (tid == nullptr) {
        LOG_ERR("Failed to create display task thread");
        return Status::kInternalError;
    }

    // Assign a human-readable name in Zephyr kernel thread monitor / shell
    k_thread_name_set(tid, "display_task");
    is_started_ = true;
    return Status::kOk;
}

void DisplayTask::ThreadEntry(void* const p1, void* const /*p2*/, void* const /*p3*/) noexcept
{
    if (p1 == nullptr) {
        return;
    }

    // Bridge from C-style thread callback to C++ member function
    auto* const self = static_cast<DisplayTask*>(p1);
    self->Run();
}

void DisplayTask::Run() noexcept
{
    LOG_INF("Display update task started");

    // Clear display and reset cursor to home (row 0, col 0)
    Status status = display_service_.Clear();
    if (!IsOk(status)) {
        LOG_WRN("Clear display failed: %d", static_cast<int>(status));
    }

    status = display_service_.SetCursor(0U, 0U);
    if (!IsOk(status)) {
        LOG_WRN("SetCursor row 0 failed: %d", static_cast<int>(status));
    }

    // Write static welcome banner on line 0 (Row 1 on physical LCD)
    status = display_service_.Print("Hello world");
    if (!IsOk(status)) {
        LOG_WRN("Print row 0 failed: %d", static_cast<int>(status));
    }

    uint32_t counter = 0U;
    // Buffer sized for 16 display characters + null terminator:
    // Format: "Count: %-9u" guarantees all 16 columns are written, overwriting
    // any leftover characters without needing a slow screen clear on each tick.
    char line_buffer[17] = {};

    while (true) {
        // Format the second line string
        const int formatted_len = snprintf(
            line_buffer,
            sizeof(line_buffer),
            "Count: %-9u",
            counter);

        if (formatted_len > 0) {
            // Position cursor at beginning of row 1 (the second line)
            status = display_service_.SetCursor(0U, 1U);
            if (IsOk(status)) {
                status = display_service_.Print(line_buffer);
                if (!IsOk(status)) {
                    LOG_WRN("Print row 1 failed: %d", static_cast<int>(status));
                }
            } else {
                LOG_WRN("SetCursor row 1 failed: %d", static_cast<int>(status));
            }
        }

        counter++;

        // Sleep for 1000 ms, releasing CPU to lower priority tasks or CPU idle/sleep
        k_sleep(K_MSEC(kPeriodMs));
    }
}

}  // namespace app::display
