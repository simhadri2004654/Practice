
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "fingerprint.h"
#include "eeprom.h"
#include "attendance.h"
#include "web_server.h"


// System status
static bool storageReady = false;
static bool fingerprintReady = false;

// Serial command buffer
static char serialLine[100];
static size_t serialLineLength = 0;
static bool serialLineOverflow = false;

// R503 slot range assumed by this project
static const uint16_t MAX_FINGERPRINT_SLOTS = 200;


// Function declarations
static void connectWiFi();
static void printHelp();

static int16_t findFreeEmployeeIndex();
static int16_t findFreeFingerprintSlot();

static bool readEmployeeInput(
    const char *prompt,
    char *buffer,
    size_t capacity
);

static void addEmployee();
static void deleteEmployee(char *slotText);
static void listEmployees();

static void processSerialCommand(char *line);
static void serviceSerialCommands();


// --------------------------------------------------
// CONNECT WIFI AND SYNCHRONIZE CLOCK
// --------------------------------------------------

static void connectWiFi()
{
    WiFi.mode(WIFI_STA);

    if (strlen(WIFI_SSID) > 0 &&
        strcmp(WIFI_SSID, "YOUR_WIFI_NAME") != 0)
    {
        Serial.print("Connecting to Wi-Fi: ");
        Serial.println(WIFI_SSID);

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        uint32_t startTime = millis();

        while (WiFi.status() != WL_CONNECTED &&
               (uint32_t)(millis() - startTime) < 20000)
        {
            delay(300);
            Serial.print(".");
            yield();
        }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.println("Wi-Fi connected.");

        Serial.print("Dashboard address: http://");
        Serial.println(WiFi.localIP());

        configTime(
            TIME_ZONE_OFFSET_SECONDS,
            0,
            "pool.ntp.org",
            "time.google.com"
        );

        Serial.println("Synchronizing clock...");

        uint32_t startTime = millis();

        while (time(nullptr) < 1700000000 &&
               (uint32_t)(millis() - startTime) < 15000)
        {
            delay(500);
            yield();
        }

        if (time(nullptr) >= 1700000000)
        {
            Serial.println("Clock synchronized.");
        }
        else
        {
            Serial.println("NTP synchronization failed.");
        }
    }
    else
    {
        Serial.println();
        Serial.println("Router Wi-Fi unavailable.");
        Serial.println("Starting fallback access point.");

        WiFi.mode(WIFI_AP);

        WiFi.softAP(
            FALLBACK_AP_SSID,
            FALLBACK_AP_PASSWORD
        );

        Serial.print("AP name: ");
        Serial.println(FALLBACK_AP_SSID);

        Serial.print("Dashboard address: http://");
        Serial.println(WiFi.softAPIP());

        Serial.println("Internet time synchronization unavailable.");
    }
}


// --------------------------------------------------
// FIND FREE EEPROM EMPLOYEE RECORD
// --------------------------------------------------

static int16_t findFreeEmployeeIndex()
{
    EmployeeRecord record;

    for (uint16_t i = 0; i < MAX_EMPLOYEES; i++)
    {
        if (!storageReadEmployee(i, record))
        {
            Serial.println("EEPROM read error.");
            return -1;
        }

        if (!record.active)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


// --------------------------------------------------
// FIND FREE EMPLOYEE FINGERPRINT SLOT
// --------------------------------------------------

static int16_t findFreeFingerprintSlot()
{
    for (uint16_t slot = 1;
         slot <= MAX_FINGERPRINT_SLOTS;
         slot++)
    {
        int16_t employeeIndex =
            storageFindEmployeeByFingerprint(slot);

        if (employeeIndex < 0)
        {
            return (int16_t)slot;
        }
    }

    return -1;
}


// --------------------------------------------------
// SERIAL COMMAND HELP
// --------------------------------------------------

static void printHelp()
{
    Serial.println();
    Serial.println("======================================");
    Serial.println("       ATTENDANCE SYSTEM COMMANDS");
    Serial.println("======================================");

    Serial.println("ADD        - Register new employee");
    Serial.println("DEL,<slot> - Delete employee");
    Serial.println("LIST       - Display employees");
    Serial.println("HELP       - Display commands");

    Serial.println("======================================");
}


// --------------------------------------------------
// READ EMPLOYEE NAME OR ID
// --------------------------------------------------

static bool readEmployeeInput(
    const char *prompt,
    char *buffer,
    size_t capacity
)
{
    if (buffer == nullptr || capacity < 2)
    {
        return false;
    }

    Serial.println(prompt);

    size_t length = 0;
    bool overflow = false;

    uint32_t startTime = millis();

    while ((uint32_t)(millis() - startTime) < 60000)
    {
        webServerHandleClient();

        while (Serial.available())
        {
            char c = (char)Serial.read();

            if (c == '\r')
            {
                continue;
            }

            if (c == '\n')
            {
                if (overflow)
                {
                    Serial.println("Input too long.");
                    return false;
                }

                buffer[length] = '\0';

                // Remove trailing spaces
                while (length > 0 &&
                       buffer[length - 1] == ' ')
                {
                    buffer[--length] = '\0';
                }

                // Remove leading spaces
                size_t first = 0;

                while (buffer[first] == ' ')
                {
                    first++;
                }

                if (first > 0)
                {
                    memmove(
                        buffer,
                        buffer + first,
                        length - first + 1
                    );

                    length -= first;
                }

                // Ignore blank lines
                if (length == 0)
                {
                    continue;
                }

                return true;
            }

            if (c >= 32 && c <= 126)
            {
                if (length < capacity - 1)
                {
                    buffer[length++] = c;
                }
                else
                {
                    overflow = true;
                }
            }
        }

        yield();
    }

    Serial.println("Employee input timeout.");

    return false;
}


// --------------------------------------------------
// ADD NEW EMPLOYEE
// --------------------------------------------------

static void addEmployee()
{
    if (!storageReady)
    {
        Serial.println("EEPROM is not ready.");
        return;
    }

    if (!fingerprintReady)
    {
        Serial.println("R503 is not ready.");
        return;
    }

    Serial.println();
    Serial.println("======================================");
    Serial.println("       NEW EMPLOYEE REGISTRATION");
    Serial.println("======================================");

    // STEP 1: Find free EEPROM position
    int16_t freeIndex = findFreeEmployeeIndex();

    if (freeIndex < 0)
    {
        Serial.println("No free EEPROM employee record.");
        return;
    }

    // STEP 2: Find next available mapped fingerprint slot
    int16_t selectedSlot = findFreeFingerprintSlot();

    if (selectedSlot < 0)
    {
        Serial.println("No free fingerprint slot available.");
        return;
    }

    uint16_t fingerprintSlot = (uint16_t)selectedSlot;

    Serial.print("Selected fingerprint slot: ");
    Serial.println(fingerprintSlot);

    // STEP 3: Capture new employee fingerprint
    Serial.println();
    Serial.println("FINGERPRINT ENROLLMENT STARTED.");
    Serial.println("Follow the R503 sensor instructions.");

    if (!fingerprintEnroll(fingerprintSlot))
    {
        Serial.println();
        Serial.println("Fingerprint enrollment failed.");
        Serial.println("No employee details were requested.");

        return;
    }

    Serial.println();
    Serial.println("Fingerprint saved successfully in R503.");

    // STEP 4: Ask employee name
    char employeeName[24] = {0};

    if (!readEmployeeInput(
            "Enter Employee Name:",
            employeeName,
            sizeof(employeeName)))
    {
        Serial.println("Registration cancelled.");

        fingerprintDelete(fingerprintSlot);
        return;
    }

    // STEP 5: Ask employee ID
    char employeeId[16] = {0};

    if (!readEmployeeInput(
            "Enter Employee ID:",
            employeeId,
            sizeof(employeeId)))
    {
        Serial.println("Registration cancelled.");

        fingerprintDelete(fingerprintSlot);
        return;
    }

    // STEP 6: Check duplicate employee ID
    if (storageFindEmployeeById(employeeId) >= 0)
    {
        Serial.println("This employee ID already exists.");

        fingerprintDelete(fingerprintSlot);
        return;
    }

    // STEP 7: Prepare EEPROM record
    EmployeeRecord record = {};

    record.active = 1;
    record.fingerprintId = fingerprintSlot;

    strncpy(
        record.name,
        employeeName,
        sizeof(record.name) - 1
    );

    strncpy(
        record.employeeId,
        employeeId,
        sizeof(record.employeeId) - 1
    );

    // STEP 8: Save employee details
    if (!storageWriteEmployee(
            (uint16_t)freeIndex,
            record))
    {
        Serial.println("EEPROM write failed.");

        // Remove fingerprint if employee data could not be saved
        fingerprintDelete(fingerprintSlot);

        return;
    }

    // STEP 9: Registration successful
    Serial.println();
    Serial.println("======================================");
    Serial.println("   EMPLOYEE REGISTERED SUCCESSFULLY");
    Serial.println("======================================");

    Serial.print("Employee Name    : ");
    Serial.println(record.name);

    Serial.print("Employee ID      : ");
    Serial.println(record.employeeId);

    Serial.print("Fingerprint Slot : ");
    Serial.println(record.fingerprintId);

    Serial.println("Fingerprint      : Saved in R503");
    Serial.println("Employee Details : Saved in EEPROM");

    Serial.println("======================================");
}


// --------------------------------------------------
// DELETE EMPLOYEE
// --------------------------------------------------

static void deleteEmployee(char *slotText)
{
    char *endPointer;

    long slotValue = strtol(
        slotText,
        &endPointer,
        10
    );

    while (*endPointer == ' ')
    {
        endPointer++;
    }

    if (slotText[0] == '\0' ||
        *endPointer != '\0' ||
        slotValue < 1 ||
        slotValue > MAX_FINGERPRINT_SLOTS)
    {
        Serial.println("Invalid fingerprint slot.");
        return;
    }

    if (!storageReady || !fingerprintReady)
    {
        Serial.println("EEPROM or R503 is not ready.");
        return;
    }

    int16_t employeeIndex =
        storageFindEmployeeByFingerprint(
            (uint16_t)slotValue
        );

    if (employeeIndex < 0)
    {
        Serial.println("Employee not found.");
        return;
    }

    // Delete fingerprint from R503 first
    if (!fingerprintDelete((uint16_t)slotValue))
    {
        Serial.println("Fingerprint deletion failed.");
        return;
    }

    // Delete mapping from EEPROM
    if (storageDeleteEmployee((uint16_t)employeeIndex))
    {
        Serial.println("Employee deleted successfully.");
    }
    else
    {
        Serial.println("EEPROM deletion failed.");
        Serial.println("Check employee record manually.");
    }
}


// --------------------------------------------------
// LIST REGISTERED EMPLOYEES
// --------------------------------------------------

static void listEmployees()
{
    EmployeeRecord record;

    Serial.println();
    Serial.println("======================================");
    Serial.println("           EMPLOYEE DATABASE");
    Serial.println("======================================");

    Serial.println("SLOT | EMPLOYEE ID | EMPLOYEE NAME");
    Serial.println("--------------------------------------");

    for (uint16_t i = 0; i < MAX_EMPLOYEES; i++)
    {
        if (!storageReadEmployee(i, record))
        {
            Serial.println("EEPROM read error.");
            return;
        }

        if (record.active)
        {
            Serial.print(record.fingerprintId);
            Serial.print(" | ");

            Serial.print(record.employeeId);
            Serial.print(" | ");

            Serial.println(record.name);
        }
    }

    Serial.println("======================================");
}


// --------------------------------------------------
// PROCESS SERIAL COMMAND
// --------------------------------------------------

static void processSerialCommand(char *line)
{
    // Remove leading spaces
    while (*line == ' ' || *line == '\t')
    {
        line++;
    }

    // Remove trailing spaces
    size_t length = strlen(line);

    while (length > 0 &&
           (line[length - 1] == ' ' ||
            line[length - 1] == '\t'))
    {
        line[--length] = '\0';
    }

    if (strcmp(line, "ADD") == 0)
    {
        Serial.println();
        Serial.println("Staring employee registration...");

        addEmployee();
        Serial.println();
        Serial.println("Returing to fingerprint attendance mode...");        printHelp();
    }
    else if (strncmp(line, "DEL,", 4) == 0)
    {
        deleteEmployee(line + 4);
    }
    else if (strcmp(line, "LIST") == 0)
    {
        if (storageReady)
        {
            listEmployees();
        }
        else
        {
            Serial.println("EEPROM not ready.");
        }
    }
    else if (strcmp(line, "HELP") == 0)
    {
        printHelp();
    }
    else if (line[0] != '\0')
    {
        Serial.println("Unknown command. Type HELP.");
    }
}


// --------------------------------------------------
// RECEIVE SERIAL COMMANDS
// --------------------------------------------------

static void serviceSerialCommands()
{
    while (Serial.available())
    {
        char c = (char)Serial.read();

        if (c == '\r' || c == '\n')
        {
            if (serialLineOverflow)
            {
                Serial.println("Serial command too long.");
            }
            else if (serialLineLength > 0)
            {
                serialLine[serialLineLength] = '\0';

                processSerialCommand(serialLine);
            }

            serialLineLength = 0;
            serialLineOverflow = false;
        }
        else
        {
            if (serialLineLength < sizeof(serialLine) - 1)
            {
                serialLine[serialLineLength++] = c;
            }
            else
            {
                serialLineOverflow = true;
            }
        }
    }
}


// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup()
{
    Serial.begin(115200);

    delay(300);

    Serial.println();
    Serial.println("======================================");
    Serial.println(" ESP8266 FINGERPRINT ATTENDANCE SYSTEM");
    Serial.println("======================================");

    // Initialize EEPROM
    Serial.println();
    Serial.println("Initializing AT24C256...");

    storageReady = storageBegin();

    if (storageReady)
    {
        Serial.println("EEPROM ready.");
    }
    else
    {
        Serial.println("EEPROM initialization failed.");
    }

    // Initialize fingerprint sensor
    Serial.println();
    Serial.println("Initializing R503...");

    fingerprintReady = fingerprintBegin();

    if (fingerprintReady)
    {
        Serial.println("Fingerprint sensor ready.");
    }
    else
    {
        Serial.println("Fingerprint sensor initialization failed.");
    }

    // Wi-Fi and clock
    Serial.println();
    connectWiFi();

    // Start website
    Serial.println();
    Serial.println("Starting attendance website...");

    webServerBegin();

    Serial.println("Website server started.");

    // Show commands
    printHelp();
}


// --------------------------------------------------
// MAIN LOOP
// --------------------------------------------------




void loop()
{
    // Keep website active
    webServerHandleClient();

    // Process serial commands
    serviceSerialCommands();

    // Continuously check R503
    uint16_t matchedSlot = 0;

    if (fingerprintIdentify(matchedSlot))
    {
        Serial.println("Valid employee fingerprint detected.");

        // Save IN or OUT attendance
        attendanceHandleFingerprint(matchedSlot);
    }

    // Continue handling website requests
    webServerHandleClient();

    delay(20);
}


