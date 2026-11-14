
#include "web_server.h"
#include "eeprom.h"
#include "config.h"

#include <ESP8266WebServer.h>
#include <time.h>

static ESP8266WebServer server(80);


// =====================================================
// WEBSITE HTML
// =====================================================

static const char DASHBOARD_PAGE[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html lang="en">

<head>

<meta charset="UTF-8">

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>LAKSHIV TECHNOLOGY & SERVICES PVT LIMITED - Employee Attendance</title>

<style>

* {
    box-sizing: border-box;
}

body {
    margin: 0;
    font-family: Arial, sans-serif;
    background: #f3f6fb;
    color: #172033;
}

/* Company header */

header {
    background: #15294b;
    color: white;
    padding: 22px 5%;
}

.company-name {
    margin: 0 0 12px;
    font-size: 21px;
    font-weight: bold;
    letter-spacing: 1px;
    color: #ffffff;
}

header h1 {
    margin: 0;
    font-size: 25px;
}

header p {
    color: #c8d5ea;
    font-size: 14px;
}

/* Main layout */

main {
    max-width: 1200px;
    margin: 25px auto;
    padding: 0 16px;
}

.panel {
    background: white;
    border-radius: 12px;
    padding: 18px;
    box-shadow: 0 3px 14px #17203312;
}

.top {
    display: flex;
    gap: 12px;
    align-items: center;
    justify-content: space-between;
    flex-wrap: wrap;
    margin-bottom: 16px;
}

input {
    padding: 10px 12px;
    border: 1px solid #cbd5e1;
    border-radius: 8px;
    min-width: 230px;
    font-size: 14px;
}

button {
    padding: 10px 14px;
    border: 0;
    border-radius: 8px;
    background: #2457a7;
    color: white;
    cursor: pointer;
}

button:hover {
    background: #194684;
}

/* Tables */

.tablewrap {
    overflow-x: auto;
}

table {
    width: 100%;
    border-collapse: collapse;
    min-width: 650px;
}

th, td {
    text-align: left;
    padding: 13px 11px;
    border-bottom: 1px solid #e8edf4;
    font-size: 14px;
}

th {
    background: #f7f9fc;
    color: #526078;
}

.empty {
    text-align: center;
    padding: 24px;
    color: #64748b;
}

/* Information messages */

.info {
    color: #64748b;
    font-size: 13px;
    margin-top: 12px;
}

/* Section spacing */

.employee-panel {
    margin-bottom: 20px;
}

</style>

</head>

<body>

<!-- COMPANY HEADER -->

<header>

    <h2 class="company-name">
        LAKSHIV TECHNOLOGY & SERVICES PVT LIMITED.
    </h2>

    <h1>Employee Attendance</h1>

    <p>Fingerprint-Based Attendance Management</p>

</header>


<main>


<!-- =================================================
     EMPLOYEE DATABASE
================================================== -->

<section class="panel employee-panel">

    <div class="top">

        <div>

            <h2 style="margin:0 0 5px">
                Employee Database
            </h2>

            <span style="color:#64748b;font-size:13px">
                Registered employees stored in external EEPROM
            </span>

        </div>

        <div>

            <input
                id="employeeSearch"
                type="search"
                placeholder="Search employee or ID">

            <button onclick="loadEmployees()">
                Refresh
            </button>

        </div>

    </div>


    <div class="tablewrap">

        <table>

            <thead>

                <tr>

                    <th>Employee Name</th>

                    <th>Employee ID</th>

                    <th>Fingerprint Slot</th>

                </tr>

            </thead>

            <tbody id="employeeRows">

                <tr>

                    <td class="empty" colspan="3">
                        Loading employees...
                    </td>

                </tr>

            </tbody>

        </table>

    </div>


    <div id="employeeInfo" class="info">
        Waiting for employee data...
    </div>

</section>


<!-- =================================================
     ATTENDANCE RECORDS
================================================== -->

<section class="panel">

    <div class="top">

        <div>

            <h2 style="margin:0 0 5px">
                Attendance Records
            </h2>

            <span style="color:#64748b;font-size:13px">
                Latest 100 records - Auto refresh every 5 seconds
            </span>

        </div>

        <div>

            <input
                id="search"
                type="search"
                placeholder="Search employee or ID">

            <button onclick="loadRecords()">
                Refresh
            </button>

        </div>

    </div>


    <div class="tablewrap">

        <table>

            <thead>

                <tr>

                    <th>Employee Name</th>

                    <th>Employee ID</th>

                    <th>Date</th>

                    <th>IN Time</th>

                    <th>OUT Time</th>

                    <th>Total Duration</th>

                </tr>

            </thead>

            <tbody id="rows">

                <tr>

                    <td class="empty" colspan="6">
                        Loading attendance records...
                    </td>

                </tr>

            </tbody>

        </table>

    </div>


    <div id="info" class="info">
        Waiting for ESP8266...
    </div>

</section>

</main>


<!-- =================================================
     JAVASCRIPT
================================================== -->

<script>

let allEmployees = [];
let allRows = [];


// =====================================================
// EMPLOYEE DATABASE
// =====================================================


// Display registered employees
function renderEmployees()
{
    const query =
        document.getElementById("employeeSearch")
        .value.toLowerCase();

    const rows =
        document.getElementById("employeeRows");

    const filtered = allEmployees.filter(x =>
    {
        return (
            (x.employeeName + " " + x.employeeId)
            .toLowerCase()
            .includes(query)
        );
    });


    if (filtered.length === 0)
    {
        rows.innerHTML =
            '<tr><td class="empty" colspan="3">No registered employees found.</td></tr>';

        return;
    }


    rows.innerHTML = "";


    filtered.forEach(x =>
    {
        const tr = document.createElement("tr");

        [
            x.employeeName,
            x.employeeId,
            x.fingerprintSlot

        ].forEach(value =>
        {
            const td = document.createElement("td");

            td.textContent = value;

            tr.appendChild(td);
        });

        rows.appendChild(tr);
    });
}


// Load employees from ESP8266 EEPROM API
async function loadEmployees()
{
    const info =
        document.getElementById("employeeInfo");

    try
    {
        const response = await fetch(
            "/api/employees",
            { cache: "no-store" }
        );

        if (!response.ok)
        {
            throw new Error("HTTP " + response.status);
        }

        allEmployees = await response.json();

        renderEmployees();

        info.textContent =
            "Registered employees: " +
            allEmployees.length +
            " | Last updated: " +
            new Date().toLocaleTimeString();
    }
    catch(error)
    {
        info.textContent =
            "Could not load employees: " +
            error.message;
    }
}


// Employee search
document
.getElementById("employeeSearch")
.addEventListener("input", renderEmployees);


// =====================================================
// ATTENDANCE RECORDS
// =====================================================


// Convert seconds to hours and minutes
function durationText(seconds)
{
    seconds = Number(seconds || 0);

    let hours = Math.floor(seconds / 3600);

    let minutes = Math.floor(
        (seconds % 3600) / 60
    );

    return hours + "h " + minutes + "m";
}


// Display attendance records
function render()
{
    const query =
        document.getElementById("search")
        .value.toLowerCase();

    const rows =
        document.getElementById("rows");

    const filtered = allRows.filter(x =>
    {
        return (
            (x.employeeName + " " + x.employeeId)
            .toLowerCase()
            .includes(query)
        );
    });


    if (filtered.length === 0)
    {
        rows.innerHTML =
            '<tr><td class="empty" colspan="6">No attendance records found.</td></tr>';

        return;
    }


    rows.innerHTML = "";


    filtered.forEach(x =>
    {
        const tr = document.createElement("tr");

        [
            x.employeeName,
            x.employeeId,
            x.date,
            x.inTime,
            x.outTime,
            durationText(x.durationSeconds)

        ].forEach(value =>
        {
            const td = document.createElement("td");

            td.textContent = value;

            tr.appendChild(td);
        });

        rows.appendChild(tr);
    });
}


// Get attendance data from ESP8266
async function loadRecords()
{
    const info =
        document.getElementById("info");

    try
    {
        const response = await fetch(
            "/api/attendance",
            { cache: "no-store" }
        );

        if (!response.ok)
        {
            throw new Error("HTTP " + response.status);
        }

        allRows = await response.json();

        render();

        info.textContent =
            "Last updated: " +
            new Date().toLocaleTimeString();
    }
    catch(error)
    {
        info.textContent =
            "Could not load records: " +
            error.message;
    }
}


// Attendance search
document
.getElementById("search")
.addEventListener("input", render);


// =====================================================
// AUTOMATIC REFRESH
// =====================================================

// Load both sections when website opens

loadEmployees();
loadRecords();


// Refresh both sections every 5 seconds

setInterval(loadEmployees, 5000);

setInterval(loadRecords, 5000);

</script>

</body>
</html>

)rawliteral";


// =====================================================
// JSON ESCAPE
// =====================================================

static String jsonEscape(const char *input)
{
    String output;

    if (!input)
        return output;

    while (*input)
    {
        const char c = *input++;

        if (c == '"' || c == '\\')
        {
            output += '\\';
            output += c;
        }
        else if (c == '\n')
        {
            output += "\\n";
        }
        else if (c == '\r')
        {
            output += "\\r";
        }
        else if ((uint8_t)c >= 0x20)
        {
            output += c;
        }
    }

    return output;
}


// =====================================================
// FORMAT TIME
// =====================================================

static String formatTime(uint32_t epoch)
{
    if (epoch == 0)
        return "--";

    time_t value = (time_t)epoch;

    struct tm localTime;

    char buffer[20];

    if (localtime_r(&value, &localTime) == nullptr)
        return "--";

    strftime(
        buffer,
        sizeof(buffer),
        "%I:%M:%S %p",
        &localTime
    );

    return String(buffer);
}


// =====================================================
// FORMAT DATE
// =====================================================

static String formatDate(uint32_t dayKey)
{
    const uint32_t year = dayKey / 10000UL;

    const uint32_t month =
        (dayKey / 100UL) % 100UL;

    const uint32_t day = dayKey % 100UL;

    char buffer[16];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02lu-%02lu-%04lu",
        (unsigned long)day,
        (unsigned long)month,
        (unsigned long)year
    );

    return String(buffer);
}


// =====================================================
// EMPLOYEE DATABASE API
// =====================================================

// Read registered employees from external EEPROM
// URL: /api/employees

static void handleEmployeesApi()
{
    String json;

    json.reserve(10000);

    json = "[";

    uint16_t sent = 0;


    // Read all employee slots from EEPROM
    for (
        uint16_t i = 0;
        i < MAX_EMPLOYEES;
        i++
    )
    {
        EmployeeRecord employee;

        if (!storageReadEmployee(i, employee))
        {
            continue;
        }

        if (!employee.active)
        {
            continue;
        }


        if (sent > 0)
        {
            json += ",";
        }


        json +=
            "{\"employeeName\":\"" +
            jsonEscape(employee.name) +

            "\",\"employeeId\":\"" +
            jsonEscape(employee.employeeId) +

            "\",\"fingerprintSlot\":" +
            String(employee.fingerprintId) +

            "}";

        sent++;
    }


    json += "]";


    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}


// =====================================================
// ATTENDANCE API
// =====================================================

// Read attendance records from EEPROM
// URL: /api/attendance

static void handleAttendanceApi()
{
    String json;

    json.reserve(12000);

    json = "[";


    const uint16_t count =
        storageAttendanceCount();

    const uint16_t next =
        storageNextAttendance();

    const time_t now = time(nullptr);

    uint16_t sent = 0;


    // Newest records first
    for (
        uint16_t offset = 0;
        offset < count && sent < WEB_MAX_ROWS;
        offset++
    )
    {
        const uint16_t index =
            (next + MAX_ATTENDANCE_RECORDS - 1 - offset)
            % MAX_ATTENDANCE_RECORDS;


        AttendanceRecord record;

        EmployeeRecord employee;


        if (!storageReadAttendance(index, record) ||
            !record.active)
        {
            continue;
        }


        if (!storageReadEmployee(
                record.employeeIndex,
                employee) ||
            !employee.active)
        {
            continue;
        }


        uint32_t duration = 0;


        if (record.outEpoch > 0 &&
            record.outEpoch >= record.inEpoch)
        {
            duration =
                record.outEpoch - record.inEpoch;
        }
        else if (now > (time_t)record.inEpoch)
        {
            duration =
                (uint32_t)now - record.inEpoch;
        }


        if (sent > 0)
        {
            json += ",";
        }


        json +=
            "{\"employeeName\":\"" +
            jsonEscape(employee.name) +

            "\",\"employeeId\":\"" +
            jsonEscape(employee.employeeId) +

            "\",\"date\":\"" +
            formatDate(record.dayKey) +

            "\",\"inTime\":\"" +
            formatTime(record.inEpoch) +

            "\",\"outTime\":\"" +
            formatTime(record.outEpoch) +

            "\",\"durationSeconds\":" +
            String(duration) +

            "}";

        sent++;
    }


    json += "]";


    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}


// =====================================================
// START WEBSITE SERVER
// =====================================================

void webServerBegin()
{
    // Main website
    server.on("/", HTTP_GET, []()
    {
        server.send_P(
            200,
            "text/html; charset=utf-8",
            DASHBOARD_PAGE
        );
    });


    // Employee database API
    server.on(
        "/api/employees",
        HTTP_GET,
        handleEmployeesApi
    );


    // Attendance records API
    server.on(
        "/api/attendance",
        HTTP_GET,
        handleAttendanceApi
    );


    // Unknown URL
    server.onNotFound([]()
    {
        server.send(
            404,
            "text/plain",
            "Not found"
        );
    });


    server.begin();

    Serial.println("Attendance website server started.");
}


// =====================================================
// PROCESS WEBSITE REQUESTS
// =====================================================

void webServerHandleClient()
{
    server.handleClient();
}
