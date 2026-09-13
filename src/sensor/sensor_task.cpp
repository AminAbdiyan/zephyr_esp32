#include "sensor/sensor_task.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// Register logging module for sensor task
LOG_MODULE_REGISTER(sensor_task, LOG_LEVEL_INF);

namespace app::sensor {

namespace {
// Statically allocate stack and thread control block in BSS (zero heap malloc)
K_THREAD_STACK_DEFINE(g_sensor_task_stack, SensorTask::kStackSizeBytes);
struct k_thread g_sensor_task_thread {};
}  // namespace

SensorTask::SensorTask(ISensorService& sensor_service, SensorDataHub& data_hub) noexcept
    : sensor_service_(sensor_service)
    , data_hub_(data_hub)
{
}

Status SensorTask::Start() noexcept
{
    if (is_started_) {
        return Status::kBusy;
    }

    // Initialize sensor hardware
    const Status init_status = sensor_service_.Initialize();
    if (!IsOk(init_status)) {
        LOG_ERR("Failed to initialize sensor service: %d", static_cast<int>(init_status));
        return init_status;
    }

    // Spawn dedicated sensor sampling thread
    const k_tid_t tid = k_thread_create(
        &g_sensor_task_thread,                      // Pointer to thread control block (struct k_thread)
        g_sensor_task_stack,                        // Pointer to thread stack memory buffer
        K_THREAD_STACK_SIZEOF(g_sensor_task_stack), // Stack size in bytes (2048U)
        &SensorTask::ThreadEntry,                   // Static C-compatible entry point trampoline
        this,                                       // User parameter 1 (p1): pointer to this SensorTask instance
        nullptr,                                    // User parameter 2 (p2): unused
        nullptr,                                    // User parameter 3 (p3): unused
        kThreadPriority,                            // Preemptive thread priority (6: high priority sampling)
        0U,                                         // Thread options (0: standard preemptible thread)
        K_NO_WAIT);                                 // Scheduling delay (start thread immediately)

    if (tid == nullptr) {
        LOG_ERR("Failed to create sensor task thread");
        return Status::kInternalError;
    }

    k_thread_name_set(tid, "sensor_task");
    is_started_ = true;
    return Status::kOk;
}

void SensorTask::ThreadEntry(void* const p1, void* const /*p2*/, void* const /*p3*/) noexcept
{
    if (p1 == nullptr) {
        return;
    }
    auto* const self = static_cast<SensorTask*>(p1);
    self->Run();
}

void SensorTask::Run() noexcept
{
    LOG_INF("Sensor sampling task started (1 Hz period)");

    ImuMeasurement measurement{};

    while (true) {
        const Status status = sensor_service_.FetchSample(measurement);

        if (IsOk(status)) {
            // Write sensor values to terminal as info (User Requirement 3)
            LOG_INF("MPU6050 | Accel [m/s^2]: X=%+6.2f Y=%+6.2f Z=%+6.2f | Gyro [rad/s]: X=%+6.2f Y=%+6.2f Z=%+6.2f | Temp: %.1f C",
                    measurement.accel_x,
                    measurement.accel_y,
                    measurement.accel_z,
                    measurement.gyro_x,
                    measurement.gyro_y,
                    measurement.gyro_z,
                    measurement.temperature);

            // Atomically update shared repository for DisplayTask
            data_hub_.Update(measurement);
        } else {
            LOG_WRN("Failed to read MPU6050 sample: %d", static_cast<int>(status));
        }

        // Wait 1 second until next sample
        k_sleep(K_MSEC(kPeriodMs));
    }
}

}  // namespace app::sensor
