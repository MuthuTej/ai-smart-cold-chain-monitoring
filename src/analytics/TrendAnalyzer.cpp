// =============================================================================
//  analytics/TrendAnalyzer.cpp
// =============================================================================
#include "analytics/TrendAnalyzer.h"

#include <math.h>

void TrendAnalyzer::reset() {
    _primed       = false;
    _ewmaTempC    = 0.0f;
    _trendCPerMin = 0.0f;
    _lastSampleMs = 0;
    _peakShockMs2 = 0.0f;
    _shockEvents  = 0;
}

void TrendAnalyzer::update(SensorData& sample) {
    // ---------------------------------------------------------------------
    //  Shock magnitude
    //
    //  An accelerometer at rest reads 1 g, not 0. So the interesting quantity
    //  is how far the total acceleration vector departs from gravity in either
    //  direction: a jolt pushes it above 9.81, and free fall pushes it toward
    //  zero. Taking the absolute difference catches both.
    // ---------------------------------------------------------------------
    if (sample.motionSource != DataSource::UNAVAILABLE) {
        const float magnitude = sqrtf(
            sample.accelerationX * sample.accelerationX +
            sample.accelerationY * sample.accelerationY +
            sample.accelerationZ * sample.accelerationZ);

        const float excess = fabsf(magnitude - GRAVITY_MS2);

        sample.shockMagnitudeMs2  = excess;
        sample.shockEventDetected = (excess >= SHOCK_WARNING_MS2);

        if (excess > _peakShockMs2) {
            _peakShockMs2 = excess;
        }
        if (sample.shockEventDetected) {
            _shockEvents++;
        }
    }

    // ---------------------------------------------------------------------
    //  Temperature trend
    // ---------------------------------------------------------------------
    // Only meaningful when the temperature is genuinely fresh. A STALE value is
    // the same number re-served, which would fake a trend of exactly zero and
    // suppress a real warning.
    const bool envUsable = (sample.envSource == DataSource::REAL_SENSOR ||
                            sample.envSource == DataSource::SIMULATED);
    if (!envUsable) {
        sample.temperatureTrendC = _trendCPerMin;   // hold the last estimate
        return;
    }

    if (!_primed) {
        // Seed the average with the first real sample instead of with 0.0,
        // which would otherwise produce an enormous false warming trend on the
        // first few cycles as the EWMA climbed from zero to ambient.
        _ewmaTempC    = sample.temperatureC;
        _trendCPerMin = 0.0f;
        _lastSampleMs = sample.timestampMs;
        _primed       = true;

        sample.temperatureTrendC = 0.0f;
        return;
    }

    const float previousEwma = _ewmaTempC;
    _ewmaTempC = (EWMA_ALPHA * sample.temperatureC) +
                 ((1.0f - EWMA_ALPHA) * _ewmaTempC);

    // Convert the change in the smoothed value into °C/min using the actual
    // elapsed time, not the nominal interval, so a loop that runs late does not
    // distort the rate.
    const uint32_t elapsedMs = sample.timestampMs - _lastSampleMs;
    if (elapsedMs > 0) {
        const float deltaC       = _ewmaTempC - previousEwma;
        const float elapsedMin   = static_cast<float>(elapsedMs) / 60000.0f;
        const float instantRate  = deltaC / elapsedMin;

        // Smooth the rate with its own, much slower coefficient. Using
        // EWMA_ALPHA here would leave the rate dominated by single-sample
        // sensor noise rather than by real drift -- see EWMA_TREND_ALPHA.
        _trendCPerMin = (EWMA_TREND_ALPHA * instantRate) +
                        ((1.0f - EWMA_TREND_ALPHA) * _trendCPerMin);
    }
    _lastSampleMs = sample.timestampMs;

    sample.temperatureTrendC = _trendCPerMin;
}
