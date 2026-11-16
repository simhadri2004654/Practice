
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>

#include "config.h"
#include "fingerprint.h"


// ==================================================
// R503 UART CONNECTION
// ==================================================

static SoftwareSerial fingerprintSerial(
    FINGERPRINT_RX_PIN,
    FINGERPRINT_TX_PIN
);

static Adafruit_Fingerprint finger(&fingerprintSerial);


// ==================================================
// INTERNAL VARIABLES
// ==================================================

static bool attendanceWaitingForRemoval = false;

static uint32_t lastFingerprintPollMs = 0;
static uint32_t lastCommunicationMessageMs = 0;
static uint32_t lastCommunicationLedMs = 0;
static uint32_t lastSensorErrorMessageMs = 0;

static FingerprintScanStatus lastScanStatus =
    FINGERPRINT_SCAN_IDLE;


// ==================================================
// FUNCTION PROTOTYPES
// ==================================================

static bool waitForFinger(
    uint32_t timeoutMs,
    const char *message
);

static bool waitForFingerRemoval(uint32_t timeoutMs);

static void fingerprintSetErrorLight();

static void printCommunicationError(const char *message);

static bool communicationErrorLedDue();


// ==================================================
// R503 ERROR LED
// ==================================================

static void fingerprintSetErrorLight()
{
    finger.LEDcontrol(
        FINGERPRINT_LED_FLASHING,
        30,
        FINGERPRINT_LED_RED,
        3
    );
}


// ==================================================
// COMMUNICATION ERROR MESSAGE
// ==================================================

static void printCommunicationError(const char *message)
{
    uint32_t currentTime = millis();

    if ((uint32_t)(currentTime - lastCommunicationMessageMs) >= 2000)
    {
        Serial.println(message);

        lastCommunicationMessageMs = currentTime;
    }
}


// ==================================================
// COMMUNICATION ERROR INDICATION INTERVAL
// ==================================================

static bool communicationErrorLedDue()
{
    uint32_t currentTime = millis();

    if ((uint32_t)(currentTime - lastCommunicationLedMs) >= 2000)
    {
        lastCommunicationLedMs = currentTime;
        return true;
    }

    return false;
}


// ==================================================
// WAIT FOR FINGER PLACEMENT
// ==================================================

static bool waitForFinger(
    uint32_t timeoutMs,
    const char *message
)
{
    Serial.println(message);

    uint32_t startTime = millis();

    while ((uint32_t)(millis() - startTime) < timeoutMs)
    {
        uint8_t response = finger.getImage();

        if (response == FINGERPRINT_OK)
        {
            return true;
        }

        if (response == FINGERPRINT_NOFINGER)
        {
            delay(50);
            yield();
            continue;
        }

        if (response == FINGERPRINT_PACKETRECIEVEERR)
        {
            printCommunicationError(
                "R503 communication error during enrollment."
            );

            delay(50);
            yield();
            continue;
        }

        Serial.print("Fingerprint capture error: 0x");
        Serial.println(response, HEX);

        return false;
    }

    Serial.println("Fingerprint capture timeout.");

    return false;
}


// ==================================================
// WAIT FOR FINGER REMOVAL
// ==================================================

static bool waitForFingerRemoval(uint32_t timeoutMs)
{
    Serial.println("Remove your finger from the sensor.");

    uint32_t startTime = millis();

    while ((uint32_t)(millis() - startTime) < timeoutMs)
    {
        uint8_t response = finger.getImage();

        if (response == FINGERPRINT_NOFINGER)
        {
            Serial.println("Finger removed successfully.");
            return true;
        }

        if (response == FINGERPRINT_OK)
        {
            delay(50);
            yield();
            continue;
        }

        if (response == FINGERPRINT_PACKETRECIEVEERR)
        {
            printCommunicationError(
                "R503 communication error while removing finger."
            );

            delay(50);
            yield();
            continue;
        }

        Serial.print("Sensor response while removing finger: 0x");
        Serial.println(response, HEX);

        return false;
    }

    Serial.println("Finger removal timeout.");

    return false;
}


// ==================================================
// INITIALIZE FINGERPRINT SENSOR
// ==================================================

bool fingerprintBegin()
{
    Serial.println();
    Serial.println("Starting R503 fingerprint sensor...");

    attendanceWaitingForRemoval = false;

    lastScanStatus = FINGERPRINT_SCAN_IDLE;

    lastFingerprintPollMs = millis();

    lastCommunicationMessageMs = 0;

    lastCommunicationLedMs = millis() - 2000UL;

    lastSensorErrorMessageMs = 0;

    finger.begin(FINGERPRINT_BAUD);

    delay(100);

    if (!finger.verifyPassword())
    {
        Serial.println("R503 communication failed.");
        Serial.println("Check TX, RX, VCC and GND.");

        return false;
    }

    Serial.println("R503 communication successful.");

    uint8_t response = finger.getParameters();

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Unable to read sensor parameters: 0x");
        Serial.println(response, HEX);

        return false;
    }

    Serial.print("Fingerprint capacity: ");
    Serial.println(finger.capacity);

    Serial.print("Sensor baud rate: ");
    Serial.println(finger.baud_rate);

    fingerprintSetIdleLight();

    Serial.println("Fingerprint sensor ready.");

    return true;
}


// ==================================================
// R503 BLUE BREATHING LED: IDLE
// ==================================================

void fingerprintSetIdleLight()
{
    finger.LEDcontrol(
        FINGERPRINT_LED_BREATHING,
        60,
        FINGERPRINT_LED_BLUE,
        0
    );
}


// ==================================================
// ENROLL NEW FINGERPRINT
// ==================================================

bool fingerprintEnroll(uint16_t slotId)
{
    uint8_t response;

    Serial.println();
    Serial.println("================================");
    Serial.println("NEW FINGERPRINT ENROLLMENT");
    Serial.println("================================");

    Serial.print("Fingerprint slot: ");
    Serial.println(slotId);

    lastScanStatus = FINGERPRINT_SCAN_IDLE;

    if (slotId == 0 || slotId > finger.capacity)
    {
        Serial.println("Invalid fingerprint slot.");
        fingerprintSetIdleLight();
        return false;
    }

    // Blue flashing during enrollment
    finger.LEDcontrol(
        FINGERPRINT_LED_FLASHING,
        40,
        FINGERPRINT_LED_BLUE,
        0
    );


    // STEP 1: First fingerprint capture

    if (!waitForFinger(
            30000,
            "Place the employee's finger on the sensor."))
    {
        Serial.println("First fingerprint capture failed.");
        fingerprintSetIdleLight();
        return false;
    }

    attendanceWaitingForRemoval = true;

    Serial.println("First fingerprint captured.");


    // STEP 2: Process first image

    response = finger.image2Tz(1);

    if (response != FINGERPRINT_OK)
    {
        Serial.print("First fingerprint processing failed: 0x");
        Serial.println(response, HEX);

        fingerprintSetErrorLight();
        return false;
    }

    Serial.println("First fingerprint processed.");


    // STEP 3: Remove finger

    if (!waitForFingerRemoval(15000))
    {
        fingerprintSetErrorLight();
        return false;
    }

    attendanceWaitingForRemoval = false;


    // STEP 4: Second fingerprint capture

    if (!waitForFinger(
            30000,
            "Place the SAME finger again."))
    {
        Serial.println("Second fingerprint capture failed.");
        fingerprintSetIdleLight();
        return false;
    }

    attendanceWaitingForRemoval = true;

    Serial.println("Second fingerprint captured.");


    // STEP 5: Process second image

    response = finger.image2Tz(2);

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Second fingerprint processing failed: 0x");
        Serial.println(response, HEX);

        fingerprintSetErrorLight();
        return false;
    }

    Serial.println("Second fingerprint processed.");


    // STEP 6: Compare both captures

    Serial.println("Comparing both fingerprint captures...");

    response = finger.createModel();

    if (response == FINGERPRINT_ENROLLMISMATCH)
    {
        Serial.println("Fingerprint mismatch!");
        Serial.println("Please use the same finger again.");

        fingerprintSetErrorLight();
        return false;
    }

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Fingerprint model creation error: 0x");
        Serial.println(response, HEX);

        fingerprintSetErrorLight();
        return false;
    }

    Serial.println("Fingerprint matching successful.");


    // STEP 7: Save fingerprint in R503

    Serial.println("Saving fingerprint in R503 library...");

    response = finger.storeModel(slotId);

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Fingerprint storage failed: 0x");
        Serial.println(response, HEX);

        fingerprintSetErrorLight();
        return false;
    }


    // STEP 8: Enrollment successful

    Serial.println();
    Serial.println("================================");
    Serial.println("FINGERPRINT SAVED SUCCESSFULLY");
    Serial.println("================================");

    Serial.print("Saved fingerprint slot: ");
    Serial.println(slotId);

    attendanceWaitingForRemoval = true;

    finger.LEDcontrol(
        FINGERPRINT_LED_FLASHING,
        30,
        FINGERPRINT_LED_PURPLE,
        3
    );

    delay(1000);

    fingerprintSetIdleLight();

    return true;
}


// ==================================================
// GET LAST FINGERPRINT SCAN STATUS
// ==================================================

FingerprintScanStatus fingerprintGetLastScanStatus()
{
    return lastScanStatus;
}


// ==================================================
// IDENTIFY FINGERPRINT FOR ATTENDANCE
// ==================================================

bool fingerprintIdentify(uint16_t &matchedSlotId)
{
    matchedSlotId = 0;

    lastScanStatus = FINGERPRINT_SCAN_IDLE;

    uint32_t currentTime = millis();

    uint8_t response;


    // Poll every 100 ms

    if ((uint32_t)(currentTime - lastFingerprintPollMs) < 100)
    {
        return false;
    }

    lastFingerprintPollMs = currentTime;


    // WAIT FOR PREVIOUS FINGER REMOVAL

    if (attendanceWaitingForRemoval)
    {
        response = finger.getImage();

        if (response == FINGERPRINT_NOFINGER)
        {
            attendanceWaitingForRemoval = false;

            Serial.println();
            Serial.println("Finger removed successfully.");
            Serial.println("Scanner ready for next fingerprint.");

            fingerprintSetIdleLight();
        }
        else if (response == FINGERPRINT_PACKETRECIEVEERR)
        {
            printCommunicationError(
                "R503 UART communication error while waiting for finger removal."
            );

            if (communicationErrorLedDue())
            {
                lastScanStatus = FINGERPRINT_SCAN_ERROR;
            }
        }
        else if (response != FINGERPRINT_OK)
        {
            if ((uint32_t)(currentTime - lastSensorErrorMessageMs) >= 2000)
            {
                Serial.print("Sensor removal-check error: 0x");
                Serial.println(response, HEX);

                lastSensorErrorMessageMs = currentTime;
            }

            lastScanStatus = FINGERPRINT_SCAN_ERROR;
        }

        return false;
    }


    // CAPTURE FINGERPRINT IMAGE

    response = finger.getImage();

    if (response == FINGERPRINT_NOFINGER)
    {
        return false;
    }


    // UART communication error

    if (response == FINGERPRINT_PACKETRECIEVEERR)
    {
        printCommunicationError(
            "R503 UART communication error. Check sensor wiring and power."
        );

        if (communicationErrorLedDue())
        {
            lastScanStatus = FINGERPRINT_SCAN_ERROR;
        }

        return false;
    }


    // Other image capture error

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Fingerprint image capture error: 0x");
        Serial.println(response, HEX);

        attendanceWaitingForRemoval = true;

        lastScanStatus = FINGERPRINT_SCAN_ERROR;

        fingerprintSetErrorLight();

        return false;
    }


    attendanceWaitingForRemoval = true;

    Serial.println();
    Serial.println("Fingerprint detected.");


    // PROCESS IMAGE

    response = finger.image2Tz(1);

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Fingerprint processing error: 0x");
        Serial.println(response, HEX);

        if (response == FINGERPRINT_PACKETRECIEVEERR)
        {
            printCommunicationError(
                "R503 UART error during fingerprint processing."
            );
        }

        lastScanStatus = FINGERPRINT_SCAN_ERROR;

        fingerprintSetErrorLight();

        return false;
    }


    // SEARCH LIBRARY

    response = finger.fingerSearch(1);


    // UNKNOWN FINGERPRINT

    if (response == FINGERPRINT_NOTFOUND)
    {
        Serial.println();
        Serial.println("UNKNOWN FINGERPRINT!");
        Serial.println("Access denied.");

        lastScanStatus = FINGERPRINT_SCAN_UNKNOWN;

        fingerprintSetErrorLight();

        return false;
    }


    // SEARCH COMMUNICATION ERROR

    if (response == FINGERPRINT_PACKETRECIEVEERR)
    {
        printCommunicationError(
            "R503 UART error during fingerprint search."
        );

        if (communicationErrorLedDue())
        {
            lastScanStatus = FINGERPRINT_SCAN_ERROR;
        }

        return false;
    }


    // OTHER SEARCH ERROR

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Fingerprint search error: 0x");
        Serial.println(response, HEX);

        lastScanStatus = FINGERPRINT_SCAN_ERROR;

        fingerprintSetErrorLight();

        return false;
    }


    // REGISTERED FINGERPRINT FOUND

    matchedSlotId = finger.fingerID;

    Serial.println();
    Serial.println("================================");
    Serial.println("FINGERPRINT IDENTIFIED SUCCESSFULLY");
    Serial.println("================================");

    Serial.print("Matched fingerprint slot: ");
    Serial.println(matchedSlotId);

    Serial.print("Confidence: ");
    Serial.println(finger.confidence);

    lastScanStatus = FINGERPRINT_SCAN_MATCH;

    // Purple flashing for successful recognition
    finger.LEDcontrol(
        FINGERPRINT_LED_FLASHING,
        30,
        FINGERPRINT_LED_PURPLE,
        2
    );

    return true;
}


// ==================================================
// DELETE FINGERPRINT FROM R503
// ==================================================

bool fingerprintDelete(uint16_t slotId)
{
    if (slotId == 0 || slotId > finger.capacity)
    {
        Serial.println("Invalid fingerprint slot.");
        return false;
    }

    uint8_t response = finger.deleteModel(slotId);

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Fingerprint deletion failed: 0x");
        Serial.println(response, HEX);

        fingerprintSetIdleLight();

        return false;
    }

    Serial.print("Fingerprint deleted from slot: ");
    Serial.println(slotId);

    fingerprintSetIdleLight();

    return true;
}
