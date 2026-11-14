
#pragma once

#include <Arduino.h>
#include <stdint.h>

bool fingerprintBegin();

bool fingerprintEnroll(uint16_t slotId);

bool fingerprintIdentify(uint16_t &matchedSlotId);

bool fingerprintDelete(uint16_t slotId);

void fingerprintSetIdleLight();
