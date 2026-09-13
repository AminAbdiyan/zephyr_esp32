#pragma once

#include <stddef.h>
#include <stdint.h>

#include "common/app_status.hpp"
#include "display/display_service.hpp"

namespace app::display {

/**
 * @brief Autonomous background task responsible for updating the display.
 */
class DisplayTask final {
public:
    static constexpr size_t kStackSizeBytes = 2048U;
    static constexpr int kThreadPriority = 7;
    static constexpr uint32_t kPeriodMs = 1000U;

    explicit DisplayTask(IDisplayService& display_service) noexcept;
    ~DisplayTask() = default;

    DisplayTask(const DisplayTask&) = delete;
    DisplayTask& operator=(const DisplayTask&) = delete;
    DisplayTask(DisplayTask&&) = delete;
    DisplayTask& operator=(DisplayTask&&) = delete;

    /**
     * @brief Initialize display hardware and start the background update task.
     * @return Status::kOk on success, error code otherwise.
     */
    [[nodiscard]] Status Start() noexcept;

private:
    static void ThreadEntry(void* p1, void* p2, void* p3) noexcept;
    void Run() noexcept;

    IDisplayService& display_service_;
    bool is_started_{false};
};

}  // namespace app::display

