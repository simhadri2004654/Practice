#pragma once

#include <Arduino.h>

// Wi-Fi credentials
#define WIFI_SSID "SRC 24G"
#define WIFI_PASSWORD "12345678"

// Fallback Wi-Fi access point
#define FALLBACK_AP_SSID "Lakshiv"
#define FALLBACK_AP_PASSWORD "Attend123"

// R503 UART pins
static constexpr uint8_t FINGERPRINT_RX_PIN = 12; // GPIO12 / D6
static constexpr uint8_t FINGERPRINT_TX_PIN = 13; // GPIO13 / D7
static constexpr uint32_t FINGERPRINT_BAUD = 57600;

// AT24C256 I2C
static constexpr uint8_t EEPROM_SDA_PIN = 4; // GPIO4 / D2
static constexpr uint8_t EEPROM_SCL_PIN = 5; // GPIO5 / D1
static constexpr uint8_t EEPROM_I2C_ADDRESS = 0x50;

static constexpr uint32_t EEPROM_SIZE_BYTES = 32768UL;
static constexpr uint8_t EEPROM_PAGE_SIZE = 64;

// Employee and attendance capacity
static constexpr uint16_t MAX_EMPLOYEES = 180;
static constexpr uint16_t MAX_ATTENDANCE_RECORDS = 1500;

static constexpr uint16_t EMPLOYEE_BASE_ADDRESS = 16;
static constexpr uint16_t ATTENDANCE_BASE_ADDRESS = 8000;

// India timezone UTC+5:30
static constexpr long TIME_ZONE_OFFSET_SECONDS = 19800;

// Website
static constexpr uint16_t WEB_MAX_ROWS = 100;