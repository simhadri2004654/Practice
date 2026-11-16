
#pragma once

#include <Arduino.h>
#include <stdint.h>

// Fingerprint scan result
enum FingerprintScanStatus : uint8_t
{
    FINGERPRINT_SCAN_IDLE,
    FINGERPRINT_SCAN_MATCH,
    FINGERPRINT_SCAN_UNKNOWN,
    FINGERPRINT_SCAN_ERROR
};

// Initialize R503 sensor
bool fingerprintBegin();

// Register new fingerprint
bool fingerprintEnroll(uint16_t slotId);

// Identify fingerprint for attendance
bool fingerprintIdentify(uint16_t &matchedSlotId);

// Get most recent scan result
FingerprintScanStatus fingerprintGetLastScanStatus();

// Delete fingerprint from R503
bool fingerprintDelete(uint16_t slotId);

// Set blue breathing idle LED
void fingerprintSetIdleLight();
