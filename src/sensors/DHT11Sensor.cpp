// =============================================================================
//  sensors/DHT11Sensor.cpp
// =============================================================================
#include "sensors/DHT11Sensor.h"

#include <Arduino.h>
#include <math.h>

DHT11Sensor::DHT11Sensor(uint8_t pin)
    : _dht(pin, DHT11) {}

bool DHT11Sensor::begin() {
    _dht.begin();

    // The DHT11 needs roughly a second after power-up before its first
    // conversion is trustworthy. Probe once so the boot banner can report
    // honestly whether the part answered at all.
    delay(1200);

    const float h = _dht.readHumidity();
    const float t = _dht.readTemperature();   // Celsius

    if (isnan(h) || isnan(t)) {
        // Not fatal. The sensor may simply have been slow; read() will retry
        // every cycle. But do not claim the sensor is present.
        _ready = false;
        return false;
    }

    _lastHumid = h;
    _lastTempC = t;
    _haveGood  = true;
    _ready     = true;
    return true;
}

void DHT11Sensor::read(SensorData& out) {
    const float h = _dht.readHumidity();
    const float t = _dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        if (_failCount < 255) {
            _failCount++;
        }

        if (_haveGood && _failCount <= DHT_FAIL_TOLERANCE) {
            // Recent enough to be worth re-serving, but say so.
            out.temperatureC = _lastTempC;
            out.humidityPct  = _lastHumid;
            out.envSource    = DataSource::STALE;
        } else {
            out.temperatureC = 0.0f;
            out.humidityPct  = 0.0f;
            out.envSource    = DataSource::UNAVAILABLE;
        }
        return;
    }

    _failCount = 0;
    _lastTempC = t;
    _lastHumid = h;
    _haveGood  = true;
    _ready     = true;

    out.temperatureC = t;
    out.humidityPct  = h;
    out.envSource    = DataSource::REAL_SENSOR;
}
