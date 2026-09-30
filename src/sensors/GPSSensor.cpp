// =============================================================================
//  sensors/GPSSensor.cpp
// =============================================================================
#include "sensors/GPSSensor.h"

#include <Arduino.h>

GPSSensor::GPSSensor(uint8_t uartPort, int rxPin, int txPin, uint32_t baud)
    : _serial(uartPort), _rxPin(rxPin), _txPin(txPin), _baud(baud) {}

bool GPSSensor::begin() {
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);

    // A cold NEO-6M can take 30 s to several minutes to achieve its first fix,
    // and indoors it may never get one. So begin() deliberately does NOT wait
    // for a fix -- it only confirms the module is talking to us at all. Waiting
    // here would stall the whole system on something outside its control.
    const uint32_t probeDeadline = millis() + 1500;
    while (millis() < probeDeadline) {
        while (_serial.available()) {
            _gps.encode(_serial.read());
        }
        delay(10);
    }

    // charsProcessed() > 0 means bytes arrived on the wire. That is the honest
    // definition of "the GPS module is wired up and powered", independent of
    // whether it can currently see satellites.
    _ready = (_gps.charsProcessed() > 0);
    return _ready;
}

void GPSSensor::service() {
    // At 9600 baud a full NMEA burst is ~600 bytes per second. The ESP32 UART
    // FIFO is 128 bytes, so this must be drained several times a second or
    // sentences are lost to overflow.
    while (_serial.available()) {
        if (_gps.encode(_serial.read())) {
            if (_gps.location.isValid()) {
                _lastLat   = _gps.location.lat();
                _lastLon   = _gps.location.lng();
                _haveFix   = true;
                _lastFixMs = millis();
            }
            if (_gps.altitude.isValid()) {
                _lastAlt = _gps.altitude.meters();
            }
            if (_gps.speed.isValid()) {
                _lastSpd = _gps.speed.kmph();
            }
            if (_gps.satellites.isValid()) {
                _lastSats = static_cast<uint8_t>(_gps.satellites.value());
            }
        }
    }
}

void GPSSensor::read(SensorData& out) {
    service();   // make sure we are reporting the freshest parse available

    if (!_ready && _gps.charsProcessed() == 0) {
        // Nothing has ever arrived: the module is absent, unpowered, or the
        // RX/TX pair is swapped.
        out.gpsFixValid    = false;
        out.satellites     = 0;
        out.locationSource = DataSource::UNAVAILABLE;
        return;
    }

    _ready = true;   // bytes are flowing even if a fix is not yet available

    const bool fixIsFresh =
        _haveFix && (millis() - _lastFixMs) < GPS_FIX_TIMEOUT_MS;

    out.latitude    = _lastLat;
    out.longitude   = _lastLon;
    out.altitudeM   = _lastAlt;
    out.speedKmph   = _lastSpd;
    out.satellites  = _lastSats;
    out.gpsFixValid = fixIsFresh;

    if (fixIsFresh) {
        out.locationSource = DataSource::REAL_SENSOR;
    } else if (_haveFix) {
        // We had a fix once; it has aged out (tunnel, indoors, antenna issue).
        out.locationSource = DataSource::STALE;
    } else {
        // Module is alive and streaming, but has never resolved a position.
        out.locationSource = DataSource::UNAVAILABLE;
    }
}
