// =============================================================================
//  analytics/TrendAnalyzer.h
//  Lightweight on-device signal analysis.
//
//  HONEST SCOPE
//  ------------
//  There is no neural network, no trained model, and no inference engine on
//  this ESP32. This class is an exponentially-weighted moving average plus
//  arithmetic. It is called "AI-assisted" in the project title in the loose
//  sense that it makes a forward-looking judgement rather than only comparing
//  the present value to a limit -- nothing more. Please do not describe it as
//  machine learning.
//
//  WHY IT EARNS ITS PLACE
//  ----------------------
//  A pure threshold check can only say "the cargo is already too warm". By the
//  time that fires, the excursion has happened. Tracking the smoothed rate of
//  change lets the status engine say "the cargo is still in band but warming
//  fast enough to leave it in a few minutes", which is the difference between
//  an alarm and a warning that is still actionable.
//
//  WHY EWMA AND NOT A SLIDING WINDOW
//  ---------------------------------
//  An EWMA needs one float of state instead of an N-sample ring buffer, has no
//  window-boundary artefacts, and responds smoothly. On a part where RAM is
//  cheap but predictability matters, the constant-memory option wins.
// =============================================================================
#pragma once

#include <stdint.h>

#include "config.h"
#include "models/SensorData.h"

class TrendAnalyzer {
public:
    // Feed one sample. Must be called once per acquisition cycle, in order.
    // Fills out.temperatureTrendC, out.shockMagnitudeMs2 and
    // out.shockEventDetected.
    void update(SensorData& sample);

    // Smoothed temperature, °C. Less noisy than the raw reading.
    float smoothedTemperatureC() const { return _ewmaTempC; }

    // Smoothed rate of change, °C per minute. Positive = warming.
    float temperatureTrendCPerMin() const { return _trendCPerMin; }

    // Peak |a| excess over gravity seen since boot, m/s^2. A shipment's worst
    // single jolt is often the only number a consignee cares about.
    float peakShockMs2() const { return _peakShockMs2; }

    // Total number of samples that exceeded SHOCK_WARNING_MS2.
    uint32_t shockEventCount() const { return _shockEvents; }

    void reset();

private:
    bool     _primed        = false;   // has the EWMA been seeded yet
    float    _ewmaTempC     = 0.0f;
    float    _trendCPerMin  = 0.0f;
    uint32_t _lastSampleMs  = 0;

    float    _peakShockMs2  = 0.0f;
    uint32_t _shockEvents   = 0;
};
