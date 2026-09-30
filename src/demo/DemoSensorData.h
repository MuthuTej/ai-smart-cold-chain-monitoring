// =============================================================================
//  demo/DemoSensorData.h
//  Simulated sensor providers for DEMO_MODE.
//
//  WHAT THIS IS
//  ------------
//  Drop-in implementations of the same three interfaces the real drivers
//  implement, producing plausible cold-chain telemetry with no hardware
//  attached. Every value they emit is tagged DataSource::SIMULATED, so the
//  serial report and the CSV always disclose that the number was synthesised.
//
//  WHAT THIS IS NOT
//  ----------------
//  These classes do not read, probe, or in any way touch a physical sensor.
//  Output from them is never evidence that the hardware works.
//
//  HOW THE VALUES ARE GENERATED
//  ----------------------------
//  Not rand() per sample. Each signal is a slow random walk pinned inside a
//  configured band:
//
//      value += small_step;  then soft-pull back toward the band centre
//
//  The pull-back is what keeps a walk from drifting off to absurdity over a
//  long run, and it is why consecutive samples look related the way real
//  sensor output does (32.1 -> 32.3 -> 31.9 -> 32.4) instead of jumping
//  randomly across the whole range.
//
//  See docs/demo-mode.md for the full table of simulated ranges.
// =============================================================================
#pragma once

#include "config.h"
#include "sensors/ISensorProvider.h"

// -----------------------------------------------------------------------------
//  Simulation parameters. Kept here, next to the simulator, because they are
//  demo-presentation choices rather than system configuration.
// -----------------------------------------------------------------------------
namespace demo {

// --- environment ---
//
//  The simulated temperature is the sum of two components:
//
//    (a) a slow thermal CYCLE, and
//    (b) a small fast random WALK on top of it.
//
//  Why not the walk alone? A bounded random walk hovers near the middle of its
//  band -- over a 200-sample run it explores barely +/-1.5 °C, so it never
//  approaches a threshold and the status engine is never seen to change state.
//  Worse, it misrepresents the physics: a real cold-chain excursion is a slow
//  directional ramp (a compressor duty cycle, a door left open, ice melting),
//  not white noise.
//
//  The cycle gives the demo a genuine story arc -- the cargo warms, crosses the
//  warning bound, peaks near critical, then recovers -- which is what actually
//  exercises the threshold rules and the predictive trend rule. The walk on top
//  supplies the sample-to-sample texture a real DHT11 has.
constexpr float ENV_TEMP_MIN_C      = 28.0f;
constexpr float ENV_TEMP_MAX_C      = 35.0f;

// Cycle: centre, half-amplitude, and period. The amplitude is set so the peak
// crosses TEMP_WARNING_HIGH_C and just reaches TEMP_CRITICAL_HIGH_C, and the
// trough stays inside the band -- so one cycle visits every upper state.
constexpr float ENV_TEMP_CENTRE_C   = 31.2f;
constexpr float ENV_TEMP_SWING_C    = 3.5f;    // -> roughly 27.7 .. 34.7 °C
//
//  The period is set from the RAMP RATE it implies, not from impatience. For a
//  sinusoid the steepest slope is 2*pi*swing/period; at 7 minutes that is about
//  3.1 °C/min, which is a plausible rate for a small insulated box losing its
//  cooling. A 3-minute cycle -- tempting, because the demo arc completes
//  sooner -- implies 6.7 °C/min, which is not plausible for cargo with any
//  thermal mass, and it also holds the predictive trend rule permanently above
//  its threshold so that rule stops distinguishing anything.
//
//  Consequence worth knowing before you watch: one full arc takes 7 minutes, so
//  a 30-second glance at the demo will usually show a steady NORMAL. That is
//  the system working, not the demo being broken.
constexpr uint32_t ENV_TEMP_CYCLE_MS = 420000UL;  // 7 minutes per cycle

// Walk: the fast noise riding on the cycle. Deliberately small -- this is
// sensor texture, not the signal.
constexpr float ENV_TEMP_NOISE_C    = 0.18f;

constexpr float ENV_HUM_MIN_PCT     = 45.0f;
constexpr float ENV_HUM_MAX_PCT     = 70.0f;
constexpr float ENV_HUM_CENTRE_PCT  = 56.0f;
constexpr float ENV_HUM_NOISE_PCT   = 0.7f;

// Relative humidity falls as air warms, because warmer air holds more moisture
// before saturating. Coupling the two (rather than walking them independently)
// is both more realistic and more useful: the humidity trace corroborates the
// temperature story instead of contradicting it. Percent RH per °C.
constexpr float ENV_HUM_PER_DEG_C   = -2.2f;

// --- motion ---
// Stationary package lying flat: gravity on Z, near-zero elsewhere, plus the
// few-hundredths-of-a-g noise floor a real MPU6050 shows on a desk.
constexpr float MOTION_NOISE_XY_MS2 = 0.18f;
constexpr float MOTION_NOISE_Z_MS2  = 0.10f;
constexpr float MOTION_GYRO_NOISE   = 0.03f;   // rad/s

// Occasionally inject a transport bump, so the shock detector and the WARNING /
// CRITICAL paths are actually exercised during a demo. Without this the demo
// would sit at one status forever and never show the status engine switching.
//
// Two tiers, because a real shipment sees two different things:
//   - rough handling (a pothole, a shove) -- common, crosses SHOCK_WARNING_MS2
//   - an impact (dropped, struck)         -- rare, crosses SHOCK_CRITICAL_MS2
//
// 1-in-MOTION_BUMP_ODDS samples get a bump; 1-in-MOTION_IMPACT_ODDS of those
// bumps is the hard kind.
constexpr uint8_t MOTION_BUMP_ODDS    = 12;
constexpr uint8_t MOTION_IMPACT_ODDS  = 5;

constexpr float MOTION_BUMP_MIN_MS2   = 3.5f;   // rough handling, lower edge
constexpr float MOTION_BUMP_MAX_MS2   = 6.0f;   // rough handling, upper edge
constexpr float MOTION_IMPACT_MIN_MS2 = 9.0f;   // impact, lower edge
constexpr float MOTION_IMPACT_MAX_MS2 = 14.0f;  // impact, upper edge

// --- location ---
// Chennai Institute of Technology / Kundrathur, Chennai, Tamil Nadu, India.
constexpr double GPS_ORIGIN_LAT     = 12.9716;
constexpr double GPS_ORIGIN_LON     = 80.0434;

// ~0.00002 deg is roughly 2 m at this latitude: a believable per-sample
// creep for a vehicle crawling or a handset wandering, not a teleport.
constexpr double GPS_STEP_DEG       = 0.00002;
// Keep the simulated track inside ~250 m of the origin so it stays recognisably
// "at CIT" over a long demo instead of wandering into the next district.
constexpr double GPS_MAX_DRIFT_DEG  = 0.0025;

constexpr float  GPS_ALT_BASE_M     = 18.0f;   // Kundrathur sits ~15-20 m AMSL
constexpr float  GPS_ALT_JITTER_M   = 1.2f;
constexpr uint8_t GPS_SATS_MIN      = 6;
constexpr uint8_t GPS_SATS_MAX      = 10;

}  // namespace demo

// -----------------------------------------------------------------------------
//  Simulated temperature + humidity.
// -----------------------------------------------------------------------------
class DemoEnvironmentProvider : public IEnvironmentProvider {
public:
    bool begin() override;
    void read(SensorData& out) override;
    const char* name() const override { return "DHT11 (DEMO/SIMULATED)"; }
    bool isReady() const override { return _ready; }

private:
    bool  _ready = false;

    // Phase offset into the thermal cycle, randomised at begin() so two runs do
    // not open at exactly the same point in the story.
    float _phaseOffsetRad = 0.0f;

    // Slow-moving noise terms, carried between samples so the texture drifts
    // rather than being redrawn independently each time.
    float _tempNoiseC = 0.0f;
    float _humNoisePct = 0.0f;
};

// -----------------------------------------------------------------------------
//  Simulated accelerometer + gyroscope.
// -----------------------------------------------------------------------------
class DemoMotionProvider : public IMotionProvider {
public:
    bool begin() override;
    void read(SensorData& out) override;
    const char* name() const override { return "MPU6050 (DEMO/SIMULATED)"; }
    bool isReady() const override { return _ready; }

private:
    bool     _ready  = false;
    uint32_t _sample = 0;
};

// -----------------------------------------------------------------------------
//  Simulated GPS.
// -----------------------------------------------------------------------------
class DemoLocationProvider : public ILocationProvider {
public:
    bool begin() override;
    void read(SensorData& out) override;
    const char* name() const override { return "NEO-6M GPS (DEMO/SIMULATED)"; }
    bool isReady() const override { return _ready; }

private:
    bool    _ready = false;
    double  _lat   = demo::GPS_ORIGIN_LAT;
    double  _lon   = demo::GPS_ORIGIN_LON;
    uint8_t _sats  = 8;
    // Simulated heading, so the track walks as a path rather than jittering
    // about a point. Turns slowly, like a vehicle on a road.
    float   _headingRad = 0.0f;
};

// -----------------------------------------------------------------------------
//  Shared helpers, exposed for reuse by the storage simulator.
// -----------------------------------------------------------------------------
namespace demo {

// Uniform float in [lo, hi].
float randomFloat(float lo, float hi);

// Advance a bounded random walk one step and return the new value:
// jitter by up to +/-step, then pull gently back toward the band centre so the
// series stays inside [lo, hi] without ever hard-clamping visibly.
float walk(float current, float step, float lo, float hi);

// Seed the PRNG from whatever entropy a bare ESP32 has. Called by begin().
void seedOnce();

}  // namespace demo
