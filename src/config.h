// =============================================================================
//  config.h
//  Single source of truth for pins, timing, and cold-chain thresholds.
//
//  AI-Powered Smart Cold-Chain Monitoring System
// =============================================================================
#pragma once

// -----------------------------------------------------------------------------
//  BUILD MODE
//
//  DEMO_MODE is normally supplied by platformio.ini (-D DEMO_MODE=1|0).
//  The fallback below keeps the sources compilable in a plain Arduino IDE
//  project, where build flags are not available.
//
//    DEMO_MODE == 1 -> simulated providers; no hardware needs to be attached.
//    DEMO_MODE == 0 -> real drivers talk to the physical sensors.
//
//  See docs/demo-mode.md.
// -----------------------------------------------------------------------------
#ifndef DEMO_MODE
#define DEMO_MODE 1
#endif

// When DEMO_MODE is on, the DHT11 is the one sensor that is cheap and forgiving
// enough to try for real and fall back gracefully. Set this to 1 to attempt a
// genuine DHT11 read first and only simulate if the read fails; set it to 0 to
// simulate temperature/humidity unconditionally.
//
// Every record carries per-field provenance, so the serial report and the CSV
// always state which of the two actually happened. Nothing is claimed to be
// real unless a real read succeeded.
#ifndef DEMO_TRY_REAL_DHT
#define DEMO_TRY_REAL_DHT 0
#endif

// -----------------------------------------------------------------------------
//  IDENTITY
// -----------------------------------------------------------------------------
#define FIRMWARE_NAME     "AI Smart Cold-Chain Monitoring"
#define FIRMWARE_VERSION  "0.1.0-prototype"
#define DEVICE_ID         "CCM-ESP32-01"

// -----------------------------------------------------------------------------
//  SERIAL
// -----------------------------------------------------------------------------
#define SERIAL_BAUD              115200
#define SERIAL_BOOT_WAIT_MS      1500   // grace period so the USB CDC attaches

// -----------------------------------------------------------------------------
//  PIN MAP  (ESP32 DevKit v1)
//
//  These GPIO numbers match the "D" labels silk-screened on a DevKit v1.
//  The authoritative wiring table lives in docs/pinout.md.
//  LEDs / buzzers are intentionally NOT part of this revision.
// -----------------------------------------------------------------------------

// DHT11 — temperature + humidity (1-wire, 3.3 V)
#define PIN_DHT_DATA        4        // DHT11 OUT -> D4

// MPU6050 — accelerometer + gyroscope (I2C, 3.3 V)
#define PIN_I2C_SDA         21       // MPU6050 SDA -> D21
#define PIN_I2C_SCL         22       // MPU6050 SCL -> D22
#define MPU6050_I2C_ADDR    0x68     // 0x69 when AD0 is tied high

// NEO-6M — GPS (UART2, 9600 baud)
// NOTE ON DIRECTION: PIN_GPS_RX is the ESP32 pin that RECEIVES, so it must be
// wired to the GPS module's TX. The project brief lists the module side
// (NEO-6M TX -> D16, NEO-6M RX -> D17), which is exactly the mapping below.
#define PIN_GPS_RX          16       // ESP32 RX2  <- NEO-6M TX
#define PIN_GPS_TX          17       // ESP32 TX2  -> NEO-6M RX
#define GPS_BAUD            9600
#define GPS_UART_PORT       2        // use UART2 (Serial2)

// MicroSD — SPI, 3.3 V
#define PIN_SD_MOSI         23       // SD MOSI -> D23
#define PIN_SD_MISO         19       // SD MISO -> D19
#define PIN_SD_SCK          18       // SD SCK  -> D18
#define PIN_SD_CS           13       // SD CS   -> D13
#define SD_SPI_FREQ_HZ      4000000  // 4 MHz — conservative, tolerant of jumpers

// -----------------------------------------------------------------------------
//  TIMING
// -----------------------------------------------------------------------------
// One full acquire -> classify -> log -> report cycle.
// The DHT11 datasheet allows roughly one conversion per second, so do not drop
// this below ~2000 ms if the real DHT11 is in use.
#define SAMPLE_INTERVAL_MS       2000

// Push the CSV file handle to the card at least this often. Between flushes the
// data sits in the SD driver's buffer and would be lost on a sudden power cut.
#define STORAGE_FLUSH_INTERVAL_MS 10000

// How long a real DHT11 read may fail before the field is marked STALE.
#define DHT_FAIL_TOLERANCE       3       // consecutive failures

// How long the GPS may go without a valid fix before location is marked STALE.
#define GPS_FIX_TIMEOUT_MS       30000

// -----------------------------------------------------------------------------
//  COLD-CHAIN THRESHOLDS
//
//  *** PROTOTYPE VALUES ONLY ***
//
//  These numbers were chosen so that the demo produces a readable mix of
//  NORMAL / WARNING / CRITICAL states on a desk at ambient Chennai temperature.
//  They are NOT derived from, and must NOT be confused with, any pharmaceutical,
//  food-safety, WHO, or industrial cold-chain specification. They are not
//  medically or industrially certified. Nothing here is fit for transporting
//  real perishable or medical cargo.
//
//  A real deployment would replace this block with the thresholds mandated by
//  the specific commodity being shipped.
//
//  Band layout for temperature:
//
//     |<-CRITICAL->|<-WARN->|<---- NORMAL ---->|<-WARN->|<-CRITICAL->|
//                 4.0      8.0               33.0     34.5
//                 ^        ^                  ^        ^
//        CRIT_LOW |  WARN_LOW         WARN_HIGH   CRIT_HIGH
//
//  WHY THE UPPER BOUND IS 33 °C AND NOT 8 °C
//  -----------------------------------------
//  A genuine vaccine cold chain is 2-8 °C. This prototype is demonstrated on a
//  desk at ambient Chennai temperature, and the simulator walks temperature
//  through 28-35 °C accordingly. Applying a real 2-8 °C band to that input
//  would report CRITICAL on every single sample, which demonstrates nothing:
//  a status engine that never changes state cannot be seen to work.
//
//  So the upper bounds are placed *inside* the simulated band, positioned so a
//  demo run produces a readable mix -- mostly NORMAL, WARNING when the walk
//  drifts high, CRITICAL at the top of its range. The lower bounds are kept at
//  plausible chilled-cargo values; they are unreachable in demo mode and exist
//  to show the two-sided logic.
//
//  Swapping these four numbers for the real ones is the entire change needed to
//  point this at actual chilled cargo. docs/demo-mode.md covers the trade-off.
// -----------------------------------------------------------------------------
#define TEMP_CRITICAL_LOW_C      4.0f
#define TEMP_WARNING_LOW_C       8.0f
#define TEMP_WARNING_HIGH_C     33.0f
#define TEMP_CRITICAL_HIGH_C    34.5f

// Humidity: the simulator walks 45-70 %, so the warning high bound is reachable
// near the top of that walk while the critical bound is not. The low bound
// guards against over-dry storage and does not fire in demo mode.
#define HUMIDITY_WARNING_LOW_PCT    35.0f
#define HUMIDITY_WARNING_HIGH_PCT   68.0f
#define HUMIDITY_CRITICAL_HIGH_PCT  85.0f

// Motion: a transport shock is a spike in total acceleration magnitude away
// from 1 g. Stationary noise on an MPU6050 sitting on a table is well under
// 0.5 m/s^2, so these are comfortably above the noise floor.
#define GRAVITY_MS2                 9.80665f
#define SHOCK_WARNING_MS2           3.0f   // ||a| - g| above this = rough handling
#define SHOCK_CRITICAL_MS2          8.0f   // above this = drop / impact event

// Gyroscope: sustained rotation suggests the package has been tipped or rolled.
#define TILT_WARNING_RAD_S          1.5f
#define TILT_CRITICAL_RAD_S         4.0f

// -----------------------------------------------------------------------------
//  ON-DEVICE ANALYTICS
//
//  This is a rule-based + exponentially-weighted-moving-average heuristic.
//  There is NO trained machine-learning model on the ESP32. See the
//  "What the AI actually is" section of README.md before describing this
//  as machine learning.
// -----------------------------------------------------------------------------
// EWMA smoothing factor for the temperature value itself. Lower = smoother.
#define EWMA_ALPHA                  0.20f

// Separate, much slower smoothing for the RATE of change.
//
// Differentiating a signal amplifies its noise, so the rate estimate needs far
// heavier smoothing than the value does. At a 2 s sample interval, a single
// 0.25 °C sensor wobble is an instantaneous 7.5 °C/min -- so a lightly smoothed
// rate would spend its life above any useful threshold and the "predictive"
// warning would fire constantly on noise. This alpha makes the rate reflect
// drift sustained over roughly a minute instead.
#define EWMA_TREND_ALPHA            0.05f

// A sustained drift of this many °C per minute is flagged as an excursion risk
// even while the absolute reading is still inside the NORMAL band. This is the
// "predictive" part of the status engine: it warns before the limit is crossed.
#define TREND_WARNING_C_PER_MIN     2.5f

// Number of consecutive out-of-band samples required before the status engine
// escalates. Debouncing stops a single noisy DHT11 reading from raising an
// alarm on its own.
//
// Impact and rough-handling verdicts are EXEMPT from this, and must be: a drop
// is over in milliseconds and will only ever appear in one sample, so requiring
// two consecutive samples would mean impacts were never reported at all. See
// StatusEngine::classify().
#define STATUS_DEBOUNCE_SAMPLES     2

// -----------------------------------------------------------------------------
//  STORAGE
// -----------------------------------------------------------------------------
#define CSV_LOG_PATH         "/coldchain.csv"
#define CSV_HEADER \
    "timestamp,temperature,humidity,accel_x,accel_y,accel_z," \
    "gyro_x,gyro_y,gyro_z,latitude,longitude,altitude,satellites," \
    "system_status,demo_mode"
