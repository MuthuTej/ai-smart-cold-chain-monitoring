// =============================================================================
//  reporting/SerialReporter.cpp
// =============================================================================
#include "reporting/SerialReporter.h"

#include <Arduino.h>

#include "config.h"

namespace {

constexpr char RULE[] =
    "==================================================";

void printRule() {
    Serial.println(RULE);
}

// Render uptime as HH:MM:SS. Raw milliseconds are unreadable once a demo has
// been running for a few minutes.
void printUptime(uint32_t ms) {
    const uint32_t totalSec = ms / 1000UL;
    const uint32_t h = totalSec / 3600UL;
    const uint32_t m = (totalSec % 3600UL) / 60UL;
    const uint32_t s = totalSec % 60UL;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
             static_cast<unsigned long>(h),
             static_cast<unsigned long>(m),
             static_cast<unsigned long>(s));
    Serial.print(buf);
}

// Section heading plus the provenance tag for that section.
void printSectionHeader(const char* title, DataSource source) {
    Serial.println();
    Serial.print(title);
    Serial.print(F("   ["));
    Serial.print(toString(source));
    Serial.println(F("]"));
}

}  // namespace

namespace SerialReporter {

void printDisclaimer() {
    Serial.println(F("NOTE: Prototype thresholds only. The temperature and"));
    Serial.println(F("      humidity limits used by the status engine were"));
    Serial.println(F("      chosen for demonstration and are NOT medically or"));
    Serial.println(F("      industrially certified. Do not use this device to"));
    Serial.println(F("      monitor real perishable or medical cargo."));
}

void printBootBanner(bool demoMode,
                     const char* envName,  bool envOk,
                     const char* motionName, bool motionOk,
                     const char* locName,  bool locOk,
                     const StorageManager& storage) {
    Serial.println();
    printRule();
    Serial.println(F("  AI SMART COLD-CHAIN MONITORING"));
    printRule();
    Serial.print(F("Firmware : "));
    Serial.print(F(FIRMWARE_NAME));
    Serial.print(F(" v"));
    Serial.println(F(FIRMWARE_VERSION));
    Serial.print(F("Device   : "));
    Serial.println(F(DEVICE_ID));
    Serial.print(F("MODE     : "));
    Serial.println(demoMode ? F("DEMO / SIMULATION")
                            : F("REAL SENSOR"));
    Serial.print(F("Interval : "));
    Serial.print(SAMPLE_INTERVAL_MS);
    Serial.println(F(" ms"));

    Serial.println();
    Serial.println(F("SUBSYSTEM INITIALISATION"));
    Serial.print(F("  Environment : "));
    Serial.print(envName);
    Serial.println(envOk ? F("  -> OK") : F("  -> FAILED"));

    Serial.print(F("  Motion      : "));
    Serial.print(motionName);
    Serial.println(motionOk ? F("  -> OK") : F("  -> FAILED"));

    Serial.print(F("  Location    : "));
    Serial.print(locName);
    // "OK" for a GPS means the module is streaming NMEA, not that it has a fix.
    // Saying so here avoids a false expectation when the first records show no
    // position.
    Serial.println(locOk ? F("  -> OK (streaming; fix may take minutes)")
                         : F("  -> FAILED"));

    Serial.print(F("  Storage     : "));
    if (storage.isSimulated()) {
        Serial.println(F("MicroSD (DEMO/SIMULATED)  -> no card accessed"));
    } else if (storage.isReady()) {
        Serial.print(F("MicroSD (real)  -> OK, "));
        Serial.print(storage.cardTypeName());
        Serial.print(F(" "));
        Serial.print(static_cast<unsigned long>(storage.cardSizeMB()));
        Serial.println(F(" MB"));
    } else {
        Serial.println(F("MicroSD (real)  -> FAILED (no card / wiring?)"));
    }

    if (!envOk || !motionOk || !locOk) {
        Serial.println();
        Serial.println(F("One or more subsystems did not initialise. The system"));
        Serial.println(F("will keep running; affected fields are reported as"));
        Serial.println(F("UNAVAILABLE rather than being filled with fake data."));
    }

    Serial.println();
    printDisclaimer();
    printRule();
    Serial.println();
}

void printRecord(const SensorData& s, const TrendAnalyzer& analytics) {
    printRule();
    Serial.println(F("  AI SMART COLD-CHAIN MONITORING"));
    printRule();

    Serial.print(F("MODE     : "));
    Serial.println(s.demoMode ? F("DEMO") : F("REAL SENSOR"));
    Serial.print(F("SAMPLE   : #"));
    Serial.print(s.sequence);
    Serial.print(F("   UPTIME: "));
    printUptime(s.timestampMs);
    Serial.println();

    // ---- environment ----------------------------------------------------
    printSectionHeader("ENVIRONMENT", s.envSource);
    if (s.envSource == DataSource::UNAVAILABLE) {
        Serial.println(F("  Temperature : --   (sensor not responding)"));
        Serial.println(F("  Humidity    : --   (sensor not responding)"));
    } else {
        Serial.print(F("  Temperature : "));
        Serial.print(s.temperatureC, 1);
        Serial.println(F(" C"));
        Serial.print(F("  Humidity    : "));
        Serial.print(s.humidityPct, 1);
        Serial.println(F(" %"));
        Serial.print(F("  Trend       : "));
        Serial.print(s.temperatureTrendC, 2);
        Serial.println(F(" C/min  (EWMA smoothed)"));
    }

    // ---- motion ---------------------------------------------------------
    printSectionHeader("MOTION", s.motionSource);
    if (s.motionSource == DataSource::UNAVAILABLE) {
        Serial.println(F("  Accelerometer / gyroscope unavailable"));
    } else {
        Serial.print(F("  Acceleration X : "));
        Serial.print(s.accelerationX, 2); Serial.println(F(" m/s2"));
        Serial.print(F("  Acceleration Y : "));
        Serial.print(s.accelerationY, 2); Serial.println(F(" m/s2"));
        Serial.print(F("  Acceleration Z : "));
        Serial.print(s.accelerationZ, 2); Serial.println(F(" m/s2"));
        Serial.println();
        Serial.print(F("  Gyroscope X : "));
        Serial.print(s.gyroscopeX, 2); Serial.println(F(" rad/s"));
        Serial.print(F("  Gyroscope Y : "));
        Serial.print(s.gyroscopeY, 2); Serial.println(F(" rad/s"));
        Serial.print(F("  Gyroscope Z : "));
        Serial.print(s.gyroscopeZ, 2); Serial.println(F(" rad/s"));
        Serial.println();
        Serial.print(F("  Shock       : "));
        Serial.print(s.shockMagnitudeMs2, 2);
        Serial.print(F(" m/s2 excess"));
        if (s.shockEventDetected) {
            Serial.print(F("   <-- SHOCK EVENT"));
        }
        Serial.println();
        Serial.print(F("  Peak shock  : "));
        Serial.print(analytics.peakShockMs2(), 2);
        Serial.print(F(" m/s2   Events: "));
        Serial.println(analytics.shockEventCount());
    }

    // ---- location -------------------------------------------------------
    printSectionHeader("LOCATION", s.locationSource);
    if (s.locationSource == DataSource::UNAVAILABLE) {
        Serial.println(F("  No position fix yet"));
        Serial.print(F("  Satellites : "));
        Serial.println(s.satellites);
    } else {
        Serial.print(F("  Latitude   : "));
        Serial.println(s.latitude, 6);
        Serial.print(F("  Longitude  : "));
        Serial.println(s.longitude, 6);
        Serial.print(F("  Altitude   : "));
        Serial.print(s.altitudeM, 1); Serial.println(F(" m"));
        Serial.print(F("  Speed      : "));
        Serial.print(s.speedKmph, 1); Serial.println(F(" km/h"));
        Serial.print(F("  Satellites : "));
        Serial.println(s.satellites);
        Serial.print(F("  Fix        : "));
        Serial.println(s.gpsFixValid ? F("VALID") : F("NO FIX"));
    }

    // ---- storage --------------------------------------------------------
    Serial.println();
    Serial.println(F("STORAGE"));
    Serial.print(F("  Status  : "));
    Serial.println(toString(s.storageStatus));
    Serial.print(F("  Records : "));
    Serial.println(s.recordsWritten);

    // ---- verdict --------------------------------------------------------
    Serial.println();
    Serial.println(F("SYSTEM STATUS"));
    Serial.print(F("  "));
    Serial.println(toString(s.systemStatus));
    if (s.statusReason != nullptr && s.statusReason[0] != '\0') {
        Serial.print(F("  Reason: "));
        Serial.println(s.statusReason);
    }

    printRule();
    Serial.println();
}

void printCompact(const SensorData& s) {
    char line[160];
    snprintf(line, sizeof(line),
             "#%lu  %.1fC  %.1f%%  |a|excess %.2f  %.6f,%.6f  sat %u  %s  %s",
             static_cast<unsigned long>(s.sequence),
             s.temperatureC, s.humidityPct,
             s.shockMagnitudeMs2,
             s.latitude, s.longitude,
             static_cast<unsigned>(s.satellites),
             toString(s.systemStatus),
             s.demoMode ? "DEMO" : "REAL");
    Serial.println(line);
}

}  // namespace SerialReporter
