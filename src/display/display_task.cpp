#include "display/display_task.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(display_task, LOG_LEVEL_INF);

namespace app::display {

namespace {
K_THREAD_STACK_DEFINE(g_display_task_stack, DisplayTask::kStackSizeBytes);
struct k_thread g_display_task_thread {};
}  // namespace

DisplayTask::DisplayTask(IDisplayService& display_service) noexcept
    : display_service_(display_service)
{
}

Status DisplayTask::Start() noexcept
{
    if (is_started_) {
        return Status::kBusy;
    }

    const Status init_status = display_service_.Initialize();
    if (!IsOk(init_status)) {
        LOG_ERR("Failed to initialize display service: %d", static_cast<int>(init_status));
        return init_status;
    }

    const Status bl_status = display_service_.SetBacklight(true);
    if (!IsOk(bl_status)) {
        LOG_WRN("Failed to enable backlight: %d", static_cast<int>(bl_status));
    }

    const k_tid_t tid = k_thread_create(
        &g_display_task_thread,
        g_display_task_stack,
        K_THREAD_STACK_SIZEOF(g_display_task_stack),
        &DisplayTask::ThreadEntry,
        this,
        nullptr,
        nullptr,
        kThreadPriority,
        0U,
        K_NO_WAIT);

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

    Status status = display_service_.Clear();
    if (!IsOk(status)) {
        LOG_WRN("Clear display failed: %d", static_cast<int>(status));
    }

    status = display_service_.SetCursor(0U, 0U);
    if (!IsOk(status)) {
        LOG_WRN("SetCursor row 0 failed: %d", static_cast<int>(status));
    }

    status = display_service_.Print("Hello world");
    if (!IsOk(status)) {
        LOG_WRN("Print row 0 failed: %d", static_cast<int>(status));
    }

    uint32_t counter = 0U;
    char line_buffer[17] = {};

    while (true) {
        const int formatted_len = snprintf(
            line_buffer,
            sizeof(line_buffer),
            "Count: %-9u",
            counter);

        if (formatted_len > 0) {
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
        k_sleep(K_MSEC(kPeriodMs));
    }
}

}  // namespace app::display
