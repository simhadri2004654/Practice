
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "fingerprint.h"
#include "eeprom.h"
#include "attendance.h"
#include "web_server.h"


// ==================================================
// SYSTEM STATUS
// ==================================================

static bool storageReady = false;
static bool fingerprintReady = false;


// ==================================================
// FOTA / OTA CONFIGURATION
// ==================================================

static const char *OTA_HOSTNAME = "lakshiv-attendance";
static const char *OTA_PASSWORD = "PASSWORD";

static bool otaReady = false;


// ==================================================
// SERIAL COMMAND BUFFER
// ==================================================

static char serialLine[100];
static size_t serialLineLength = 0;
static bool serialLineOverflow = false;


// R503 slot range
static const uint16_t MAX_FINGERPRINT_SLOTS = 200;


// ==================================================
// ONBOARD LED CONFIGURATION
// ==================================================

// Assumes two controllable onboard LEDs
static const uint8_t HEARTBEAT_LED_PIN = 16;  // GPIO16 / D0
static const uint8_t ACTIVITY_LED_PIN  = 2;   // GPIO2 / D4

// Common onboard LEDs are active LOW
static const uint8_t LED_ON_LEVEL  = LOW;
static const uint8_t LED_OFF_LEVEL = HIGH;

static Ticker ledTicker;


// ==================================================
// HEARTBEAT LED VARIABLES
// ==================================================

static volatile uint8_t heartbeatCounter = 0;
static volatile bool heartbeatLedState = false;


// ==================================================
// ACTIVITY LED MODES
// ==================================================

static const uint8_t LED_MODE_OFF   = 0;
static const uint8_t LED_MODE_SOLID = 1;
static const uint8_t LED_MODE_FLASH = 2;

static volatile uint8_t activityLedMode = LED_MODE_OFF;
static volatile uint8_t activityFlashCount = 0;
static volatile uint8_t activityFlashTick = 0;


// ==================================================
// FUNCTION DECLARATIONS
// ==================================================

static void connectWiFi();
static void setupOTA();
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

static void processFingerprintAttendance(
    uint16_t fingerprintSlot
);


// ==================================================
// LED TIMER CALLBACK
// ==================================================

static void ledTimerCallback()
{
    // ==============================================
    // LED 1: CONTINUOUS BLINKING
    // 500 ms ON, 500 ms OFF
    // ==============================================

    heartbeatCounter++;

    if (heartbeatCounter >= 5)
    {
        heartbeatCounter = 0;

        heartbeatLedState = !heartbeatLedState;

        digitalWrite(
            HEARTBEAT_LED_PIN,
            heartbeatLedState ? LED_ON_LEVEL : LED_OFF_LEVEL
        );
    }


    // ==============================================
    // LED 2: ACTIVITY INDICATOR
    // ==============================================

    uint8_t mode = activityLedMode;

    if (mode == LED_MODE_OFF)
    {
        digitalWrite(ACTIVITY_LED_PIN, LED_OFF_LEVEL);
    }

    else if (mode == LED_MODE_SOLID)
    {
        digitalWrite(ACTIVITY_LED_PIN, LED_ON_LEVEL);
    }

    else if (mode == LED_MODE_FLASH)
    {
        uint8_t count = activityFlashCount;
        uint8_t tick = activityFlashTick;

        uint8_t totalTicks = count * 4;

        if (count == 0 || tick >= totalTicks)
        {
            digitalWrite(ACTIVITY_LED_PIN, LED_OFF_LEVEL);

            activityLedMode = LED_MODE_OFF;
            activityFlashTick = 0;
        }
        else
        {
            // 200 ms ON and 200 ms OFF

            digitalWrite(
                ACTIVITY_LED_PIN,
                (tick % 4 < 2)
                    ? LED_ON_LEVEL
                    : LED_OFF_LEVEL
            );

            activityFlashTick++;
        }
    }
}


// ==================================================
// INITIALIZE ONBOARD LEDS
// ==================================================

static void onboardLedsBegin()
{
    pinMode(HEARTBEAT_LED_PIN, OUTPUT);
    pinMode(ACTIVITY_LED_PIN, OUTPUT);

    // Start LED 1 blinking immediately
    heartbeatCounter = 0;
    heartbeatLedState = true;

    digitalWrite(
        HEARTBEAT_LED_PIN,
        LED_ON_LEVEL
    );

    // LED 2 initially OFF
    activityLedMode = LED_MODE_OFF;
    activityFlashCount = 0;
    activityFlashTick = 0;

    digitalWrite(
        ACTIVITY_LED_PIN,
        LED_OFF_LEVEL
    );

    // Update LEDs every 100 ms
    ledTicker.attach_ms(100, ledTimerCallback);

    Serial.println("Onboard LEDs initialized.");
    Serial.println("LED 1: Continuous heartbeat blinking.");
    Serial.println("LED 2: Attendance activity indicator.");
}


// ==================================================
// ACTIVITY LED CONTROL FUNCTIONS
// ==================================================

static void activityLedOff()
{
    activityFlashCount = 0;
    activityFlashTick = 0;

    activityLedMode = LED_MODE_OFF;
}


static void activityLedSolidOn()
{
    activityFlashCount = 0;
    activityFlashTick = 0;

    activityLedMode = LED_MODE_SOLID;
}


static void activityLedFlash(uint8_t numberOfFlashes)
{
    if (numberOfFlashes == 0)
    {
        activityLedOff();
        return;
    }

    activityFlashTick = 0;
    activityFlashCount = numberOfFlashes;

    activityLedMode = LED_MODE_FLASH;
}


// ==================================================
// PROCESS ATTENDANCE AND SHOW LED RESULT
// ==================================================

static void processFingerprintAttendance(
    uint16_t fingerprintSlot
)
{
    Serial.println();
    Serial.println("Processing fingerprint attendance...");

    // Execute existing attendance logic
    attendanceHandleFingerprint(fingerprintSlot);


    // Get actual attendance result
    AttendanceResult result =
        attendanceGetLastResult();


    switch (result)
    {
        // ==========================================
        // IN SUCCESSFULLY RECORDED
        // ==========================================

        case ATTENDANCE_RESULT_IN_RECORDED:

            Serial.println("IN recorded successfully.");
            Serial.println("LED 2: One flash.");

            activityLedFlash(1);

            break;


        // ==========================================
        // OUT SUCCESSFULLY RECORDED
        // ==========================================

        case ATTENDANCE_RESULT_OUT_RECORDED:

            Serial.println("OUT recorded successfully.");
            Serial.println("LED 2: Two flashes.");

            activityLedFlash(2);

            break;


        // ==========================================
        // UNKNOWN EMPLOYEE
        // ==========================================

        case ATTENDANCE_RESULT_UNKNOWN_EMPLOYEE:

            Serial.println("Unknown employee.");
            Serial.println("LED 2: Three flashes.");

            activityLedFlash(3);

            break;


        // ==========================================
        // ATTENDANCE ALREADY COMPLETED
        // ==========================================

        case ATTENDANCE_RESULT_ALREADY_COMPLETED:

            Serial.println("Attendance already completed today.");

            activityLedOff();

            break;


        // ==========================================
        // EEPROM OR ATTENDANCE ERROR
        // ==========================================

        case ATTENDANCE_RESULT_ERROR:

            Serial.println("Attendance processing error.");
            Serial.println("LED 2: Five flashes.");

            activityLedFlash(5);

            break;


        default:

            activityLedOff();

            break;
    }
}


// ==================================================
// CONNECT WIFI AND SYNCHRONIZE CLOCK
// ==================================================

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


// ==================================================
// INITIALIZE FOTA / OTA
// ==================================================

static void setupOTA()
{
    // OTA requires the ESP8266 to be connected to router Wi-Fi.
    // It is not started when connectWiFi() uses the fallback AP.
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("OTA disabled: Router Wi-Fi is not connected.");
        otaReady = false;
        return;
    }

    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]()
    {
        Serial.println();
        Serial.println("OTA update started.");
        Serial.println("Receiving firmware...");
    });

    ArduinoOTA.onEnd([]()
    {
        Serial.println();
        Serial.println("OTA update completed.");
        Serial.println("ESP8266 will restart.");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        if (total > 0)
        {
            Serial.printf(
                "OTA Progress: %u%%\r",
                (progress * 100U) / total
            );
        }
    });

    ArduinoOTA.onError([](ota_error_t error)
    {
        Serial.printf("\nOTA Error: %u\n", error);

        if (error == OTA_AUTH_ERROR)
        {
            Serial.println("OTA authentication failed.");
        }
        else if (error == OTA_BEGIN_ERROR)
        {
            Serial.println("OTA begin failed.");
        }
        else if (error == OTA_CONNECT_ERROR)
        {
            Serial.println("OTA connection failed.");
        }
        else if (error == OTA_RECEIVE_ERROR)
        {
            Serial.println("OTA firmware receiving failed.");
        }
        else if (error == OTA_END_ERROR)
        {
            Serial.println("OTA finalization failed.");
        }
    });

    ArduinoOTA.begin();
    otaReady = true;

    Serial.println();
    Serial.println("======================================");
    Serial.println("FOTA SERVICE READY");
    Serial.print("OTA Hostname: ");
    Serial.println(OTA_HOSTNAME);
    Serial.print("OTA IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("OTA Port: 8266");
    Serial.println("======================================");
}


// ==================================================
// FIND FREE EEPROM EMPLOYEE RECORD
// ==================================================

static int16_t findFreeEmployeeIndex()
{
    EmployeeRecord record;

    for (uint16_t i = 0; i < MAX_EMPLOYEES; i++)
    {
        if (!storageReadEmployee(i, record))
        {
            Serial.println("EEPROM read error.");

            activityLedFlash(5);

            return -1;
        }

        if (!record.active)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


// ==================================================
// FIND FREE FINGERPRINT SLOT
// ==================================================

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


// ==================================================
// SERIAL COMMAND HELP
// ==================================================

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


// ==================================================
// READ EMPLOYEE NAME OR ID
// ==================================================

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


// ==================================================
// ADD NEW EMPLOYEE
// ==================================================

static void addEmployee()
{
    if (!storageReady)
    {
        Serial.println("EEPROM is not ready.");

        activityLedFlash(5);

        return;
    }

    if (!fingerprintReady)
    {
        Serial.println("R503 is not ready.");

        activityLedFlash(5);

        return;
    }


    Serial.println();
    Serial.println("======================================");
    Serial.println("       NEW EMPLOYEE REGISTRATION");
    Serial.println("======================================");


    // Activity LED ON during registration
    activityLedSolidOn();


    // STEP 1: Find free EEPROM position

    int16_t freeIndex = findFreeEmployeeIndex();

    if (freeIndex < 0)
    {
        Serial.println("No free EEPROM employee record.");

        activityLedFlash(5);

        return;
    }


    // STEP 2: Find free fingerprint slot

    int16_t selectedSlot = findFreeFingerprintSlot();

    if (selectedSlot < 0)
    {
        Serial.println("No free fingerprint slot available.");

        activityLedFlash(5);

        return;
    }

    uint16_t fingerprintSlot = (uint16_t)selectedSlot;

    Serial.print("Selected fingerprint slot: ");
    Serial.println(fingerprintSlot);


    // STEP 3: Fingerprint enrollment

    Serial.println();
    Serial.println("FINGERPRINT ENROLLMENT STARTED.");
    Serial.println("Follow the R503 sensor instructions.");

    if (!fingerprintEnroll(fingerprintSlot))
    {
        Serial.println();
        Serial.println("Fingerprint enrollment failed.");

        activityLedFlash(5);

        return;
    }

    Serial.println();
    Serial.println("Fingerprint saved successfully in R503.");


    // STEP 4: Employee name

    char employeeName[24] = {0};

    if (!readEmployeeInput(
            "Enter Employee Name:",
            employeeName,
            sizeof(employeeName)))
    {
        Serial.println("Registration cancelled.");

        if (!fingerprintDelete(fingerprintSlot))
        {
            activityLedFlash(5);
        }
        else
        {
            activityLedOff();
        }

        return;
    }


    // STEP 5: Employee ID

    char employeeId[16] = {0};

    if (!readEmployeeInput(
            "Enter Employee ID:",
            employeeId,
            sizeof(employeeId)))
    {
        Serial.println("Registration cancelled.");

        if (!fingerprintDelete(fingerprintSlot))
        {
            activityLedFlash(5);
        }
        else
        {
            activityLedOff();
        }

        return;
    }


    // STEP 6: Check duplicate employee ID

    if (storageFindEmployeeById(employeeId) >= 0)
    {
        Serial.println("This employee ID already exists.");

        if (!fingerprintDelete(fingerprintSlot))
        {
            activityLedFlash(5);
        }
        else
        {
            activityLedOff();
        }

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

        if (!fingerprintDelete(fingerprintSlot))
        {
            Serial.println("Fingerprint cleanup also failed.");
        }

        activityLedFlash(5);

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


    // Registration complete
    activityLedOff();
}


// ==================================================
// DELETE EMPLOYEE
// ==================================================

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

        activityLedFlash(5);

        return;
    }


    // Find employee

    int16_t employeeIndex =
        storageFindEmployeeByFingerprint(
            (uint16_t)slotValue
        );

    if (employeeIndex < 0)
    {
        Serial.println("Employee not found.");

        activityLedFlash(3);

        return;
    }


    // Delete fingerprint from R503

    if (!fingerprintDelete((uint16_t)slotValue))
    {
        Serial.println("Fingerprint deletion failed.");

        activityLedFlash(5);

        return;
    }


    // Delete employee mapping from EEPROM

    if (storageDeleteEmployee((uint16_t)employeeIndex))
    {
        Serial.println("Employee deleted successfully.");

        activityLedFlash(1);
    }
    else
    {
        Serial.println("EEPROM deletion failed.");
        Serial.println("Check employee record manually.");

        activityLedFlash(5);
    }
}


// ==================================================
// LIST REGISTERED EMPLOYEES
// ==================================================

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

            activityLedFlash(5);

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


// ==================================================
// PROCESS SERIAL COMMAND
// ==================================================

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
        Serial.println("Starting employee registration...");

        addEmployee();

        Serial.println();
        Serial.println("Returning to fingerprint attendance mode...");

        printHelp();
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

            activityLedFlash(5);
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


// ==================================================
// RECEIVE SERIAL COMMANDS
// ==================================================

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


// ==================================================
// SETUP
// ==================================================

void setup()
{
    Serial.begin(115200);

    delay(100);

    // Initialize onboard LEDs first
    onboardLedsBegin();

    delay(300);

    Serial.println();
    Serial.println("======================================");
    Serial.println(" ESP8266 FINGERPRINT ATTENDANCE SYSTEM");
    Serial.println("======================================");


    // ==============================================
    // INITIALIZE EEPROM
    // ==============================================

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

        activityLedFlash(5);
    }


    // ==============================================
    // INITIALIZE FINGERPRINT SENSOR
    // ==============================================

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

        activityLedFlash(5);
    }


    // ==============================================
    // SYSTEM INITIALIZATION STATUS
    // ==============================================

    if (!storageReady || !fingerprintReady)
    {
        Serial.println("SYSTEM ERROR: Check EEPROM or R503.");

        activityLedFlash(5);
    }
    else
    {
        Serial.println("All hardware initialized successfully.");

        activityLedOff();
    }


    // ==============================================
    // WIFI AND CLOCK
    // ==============================================

    Serial.println();
    connectWiFi();

    // Initialize wireless firmware update service
    setupOTA();


    // ==============================================
    // START WEBSITE
    // ==============================================

    Serial.println();
    Serial.println("Starting attendance website...");

    webServerBegin();

    Serial.println("Website server started.");


    // Display commands
    printHelp();
}


// ==================================================
// MAIN LOOP
// ==================================================

void loop()
{
    // Handle wireless firmware update requests
    if (otaReady)
    {
        ArduinoOTA.handle();
    }

    // Handle website clients
    webServerHandleClient();


    // Process serial commands
    serviceSerialCommands();


    // ==============================================
    // FINGERPRINT SCANNING
    // ==============================================

    if (fingerprintReady)
    {
        uint16_t matchedSlot = 0;

        bool matched =
            fingerprintIdentify(matchedSlot);


        if (matched)
        {
            Serial.println();
            Serial.println("Registered fingerprint detected.");

            // Process IN or OUT attendance
            processFingerprintAttendance(matchedSlot);
        }
        else
        {
            // Check why fingerprintIdentify returned false
            FingerprintScanStatus scanStatus =
                fingerprintGetLastScanStatus();


            switch (scanStatus)
            {
                // UNKNOWN FINGERPRINT
                case FINGERPRINT_SCAN_UNKNOWN:

                    Serial.println(
                        "Unknown fingerprint - 3 flashes."
                    );

                    activityLedFlash(3);

                    break;


                // R503 COMMUNICATION OR SENSOR ERROR
                case FINGERPRINT_SCAN_ERROR:

                    Serial.println(
                        "Fingerprint sensor error - 5 flashes."
                    );

                    activityLedFlash(5);

                    break;


                // NO FINGER OR WAITING FOR REMOVAL
                case FINGERPRINT_SCAN_IDLE:

                case FINGERPRINT_SCAN_MATCH:

                default:

                    break;
            }
        }
    }


    // Handle website clients again
    webServerHandleClient();

    delay(20);
}
