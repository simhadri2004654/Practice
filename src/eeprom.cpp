#include "eeprom.h"
#include <Wire.h>
#include <string.h>

static constexpr uint32_t STORAGE_MAGIC = 0x41545431UL;

static constexpr uint16_t HEADER_ADDRESS = 0;

static constexpr uint16_t ATTENDANCE_RECORD_SIZE =
    sizeof(AttendanceRecord);

struct StorageHeader
{
    uint32_t magic;
    uint16_t nextAttendance;
    uint16_t attendanceCount;
};

static StorageHeader storageHeader = {0, 0, 0};

static constexpr uint32_t EMPLOYEE_END_ADDRESS =
    EMPLOYEE_BASE_ADDRESS +
    (uint32_t)MAX_EMPLOYEES * sizeof(EmployeeRecord);


// Wait for EEPROM write completion
static bool waitForEepromReady()
{
    const uint32_t start = millis();

    while ((uint32_t)(millis() - start) < 30)
    {
        Wire.beginTransmission(EEPROM_I2C_ADDRESS);

        if (Wire.endTransmission() == 0)
            return true;

        delay(1);
        yield();
    }

    return false;
}


// Write multiple bytes
static bool writeBytes(
    uint16_t address,
    const uint8_t *data,
    size_t length
)
{
    while (length > 0)
    {
        size_t chunk = length;

        const size_t pageRemaining =
            EEPROM_PAGE_SIZE - (address % EEPROM_PAGE_SIZE);

        // Keep each I2C transaction within Wire's buffer.
        if (chunk > 30)
            chunk = 30;

        if (chunk > pageRemaining)
            chunk = pageRemaining;

        Wire.beginTransmission(EEPROM_I2C_ADDRESS);

        // AT24C256 uses a two-byte memory address.
        Wire.write((uint8_t)(address >> 8));
        Wire.write((uint8_t)(address & 0xFF));

        Wire.write(data, chunk);

        if (Wire.endTransmission() != 0)
            return false;

        if (!waitForEepromReady())
            return false;

        address += chunk;
        data += chunk;
        length -= chunk;
    }

    return true;
}


// Read multiple bytes
static bool readBytes(
    uint16_t address,
    uint8_t *data,
    size_t length
)
{
    while (length > 0)
    {
        size_t chunk = length;

        if (chunk > 30)
            chunk = 30;

        Wire.beginTransmission(EEPROM_I2C_ADDRESS);

        Wire.write((uint8_t)(address >> 8));
        Wire.write((uint8_t)(address & 0xFF));

        if (Wire.endTransmission(false) != 0)
            return false;

        const uint8_t received = Wire.requestFrom(
            (uint8_t)EEPROM_I2C_ADDRESS,
            (uint8_t)chunk
        );

        if (received != chunk)
            return false;

        size_t copied = 0;

        while (Wire.available() && copied < chunk)
        {
            data[copied++] = (uint8_t)Wire.read();
        }

        if (copied != chunk)
            return false;

        address += chunk;
        data += chunk;
        length -= chunk;
    }

    return true;
}


// Write storage header
static bool writeHeader()
{
    return writeBytes(
        HEADER_ADDRESS,
        (const uint8_t *)&storageHeader,
        sizeof(storageHeader)
    );
}


// Initialize EEPROM
bool storageBegin()
{
    Wire.begin(
        EEPROM_SDA_PIN,
        EEPROM_SCL_PIN
    );

    Wire.setClock(100000);

    delay(20);

    Wire.beginTransmission(EEPROM_I2C_ADDRESS);

    if (Wire.endTransmission() != 0)
    {
        Serial.println("AT24C256 not detected at 0x50.");

        return false;
    }

    if (!readBytes(
            HEADER_ADDRESS,
            (uint8_t *)&storageHeader,
            sizeof(storageHeader)))
    {
        return false;
    }

    // Format EEPROM if it is new or unformatted.
    if (storageHeader.magic != STORAGE_MAGIC ||
        storageHeader.nextAttendance >= MAX_ATTENDANCE_RECORDS ||
        storageHeader.attendanceCount > MAX_ATTENDANCE_RECORDS)
    {
        Serial.println("EEPROM formatting started...");

        uint8_t zeros[30] = {0};

        uint32_t address = 16;

        while (address < EEPROM_SIZE_BYTES)
        {
            size_t count = EEPROM_SIZE_BYTES - address;

            if (count > sizeof(zeros))
                count = sizeof(zeros);

            if (!writeBytes(
                    (uint16_t)address,
                    zeros,
                    count))
            {
                Serial.println("EEPROM format failed.");

                return false;
            }

            address += count;
        }

        storageHeader.magic = STORAGE_MAGIC;
        storageHeader.nextAttendance = 0;
        storageHeader.attendanceCount = 0;

        if (!writeHeader())
            return false;

        Serial.println("EEPROM format completed.");
    }

    if (EMPLOYEE_END_ADDRESS > ATTENDANCE_BASE_ADDRESS ||
        (uint32_t)ATTENDANCE_BASE_ADDRESS +
        (uint32_t)MAX_ATTENDANCE_RECORDS *
        ATTENDANCE_RECORD_SIZE > EEPROM_SIZE_BYTES)
    {
        Serial.println("EEPROM layout exceeds chip capacity.");

        return false;
    }

    Serial.println("AT24C256 ready.");

    return true;
}


// Read employee
bool storageReadEmployee(
    uint16_t index,
    EmployeeRecord &record
)
{
    if (index >= MAX_EMPLOYEES)
        return false;

    const uint16_t address =
        EMPLOYEE_BASE_ADDRESS +
        index * sizeof(EmployeeRecord);

    return readBytes(
        address,
        (uint8_t *)&record,
        sizeof(record)
    );
}


// Save employee
bool storageWriteEmployee(
    uint16_t index,
    const EmployeeRecord &record
)
{
    if (index >= MAX_EMPLOYEES)
        return false;

    const uint16_t address =
        EMPLOYEE_BASE_ADDRESS +
        index * sizeof(EmployeeRecord);

    return writeBytes(
        address,
        (const uint8_t *)&record,
        sizeof(record)
    );
}


// Delete employee mapping
bool storageDeleteEmployee(uint16_t index)
{
    if (index >= MAX_EMPLOYEES)
        return false;

    const uint8_t inactive = 0;

    const uint16_t address =
        EMPLOYEE_BASE_ADDRESS +
        index * sizeof(EmployeeRecord);

    return writeBytes(
        address,
        &inactive,
        sizeof(inactive)
    );
}


// Find employee by R503 fingerprint slot
int16_t storageFindEmployeeByFingerprint(
    uint16_t fingerprintId
)
{
    EmployeeRecord record;

    for (uint16_t i = 0; i < MAX_EMPLOYEES; i++)
    {
        if (!storageReadEmployee(i, record))
            return -1;

        if (record.active &&
            record.fingerprintId == fingerprintId)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


// Find employee by employee ID
int16_t storageFindEmployeeById(
    const char *employeeId
)
{
    EmployeeRecord record;

    for (uint16_t i = 0; i < MAX_EMPLOYEES; i++)
    {
        if (!storageReadEmployee(i, record))
            return -1;

        if (record.active &&
            strncmp(
                record.employeeId,
                employeeId,
                sizeof(record.employeeId)
            ) == 0)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


// Read attendance record
bool storageReadAttendance(
    uint16_t index,
    AttendanceRecord &record
)
{
    if (index >= MAX_ATTENDANCE_RECORDS)
        return false;

    const uint16_t address =
        ATTENDANCE_BASE_ADDRESS +
        index * sizeof(AttendanceRecord);

    return readBytes(
        address,
        (uint8_t *)&record,
        sizeof(record)
    );
}


// Save attendance record
bool storageWriteAttendance(
    uint16_t index,
    const AttendanceRecord &record
)
{
    if (index >= MAX_ATTENDANCE_RECORDS)
        return false;

    const uint16_t address =
        ATTENDANCE_BASE_ADDRESS +
        index * sizeof(AttendanceRecord);

    return writeBytes(
        address,
        (const uint8_t *)&record,
        sizeof(record)
    );
}


// Get next attendance position
uint16_t storageNextAttendance()
{
    return storageHeader.nextAttendance;
}


// Get stored attendance count
uint16_t storageAttendanceCount()
{
    return storageHeader.attendanceCount;
}


// Advance circular attendance log
bool storageAdvanceAttendance()
{
    storageHeader.nextAttendance++;

    if (storageHeader.nextAttendance >= MAX_ATTENDANCE_RECORDS)
        storageHeader.nextAttendance = 0;

    if (storageHeader.attendanceCount < MAX_ATTENDANCE_RECORDS)
        storageHeader.attendanceCount++;

    return writeHeader();
}