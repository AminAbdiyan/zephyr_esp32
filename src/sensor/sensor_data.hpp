#pragma once

#include <zephyr/kernel.h>
#include <stdint.h>

namespace app::sensor {

/**
 * @brief Plain Data Structure holding 6-DOF IMU measurements and temperature.
 */
struct ImuMeasurement {
    double accel_x{0.0};      ///< Linear acceleration X-axis in m/s^2.
    double accel_y{0.0};      ///< Linear acceleration Y-axis in m/s^2.
    double accel_z{0.0};      ///< Linear acceleration Z-axis in m/s^2 (nominal 1g ~ 9.81 m/s^2 when stationary).
    double gyro_x{0.0};       ///< Angular velocity X-axis in rad/s.
    double gyro_y{0.0};       ///< Angular velocity Y-axis in rad/s.
    double gyro_z{0.0};       ///< Angular velocity Z-axis in rad/s.
    double temperature{0.0};  ///< Die temperature in degrees Celsius.
    uint32_t timestamp_ms{0}; ///< Kernel uptime timestamp in milliseconds when sample was taken.
    bool is_valid{false};     ///< True if measurement was successfully captured from hardware.
};

/**
 * @brief Thread-safe repository for transferring sensor measurements between tasks.
 *
 * Concurrency Design:
 * - Uses a standard Zephyr `struct k_mutex` to ensure mutual exclusion.
 * - Because `mutex_` is declared `mutable`, `k_mutex_lock` and `k_mutex_unlock`
 *   operate directly inside `const` methods without needing `const_cast` or helper classes.
 * - Eliminates data tearing and race conditions across RTOS preemptive threads.
 */
class SensorDataHub final {
public:
    SensorDataHub() noexcept {
        k_mutex_init(&mutex_);
    }

    ~SensorDataHub() = default;

    // Disallow copy/move to prevent aliasing mutex handles (MISRA C++ rule)
    SensorDataHub(const SensorDataHub&) = delete;
    SensorDataHub& operator=(const SensorDataHub&) = delete;
    SensorDataHub(SensorDataHub&&) = delete;
    SensorDataHub& operator=(SensorDataHub&&) = delete;

    /**
     * @brief Updates the latest measurement (invoked by SensorTask).
     * @param measurement Newly sampled IMU values.
     */
    void Update(const ImuMeasurement& measurement) noexcept {
        (void)k_mutex_lock(&mutex_, K_FOREVER);
        latest_data_ = measurement;
        latest_data_.is_valid = true;
        k_mutex_unlock(&mutex_);
    }

    /**
     * @brief Retrieves a copy of the latest measurement (invoked by DisplayTask).
     * @return Snapshot copy of the most recent ImuMeasurement.
     */
    [[nodiscard]] ImuMeasurement GetLatest() const noexcept {
        (void)k_mutex_lock(&mutex_, K_FOREVER);
        const ImuMeasurement copy = latest_data_;
        k_mutex_unlock(&mutex_);
        return copy;
    }

    /**
     * @brief Checks whether at least one valid measurement has been published.
     * @return true if valid data is available, false otherwise.
     */
    [[nodiscard]] bool HasValidData() const noexcept {
        (void)k_mutex_lock(&mutex_, K_FOREVER);
        const bool valid = latest_data_.is_valid;
        k_mutex_unlock(&mutex_);
        return valid;
    }

private:
    mutable struct k_mutex mutex_{}; ///< Zephyr mutex marked mutable for direct locking in const methods.
    ImuMeasurement latest_data_{};    ///< Most recent sensor snapshot.
};

}  // namespace app::sensor
