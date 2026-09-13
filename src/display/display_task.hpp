#pragma once

#include <stddef.h>
#include <stdint.h>

#include "common/app_status.hpp"
#include "display/display_service.hpp"
#include "sensor/sensor_data.hpp"

namespace app::display {

/**
 * @brief Autonomous background RTOS task responsible for periodic display updates.
 *
 * Architecture & Safety:
 * - Reads real-time IMU data (acceleration, gyroscope, temperature) from the shared SensorDataHub.
 * - Formats and renders measurements onto the HD44780 1602 LCD without blocking sensor acquisition.
 * - Owns its dedicated Zephyr thread and stack (`k_thread`).
 * - Interacts with hardware exclusively through the `IDisplayService` interface (Dependency Inversion).
 * - Copy and move semantics are explicitly deleted to prevent duplicate thread
 *   instances or accidental slicing of hardware-bound tasks (MISRA C++ rule).
 */
class DisplayTask final {
public:
    /// Thread stack size in bytes allocated statically in internal SRAM.
    static constexpr size_t kStackSizeBytes = 2048U;

    /// Zephyr preemptible thread priority (lower number = higher priority).
    /// Priority 7 is suitable for background UI/display updates.
    static constexpr int kThreadPriority = 7;

    /// Periodic refresh cycle duration in milliseconds (500ms for smooth, responsive LCD updates).
    static constexpr uint32_t kPeriodMs = 500U;

    /**
     * @brief Constructs the display task with an injected display service and sensor data hub.
     * @param display_service Reference to an initialized or probeable display driver.
     * @param data_hub Reference to thread-safe sensor repository (shared with SensorTask).
     */
    DisplayTask(IDisplayService& display_service, const sensor::SensorDataHub& data_hub) noexcept;
    ~DisplayTask() = default;

    // Prevent copying and moving (MISRA C++ rule for task controller objects)
    DisplayTask(const DisplayTask&) = delete;
    DisplayTask& operator=(const DisplayTask&) = delete;
    DisplayTask(DisplayTask&&) = delete;
    DisplayTask& operator=(DisplayTask&&) = delete;

    /**
     * @brief Initializes the display hardware and launches the periodic background thread.
     *
     * Calling `Start()` multiple times safely returns `Status::kBusy`.
     *
     * @return Status::kOk if hardware initialized and thread spawned successfully.
     * @return Status::kBusy if the task has already been started.
     * @return Status::kNotReady or Status::kIoError if display hardware initialization fails.
     */
    [[nodiscard]] Status Start() noexcept;

private:
    /**
     * @brief Static C-style function required by Zephyr's `k_thread_create`.
     *
     * Acts as a trampoline, casting `p1` back to `DisplayTask*` to call the private `Run()` loop.
     *
     * @param p1 Pointer to the `DisplayTask` instance (`this`).
     * @param p2 Unused parameter required by Zephyr signature.
     * @param p3 Unused parameter required by Zephyr signature.
     */
    static void ThreadEntry(void* p1, void* p2, void* p3) noexcept;

    /**
     * @brief Main periodic worker loop executed within the dedicated thread.
     *
     * Fetches latest IMU measurements from `SensorDataHub` and refreshes the LCD screen every `kPeriodMs`.
     */
    void Run() noexcept;

    IDisplayService& display_service_;     ///< Injected display driver instance.
    const sensor::SensorDataHub& data_hub_; ///< Injected thread-safe sensor data repository.
    bool is_started_{false};               ///< Guards against multiple thread starts.
};

}  // namespace app::display
