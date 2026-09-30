// =============================================================================
//  sensors/GPSSensor.h
//  Real NEO-6M GPS driver (TinyGPSPlus over ESP32 HardwareSerial / UART2).
//
//  Wiring:  NEO-6M TX -> D16 (ESP32 RX2),  NEO-6M RX -> D17 (ESP32 TX2)
//           GND -> GND,  VCC -> 3.3V (check your breakout: many NEO-6M boards
//           are 5 V tolerant on VCC but output 3.3 V logic)
//  Baud:    9600
//  See docs/pinout.md.
// =============================================================================
#pragma once

#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

#include "config.h"
#include "sensors/ISensorProvider.h"

class GPSSensor : public ILocationProvider {
public:
    GPSSensor(uint8_t uartPort = GPS_UART_PORT,
              int rxPin        = PIN_GPS_RX,
              int txPin        = PIN_GPS_TX,
              uint32_t baud    = GPS_BAUD);

    bool begin() override;

    // Drain the UART and feed every byte to the NMEA parser. Must be called
    // frequently -- see ILocationProvider::service().
    void service() override;

    void read(SensorData& out) override;

    const char* name() const override { return "NEO-6M GPS (real sensor)"; }
    bool isReady() const override { return _ready; }

    // Diagnostics for the boot banner / troubleshooting.
    uint32_t charsProcessed() const { return _gps.charsProcessed(); }
    uint32_t sentencesWithFix() const { return _gps.sentencesWithFix(); }
    uint32_t checksumFailures() const { return _gps.failedChecksum(); }

private:
    TinyGPSPlus   _gps;
    HardwareSerial _serial;
    int      _rxPin;
    int      _txPin;
    uint32_t _baud;
    bool     _ready = false;

    // Last moment a valid fix was parsed, so a fix that goes away can be
    // reported as STALE rather than silently held forever.
    uint32_t _lastFixMs = 0;
    bool     _haveFix   = false;

    double  _lastLat = 0.0;
    double  _lastLon = 0.0;
    float   _lastAlt = 0.0f;
    float   _lastSpd = 0.0f;
    uint8_t _lastSats = 0;
};
