// =============================================================================
//  sensors/DHT11Sensor.h
//  Real DHT11 temperature + humidity driver (Adafruit DHT library).
//
//  Wiring:  DHT11 OUT -> D4,  + -> 3.3V,  - -> GND
//  See docs/pinout.md.
// =============================================================================
#pragma once

#include <DHT.h>

#include "config.h"
#include "sensors/ISensorProvider.h"

class DHT11Sensor : public IEnvironmentProvider {
public:
    DHT11Sensor(uint8_t pin = PIN_DHT_DATA);

    bool begin() override;
    void read(SensorData& out) override;
    const char* name() const override { return "DHT11 (real sensor)"; }
    bool isReady() const override { return _ready; }

    // How many consecutive reads have failed. Useful for the boot diagnostics.
    uint8_t consecutiveFailures() const { return _failCount; }

private:
    DHT      _dht;
    bool     _ready     = false;

    // The DHT11 returns NaN fairly often -- it is a slow, cheap part and a
    // single dropped bit invalidates the whole 40-bit frame. Rather than
    // publishing NaN downstream, the last good sample is re-served and tagged
    // STALE, until the failure run exceeds DHT_FAIL_TOLERANCE.
    float    _lastTempC = 0.0f;
    float    _lastHumid = 0.0f;
    bool     _haveGood  = false;
    uint8_t  _failCount = 0;
};
