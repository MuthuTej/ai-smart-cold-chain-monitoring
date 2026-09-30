# Hardware Components

> **Status of the hardware in this repository:** the drivers here are written
> against the documented pin map and the standard Arduino libraries, but this is
> a proof-of-work prototype. The code has **not** been validated against a
> physically assembled unit. Nothing in this repository should be read as a claim
> that a working device was built and tested.

## Bill of materials

| # | Component | Interface | Supply | Role |
|---|---|---|---|---|
| 1 | ESP32 DevKit v1 | — | 5 V USB / 3.3 V | Controller, all processing |
| 2 | DHT11 | 1-Wire | 3.3 V | Temperature + humidity |
| 3 | MPU6050 | I²C | 3.3 V | Accelerometer + gyroscope |
| 4 | NEO-6M | UART 9600 | 3.3 V | GPS position |
| 5 | MicroSD module | SPI | 3.3 V | CSV data logging |

Wiring for all five is in [`pinout.md`](pinout.md).

---

## 1. ESP32 DevKit v1

A dual-core Xtensa LX6 microcontroller at 240 MHz with 520 KB SRAM, 4 MB flash,
and integrated Wi-Fi and Bluetooth.

**Why this part.** Three of the four peripherals need a different bus — 1-Wire,
I²C, UART, SPI — and the ESP32 has all of them simultaneously with pins that can
be remapped almost freely. An Arduino Uno would run out of hardware UARTs before
the GPS was connected. The onboard Wi-Fi also means the "future cloud layer" in
[`architecture.md`](architecture.md) needs no additional hardware.

**What it does here.** Runs the whole pipeline: samples the sensors, derives the
trend and shock figures, classifies the cold-chain status, appends to the card,
and prints the serial dashboard.

**Constraints worth knowing.**

- Operates at **3.3 V logic**. Every sensor here is wired to 3.3 V accordingly.
- The 3.3 V regulator on a DevKit v1 supplies a few hundred mA. That is enough
  for these four modules, but a GPS during acquisition draws its peak here — if
  the board browns out, power the GPS separately.
- Wi-Fi is **not** used in this revision, so the second core and the radio sit
  idle. That is headroom, not waste.

---

## 2. DHT11 — Temperature and Humidity

A combined capacitive humidity and thermistor sensor with a digital 1-Wire
interface.

| Parameter | Value |
|---|---|
| Temperature range | 0 – 50 °C |
| Temperature accuracy | ±2 °C |
| Humidity range | 20 – 90 % RH |
| Humidity accuracy | ±5 % RH |
| Resolution | 1 °C / 1 % RH |
| Max sample rate | ~1 Hz |

**Why this part.** It is the cheapest and most widely available combined
temperature/humidity sensor, which makes it the standard choice for a first
prototype.

**Why it is the weakest link in this design — stated plainly.** A real cold chain
is specified in fractions of a degree. The DHT11 resolves **1 °C** and is
accurate to **±2 °C**. It physically cannot verify a 2–8 °C pharmaceutical band;
its error bar is wider than the band itself. It also does not read below 0 °C, so
frozen cargo is entirely out of reach.

For a serious build the direct upgrades are:

- **DHT22 / AM2302** — same wiring, ±0.5 °C, works to −40 °C.
- **SHT31 / SHT41** — I²C, ±0.2 °C, the right answer for real work.
- **DS18B20** — ±0.5 °C, waterproof probe versions available.

**Behaviour the driver has to handle.** The DHT11 returns `NaN` fairly often — it
is a slow part and a single dropped bit invalidates the whole 40-bit frame. The
driver ([`DHT11Sensor.cpp`](../src/sensors/DHT11Sensor.cpp)) re-serves the last
good reading tagged `STALE` rather than publishing `NaN`, and gives up after
`DHT_FAIL_TOLERANCE` consecutive failures. It also enforces the ~1 Hz limit via
`SAMPLE_INTERVAL_MS`; polling faster returns cached values or errors.

---

## 3. MPU6050 — Accelerometer and Gyroscope

A 6-axis inertial measurement unit: a 3-axis accelerometer and 3-axis gyroscope
on one die, with an onboard Digital Motion Processor, over I²C.

| Parameter | Value |
|---|---|
| Accelerometer ranges | ±2 / ±4 / ±8 / ±16 g |
| Gyroscope ranges | ±250 / ±500 / ±1000 / ±2000 °/s |
| ADC resolution | 16-bit |
| I²C address | 0x68 (0x69 with AD0 high) |

**Why this part.** Temperature alone does not describe a shipment's treatment.
Cargo can arrive at the right temperature and still be ruined by being dropped.
The MPU6050 adds the handling dimension for about a dollar.

**Configuration chosen, and why** — see
[`MPU6050Sensor.cpp`](../src/sensors/MPU6050Sensor.cpp):

- **±8 g accelerometer.** A package drop easily exceeds 4 g, and clipping the
  impact would defeat the purpose of logging shocks. 8 g leaves headroom without
  throwing away resolution around 1 g.
- **±500 °/s gyroscope.** Covers a parcel being tipped or rolled by hand.
- **21 Hz digital low-pass filter.** Cuts road and motor vibration far above what
  a 0.5 Hz sampling loop could represent anyway, so each sample is a cleaner
  estimate rather than an alias of engine buzz.

**How its output is interpreted.** An accelerometer at rest reads **1 g, not
zero** — it measures the normal force supporting it. So the useful quantity is
the *departure* from gravity in either direction:

```
shock = | |a| − 9.81 |
```

A jolt pushes the magnitude above 9.81; free fall pushes it toward zero. Taking
the absolute difference catches both. This is computed in
[`TrendAnalyzer.cpp`](../src/analytics/TrendAnalyzer.cpp).

**Note on its temperature output.** The MPU6050 reports its own **die**
temperature, which runs several degrees above ambient because it shares silicon
with the sensor. It is logged for diagnostics only; the cold-chain verdict always
uses the DHT11 value.

---

## 4. NEO-6M — GPS

A u-blox 50-channel GNSS receiver, typically sold on a breakout with a patch
antenna and a backup battery, emitting NMEA sentences over UART.

| Parameter | Value |
|---|---|
| Horizontal accuracy | ~2.5 m CEP |
| Cold start | ~27 s (typical, clear sky) |
| Hot start | ~1 s |
| Update rate | 1 Hz default |
| Default baud | 9600, 8N1 |

**Why this part.** A temperature excursion is far more actionable when you know
*where* it happened — which leg of the route, which depot, which vehicle.

**Realistic expectations, stated up front.**

- **It will not get a fix indoors.** GPS needs sky view. Inside a building the
  module will stream NMEA all day and never resolve a position. This is normal.
- **First fix takes time.** 30 seconds to several minutes from cold, longer
  without the backup battery charged.
- Because of both, `begin()` deliberately **does not wait for a fix**. It waits
  1.5 s only to confirm bytes are arriving, then returns. Blocking on a fix would
  stall the entire system on something outside its control.

**What "OK" means at boot.** The banner reports the GPS as OK when
`charsProcessed() > 0` — that is, the module is powered and talking. It says
nothing about whether a fix exists. The boot line says so explicitly
(`streaming; fix may take minutes`) to avoid a false expectation when the first
records show no position.

**Stale-fix handling.** Once a fix is acquired it is held, but if no new valid
position arrives within `GPS_FIX_TIMEOUT_MS` (30 s) the location is re-served
tagged `STALE` rather than being presented as current. A vehicle entering a
tunnel should not silently appear parked at the tunnel mouth.

---

## 5. MicroSD Module — Storage

A MicroSD socket on a breakout with a 3.3 V level shifter, over SPI.

**Why local storage at all.** A shipment spends most of its life without
connectivity. Logging locally and reconciling later is the norm for cold-chain
loggers, not a limitation. Local storage is the primary record; a network uplink,
when added, is a convenience on top of it.

**Format requirement.** The card must be **FAT32**. The ESP32 SD library does not
support exFAT, and cards larger than 32 GB often ship exFAT-formatted — reformat
before use.

**Why 4 MHz.** `SD_SPI_FREQ_HZ` is set well below the bus's capability because
jumper wires on a breadboard carry enough stray capacitance to make higher
clocks intermittently unreliable. Raise it once the wiring is soldered.

**Write strategy, and its trade-off.** Each row opens the file with
`FILE_APPEND`, writes, and closes. That is slower and produces more card writes
than holding the handle open — but it means a completed row survives an
unexpected power cut, which for a data logger is the property that matters.
`flush()` exists as the explicit hook for a buffered variant and for a planned
shutdown.

---

## Known hardware limitations of this build

| Limitation | Consequence | Fix |
|---|---|---|
| DHT11 ±2 °C, 1 °C resolution | Cannot verify any real cold-chain band | SHT31 or DS18B20 |
| DHT11 min 0 °C | Frozen cargo unmeasurable | DHT22 or DS18B20 |
| No RTC | Timestamps are uptime, not wall-clock | DS3231, or NTP over Wi-Fi |
| No battery / power management | Mains or USB powered only | LiPo + charger + deep sleep |
| GPS needs sky view | No indoor position | Expected; not fixable |
| Single DHT11 | One point measurement | Multiple probes per container |
| No enclosure | Not transport-ready | IP-rated housing |
| Card not removable in software | No safe-eject | Add a detect pin + unmount |
