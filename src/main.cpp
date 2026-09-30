// =============================================================================
//  main.cpp
//  AI-Powered Smart Cold-Chain Monitoring System  --  application entry point
//
//  WHAT THIS FILE DOES, AND WHAT IT DELIBERATELY DOES NOT
//  -----------------------------------------------------
//  It owns the order of operations for one monitoring cycle:
//
//      acquire  ->  analyse  ->  classify  ->  log  ->  report
//
//  It does NOT know how any value is obtained. The three provider pointers
//  below are interface types; whether they point at a real driver or at the
//  simulator is decided once, in setup(), behind a single #if. Nothing else in
//  this file -- and nothing at all in the analytics, status, storage or
//  reporting layers -- is aware of the difference.
//
//  That is the whole design: swapping a real sensor for a simulated one, or
//  adding a third source later, touches exactly one function.
//
//  See docs/architecture.md.
// =============================================================================
#include <Arduino.h>

#include "config.h"
#include "models/SensorData.h"

#include "analytics/TrendAnalyzer.h"
#include "reporting/SerialReporter.h"
#include "sensors/ISensorProvider.h"
#include "status/StatusEngine.h"
#include "storage/StorageManager.h"

// Both sets of providers are compiled in; DEMO_MODE decides which get
// instantiated. Keeping both includes unconditional means a change to a real
// driver is still compile-checked by the demo build, so the hardware path
// cannot silently rot while only the demo is being built.
#include "demo/DemoSensorData.h"
#include "sensors/DHT11Sensor.h"
#include "sensors/GPSSensor.h"
#include "sensors/MPU6050Sensor.h"

// -----------------------------------------------------------------------------
//  Provider instances.
//
//  Statically allocated rather than new'd: an embedded target with no heap
//  pressure and a fixed device list has nothing to gain from dynamic allocation
//  and everything to lose from fragmentation over a long run.
// -----------------------------------------------------------------------------
#if DEMO_MODE
  #if DEMO_TRY_REAL_DHT
    // Mixed build: try the real DHT11, simulate the rest. Records from this
    // build carry REAL_SENSOR on the environment block and SIMULATED on the
    // others, so the transcript stays truthful field by field.
    static DHT11Sensor            envProviderImpl;
  #else
    static DemoEnvironmentProvider envProviderImpl;
  #endif
  static DemoMotionProvider       motionProviderImpl;
  static DemoLocationProvider     locationProviderImpl;
#else
  static DHT11Sensor              envProviderImpl;
  static MPU6050Sensor            motionProviderImpl;
  static GPSSensor                locationProviderImpl;
#endif

// The rest of the file sees only these.
static IEnvironmentProvider* envProvider      = &envProviderImpl;
static IMotionProvider*      motionProvider   = &motionProviderImpl;
static ILocationProvider*    locationProvider = &locationProviderImpl;

// -----------------------------------------------------------------------------
//  Processing pipeline.
// -----------------------------------------------------------------------------
static TrendAnalyzer  analytics;
static StatusEngine   statusEngine;
static StorageManager storage(DEMO_MODE ? true : false);

static uint32_t sampleCounter = 0;
static uint32_t lastSampleMs  = 0;

// -----------------------------------------------------------------------------
//  One acquisition cycle.
// -----------------------------------------------------------------------------
static SensorData acquire() {
    SensorData s;

    s.timestampMs = millis();
    s.sequence    = ++sampleCounter;
    s.demoMode    = (DEMO_MODE != 0);

    // Each provider fills only its own fields and sets its own provenance tag.
    envProvider->read(s);
    motionProvider->read(s);
    locationProvider->read(s);

    return s;
}

// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(SERIAL_BOOT_WAIT_MS);   // let the USB CDC link come up

    const bool envOk    = envProvider->begin();
    const bool motionOk = motionProvider->begin();
    const bool locOk    = locationProvider->begin();
    storage.begin();

    // In demo mode, echo the formatted CSV row alongside the dashboard so the
    // storage format is visible without a card. Off in real mode, where the
    // rows are genuinely on the card and echoing them is just noise.
    storage.setEchoRows(DEMO_MODE != 0);

    SerialReporter::printBootBanner(
        DEMO_MODE != 0,
        envProvider->name(),      envOk,
        motionProvider->name(),   motionOk,
        locationProvider->name(), locOk,
        storage);

    lastSampleMs = millis() - SAMPLE_INTERVAL_MS;   // take the first sample now
}

// -----------------------------------------------------------------------------
void loop() {
    // Drain the GPS UART on every pass, not once per sample. A NEO-6M streams
    // continuously at 9600 baud and the ESP32's 128-byte FIFO overflows in well
    // under a second, so sentences would be lost if this waited for the
    // 2-second sample tick.
    locationProvider->service();

    // Non-blocking cadence. delay(SAMPLE_INTERVAL_MS) would be shorter but it
    // would also stop servicing the GPS for two seconds at a time, and would
    // leave no room for any future concurrent work.
    if (millis() - lastSampleMs < SAMPLE_INTERVAL_MS) {
        return;
    }
    lastSampleMs = millis();

    // ---- 1. acquire -----------------------------------------------------
    SensorData sample = acquire();

    // ---- 2. analyse -----------------------------------------------------
    // Derives shock magnitude and the smoothed temperature trend. Must run
    // before the status engine, which consumes both.
    analytics.update(sample);

    // ---- 3. classify ----------------------------------------------------
    statusEngine.evaluate(sample);

    // ---- 4. log ---------------------------------------------------------
    // Runs before the report so the printed record shows the true storage
    // outcome for this very row, rather than the previous one's.
    storage.append(sample);

    // ---- 5. report ------------------------------------------------------
    SerialReporter::printRecord(sample, analytics);
}
