#pragma once

#include <stddef.h>
#include <stdint.h>

#include "common/app_status.hpp"
#include "sensor/sensor_data.hpp"
#include "sensor/sensor_service.hpp"

namespace app::sensor {

/**
 * @brief Dedicated RTOS task that periodically polls the MPU6050 sensor at 1 Hz.
 *
 * Architecture & Safety:
 * 1. Samples linear acceleration, angular velocity, and temperature once every second.
 * 2. Formats and prints real-time sensor measurements to the console / terminal via LOG_INF.
 * 3. Atomically updates the shared SensorDataHub so DisplayTask can show the latest values.
 * 4. Owns its dedicated Zephyr thread and stack (`k_thread`).
 * 5. Interacts with hardware exclusively through the `ISensorService` interface (Dependency Inversion).
 * 6. Copy and move semantics are explicitly deleted to prevent duplicate thread
 *    instances or accidental slicing of hardware-bound tasks (MISRA C++ rule).
 */
class SensorTask final {
public:
    /// Dedicated thread stack size in bytes allocated statically in internal SRAM (BSS).
    static constexpr size_t kStackSizeBytes = 2048U;

    /// Preemptive priority (6: higher than DisplayTask's 7 to prevent LCD I2C stalls from delaying sampling).
    static constexpr int kThreadPriority = 6;

    /// 1 Hz polling period (1000 milliseconds).
    static constexpr uint32_t kPeriodMs = 1000U;

    /**
     * @brief Constructs the sensor task with injected hardware service and shared data hub.
     * @param sensor_service Injected MPU6050 sensor hardware service.
     * @param data_hub Injected shared thread-safe data repository.
     */
    SensorTask(ISensorService& sensor_service, SensorDataHub& data_hub) noexcept;
    ~SensorTask() = default;

    // Prevent copying and moving (MISRA C++ rule for task controller objects)
    SensorTask(const SensorTask&) = delete;
    SensorTask& operator=(const SensorTask&) = delete;
    SensorTask(SensorTask&&) = delete;
    SensorTask& operator=(SensorTask&&) = delete;

    /**
     * @brief Initializes sensor hardware and spawns the periodic sampling thread.
     *
     * Calling `Start()` multiple times safely returns `Status::kBusy`.
     *
     * @return Status::kOk if hardware initialized and thread spawned successfully.
     * @return Status::kBusy if the task has already been started.
     * @return Status::kNotReady or Status::kIoError if sensor initialization fails.
     */
    [[nodiscard]] Status Start() noexcept;

private:
    /**
     * @brief Static C-style function required by Zephyr's `k_thread_create`.
     *
     * Acts as a trampoline, casting `p1` back to `SensorTask*` to call the private `Run()` loop.
     *
     * @param p1 Pointer to the `SensorTask` instance (`this`).
     * @param p2 Unused parameter required by Zephyr signature.
     * @param p3 Unused parameter required by Zephyr signature.
     */
    static void ThreadEntry(void* p1, void* p2, void* p3) noexcept;

    /**
     * @brief Main periodic worker loop executed within the dedicated thread.
     *
     * Samples MPU6050, logs to terminal, and writes to `SensorDataHub` every 1000 ms.
     */
    void Run() noexcept;

    ISensorService& sensor_service_; ///< Injected sensor hardware driver instance.
    SensorDataHub& data_hub_;        ///< Injected thread-safe data repository.
    bool is_started_{false};         ///< Guards against multiple thread starts.
};

}  // namespace app::sensor
