# Data Format

## The CSV record

One row is appended per sample. The header is written once, when the file is
created.

```csv
timestamp,temperature,humidity,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,latitude,longitude,altitude,satellites,system_status,demo_mode
```

Real rows, captured from a demo run:

```csv
50,30.7,56.7,0.105,0.098,9.795,0.019,-0.003,-0.012,12.971221,80.043305,17.2,6,NORMAL,1
2050,30.6,57.0,-0.066,0.098,9.850,0.027,-0.024,-0.011,12.971239,80.043316,19.0,6,NORMAL,1
4050,30.4,57.7,-0.043,0.095,9.726,0.025,0.009,0.023,12.971258,80.043322,19.0,6,NORMAL,1
286050,34.6,48.0,0.014,0.157,9.762,-0.013,-0.003,0.018,12.973453,80.042931,18.4,6,CRITICAL,1
```

- File path: `/coldchain.csv` (`CSV_LOG_PATH` in `config.h`)
- Line ending: `\r\n` (Arduino `println`)
- No quoting or escaping — every field is numeric or a fixed keyword
- The header and the writer are defined in **one** place each (`CSV_HEADER` in
  [`config.h`](../src/config.h) and `StorageManager::formatRow()`), so the two
  cannot drift apart

## Field reference

| # | Column | Type | Unit | Precision | Source |
|---|---|---|---|---|---|
| 1 | `timestamp` | uint32 | ms | integer | `millis()` |
| 2 | `temperature` | float | °C | 1 dp | DHT11 |
| 3 | `humidity` | float | % RH | 1 dp | DHT11 |
| 4 | `accel_x` | float | m/s² | 3 dp | MPU6050 |
| 5 | `accel_y` | float | m/s² | 3 dp | MPU6050 |
| 6 | `accel_z` | float | m/s² | 3 dp | MPU6050 |
| 7 | `gyro_x` | float | rad/s | 3 dp | MPU6050 |
| 8 | `gyro_y` | float | rad/s | 3 dp | MPU6050 |
| 9 | `gyro_z` | float | rad/s | 3 dp | MPU6050 |
| 10 | `latitude` | double | ° | 6 dp | NEO-6M |
| 11 | `longitude` | double | ° | 6 dp | NEO-6M |
| 12 | `altitude` | float | m | 1 dp | NEO-6M |
| 13 | `satellites` | uint8 | count | integer | NEO-6M |
| 14 | `system_status` | enum | — | keyword | StatusEngine |
| 15 | `demo_mode` | 0/1 | — | integer | build flag |

### 1. `timestamp` — milliseconds since boot

**This is uptime, not wall-clock time.** There is no RTC and no NTP in this
revision, so the firmware genuinely does not know the date. Recording uptime is
the honest option; inventing an epoch would not be.

Practical consequences:

- Rows are ordered and their *intervals* are accurate, so any rate, duration or
  trend computed from them is valid.
- Absolute time must come from elsewhere: note the departure time, or have an
  ingest layer stamp true UTC on arrival.
- The counter **wraps after ~49.7 days** of continuous uptime. A run that long
  will show the timestamp restart from near zero.
- Each reboot restarts the counter, so one file can contain several runs whose
  timestamps overlap. A rising sequence that suddenly drops marks a restart.

### 2–3. `temperature`, `humidity`

One decimal place, because that is all a DHT11 resolves — reporting more digits
would imply precision the sensor does not have. See
[`hardware.md`](hardware.md#2-dht11--temperature-and-humidity) for its ±2 °C
accuracy limit.

A row may carry a temperature that was re-served after a failed read (`STALE`).
**The CSV does not record that**, which is a known limitation of this format —
see [below](#known-limitations-of-this-format).

### 4–6. `accel_x/y/z` — acceleration in m/s²

Already converted to SI by the Adafruit Unified Sensor layer, not raw counts.

**At rest, lying flat, `accel_z` reads about +9.81, not zero.** An accelerometer
measures the normal force holding it up. A reading near zero on all three axes
means free fall, not stillness.

The derived shock figure is `| |a| − 9.81 |` — the departure from gravity in
either direction. It is computed in the firmware but, like the trend, is **not a
CSV column**; it is recoverable from columns 4–6:

```python
shock = abs((ax**2 + ay**2 + az**2) ** 0.5 - 9.80665)
```

### 7–9. `gyro_x/y/z` — angular velocity in rad/s

Radians per second, per the Adafruit Unified Sensor convention. Multiply by
57.2958 for degrees per second.

Near zero when stationary. Sustained non-zero values mean the package is being
rotated, tipped or rolled.

### 10–11. `latitude`, `longitude` — decimal degrees, WGS-84

Six decimal places, roughly 0.11 m at the equator — finer than any consumer GPS
is accurate to, but necessary: at 4 dp the simulated movement (and real slow
movement) would be quantised away entirely.

**`0.000000,0.000000` means no fix, not the Gulf of Guinea.** Filter these out
before mapping:

```python
df = df[(df.latitude != 0) | (df.longitude != 0)]
```

Whether a fix was valid is not in the CSV — see
[limitations](#known-limitations-of-this-format). Use `satellites >= 4` as the
practical proxy.

### 12. `altitude` — metres above mean sea level

GPS altitude is substantially less accurate than its horizontal position —
commonly ±10 m or worse. Treat it as indicative only.

### 13. `satellites` — satellites used in the fix

| Count | Meaning |
|---|---|
| 0 | No signal — indoors, or antenna disconnected |
| 1–3 | Tracking, but cannot compute a position |
| 4–5 | Minimum 3D fix; position may wander |
| 6–9 | Good fix |
| 10+ | Strong fix, clear sky |

### 14. `system_status` — the cold-chain verdict

One of `NORMAL`, `WARNING`, `CRITICAL`, `UNKNOWN`. Produced by
[`StatusEngine`](../src/status/StatusEngine.cpp) from the thresholds in
`config.h`.

> **These thresholds are prototype values.** They were chosen so a desk demo
> produces a readable mix of states. They are **not** medically or industrially
> certified and are not derived from any cold-chain standard. See
> [demo-mode.md](demo-mode.md#about-the-thresholds).

The status is **debounced**: a sustained condition must persist for
`STATUS_DEBOUNCE_SAMPLES` samples before it escalates. Impacts are exempt,
because a drop appears in exactly one sample. So a `CRITICAL` row caused by an
impact may be a single row surrounded by `NORMAL` — that is correct behaviour,
not a glitch.

`UNKNOWN` means there was not enough valid data to judge the sample, not that
everything was fine.

### 15. `demo_mode` — was this a simulated build

| Value | Meaning |
|---|---|
| `1` | Compiled with `DEMO_MODE=1`. **Treat as synthetic.** |
| `0` | Compiled with `DEMO_MODE=0` against real drivers. |

**A `1` here means the row is not evidence of anything physical.** Filter before
analysis:

```python
real = df[df.demo_mode == 0]
```

Note the asymmetry: `demo_mode == 1` does not guarantee *every* field was
simulated (`DEMO_TRY_REAL_DHT` produces mixed rows), but it does guarantee the row
must not be treated as measurement.

## Known limitations of this format

The CSV is a deliberately flat, 15-column format matching the project
specification. The firmware tracks more than it writes:

| Tracked in `SensorData` | In the CSV? |
|---|---|
| Per-field provenance (`envSource`, `motionSource`, `locationSource`) | ❌ |
| `gpsFixValid` | ❌ (proxy: `satellites >= 4`) |
| `shockMagnitudeMs2` | ❌ (recomputable from 4–6) |
| `temperatureTrendC` | ❌ (recomputable from 1–2) |
| `storageStatus`, `recordsWritten` | ❌ |
| `speedKmph`, `temperatureMpuC`, `sequence` | ❌ |
| `statusReason` | ❌ (shown on serial only) |

The most significant of these is **provenance**. A row cannot tell you whether
its temperature was freshly measured or re-served after a failed read. The serial
dashboard shows this per block; the CSV does not.

If you extend the format, the honest additions are `env_source`, `motion_source`,
`location_source`, `gps_fix_valid` and `status_reason`. Append them at the **end**
so existing parsers keep working, and update `CSV_HEADER` and `formatRow()`
together.

## Reading the log

```python
import pandas as pd

cols = ["timestamp","temperature","humidity","accel_x","accel_y","accel_z",
        "gyro_x","gyro_y","gyro_z","latitude","longitude","altitude",
        "satellites","system_status","demo_mode"]

df = pd.read_csv("coldchain.csv", names=cols, header=0)

# uptime -> seconds, and flag reboots (timestamp goes backwards)
df["uptime_s"] = df.timestamp / 1000.0
df["reboot"]   = df.timestamp.diff() < 0

# recover the derived figures the CSV omits
df["shock"] = ((df.accel_x**2 + df.accel_y**2 + df.accel_z**2) ** 0.5
               - 9.80665).abs()

# only trust rows from a real-hardware build, with a usable fix
real = df[(df.demo_mode == 0) & (df.satellites >= 4)]

print(df.system_status.value_counts())
print(f"worst shock: {df.shock.max():.2f} m/s^2")
```

## Consuming this from a backend

`SensorData` is shaped to be serialised directly. A JSON mapping for a future
uplink would look like:

```json
{
  "device_id": "CCM-ESP32-01",
  "sequence": 143,
  "uptime_ms": 286050,
  "environment": { "temperature_c": 34.6, "humidity_pct": 48.0, "source": "SIMULATED" },
  "motion": {
    "acceleration_ms2": { "x": 0.014, "y": 0.157, "z": 9.762 },
    "gyroscope_rads":   { "x": -0.013, "y": -0.003, "z": 0.018 },
    "shock_excess_ms2": 0.09,
    "source": "SIMULATED"
  },
  "location": {
    "latitude": 12.973453, "longitude": 80.042931,
    "altitude_m": 18.4, "satellites": 6, "fix_valid": true,
    "source": "SIMULATED"
  },
  "status": {
    "value": "CRITICAL",
    "reason": "temperature above critical high threshold",
    "temperature_trend_c_per_min": 2.15
  },
  "storage": { "status": "SIMULATED", "records": 143 },
  "demo_mode": true
}
```

Note that this carries the provenance and reason fields the CSV drops. Any ingest
layer should also stamp its own receive timestamp, since the device only knows
uptime.
