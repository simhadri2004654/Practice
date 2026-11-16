
#pragma once

#include <Arduino.h>
#include <stdint.h>


// Attendance processing result
enum AttendanceResult : uint8_t
{
    ATTENDANCE_RESULT_NONE,
    ATTENDANCE_RESULT_IN_RECORDED,
    ATTENDANCE_RESULT_OUT_RECORDED,
    ATTENDANCE_RESULT_UNKNOWN_EMPLOYEE,
    ATTENDANCE_RESULT_ALREADY_COMPLETED,
    ATTENDANCE_RESULT_ERROR
};


// Process employee fingerprint attendance
void attendanceHandleFingerprint(uint16_t fingerprintSlot);


// Get result of latest attendance operation
AttendanceResult attendanceGetLastResult();
