
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


// Fingerprint sensor object
static Adafruit_Fingerprint finger(&fingerprintSerial);


// ==================================================
// INTERNAL VARIABLES
// ==================================================

// Attendance scanner waits for finger removal
static bool attendanceWaitingForRemoval = false;


// Internal helper functions
static bool waitForFinger(
    uint32_t timeoutMs,
    const char *message
);

static bool waitForFingerRemoval(uint32_t timeoutMs);


// ==================================================
// WAIT FOR FINGER TO BE PLACED
// Used during enrollment
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

        Serial.print("Fingerprint image communication/error code: 0x");
        Serial.println(response, HEX);

        return false;
    }

    Serial.println("Fingerprint capture timeout.");

    return false;
}


// ==================================================
// WAIT FOR FINGER TO BE REMOVED
// Used during enrollment only
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
    Serial.println("Starting R503 fingerprint sensor...");

    fingerprintSerial.begin(FINGERPRINT_BAUD);

    finger.begin(FINGERPRINT_BAUD);

    delay(100);

    attendanceWaitingForRemoval = false;

    // Verify sensor communication/password
    if (!finger.verifyPassword())
    {
        Serial.println("R503 communication failed.");
        Serial.println("Check TX, RX, VCC and GND.");

        return false;
    }

    Serial.println("R503 communication successful.");

    // Read sensor parameters
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
// SET IDLE LED
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
// NO SEARCH COMMAND DURING ENROLLMENT
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

    // Check slot number
    if (slotId == 0 || slotId > finger.capacity)
    {
        Serial.println("Invalid fingerprint slot.");
        return false;
    }

    // Start blinking LED
    finger.LEDcontrol(
        FINGERPRINT_LED_FLASHING,
        40,
        FINGERPRINT_LED_BLUE,
        0
    );

    // STEP 1: Capture first fingerprint
    if (!waitForFinger(
            30000,
            "Place the employee's finger on the sensor."))
    {
        Serial.println("First fingerprint capture failed.");

        fingerprintSetIdleLight();

        return false;
    }

    Serial.println("First fingerprint captured.");

    // STEP 2: Convert first fingerprint image
    response = finger.image2Tz(1);

    if (response != FINGERPRINT_OK)
    {
        Serial.print("First fingerprint processing failed: 0x");
        Serial.println(response, HEX);

        fingerprintSetIdleLight();

        return false;
    }

    Serial.println("First fingerprint processed.");

    // STEP 3: Wait for finger removal
    if (!waitForFingerRemoval(15000))
    {
        fingerprintSetIdleLight();

        return false;
    }

    // STEP 4: Capture second fingerprint
    if (!waitForFinger(
            30000,
            "Place the SAME finger again."))
    {
        Serial.println("Second fingerprint capture failed.");

        fingerprintSetIdleLight();

        return false;
    }

    Serial.println("Second fingerprint captured.");

    // STEP 5: Convert second fingerprint image
    response = finger.image2Tz(2);

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Second fingerprint processing failed: 0x");
        Serial.println(response, HEX);

        // Do not allow attendance scanning until
        // the currently placed finger is removed.
        attendanceWaitingForRemoval = true;

        fingerprintSetIdleLight();

        return false;
    }

    Serial.println("Second fingerprint processed.");

    // STEP 6: Compare both fingerprint images
    Serial.println("Comparing both fingerprint captures...");

    response = finger.createModel();

    if (response == FINGERPRINT_ENROLLMISMATCH)
    {
        Serial.println("Fingerprint mismatch!");
        Serial.println("Please use the same finger again.");

        attendanceWaitingForRemoval = true;

        finger.LEDcontrol(
            FINGERPRINT_LED_FLASHING,
            30,
            FINGERPRINT_LED_RED,
            3
        );

        delay(500);

        fingerprintSetIdleLight();

        return false;
    }

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Fingerprint model creation error: 0x");
        Serial.println(response, HEX);

        attendanceWaitingForRemoval = true;

        fingerprintSetIdleLight();

        return false;
    }

    Serial.println("Fingerprint matching successful.");

    // STEP 7: Save fingerprint model in R503
    Serial.println("Saving fingerprint in R503 library...");

    response = finger.storeModel(slotId);

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Fingerprint storage failed: 0x");
        Serial.println(response, HEX);

        attendanceWaitingForRemoval = true;

        fingerprintSetIdleLight();

        return false;
    }

    // STEP 8: Registration successful
    Serial.println();
    Serial.println("================================");
    Serial.println("FINGERPRINT SAVED SUCCESSFULLY");
    Serial.println("================================");

    Serial.print("Saved fingerprint slot: ");
    Serial.println(slotId);

    // Require removal before the attendance scanner
    // can recognize another fingerprint.
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
// IDENTIFY FINGERPRINT FOR ATTENDANCE
//
// Uses STANDARD fingerSearch(1).
// Does NOT use fingerFastSearch().
//
// Non-blocking finger-removal detection.
// ==================================================

bool fingerprintIdentify(uint16_t &matchedSlotId)
{
    matchedSlotId = 0;

    uint8_t response;


    // ==============================================
    // WAIT UNTIL THE PREVIOUS FINGER IS REMOVED
    // ==============================================

    if (attendanceWaitingForRemoval)
    {
        response = finger.getImage();

        if (response == FINGERPRINT_NOFINGER)
        {
            attendanceWaitingForRemoval = false;

            Serial.println("Finger removed successfully.");
            Serial.println("Scanner ready for next fingerprint.");

            fingerprintSetIdleLight();
        }

        // Do not search again while the same finger
        // is still placed on the sensor.
        return false;
    }


    // ==============================================
    // STEP 1: CAPTURE FINGERPRINT IMAGE
    // ==============================================

    response = finger.getImage();

    if (response == FINGERPRINT_NOFINGER)
    {
        return false;
    }

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Attendance image error: 0x");
        Serial.println(response, HEX);

        return false;
    }


    // Finger has been detected.
    // Lock scanning until this finger is removed.
    attendanceWaitingForRemoval = true;


    Serial.println();
    Serial.println("Fingerprint detected.");


    // ==============================================
    // STEP 2: CONVERT FINGERPRINT IMAGE
    // ==============================================

    response = finger.image2Tz(1);

    if (response != FINGERPRINT_OK)
    {
        Serial.print("Attendance fingerprint processing error: 0x");
        Serial.println(response, HEX);

        return false;
    }


    // ==============================================
    // STEP 3: SEARCH SAVED FINGERPRINT LIBRARY
    // ==============================================

    // Standard search command for R503 compatibility
    response = finger.fingerSearch(1);


    if (response == FINGERPRINT_NOTFOUND)
    {
        Serial.println("Fingerprint not registered.");

        return false;
    }


    if (response != FINGERPRINT_OK)
    {
        Serial.print("Fingerprint search communication error: 0x");
        Serial.println(response, HEX);

        return false;
    }


    // ==============================================
    // STEP 4: MATCHING FINGERPRINT FOUND
    // ==============================================

    matchedSlotId = finger.fingerID;

    Serial.println();
    Serial.println("================================");
    Serial.println("FINGERPRINT IDENTIFIED SUCCESSFULLY");
    Serial.println("================================");

    Serial.print("Matched fingerprint slot: ");
    Serial.println(matchedSlotId);

    Serial.print("Confidence: ");
    Serial.println(finger.confidence);


    // Blink purple LED for successful identification
    finger.LEDcontrol(
        FINGERPRINT_LED_FLASHING,
        30,
        FINGERPRINT_LED_PURPLE,
        2
    );


    // Do not wait here for finger removal.
    // The next calls to fingerprintIdentify()
    // will check for removal without blocking.

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
