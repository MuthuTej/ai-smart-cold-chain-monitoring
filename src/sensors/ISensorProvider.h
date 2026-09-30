// =============================================================================
//  sensors/ISensorProvider.h
//  The seam between "how a number was obtained" and "what is done with it".
//
//  main.cpp holds three pointers of these interface types. It never knows
//  whether the object behind a pointer is a real driver or the simulator --
//  that choice is made once, at construction time, behind #if DEMO_MODE.
//
//  Adding a new source (a BLE temperature tag, a CAN bus feed, a replay of a
//  recorded CSV) means writing one more implementation of one of these
//  interfaces. No other file changes.
// =============================================================================
#pragma once

#include "models/SensorData.h"

// -----------------------------------------------------------------------------
//  Common contract for every provider.
// -----------------------------------------------------------------------------
class ISensorProvider {
public:
    virtual ~ISensorProvider() = default;

    // Bring the underlying hardware up. Returns false if the device did not
    // answer. A false return is NOT fatal for the application: the provider is
    // still safe to call, and will report UNAVAILABLE / STALE on its fields.
    virtual bool begin() = 0;

    // Human-readable provider name for the boot banner, e.g.
    // "DHT11 (real)" or "MPU6050 (simulated)".
    virtual const char* name() const = 0;

    // True once begin() has succeeded.
    virtual bool isReady() const = 0;
};

// -----------------------------------------------------------------------------
//  Temperature + humidity.
//  Writes: temperatureC, humidityPct, envSource
// -----------------------------------------------------------------------------
class IEnvironmentProvider : public ISensorProvider {
public:
    virtual void read(SensorData& out) = 0;
};

// -----------------------------------------------------------------------------
//  Accelerometer + gyroscope.
//  Writes: acceleration{X,Y,Z}, gyroscope{X,Y,Z}, temperatureMpuC, motionSource
// -----------------------------------------------------------------------------
class IMotionProvider : public ISensorProvider {
public:
    virtual void read(SensorData& out) = 0;
};

// -----------------------------------------------------------------------------
//  Position.
//  Writes: latitude, longitude, altitudeM, speedKmph, satellites,
//          gpsFixValid, locationSource
// -----------------------------------------------------------------------------
class ILocationProvider : public ISensorProvider {
public:
    virtual void read(SensorData& out) = 0;

    // A real NEO-6M streams NMEA continuously at 9600 baud and must be drained
    // often or the UART FIFO overflows and sentences are lost. Call this as
    // fast as the main loop allows; read() then reports the latest parse.
    // Simulated providers implement this as a no-op.
    virtual void service() {}
};
