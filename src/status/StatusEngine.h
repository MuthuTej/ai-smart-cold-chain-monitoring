// =============================================================================
//  status/StatusEngine.h
//  Turns one SensorData sample into a single cold-chain verdict.
//
//  *** PROTOTYPE THRESHOLDS -- NOT CERTIFIED ***
//  The bands this engine applies come from config.h and were chosen so that a
//  desk demo produces a readable mix of states. They are not medically or
//  industrially certified, are not derived from any cold-chain standard, and
//  must not be relied on for real perishable or pharmaceutical cargo.
//
//  RULES, IN PRIORITY ORDER
//  ------------------------
//   1. No usable environment data              -> UNKNOWN
//   2. Temperature outside a CRITICAL bound    -> CRITICAL
//   3. Shock above SHOCK_CRITICAL_MS2          -> CRITICAL   (impact/drop)
//   4. Humidity above CRITICAL high            -> CRITICAL
//   5. Temperature outside a WARNING bound     -> WARNING
//   6. Humidity outside a WARNING bound        -> WARNING
//   7. Shock above SHOCK_WARNING_MS2           -> WARNING    (rough handling)
//   8. Rotation above TILT_WARNING_RAD_S       -> WARNING    (tipped/rolled)
//   9. |trend| above TREND_WARNING_C_PER_MIN   -> WARNING    (predictive)
//  10. otherwise                               -> NORMAL
//
//  The highest-severity rule that matches wins, and its reason string is what
//  gets reported -- so an operator sees *why*, not just a colour.
//
//  DEBOUNCING
//  ----------
//  A raw verdict is computed every sample, but an escalation to a *sustained*
//  condition is only published after STATUS_DEBOUNCE_SAMPLES consecutive
//  samples agree. One noisy DHT11 reading should not raise an alarm on its own.
//
//  Instantaneous events -- an impact, a shock, a sharp rotation -- are exempt.
//  A drop is over in milliseconds and will only ever appear in a single sample,
//  so debouncing it would mean impacts were never reported at all. The
//  distinction is carried by the `instantaneous` out-parameter of classify():
//  rules 3, 7 and 8 set it, and the debounce is skipped for those.
//
//  De-escalation is always immediate. Recovering from an alarm is good news and
//  there is no reason to delay reporting it.
// =============================================================================
#pragma once

#include <stdint.h>

#include "analytics/TrendAnalyzer.h"
#include "config.h"
#include "models/SensorData.h"

class StatusEngine {
public:
    // Classify the sample and write systemStatus + statusReason into it.
    void evaluate(SensorData& sample);

    // Counters for an end-of-run summary.
    uint32_t warningCount() const  { return _warnings; }
    uint32_t criticalCount() const { return _criticals; }

    void reset();

private:
    // The un-debounced verdict for a single sample.
    //
    // `reason`        receives a static string explaining the verdict.
    // `instantaneous` receives true when the verdict came from a discrete
    //                 event (impact / shock / sharp rotation) that will not
    //                 repeat on the next sample, and so must bypass the
    //                 escalation debounce.
    ColdChainStatus classify(const SensorData& s,
                             const char** reason,
                             bool* instantaneous) const;

    ColdChainStatus _published       = ColdChainStatus::UNKNOWN;
    const char*     _publishedReason = "";

    ColdChainStatus _candidate       = ColdChainStatus::UNKNOWN;
    uint8_t         _candidateRun    = 0;
    const char*     _candidateReason = "";

    uint32_t _warnings  = 0;
    uint32_t _criticals = 0;
};
