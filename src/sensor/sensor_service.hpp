#pragma once

#include "common/app_status.hpp"
#include "sensor/sensor_data.hpp"

namespace app::sensor {

/**
 * @brief Abstract interface for motion sensor hardware services.
 *
 * Design Pattern:
 * - Follows the Dependency Inversion Principle (DIP).
 * - Higher-level sensor tasks interact with this abstraction, decoupling
 *   application logic from the Zephyr sensor subsystem and underlying I2C drivers.
 */
class ISensorService {
public:
    virtual ~ISensorService() = default;

    /**
     * @brief Verifies that the sensor hardware is ready and operational.
     * @return Status::kOk if sensor is detected and ready.
     * @return Status::kNotReady or Status::kIoError if initialization fails.
     */
    [[nodiscard]] virtual Status Initialize() noexcept = 0;

    /**
     * @brief Fetches an updated motion sample (accelerometer, gyroscope, temperature).
     * @param[out] out Structure populated with new readings.
     * @return Status::kOk on success, error code otherwise.
     */
    [[nodiscard]] virtual Status FetchSample(ImuMeasurement& out) noexcept = 0;
};

}  // namespace app::sensor

