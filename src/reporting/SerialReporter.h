// =============================================================================
//  reporting/SerialReporter.h
//  Renders a SensorData record as the live serial dashboard.
//
//  This is the only place in the firmware that formats human-facing text, and
//  it is deliberately the only place that decides how provenance is shown. Each
//  block prints its own [REAL SENSOR] / [DEMO/SIMULATED] tag, so a reader can
//  never mistake a simulated block for a measured one -- even in a mixed build
//  where the DHT11 is real and everything else is simulated.
// =============================================================================
#pragma once

#include "analytics/TrendAnalyzer.h"
#include "models/SensorData.h"
#include "status/StatusEngine.h"
#include "storage/StorageManager.h"

namespace SerialReporter {

// One-time banner: firmware identity, build mode, and the outcome of each
// subsystem's begin(). Called once from setup(), after initialisation.
void printBootBanner(bool demoMode,
                     const char* envName,  bool envOk,
                     const char* motionName, bool motionOk,
                     const char* locName,  bool locOk,
                     const StorageManager& storage);

// The repeating live view, one block per sample.
void printRecord(const SensorData& s, const TrendAnalyzer& analytics);

// Compact one-line form, for when the full block is too verbose (long runs,
// or piping into a plotter).
void printCompact(const SensorData& s);

// The threshold/limitation notice. Printed at boot so that anyone reading a
// captured transcript sees the caveat alongside the data, not only in a README
// they may never open.
void printDisclaimer();

}  // namespace SerialReporter
