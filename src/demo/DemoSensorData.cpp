// =============================================================================
//  demo/DemoSensorData.cpp
// =============================================================================
#include "demo/DemoSensorData.h"

#include <Arduino.h>
#include <math.h>

namespace demo {

static bool s_seeded = false;

void seedOnce() {
    if (s_seeded) {
        return;
    }
    // esp_random() is the ESP32 hardware RNG. Seeding from it (mixed with the
    // boot-time microsecond counter) means two boards, or two runs of the same
    // board, do not replay an identical demo trace.
    randomSeed(static_cast<uint32_t>(esp_random()) ^ micros());
    s_seeded = true;
}

float randomFloat(float lo, float hi) {
    if (hi <= lo) {
        return lo;
    }
    // random() gives a long in [0, 10000); scale it into the requested range.
    const float unit = static_cast<float>(random(0, 10000)) / 10000.0f;
    return lo + unit * (hi - lo);
}

float walk(float current, float step, float lo, float hi) {
    // 1. Random increment, symmetric about zero.
    float next = current + randomFloat(-step, step);

    // 2. Mean reversion. Without this the walk is a true random walk and will
    //    eventually escape any band; with it the series hovers around the
    //    middle the way a real environment does, and never needs to be visibly
    //    clipped at the edges.
    //
    //    The coefficient is a trade-off. Too strong and the walk is pinned to
    //    the middle few tenths of the band, so temperature never reaches a
    //    threshold and the status engine is never seen to react. Too weak and
    //    the walk hugs the edges. 0.02 was chosen by running the simulator and
    //    checking that temperature explores most of its configured band while
    //    still moving only a couple of tenths per sample.
    const float centre = (lo + hi) * 0.5f;
    next += (centre - next) * 0.02f;

    // 3. Hard safety net, should only ever bite on an extreme outlier.
    if (next < lo) next = lo;
    if (next > hi) next = hi;

    return next;
}

}  // namespace demo

// -----------------------------------------------------------------------------
//  Environment
// -----------------------------------------------------------------------------
bool DemoEnvironmentProvider::begin() {
    demo::seedOnce();

    // Enter the thermal cycle at a random phase, so consecutive demos do not
    // replay an identical story from the identical starting point.
    _phaseOffsetRad = demo::randomFloat(0.0f, 2.0f * PI);
    _tempNoiseC     = 0.0f;
    _humNoisePct    = 0.0f;
    _ready          = true;
    return true;
}

void DemoEnvironmentProvider::read(SensorData& out) {
    // ---- (a) the slow thermal cycle --------------------------------------
    // Driven from the sample's own timestamp rather than a sample counter, so
    // the cycle keeps correct wall-clock pace even if the loop runs late.
    const float phase =
        (static_cast<float>(out.timestampMs % demo::ENV_TEMP_CYCLE_MS) /
         static_cast<float>(demo::ENV_TEMP_CYCLE_MS)) * 2.0f * PI;

    const float cycleC = demo::ENV_TEMP_CENTRE_C +
                         demo::ENV_TEMP_SWING_C *
                             sinf(phase + _phaseOffsetRad);

    // ---- (b) the fast sensor texture -------------------------------------
    // A bounded walk around zero, added to the cycle. Carrying it between
    // samples (instead of drawing a fresh independent value) is what makes
    // consecutive readings look related, the way a real sensor's do.
    _tempNoiseC = demo::walk(_tempNoiseC, demo::ENV_TEMP_NOISE_C * 0.6f,
                             -demo::ENV_TEMP_NOISE_C,
                              demo::ENV_TEMP_NOISE_C);

    float tempC = cycleC + _tempNoiseC;

    // Safety net: never emit a value outside the documented simulated band.
    if (tempC < demo::ENV_TEMP_MIN_C) tempC = demo::ENV_TEMP_MIN_C;
    if (tempC > demo::ENV_TEMP_MAX_C) tempC = demo::ENV_TEMP_MAX_C;

    // ---- humidity, coupled inversely to temperature ----------------------
    _humNoisePct = demo::walk(_humNoisePct, demo::ENV_HUM_NOISE_PCT * 0.6f,
                              -demo::ENV_HUM_NOISE_PCT,
                               demo::ENV_HUM_NOISE_PCT);

    float humidity = demo::ENV_HUM_CENTRE_PCT +
                     (tempC - demo::ENV_TEMP_CENTRE_C) * demo::ENV_HUM_PER_DEG_C +
                     _humNoisePct;

    if (humidity < demo::ENV_HUM_MIN_PCT) humidity = demo::ENV_HUM_MIN_PCT;
    if (humidity > demo::ENV_HUM_MAX_PCT) humidity = demo::ENV_HUM_MAX_PCT;

    // A DHT11 reports one decimal place. Round to match, so the demo output has
    // the same texture as the real part instead of betraying itself with six.
    out.temperatureC = roundf(tempC * 10.0f) / 10.0f;
    out.humidityPct  = roundf(humidity * 10.0f) / 10.0f;
    out.envSource    = DataSource::SIMULATED;
}

// -----------------------------------------------------------------------------
//  Motion
// -----------------------------------------------------------------------------
bool DemoMotionProvider::begin() {
    demo::seedOnce();
    _ready = true;
    return true;
}

void DemoMotionProvider::read(SensorData& out) {
    _sample++;

    // Baseline: the package is lying flat and still. Gravity appears as a
    // positive ~9.81 m/s^2 on Z (that is what an accelerometer at rest reads --
    // it measures the normal force holding it up, not the fall it is not
    // making). X and Y carry only noise.
    out.accelerationX = demo::randomFloat(-demo::MOTION_NOISE_XY_MS2,
                                           demo::MOTION_NOISE_XY_MS2);
    out.accelerationY = demo::randomFloat(-demo::MOTION_NOISE_XY_MS2,
                                           demo::MOTION_NOISE_XY_MS2);
    out.accelerationZ = GRAVITY_MS2 +
                        demo::randomFloat(-demo::MOTION_NOISE_Z_MS2,
                                           demo::MOTION_NOISE_Z_MS2);

    out.gyroscopeX = demo::randomFloat(-demo::MOTION_GYRO_NOISE,
                                        demo::MOTION_GYRO_NOISE);
    out.gyroscopeY = demo::randomFloat(-demo::MOTION_GYRO_NOISE,
                                        demo::MOTION_GYRO_NOISE);
    out.gyroscopeZ = demo::randomFloat(-demo::MOTION_GYRO_NOISE,
                                        demo::MOTION_GYRO_NOISE);

    // Occasional transport bump. The first few samples are left clean so the
    // demo opens on a settled baseline rather than an immediate alarm.
    if (_sample > 3 && random(0, demo::MOTION_BUMP_ODDS) == 0) {
        const bool isImpact = (random(0, demo::MOTION_IMPACT_ODDS) == 0);

        const float bump = isImpact
            ? demo::randomFloat(demo::MOTION_IMPACT_MIN_MS2,
                                demo::MOTION_IMPACT_MAX_MS2)
            : demo::randomFloat(demo::MOTION_BUMP_MIN_MS2,
                                demo::MOTION_BUMP_MAX_MS2);

        // Bump one axis and add a matching rotation, the way a real jolt does:
        // a package that gets knocked also turns a little. The sign is random
        // so a jolt can push the reading below 1 g as well as above it.
        const float sign = (random(0, 2) == 0) ? -1.0f : 1.0f;

        switch (random(0, 3)) {
            case 0:
                out.accelerationX += sign * bump;
                out.gyroscopeY    += sign * bump * 0.25f;
                break;
            case 1:
                out.accelerationY += sign * bump;
                out.gyroscopeX    += sign * bump * 0.25f;
                break;
            default:
                out.accelerationZ += sign * bump;
                out.gyroscopeZ    += sign * bump * 0.15f;
                break;
        }
    }

    // MPU6050 die temperature runs a few degrees above ambient.
    out.temperatureMpuC = demo::randomFloat(34.0f, 39.0f);
    out.motionSource    = DataSource::SIMULATED;
}

// -----------------------------------------------------------------------------
//  Location
// -----------------------------------------------------------------------------
bool DemoLocationProvider::begin() {
    demo::seedOnce();

    _lat        = demo::GPS_ORIGIN_LAT + demo::randomFloat(-0.0004f, 0.0004f);
    _lon        = demo::GPS_ORIGIN_LON + demo::randomFloat(-0.0004f, 0.0004f);
    _sats       = static_cast<uint8_t>(random(demo::GPS_SATS_MIN,
                                              demo::GPS_SATS_MAX + 1));
    _headingRad = demo::randomFloat(0.0f, 2.0f * PI);
    _ready      = true;
    return true;
}

void DemoLocationProvider::read(SensorData& out) {
    // Turn the heading slowly, then step along it. Walking a heading (rather
    // than jittering latitude and longitude independently) is what makes the
    // simulated track look like a vehicle following a road instead of a cloud
    // of unrelated points.
    _headingRad += demo::randomFloat(-0.25f, 0.25f);

    _lat += demo::GPS_STEP_DEG * cos(_headingRad);
    // Longitude degrees are shorter than latitude degrees away from the
    // equator, by cos(latitude). Scaling by it keeps the simulated speed
    // isotropic, so the track does not appear to move faster east-west.
    const double lonScale = 1.0 / cos(demo::GPS_ORIGIN_LAT * PI / 180.0);
    _lon += demo::GPS_STEP_DEG * sin(_headingRad) * lonScale;

    // Leash the track to the neighbourhood of the origin: if it strays too far,
    // reverse the heading so it wanders back instead of drifting away forever.
    const double dLat = _lat - demo::GPS_ORIGIN_LAT;
    const double dLon = _lon - demo::GPS_ORIGIN_LON;
    if (sqrt(dLat * dLat + dLon * dLon) > demo::GPS_MAX_DRIFT_DEG) {
        _headingRad += PI;
    }

    // Satellite count wobbles by at most one per sample, as it does in reality
    // when a bird rises or sets, rather than jumping 6 -> 10 -> 6.
    int satDelta = random(-1, 2);
    int sats     = static_cast<int>(_sats) + satDelta;
    if (sats < demo::GPS_SATS_MIN) sats = demo::GPS_SATS_MIN;
    if (sats > demo::GPS_SATS_MAX) sats = demo::GPS_SATS_MAX;
    _sats = static_cast<uint8_t>(sats);

    out.latitude    = _lat;
    out.longitude   = _lon;
    out.altitudeM   = demo::GPS_ALT_BASE_M +
                      demo::randomFloat(-demo::GPS_ALT_JITTER_M,
                                         demo::GPS_ALT_JITTER_M);
    // GPS_STEP_DEG per SAMPLE_INTERVAL_MS works out to a slow walking pace;
    // report it as such rather than inventing an unrelated speed.
    out.speedKmph   = demo::randomFloat(0.0f, 6.0f);
    out.satellites  = _sats;
    // 6+ satellites is a usable 3D fix, so the simulated fix is reported valid.
    out.gpsFixValid = (_sats >= 4);
    out.locationSource = DataSource::SIMULATED;
}
