#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include "common/app_status.hpp"
#include "sensor/sensor_service.hpp"

namespace app::platform {

/**
 * @brief Platform adapter implementing ISensorService for the MPU6050 IMU using Zephyr sensor APIs.
 *
 * Hardware Details:
 * - Compatible with ICQUANZX GY-521 MPU-6050 module.
 * - Communicates via Zephyr's upstream `invensense,mpu6050` sensor driver on I2C0.
 * - Default I2C address is 0x68 (AD0 pulled low).
 */
class ZephyrMpu6050Adapter final : public sensor::ISensorService {
public:
    /**
     * @brief Constructs adapter using the Devicetree node `mpu6050`.
     */
    ZephyrMpu6050Adapter() noexcept;

    /**
     * @brief Constructs adapter with an explicit Zephyr device pointer (for testing / dependency injection).
     * @param dev Pointer to Zephyr sensor device.
     */
    explicit ZephyrMpu6050Adapter(const struct device* dev) noexcept;
    ~ZephyrMpu6050Adapter() override = default;

    // Disallow copy/move to prevent hardware resource aliasing
    ZephyrMpu6050Adapter(const ZephyrMpu6050Adapter&) = delete;
    ZephyrMpu6050Adapter& operator=(const ZephyrMpu6050Adapter&) = delete;
    ZephyrMpu6050Adapter(ZephyrMpu6050Adapter&&) = delete;
    ZephyrMpu6050Adapter& operator=(ZephyrMpu6050Adapter&&) = delete;

    /// @name ISensorService Implementation
    /// @{
    [[nodiscard]] Status Initialize() noexcept override;
    [[nodiscard]] Status FetchSample(sensor::ImuMeasurement& out) noexcept override;
    /// @}

private:
    const struct device* const sensor_dev_{nullptr}; ///< Pointer to Zephyr sensor device.
    bool is_initialized_{false};                     ///< Tracks initialization state.
};

}  // namespace app::platform

