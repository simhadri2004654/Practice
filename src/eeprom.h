#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "config.h"

struct EmployeeRecord
{
    uint8_t active;
    uint8_t reserved;
    uint16_t fingerprintId;

    char employeeId[16];
    char name[24];
};

struct AttendanceRecord
{
    uint8_t active;
    uint8_t employeeIndex;
    uint16_t reserved;

    uint32_t dayKey;
    uint32_t inEpoch;
    uint32_t outEpoch;
};

static_assert(sizeof(EmployeeRecord) == 44,
              "EmployeeRecord layout changed");

static_assert(sizeof(AttendanceRecord) == 16,
              "AttendanceRecord layout changed");

// EEPROM initialization
bool storageBegin();

// Employee operations
bool storageReadEmployee(uint16_t index, EmployeeRecord &record);

bool storageWriteEmployee(
    uint16_t index,
    const EmployeeRecord &record
);

bool storageDeleteEmployee(uint16_t index);

int16_t storageFindEmployeeByFingerprint(uint16_t fingerprintId);

int16_t storageFindEmployeeById(const char *employeeId);

// Attendance operations
bool storageReadAttendance(
    uint16_t index,
    AttendanceRecord &record
);

bool storageWriteAttendance(
    uint16_t index,
    const AttendanceRecord &record
);

uint16_t storageNextAttendance();

uint16_t storageAttendanceCount();

bool storageAdvanceAttendance();