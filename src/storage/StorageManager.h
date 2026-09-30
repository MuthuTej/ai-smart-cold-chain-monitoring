// =============================================================================
//  storage/StorageManager.h
//  CSV logging to a MicroSD card, with an honest simulated mode.
//
//  Wiring:  MOSI -> D23,  MISO -> D19,  SCK -> D18,  CS -> D13,  3.3V, GND
//  See docs/pinout.md and docs/data-format.md.
//
//  BEHAVIOUR IN DEMO_MODE
//  ----------------------
//  No card is mounted and no SPI traffic is generated. The row is formatted
//  exactly as it would be written -- same column order, same precision -- and
//  the record counter advances, but the bytes go nowhere (optionally echoed to
//  serial). Records produced this way report StorageStatus::SIMULATED, never
//  LOGGING, so a demo transcript can never be mistaken for proof that a card
//  was written.
// =============================================================================
#pragma once

#include <stddef.h>   // size_t, used by formatRow()
#include <stdint.h>

#include "config.h"
#include "models/SensorData.h"

class StorageManager {
public:
    // simulate == true  -> never touch the SPI bus or the card
    // simulate == false -> mount the card and append for real
    explicit StorageManager(bool simulate);

    // Mount the card (real mode) and make sure the CSV exists with a header
    // row. Returns false if the card could not be mounted; the object stays
    // safe to call and will report NO_CARD on every append.
    bool begin();

    // Append one row. Fills sample.storageStatus and sample.recordsWritten.
    bool append(SensorData& sample);

    // Push buffered bytes to the card. Called automatically by append() every
    // STORAGE_FLUSH_INTERVAL_MS; call it directly before a planned power-down.
    void flush();

    bool isReady() const { return _ready; }
    bool isSimulated() const { return _simulate; }
    uint32_t recordCount() const { return _records; }

    // Card facts for the boot banner. Zero in simulated mode.
    uint64_t cardSizeMB() const { return _cardSizeMB; }
    const char* cardTypeName() const { return _cardType; }

    // Format one sample as a CSV row (no trailing newline). Exposed so the
    // serial reporter and any future uplink can share one single definition of
    // the row format -- the format must never be written out in two places.
    static void formatRow(const SensorData& s, char* buffer, size_t bufferLen);

    // In simulated mode, echo each formatted row to Serial so the CSV format is
    // visible during a demo. Off by default to keep the report readable.
    void setEchoRows(bool echo) { _echoRows = echo; }

private:
    bool     _simulate;
    bool     _ready       = false;
    uint32_t _records     = 0;
    uint32_t _lastFlushMs = 0;
    bool     _echoRows    = false;

    uint64_t    _cardSizeMB = 0;
    const char* _cardType   = "none";

    bool ensureHeader();
};
