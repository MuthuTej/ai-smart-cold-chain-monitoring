// =============================================================================
//  storage/StorageManager.cpp
// =============================================================================
#include "storage/StorageManager.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// The ESP32 SD library wants an SPIClass it can own. Using an explicit instance
// (rather than the default `SPI`) documents which of the ESP32's SPI
// peripherals is in play and leaves the other free for future use.
static SPIClass g_sdSpi(VSPI);

StorageManager::StorageManager(bool simulate)
    : _simulate(simulate) {}

bool StorageManager::begin() {
    _lastFlushMs = millis();

    if (_simulate) {
        // Explicitly do nothing. No SPI.begin(), no SD.begin(), no card probe.
        _ready      = true;
        _cardSizeMB = 0;
        _cardType   = "simulated";
        return true;
    }

    g_sdSpi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

    if (!SD.begin(PIN_SD_CS, g_sdSpi, SD_SPI_FREQ_HZ)) {
        // Usual causes: no card inserted, card not FAT32, CS on the wrong pin,
        // or a 5 V-only breakout being fed 3.3 V.
        _ready = false;
        return false;
    }

    switch (SD.cardType()) {
        case CARD_MMC:  _cardType = "MMC";     break;
        case CARD_SD:   _cardType = "SDSC";    break;
        case CARD_SDHC: _cardType = "SDHC";    break;
        case CARD_NONE: _cardType = "none";    break;
        default:        _cardType = "unknown"; break;
    }

    if (SD.cardType() == CARD_NONE) {
        _ready = false;
        return false;
    }

    _cardSizeMB = SD.cardSize() / (1024ULL * 1024ULL);

    _ready = ensureHeader();
    return _ready;
}

bool StorageManager::ensureHeader() {
    // Only write the header when the file is genuinely new. Appending a header
    // to an existing log on every boot would corrupt the CSV for any consumer.
    if (SD.exists(CSV_LOG_PATH)) {
        return true;
    }

    File f = SD.open(CSV_LOG_PATH, FILE_WRITE);
    if (!f) {
        return false;
    }
    f.println(CSV_HEADER);
    f.close();
    return true;
}

void StorageManager::formatRow(const SensorData& s, char* buffer,
                              size_t bufferLen) {
    // Column order must match CSV_HEADER in config.h exactly.
    //
    // Precision notes:
    //  - temperature/humidity to 1 dp: that is all a DHT11 resolves.
    //  - acceleration/gyro to 3 dp: enough to see the noise floor.
    //  - lat/lon to 6 dp: ~0.11 m at the equator, finer than any consumer GPS.
    //    Fewer digits would quantise away the simulated movement entirely.
    snprintf(buffer, bufferLen,
             "%lu,%.1f,%.1f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
             "%.6f,%.6f,%.1f,%u,%s,%d",
             static_cast<unsigned long>(s.timestampMs),
             s.temperatureC,
             s.humidityPct,
             s.accelerationX, s.accelerationY, s.accelerationZ,
             s.gyroscopeX, s.gyroscopeY, s.gyroscopeZ,
             s.latitude, s.longitude,
             s.altitudeM,
             static_cast<unsigned>(s.satellites),
             toString(s.systemStatus),
             s.demoMode ? 1 : 0);
}

bool StorageManager::append(SensorData& sample) {
    char row[256];
    formatRow(sample, row, sizeof(row));

    // ---------------------------------------------------------------------
    //  Simulated path
    // ---------------------------------------------------------------------
    if (_simulate) {
        _records++;
        sample.recordsWritten = _records;
        // SIMULATED, never LOGGING. The distinction is the whole point: a demo
        // transcript must not read as evidence that a card was written.
        sample.storageStatus  = StorageStatus::SIMULATED;

        if (_echoRows) {
            Serial.print(F("[csv] "));
            Serial.println(row);
        }
        return true;
    }

    // ---------------------------------------------------------------------
    //  Real path
    // ---------------------------------------------------------------------
    if (!_ready) {
        sample.storageStatus  = StorageStatus::NO_CARD;
        sample.recordsWritten = _records;
        return false;
    }

    File f = SD.open(CSV_LOG_PATH, FILE_APPEND);
    if (!f) {
        sample.storageStatus  = StorageStatus::WRITE_FAIL;
        sample.recordsWritten = _records;
        return false;
    }

    const size_t written = f.println(row);
    f.close();

    if (written == 0) {
        // Card removed mid-write, or full.
        sample.storageStatus  = StorageStatus::WRITE_FAIL;
        sample.recordsWritten = _records;
        return false;
    }

    _records++;
    sample.recordsWritten = _records;
    sample.storageStatus  = StorageStatus::LOGGING;

    // Opening with FILE_APPEND and closing per row is already a flush point on
    // the ESP32 SD driver, which is why a row survives an unexpected power cut.
    // The timer below is kept for the same reason a real logger keeps one: if
    // this class is ever changed to hold the handle open between rows (much
    // faster, far fewer card writes), the flush policy is already here.
    if (millis() - _lastFlushMs >= STORAGE_FLUSH_INTERVAL_MS) {
        flush();
    }

    return true;
}

void StorageManager::flush() {
    _lastFlushMs = millis();
    // Nothing to do while each append opens and closes the file. Present as the
    // explicit hook for a pre-shutdown call and for the buffered variant above.
}
