// =============================================================================
//  sensors/MPU6050Sensor.h
//  Real MPU6050 accelerometer + gyroscope driver (Adafruit MPU6050 / Unified
//  Sensor over I2C).
//
//  Wiring:  SDA -> D21,  SCL -> D22,  VCC -> 3.3V,  GND -> GND
//  Address: 0x68 (0x69 if AD0 is pulled high)
//  See docs/pinout.md.
// =============================================================================
#pragma once

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#include "config.h"
#include "sensors/ISensorProvider.h"

class MPU6050Sensor : public IMotionProvider {
public:
    MPU6050Sensor(uint8_t address = MPU6050_I2C_ADDR);

    bool begin() override;
    void read(SensorData& out) override;
    const char* name() const override { return "MPU6050 (real sensor)"; }
    bool isReady() const override { return _ready; }

private:
    Adafruit_MPU6050 _mpu;
    uint8_t          _address;
    bool             _ready = false;
};
