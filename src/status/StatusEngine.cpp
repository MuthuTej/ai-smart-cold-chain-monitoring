// =============================================================================
//  status/StatusEngine.cpp
// =============================================================================
#include "status/StatusEngine.h"

#include <math.h>

void StatusEngine::reset() {
    _published       = ColdChainStatus::UNKNOWN;
    _publishedReason = "";
    _candidate       = ColdChainStatus::UNKNOWN;
    _candidateRun    = 0;
    _candidateReason = "";
    _warnings        = 0;
    _criticals       = 0;
}

ColdChainStatus StatusEngine::classify(const SensorData& s,
                                       const char** reason,
                                       bool* instantaneous) const {
    *instantaneous = false;
    // ---- 1. Can we judge this sample at all? ----------------------------
    const bool envUsable = (s.envSource == DataSource::REAL_SENSOR ||
                            s.envSource == DataSource::SIMULATED  ||
                            s.envSource == DataSource::STALE);
    if (!envUsable) {
        *reason = "no temperature/humidity data available";
        return ColdChainStatus::UNKNOWN;
    }

    // ---- 2. Hard temperature limits ------------------------------------
    if (s.temperatureC >= TEMP_CRITICAL_HIGH_C) {
        *reason = "temperature above critical high threshold";
        return ColdChainStatus::CRITICAL;
    }
    if (s.temperatureC <= TEMP_CRITICAL_LOW_C) {
        *reason = "temperature below critical low threshold";
        return ColdChainStatus::CRITICAL;
    }

    // ---- 3. Impact ------------------------------------------------------
    const bool motionUsable = (s.motionSource == DataSource::REAL_SENSOR ||
                               s.motionSource == DataSource::SIMULATED);
    if (motionUsable && s.shockMagnitudeMs2 >= SHOCK_CRITICAL_MS2) {
        *reason        = "impact detected - possible drop or collision";
        *instantaneous = true;   // one-sample event; must bypass the debounce
        return ColdChainStatus::CRITICAL;
    }

    // ---- 4. Hard humidity limit ----------------------------------------
    if (s.humidityPct >= HUMIDITY_CRITICAL_HIGH_PCT) {
        *reason = "humidity above critical high threshold";
        return ColdChainStatus::CRITICAL;
    }

    // ---- 5. Soft temperature limits ------------------------------------
    if (s.temperatureC >= TEMP_WARNING_HIGH_C) {
        *reason = "temperature above warning high threshold";
        return ColdChainStatus::WARNING;
    }
    if (s.temperatureC <= TEMP_WARNING_LOW_C) {
        *reason = "temperature below warning low threshold";
        return ColdChainStatus::WARNING;
    }

    // ---- 6. Soft humidity limits ---------------------------------------
    if (s.humidityPct >= HUMIDITY_WARNING_HIGH_PCT) {
        *reason = "humidity above warning high threshold";
        return ColdChainStatus::WARNING;
    }
    if (s.humidityPct <= HUMIDITY_WARNING_LOW_PCT) {
        *reason = "humidity below warning low threshold";
        return ColdChainStatus::WARNING;
    }

    if (motionUsable) {
        // ---- 7. Rough handling -----------------------------------------
        if (s.shockMagnitudeMs2 >= SHOCK_WARNING_MS2) {
            *reason        = "rough handling - acceleration spike";
            *instantaneous = true;
            return ColdChainStatus::WARNING;
        }

        // ---- 8. Tipping / rolling --------------------------------------
        const float maxRotation = fmaxf(fmaxf(fabsf(s.gyroscopeX),
                                              fabsf(s.gyroscopeY)),
                                        fabsf(s.gyroscopeZ));
        if (maxRotation >= TILT_CRITICAL_RAD_S) {
            *reason        = "severe rotation - package may have overturned";
            *instantaneous = true;
            return ColdChainStatus::WARNING;
        }
        if (maxRotation >= TILT_WARNING_RAD_S) {
            *reason        = "package tilting or rolling";
            *instantaneous = true;
            return ColdChainStatus::WARNING;
        }
    }

    // ---- 9. Predictive: drifting toward a limit -------------------------
    // This is the only rule that can fire while every absolute value is still
    // comfortably in band. See analytics/TrendAnalyzer for what the trend is.
    if (fabsf(s.temperatureTrendC) >= TREND_WARNING_C_PER_MIN) {
        *reason = (s.temperatureTrendC > 0.0f)
                      ? "temperature rising fast - excursion predicted"
                      : "temperature falling fast - excursion predicted";
        return ColdChainStatus::WARNING;
    }

    // ---- 10. All clear --------------------------------------------------
    *reason = "";
    return ColdChainStatus::NORMAL;
}

void StatusEngine::evaluate(SensorData& sample) {
    const char* rawReason   = "";
    bool        isInstant   = false;
    const ColdChainStatus raw = classify(sample, &rawReason, &isInstant);

    if (raw == _published) {
        // Already there; nothing to debounce.
        _candidate       = raw;
        _candidateRun    = 0;
        _candidateReason = rawReason;
    } else if (raw == _candidate) {
        _candidateRun++;
        _candidateReason = rawReason;
    } else {
        _candidate       = raw;
        _candidateRun    = 1;
        _candidateReason = rawReason;
    }

    // De-escalating (or leaving UNKNOWN on the very first sample) is published
    // immediately. Escalating waits for the run length to prove itself.
    const bool isDeEscalation =
        static_cast<uint8_t>(raw) < static_cast<uint8_t>(_published) ||
        _published == ColdChainStatus::UNKNOWN;

    if (raw != _published &&
        (isDeEscalation || isInstant ||
         _candidateRun >= STATUS_DEBOUNCE_SAMPLES)) {
        _published       = raw;
        _publishedReason = rawReason;

        if (raw == ColdChainStatus::WARNING)  _warnings++;
        if (raw == ColdChainStatus::CRITICAL) _criticals++;
    } else if (raw == _published) {
        // Same verdict as last time, but keep the reason current -- the cause
        // of a sustained WARNING can change from sample to sample.
        _publishedReason = rawReason;
    }
    // While an escalation is still being debounced, _publishedReason is left
    // alone. That is deliberate: the reported reason always describes the
    // status actually being reported, so the two can never contradict.

    sample.systemStatus = _published;
    sample.statusReason = (_published == ColdChainStatus::NORMAL)
                              ? "all monitored parameters within prototype limits"
                              : _publishedReason;
}
