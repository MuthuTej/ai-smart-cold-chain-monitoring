// =============================================================================
//  models/SensorData.h
//  The one record type that flows through every layer of the firmware.
//
//  Every layer above the drivers -- the status engine, the analytics helper,
//  the storage writer, the serial reporter -- consumes ONLY this struct. That
//  is what makes a real driver and a simulated provider interchangeable: they
//  fill the same fields, and nothing downstream can tell (or needs to tell)
//  which one ran.
// =============================================================================
#pragma once

#include <stdint.h>

// -----------------------------------------------------------------------------
//  Overall cold-chain verdict for one sample.
// -----------------------------------------------------------------------------
enum class ColdChainStatus : uint8_t {
    NORMAL   = 0,   // everything inside the configured prototype bands
    WARNING  = 1,   // drifting, or outside a soft band, or rough handling
    CRITICAL = 2,   // outside a hard band, or an impact event
    UNKNOWN  = 3    // not enough valid data to judge this sample
};

// -----------------------------------------------------------------------------
//  Storage subsystem state for one sample.
// -----------------------------------------------------------------------------
enum class StorageStatus : uint8_t {
    LOGGING    = 0,   // the row was appended (really, or simulated in demo)
    NO_CARD    = 1,   // card absent or SD.begin() failed
    WRITE_FAIL = 2,   // card present, append failed
    SIMULATED  = 3,   // DEMO_MODE: no card touched, counter advanced only
    DISABLED   = 4    // storage intentionally off
};

// -----------------------------------------------------------------------------
//  Per-field provenance.
//
//  This exists so the firmware can never overstate itself. A record does not
//  just say "demo mode"; each sensor group independently records whether the
//  numbers came off real silicon or out of the simulator, and the serial
//  report prints that tag next to every block.
// -----------------------------------------------------------------------------
enum class DataSource : uint8_t {
    REAL_SENSOR = 0,   // a physical read succeeded on this cycle
    SIMULATED   = 1,   // value synthesised by src/demo/DemoSensorData
    STALE       = 2,   // last known real value, re-served after a failed read
    UNAVAILABLE = 3    // no value at all (sensor absent / never initialised)
};

// -----------------------------------------------------------------------------
//  One complete cold-chain observation.
// -----------------------------------------------------------------------------
struct SensorData {
    // --- time -----------------------------------------------------------
    // Milliseconds since boot (millis()). There is no RTC and no NTP on this
    // revision, so this is uptime, NOT wall-clock time. A cloud ingest layer
    // would stamp true UTC on arrival. docs/data-format.md covers this.
    uint32_t timestampMs      = 0;
    uint32_t sequence         = 0;   // monotonic sample counter since boot

    // --- environment (DHT11) --------------------------------------------
    float temperatureC        = 0.0f;
    float humidityPct         = 0.0f;
    DataSource envSource      = DataSource::UNAVAILABLE;

    // --- motion (MPU6050) -----------------------------------------------
    float accelerationX       = 0.0f;   // m/s^2
    float accelerationY       = 0.0f;   // m/s^2
    float accelerationZ       = 0.0f;   // m/s^2, ~ +9.81 when flat and still
    float gyroscopeX          = 0.0f;   // rad/s
    float gyroscopeY          = 0.0f;   // rad/s
    float gyroscopeZ          = 0.0f;   // rad/s
    float temperatureMpuC     = 0.0f;   // MPU6050 die temperature, diagnostics
    DataSource motionSource   = DataSource::UNAVAILABLE;

    // --- location (NEO-6M) ----------------------------------------------
    double latitude           = 0.0;    // decimal degrees, WGS-84
    double longitude          = 0.0;    // decimal degrees, WGS-84
    float  altitudeM          = 0.0f;   // metres above mean sea level
    float  speedKmph          = 0.0f;
    uint8_t satellites        = 0;
    bool   gpsFixValid        = false;
    DataSource locationSource = DataSource::UNAVAILABLE;

    // --- derived --------------------------------------------------------
    // Filled by the status engine, not by any driver.
    ColdChainStatus systemStatus = ColdChainStatus::UNKNOWN;
    float  shockMagnitudeMs2  = 0.0f;   // | |a| - g |, the excess over gravity
    float  temperatureTrendC  = 0.0f;   // EWMA drift, °C per minute
    bool   shockEventDetected = false;
    // Short human-readable justification for systemStatus, e.g.
    // "temp 35.2C above critical high". Empty when NORMAL.
    const char* statusReason  = "";

    // --- storage --------------------------------------------------------
    StorageStatus storageStatus = StorageStatus::DISABLED;
    uint32_t recordsWritten     = 0;

    // --- build ----------------------------------------------------------
    // True when this firmware was compiled with DEMO_MODE=1. Note that this is
    // a build-level flag: consult envSource / motionSource / locationSource for
    // what actually happened to each individual field.
    bool demoMode             = false;
};

// -----------------------------------------------------------------------------
//  Small display helpers. Defined inline so every layer can use them without
//  pulling in another translation unit.
// -----------------------------------------------------------------------------
inline const char* toString(ColdChainStatus s) {
    switch (s) {
        case ColdChainStatus::NORMAL:   return "NORMAL";
        case ColdChainStatus::WARNING:  return "WARNING";
        case ColdChainStatus::CRITICAL: return "CRITICAL";
        default:                        return "UNKNOWN";
    }
}

inline const char* toString(StorageStatus s) {
    switch (s) {
        case StorageStatus::LOGGING:    return "LOGGING";
        case StorageStatus::NO_CARD:    return "NO_CARD";
        case StorageStatus::WRITE_FAIL: return "WRITE_FAIL";
        case StorageStatus::SIMULATED:  return "SIMULATED";
        default:                        return "DISABLED";
    }
}

inline const char* toString(DataSource s) {
    switch (s) {
        case DataSource::REAL_SENSOR: return "REAL SENSOR";
        case DataSource::SIMULATED:   return "DEMO/SIMULATED";
        case DataSource::STALE:       return "STALE (last known)";
        default:                      return "UNAVAILABLE";
    }
}
