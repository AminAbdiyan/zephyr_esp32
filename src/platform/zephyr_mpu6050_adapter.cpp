#include "platform/zephyr_mpu6050_adapter.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// Register logging module for the MPU6050 platform adapter
LOG_MODULE_REGISTER(mpu6050_adapter, LOG_LEVEL_INF);

namespace app::platform {

ZephyrMpu6050Adapter::ZephyrMpu6050Adapter() noexcept
    : sensor_dev_(DEVICE_DT_GET_OR_NULL(DT_NODELABEL(mpu6050)))
{
}

ZephyrMpu6050Adapter::ZephyrMpu6050Adapter(const struct device* const dev) noexcept
    : sensor_dev_(dev)
{
}

Status ZephyrMpu6050Adapter::Initialize() noexcept
{
    if (sensor_dev_ == nullptr) {
        LOG_ERR("MPU6050 devicetree node 'mpu6050' not found");
        return Status::kNotReady;
    }

    if (!device_is_ready(sensor_dev_)) {
        LOG_ERR("MPU6050 device '%s' is not ready (check I2C wiring: SDA=GPIO21, SCL=GPIO22, VCC=3.3V/5V)",
                sensor_dev_->name);
        return Status::kNotReady;
    }

    LOG_INF("MPU6050 IMU device '%s' initialized and ready", sensor_dev_->name);
    is_initialized_ = true;
    return Status::kOk;
}

Status ZephyrMpu6050Adapter::FetchSample(sensor::ImuMeasurement& out) noexcept
{
    if (!is_initialized_) {
        const Status init_status = Initialize();
        if (!IsOk(init_status)) {
            return init_status;
        }
    }

    // Trigger internal register sample fetch across all MPU6050 sensors
    const int fetch_rc = sensor_sample_fetch(sensor_dev_);
    if (fetch_rc != 0) {
        LOG_ERR("sensor_sample_fetch failed: %d", fetch_rc);
        return Status::kIoError;
    }

    // Read 3-axis accelerometer channels (returns array of 3 sensor_value: X, Y, Z in m/s^2)
    struct sensor_value accel[3]{};
    int rc = sensor_channel_get(sensor_dev_, SENSOR_CHAN_ACCEL_XYZ, accel);
    if (rc != 0) {
        LOG_ERR("sensor_channel_get ACCEL failed: %d", rc);
        return Status::kIoError;
    }

    // Read 3-axis gyroscope channels (returns array of 3 sensor_value: X, Y, Z in rad/s)
    struct sensor_value gyro[3]{};
    rc = sensor_channel_get(sensor_dev_, SENSOR_CHAN_GYRO_XYZ, gyro);
    if (rc != 0) {
        LOG_ERR("sensor_channel_get GYRO failed: %d", rc);
        return Status::kIoError;
    }

    // Read internal die temperature (degrees Celsius)
    struct sensor_value temp{};
    rc = sensor_channel_get(sensor_dev_, SENSOR_CHAN_DIE_TEMP, &temp);
    if (rc != 0) {
        LOG_WRN("sensor_channel_get DIE_TEMP failed: %d", rc);
    }

    // Convert fixed-point Zephyr sensor_values to standard doubles
    out.accel_x = sensor_value_to_double(&accel[0]);
    out.accel_y = sensor_value_to_double(&accel[1]);
    out.accel_z = sensor_value_to_double(&accel[2]);

    out.gyro_x = sensor_value_to_double(&gyro[0]);
    out.gyro_y = sensor_value_to_double(&gyro[1]);
    out.gyro_z = sensor_value_to_double(&gyro[2]);

    out.temperature = sensor_value_to_double(&temp);
    out.timestamp_ms = k_uptime_get_32();
    out.is_valid = true;

    return Status::kOk;
}

}  // namespace app::platform

