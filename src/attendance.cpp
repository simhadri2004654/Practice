
#include "attendance.h"
#include "eeprom.h"
#include "config.h"

#include <time.h>


// Latest attendance result
static AttendanceResult lastAttendanceResult =
    ATTENDANCE_RESULT_NONE;


// ==================================================
// GENERATE DATE KEY: YYYYMMDD
// ==================================================

static uint32_t makeDayKey(const struct tm &localTime)
{
    return
        (uint32_t)(localTime.tm_year + 1900) * 10000UL +
        (uint32_t)(localTime.tm_mon + 1) * 100UL +
        (uint32_t)localTime.tm_mday;
}


// ==================================================
// PRINT LOCAL DATE AND TIME
// ==================================================

static void printLocalTime(
    const char *label,
    uint32_t epoch
)
{
    time_t value = (time_t)epoch;

    struct tm localTime;

    char text[32];

    if (localtime_r(&value, &localTime) == nullptr)
    {
        Serial.println("Time conversion error.");
        return;
    }

    strftime(
        text,
        sizeof(text),
        "%d-%m-%Y %I:%M:%S %p",
        &localTime
    );

    Serial.print(label);
    Serial.println(text);
}


// ==================================================
// PRINT EMPLOYEE INFORMATION
// ==================================================

static void printEmployee(const EmployeeRecord &employee)
{
    Serial.print("Employee Name: ");
    Serial.println(employee.name);

    Serial.print("Employee ID: ");
    Serial.println(employee.employeeId);
}


// ==================================================
// GET LAST ATTENDANCE RESULT
// ==================================================

AttendanceResult attendanceGetLastResult()
{
    return lastAttendanceResult;
}


// ==================================================
// PROCESS EMPLOYEE FINGERPRINT ATTENDANCE
// ==================================================

void attendanceHandleFingerprint(uint16_t fingerprintSlot)
{
    // Assume an error until a result is confirmed
    lastAttendanceResult = ATTENDANCE_RESULT_ERROR;

    Serial.println();
    Serial.println("================================");
    Serial.println("FINGERPRINT ATTENDANCE");
    Serial.println("================================");

    Serial.print("Fingerprint Slot: ");
    Serial.println(fingerprintSlot);


    // ==============================================
    // VALIDATE FINGERPRINT SLOT
    // ==============================================

    if (fingerprintSlot == 0)
    {
        Serial.println("Invalid fingerprint slot.");
        return;
    }


    // ==============================================
    // FIND EMPLOYEE USING FINGERPRINT SLOT
    // ==============================================

    const int16_t employeeIndex =
        storageFindEmployeeByFingerprint(fingerprintSlot);

    if (employeeIndex < 0)
    {
        Serial.println(
            "Unknown fingerprint. Employee is not registered."
        );

        lastAttendanceResult =
            ATTENDANCE_RESULT_UNKNOWN_EMPLOYEE;

        return;
    }


    // ==============================================
    // READ EMPLOYEE INFORMATION
    // ==============================================

    EmployeeRecord employee = {};

    if (!storageReadEmployee(employeeIndex, employee))
    {
        Serial.println("EEPROM employee read error.");
        return;
    }

    if (!employee.active)
    {
        Serial.println("Employee is inactive.");

        lastAttendanceResult =
            ATTENDANCE_RESULT_UNKNOWN_EMPLOYEE;

        return;
    }

    printEmployee(employee);


    // ==============================================
    // GET CURRENT TIME
    // ==============================================

    const time_t now = time(nullptr);

    if (now < 1700000000)
    {
        Serial.println(
            "Clock not synchronized. Check Wi-Fi and NTP."
        );

        return;
    }

    struct tm localTime;

    if (localtime_r(&now, &localTime) == nullptr)
    {
        Serial.println("Local time conversion failed.");
        return;
    }

    const uint32_t todayKey =
        makeDayKey(localTime);

    const uint32_t nowEpoch =
        (uint32_t)now;

    Serial.print("Today's Date Key: ");
    Serial.println(todayKey);


    // ==============================================
    // GET ATTENDANCE LOG INFORMATION
    // ==============================================

    const uint16_t recordCount =
        storageAttendanceCount();

    const uint16_t nextIndex =
        storageNextAttendance();

    // Validate circular-buffer information
    if (
        recordCount > MAX_ATTENDANCE_RECORDS ||
        nextIndex >= MAX_ATTENDANCE_RECORDS
    )
    {
        Serial.println("Invalid EEPROM attendance pointer.");
        return;
    }

    const uint16_t oldestIndex =
        (recordCount == MAX_ATTENDANCE_RECORDS)
        ? nextIndex
        : 0;


    // ==============================================
    // SEARCH TODAY'S RECORD FOR THIS EMPLOYEE
    // ==============================================

    for (uint16_t i = 0; i < recordCount; i++)
    {
        const uint16_t index =
            (oldestIndex + i) % MAX_ATTENDANCE_RECORDS;

        AttendanceRecord record = {};

        if (!storageReadAttendance(index, record))
        {
            Serial.println("EEPROM attendance read error.");
            return;
        }

        if (
            record.active &&
            record.employeeIndex == (uint8_t)employeeIndex &&
            record.dayKey == todayKey
        )
        {
            // ======================================
            // SECOND SCAN: RECORD OUT TIME
            // ======================================

            if (record.outEpoch == 0)
            {
                if (nowEpoch < record.inEpoch)
                {
                    Serial.println(
                        "Invalid time: OUT is earlier than IN."
                    );

                    return;
                }

                record.outEpoch = nowEpoch;


                // Save OUT time to EEPROM
                if (!storageWriteAttendance(index, record))
                {
                    Serial.println(
                        "Failed to save OUT time in EEPROM."
                    );

                    return;
                }

                const uint32_t duration =
                    record.outEpoch - record.inEpoch;


                Serial.println();
                Serial.println("OUT SUCCESSFULLY RECORDED");

                Serial.print("Employee: ");
                Serial.println(employee.name);

                printLocalTime(
                    "OUT Time: ",
                    nowEpoch
                );


                Serial.print("Total Duration: ");

                Serial.print(duration / 3600UL);

                Serial.print(" Hours ");

                Serial.print(
                    (duration % 3600UL) / 60UL
                );

                Serial.println(" Minutes");


                Serial.println(
                    "Attendance updated in EEPROM."
                );

                // Report successful OUT
                lastAttendanceResult =
                    ATTENDANCE_RESULT_OUT_RECORDED;
            }
            else
            {
                // ==================================
                // THIRD OR LATER SCAN: IGNORE
                // ==================================

                Serial.println(
                    "Attendance already completed today."
                );

                Serial.print("Employee: ");
                Serial.println(employee.name);

                printLocalTime(
                    "IN Time: ",
                    record.inEpoch
                );

                printLocalTime(
                    "OUT Time: ",
                    record.outEpoch
                );

                lastAttendanceResult =
                    ATTENDANCE_RESULT_ALREADY_COMPLETED;
            }

            return;
        }
    }


    // ==============================================
    // FIRST SCAN: CREATE IN RECORD
    // ==============================================

    AttendanceRecord newRecord = {};

    newRecord.active = 1;

    newRecord.employeeIndex =
        (uint8_t)employeeIndex;

    newRecord.dayKey =
        todayKey;

    newRecord.inEpoch =
        nowEpoch;

    newRecord.outEpoch =
        0;


    // Get next EEPROM attendance location
    const uint16_t writeIndex =
        storageNextAttendance();

    if (writeIndex >= MAX_ATTENDANCE_RECORDS)
    {
        Serial.println("Invalid EEPROM write index.");
        return;
    }


    // Save IN record
    if (!storageWriteAttendance(writeIndex, newRecord))
    {
        Serial.println(
            "Failed to save IN time in EEPROM."
        );

        return;
    }


    // Advance circular attendance pointer
    if (!storageAdvanceAttendance())
    {
        Serial.println(
            "IN record saved, but attendance pointer update failed."
        );

        return;
    }


    // ==============================================
    // IN SUCCESSFULLY RECORDED
    // ==============================================

    Serial.println();
    Serial.println("IN SUCCESSFULLY RECORDED");

    Serial.print("Employee: ");
    Serial.println(employee.name);

    printLocalTime(
        "IN Time: ",
        nowEpoch
    );

    Serial.println(
        "Attendance saved in EEPROM."
    );

    Serial.println(
        "Waiting for the employee's next scan to record OUT."
    );

    Serial.println("================================");

    // Report successful IN
    lastAttendanceResult =
        ATTENDANCE_RESULT_IN_RECORDED;
}
