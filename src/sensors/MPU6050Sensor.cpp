// =============================================================================
//  sensors/MPU6050Sensor.cpp
// =============================================================================
#include "sensors/MPU6050Sensor.h"

MPU6050Sensor::MPU6050Sensor(uint8_t address)
    : _address(address) {}

bool MPU6050Sensor::begin() {
    // The ESP32 I2C pins are remappable, so they are named explicitly rather
    // than relying on the board default.
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!_mpu.begin(_address, &Wire)) {
        _ready = false;
        return false;
    }

    // Ranges chosen for transport monitoring rather than for a quadcopter:
    //
    //  - +/-8 g accelerometer: a package drop easily exceeds 4 g, and clipping
    //    an impact would defeat the point of logging shocks. 8 g keeps headroom
    //    without throwing away resolution at 1 g.
    //  - 500 deg/s gyroscope: covers a parcel being tipped or rolled by hand.
    //  - 21 Hz DLPF: cuts motor/road vibration well above what the 0.5 Hz
    //    sampling loop can represent anyway, so each sample is a cleaner
    //    estimate instead of an alias of engine buzz.
    _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    _ready = true;
    return true;
}

void MPU6050Sensor::read(SensorData& out) {
    if (!_ready) {
        out.motionSource = DataSource::UNAVAILABLE;
        return;
    }

    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;

    if (!_mpu.getEvent(&accel, &gyro, &temp)) {
        out.motionSource = DataSource::UNAVAILABLE;
        return;
    }

    // Adafruit_Sensor normalises units for us: acceleration in m/s^2 and
    // rotation in rad/s, which is exactly what SensorData documents.
    out.accelerationX   = accel.acceleration.x;
    out.accelerationY   = accel.acceleration.y;
    out.accelerationZ   = accel.acceleration.z;

    out.gyroscopeX      = gyro.gyro.x;
    out.gyroscopeY      = gyro.gyro.y;
    out.gyroscopeZ      = gyro.gyro.z;

    // Die temperature, not ambient. It reads several degrees high because it
    // sits on the same silicon as the sensor. Logged for diagnostics only --
    // the cold-chain verdict always uses the DHT11 value.
    out.temperatureMpuC = temp.temperature;

    out.motionSource    = DataSource::REAL_SENSOR;
}
