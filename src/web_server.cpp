
#include "web_server.h"
#include "eeprom.h"
#include "config.h"

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <time.h>

static ESP8266WebServer server(80);

static bool littleFsReady = false;


// =====================================================
// WEBSITE HTML
// =====================================================

static const char DASHBOARD_PAGE[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html lang="en">

<head>

<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>LAKSHIV TECHNOLOGY &amp; SERVICES PVT LIMITED</title>

<style>

:root {
    --navy: #082d68;
    --blue: #075bd8;
    --ink: #102d59;
    --muted: #59708f;
}

* {
    box-sizing: border-box;
}

body {
    margin: 0;
    min-height: 100vh;
    font-family: Arial, Helvetica, sans-serif;
    color: var(--ink);
    background: #eaf3ff;
}


/* =========================================
   COMPANY BUILDING HEADER
========================================= */

.hero {
    min-height: 230px;
    padding: 25px 4.5%;

    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 20px;

    background-color: #0b3471;

    background-image:
        linear-gradient(
            90deg,
            rgba(225, 241, 255, .96) 0%,
            rgba(225, 241, 255, .87) 40%,
            rgba(7, 37, 83, .15) 100%
        ),
        url('/building.jpg');

    background-size: cover;
    background-position: center;
    background-repeat: no-repeat;

    border-bottom: 3px solid #58aaff;
}

.brand {
    display: flex;
    align-items: center;
    gap: 18px;
    min-width: 0;
}

.brand-mark {
    width: 72px;
    height: 72px;
    flex: 0 0 72px;

    display: grid;
    place-items: center;

    border: 3px solid #1761bd;
    border-radius: 15px;

    color: white;
    background: linear-gradient(145deg, #0b65d9, #092b62);

    box-shadow: 0 5px 16px rgba(0, 51, 130, .25);

    font-size: 27px;
    font-weight: 800;
}

.brand-copy h2 {
    margin: 0;
    max-width: 780px;

    color: #082b62;

    font-size: clamp(20px, 2.5vw, 34px);
    line-height: 1.15;
    font-weight: 800;
    letter-spacing: .2px;
    text-shadow: 0 1px 2px rgba(255,255,255,.4);
}

.tagline {
    margin: 12px 0 0;

    color: #1252a0;
    font-size: 17px;
    font-weight: 700;
    font-style: italic;
    letter-spacing: .7px;
}

.hero-right {
    min-width: 175px;
    text-align: right;
    color: white;
}

.device-badge {
    display: inline-flex;
    align-items: center;
    gap: 9px;

    padding: 11px 16px;
    border-radius: 25px;

    color: white;
    background: #718096;

    font-size: 12px;
    font-weight: 800;

    box-shadow: 0 3px 10px rgba(0,0,0,.2);
}

.device-badge.online {
    background: #078447;
}

.device-badge.offline {
    background: #b45309;
}

.device-dot {
    width: 10px;
    height: 10px;

    border-radius: 50%;
    background: white;
}

.clock-label {
    margin-top: 10px;
    color: #082b62;
    font-size: 13px;
    font-weight: 700;
}


/* =========================================
   NAVIGATION
========================================= */

nav {
    display: flex;
    align-items: center;
    flex-wrap: wrap;
    gap: 8px;

    padding: 11px 4.5%;

    background: linear-gradient(100deg, #042860, #084997);

    box-shadow: 0 5px 18px rgba(0, 27, 70, .22);
}

.nav-btn {
    border: 1px solid transparent;
    border-radius: 9px;

    padding: 12px 22px;

    color: white;
    background: transparent;

    font-size: 14px;
    font-weight: 800;

    cursor: pointer;
    transition: .2s;
}

.nav-btn:hover {
    background: rgba(255,255,255,.13);
}

.nav-btn.active {
    background: #0867ef;
    border-color: #7db8ff;
    box-shadow: 0 3px 12px rgba(0,0,0,.18);
}


/* =========================================
   MAIN CONTENT
========================================= */

main {
    width: min(1550px, 100%);
    margin: 25px auto;
    padding: 0 18px 25px;
}

.page {
    display: none;
}

.page.active {
    display: block;
}

.glass-panel {
    margin-bottom: 17px;
    padding: 22px;

    border: 1px solid white;
    border-radius: 17px;

    background: rgba(245, 250, 255, .91);

    box-shadow: 0 7px 24px rgba(0, 42, 105, .18);

    backdrop-filter: blur(9px);
    -webkit-backdrop-filter: blur(9px);
}


/* =========================================
   DASHBOARD HEADING
========================================= */

.page-heading {
    display: flex;
    align-items: center;
    gap: 14px;

    padding: 2px 4px 15px;
    margin-bottom: 19px;

    border-bottom: 2px solid #7eb7ff;
}

.heading-icon {
    width: 54px;
    height: 54px;

    display: grid;
    place-items: center;

    border-radius: 12px;

    color: white;
    background: linear-gradient(145deg, #1675e8, #073f9d);

    font-size: 28px;
}

.page-heading h1 {
    margin: 0 0 5px;
    color: #082f70;
    font-size: 25px;
}

.page-heading p {
    margin: 0;
    color: #284b7b;
    font-size: 14px;
}


/* =========================================
   DASHBOARD SUMMARY CARDS
========================================= */

.card-grid {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 17px;
}

.stat-card {
    min-height: 140px;
    padding: 18px;

    display: flex;
    align-items: center;
    gap: 15px;

    border: 1px solid white;
    border-radius: 16px;

    background: rgba(222, 239, 255, .96);

    box-shadow: 0 5px 16px rgba(0, 51, 120, .14);
}

.stat-icon {
    width: 68px;
    height: 68px;
    flex: 0 0 68px;

    display: grid;
    place-items: center;

    border-radius: 50%;
    background: rgba(49, 151, 240, .18);

    font-size: 32px;
}

.stat-copy {
    min-width: 0;
}

.stat-label {
    margin-bottom: 8px;

    color: #0755bd;

    font-size: 13px;
    font-weight: 800;
    text-transform: uppercase;
}

.stat-value {
    color: #0752b8;

    font-size: 38px;
    line-height: 1;
    font-weight: 800;
}

.stat-note {
    margin-top: 7px;
    color: #24436c;
    font-size: 12px;
}


/* GREEN CARD */

.stat-card.green {
    background: rgba(225, 250, 239, .97);
    border-color: #80dfb2;
}

.stat-card.green .stat-icon {
    background: rgba(30, 184, 107, .17);
}

.stat-card.green .stat-label,
.stat-card.green .stat-value {
    color: #07864d;
}


/* PURPLE CARD */

.stat-card.purple {
    background: rgba(239, 233, 255, .97);
    border-color: #bba1ff;
}

.stat-card.purple .stat-icon {
    background: rgba(119, 78, 230, .15);
}

.stat-card.purple .stat-label,
.stat-card.purple .stat-value {
    color: #6231c7;
}


/* ORANGE CARD */

.stat-card.orange {
    background: rgba(255, 239, 224, .97);
    border-color: #ffc184;
}

.stat-card.orange .stat-icon {
    background: rgba(240, 138, 36, .17);
}

.stat-card.orange .stat-label,
.stat-card.orange .stat-value {
    color: #c75b0a;
}


/* =========================================
   COMPANY INFORMATION ON DASHBOARD
========================================= */

.company-message {
    margin-top: 18px;
    padding: 25px;

    border-radius: 14px;

    color: white;

    background:
        linear-gradient(
            110deg,
            rgba(5, 42, 94, .96),
            rgba(11, 90, 173, .90)
        );

    box-shadow: 0 5px 18px rgba(0, 42, 105, .18);
}

.company-message h2 {
    margin: 0 0 10px;
    font-size: 22px;
}

.company-message p {
    margin: 0;
    color: #e1efff;
    font-size: 14px;
    line-height: 1.7;
}

.company-message .company-motto {
    margin-top: 13px;
    font-weight: 700;
    font-style: italic;
    color: #ffffff;
}


/* =========================================
   EMPLOYEE AND ATTENDANCE TABLES
========================================= */

.table-panel {
    padding: 0;
    overflow: hidden;

    border: 1px solid white;
    border-radius: 15px;

    background: rgba(245, 250, 255, .95);

    box-shadow: 0 6px 19px rgba(0, 39, 98, .16);
}

.table-heading {
    display: flex;
    align-items: center;
    justify-content: space-between;

    gap: 12px;
    flex-wrap: wrap;

    padding: 15px 17px;

    color: white;
    background: linear-gradient(100deg, #073b82, #176fc5);
}

.table-heading h2 {
    margin: 0 0 4px;
    font-size: 19px;
}

.table-heading p {
    margin: 0;
    color: #e0efff;
    font-size: 12px;
}

.controls {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
}

input[type="search"] {
    width: 245px;
    max-width: 100%;

    padding: 10px 12px;

    border: 1px solid #b8cce4;
    border-radius: 9px;

    background: white;
    color: #17345e;

    font-size: 13px;
}

.action-btn {
    padding: 10px 17px;

    border: 1px solid #8ec2ff;
    border-radius: 9px;

    color: white;
    background: #075cda;

    font-size: 13px;
    font-weight: 800;
    cursor: pointer;
}

.action-btn:hover {
    background: #0649ae;
}

.table-wrap {
    width: 100%;
    overflow-x: auto;
    padding: 5px 7px 8px;
}

table {
    width: 100%;
    min-width: 650px;
    border-collapse: collapse;
}

th, td {
    padding: 12px;

    border: 1px solid #c8d9ed;

    text-align: left;

    font-size: 13px;
    white-space: nowrap;
}

th {
    color: #0a326b;
    background: #dceafb;

    font-size: 12px;
    font-weight: 800;
    text-transform: uppercase;
}

tbody tr:nth-child(even) {
    background: rgba(230, 241, 253, .72);
}

tbody tr:hover {
    background: #e5f1ff;
}

.empty {
    padding: 23px;
    color: #60738d;
    text-align: center;
}


/* =========================================
   ATTENDANCE STATUS
========================================= */

.status-pill {
    display: inline-block;
    min-width: 105px;

    padding: 6px 11px;

    border-radius: 20px;

    text-align: center;

    font-size: 11px;
    font-weight: 800;
}

.status-progress {
    color: #674000;
    background: #ffc45c;
}

.status-complete {
    color: #005d3d;
    background: #67e5b5;
}


/* =========================================
   INFORMATION
========================================= */

.info {
    padding: 10px 14px;
    color: #526985;
    font-size: 12px;
}


/* =========================================
   FOOTER
========================================= */

footer {
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 12px;

    padding: 19px 4.5%;

    color: white;
    background: linear-gradient(100deg, #06245a, #0754b7);

    border-top: 3px solid #5cb4ff;

    font-size: 13px;
}

.footer-name {
    font-weight: 800;
    letter-spacing: .3px;
}

.footer-tagline {
    font-style: italic;
    color: #e1efff;
}


/* =========================================
   MOBILE RESPONSIVE DESIGN
========================================= */

@media (max-width: 1050px) {

    .card-grid {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
}

@media (max-width: 600px) {

    .hero {
        min-height: 210px;
        align-items: flex-start;
        flex-direction: column;
        padding: 20px 15px;
    }

    .brand {
        gap: 11px;
    }

    .brand-mark {
        width: 53px;
        height: 53px;
        flex-basis: 53px;
        font-size: 21px;
    }

    .brand-copy h2 {
        font-size: 19px;
    }

    .tagline {
        font-size: 13px;
    }

    .hero-right {
        width: 100%;
        text-align: left;
    }

    nav {
        padding: 9px;
    }

    .nav-btn {
        flex: 1 1 100%;
        text-align: left;
    }

    main {
        padding: 0 10px 20px;
        margin-top: 13px;
    }

    .glass-panel {
        padding: 14px;
    }

    .card-grid {
        grid-template-columns: 1fr;
        gap: 11px;
    }

    .stat-card {
        min-height: 105px;
    }

    .page-heading h1 {
        font-size: 20px;
    }

    .table-heading {
        align-items: flex-start;
    }

    .controls {
        width: 100%;
    }

    input[type="search"] {
        width: 100%;
    }

    .action-btn {
        flex: 1;
    }

    footer {
        align-items: flex-start;
        flex-direction: column;
    }
}

</style>

</head>


<body>


<!-- =========================================
     COMPANY BUILDING AND NAME
========================================= -->

<header class="hero">

    <div class="brand">

        <div class="brand-mark">
            LT
        </div>

        <div class="brand-copy">

            <h2>
                LAKSHIV TECHNOLOGY &amp; SERVICES PVT LIMITED.
            </h2>

            <p class="tagline">
                Build &nbsp;•&nbsp; Innovate &nbsp;•&nbsp; Grow
            </p>

        </div>

    </div>


    <div class="hero-right">

        <div id="deviceBadge" class="device-badge offline">

            <span class="device-dot"></span>

            <span id="deviceText">
                CONNECTING
            </span>

        </div>

        <div id="deviceClock" class="clock-label">
            --
        </div>

    </div>

</header>


<!-- =========================================
     THREE NAVIGATION BUTTONS
========================================= -->

<nav>

    <button
        class="nav-btn active"
        onclick="showPage('dashboardPage', this)">

        ⌂ &nbsp; Dashboard

    </button>


    <button
        class="nav-btn"
        onclick="showPage('employeePage', this)">

        ♟ &nbsp; Employee Details

    </button>


    <button
        class="nav-btn"
        onclick="showPage('attendancePage', this)">

        ▦ &nbsp; EMP Attendance

    </button>

</nav>


<main>


<!-- =========================================
     PAGE 1: DASHBOARD
========================================= -->

<section id="dashboardPage" class="page active">

    <div class="glass-panel">

        <div class="page-heading">

            <div class="heading-icon">
                ▥
            </div>

            <div>

                <h1>
                    Attendance Dashboard
                </h1>

                <p>
                    Company employee and attendance overview.
                </p>

            </div>

        </div>


        <!-- FOUR DASHBOARD SUMMARY CARDS -->

        <div class="card-grid">


            <!-- REGISTERED EMPLOYEES -->

            <div class="stat-card">

                <div class="stat-icon">
                    👥
                </div>

                <div class="stat-copy">

                    <div class="stat-label">
                        Registered Employees
                    </div>

                    <div class="stat-value" id="totalEmployees">
                        --
                    </div>

                    <div class="stat-note">
                        Active employee profiles
                    </div>

                </div>

            </div>


            <!-- TODAY'S ATTENDANCE -->

            <div class="stat-card green">

                <div class="stat-icon">
                    ▣
                </div>

                <div class="stat-copy">

                    <div class="stat-label">
                        Today's Attendance
                    </div>

                    <div class="stat-value" id="todayAttendance">
                        --
                    </div>

                    <div class="stat-note">
                        Unique employee entries today
                    </div>

                </div>

            </div>


            <!-- EMPLOYEES IN -->

            <div class="stat-card purple">

                <div class="stat-icon">
                    ✓
                </div>

                <div class="stat-copy">

                    <div class="stat-label">
                        Employees IN
                    </div>

                    <div class="stat-value" id="employeesIn">
                        --
                    </div>

                    <div class="stat-note">
                        Awaiting OUT scan
                    </div>

                </div>

            </div>


            <!-- EMPLOYEES OUT -->

            <div class="stat-card orange">

                <div class="stat-icon">
                    ▤
                </div>

                <div class="stat-copy">

                    <div class="stat-label">
                        Employees OUT
                    </div>

                    <div class="stat-value" id="employeesOut">
                        --
                    </div>

                    <div class="stat-note">
                        OUT successfully recorded
                    </div>

                </div>

            </div>

        </div>


        <div id="summaryInfo" class="info">
            Connecting to attendance device...
        </div>


        <!-- COMPANY INFORMATION -->

        <div class="company-message">

            <h2>
                Welcome to LAKSHIV TECHNOLOGY &amp; SERVICES
            </h2>

            <p>
                Our fingerprint-based employee attendance management
                system provides a simple and efficient way to manage
                employee attendance, registration and daily working hours.
            </p>

            <p class="company-motto">
                Build • Innovate • Grow — Together Towards a Smarter Tomorrow.
            </p>

        </div>

    </div>

</section>


<!-- =========================================
     PAGE 2: EMPLOYEE DETAILS
========================================= -->

<section id="employeePage" class="page">

    <section class="table-panel">

        <div class="table-heading">

            <div>

                <h2>
                    ♟ &nbsp; Employee Details
                </h2>

                <p>
                    Registered employee information stored in external EEPROM
                </p>

            </div>

            <div class="controls">

                <input
                    id="employeeSearch"
                    type="search"
                    placeholder="Search employee or ID">

                <button
                    class="action-btn"
                    onclick="loadEmployees()">

                    Refresh

                </button>

            </div>

        </div>


        <div class="table-wrap">

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

</section>


<!-- =========================================
     PAGE 3: EMP ATTENDANCE
========================================= -->

<section id="attendancePage" class="page">

    <section class="table-panel">

        <div class="table-heading">

            <div>

                <h2>
                    ▦ &nbsp; EMP Attendance
                </h2>

                <p>
                    Employee IN time, OUT time and total working duration
                </p>

            </div>

            <div class="controls">

                <input
                    id="attendanceSearch"
                    type="search"
                    placeholder="Search employee or ID">

                <button
                    class="action-btn"
                    onclick="loadRecords()">

                    Refresh

                </button>

            </div>

        </div>


        <div class="table-wrap">

            <table>

                <thead>

                    <tr>

                        <th>Employee Name</th>
                        <th>Employee ID</th>
                        <th>Date</th>
                        <th>IN Time</th>
                        <th>OUT Time</th>
                        <th>Status</th>
                        <th>Total Duration</th>

                    </tr>

                </thead>


                <tbody id="attendanceRows">

                    <tr>

                        <td class="empty" colspan="7">
                            Loading attendance records...
                        </td>

                    </tr>

                </tbody>

            </table>

        </div>


        <div id="attendanceInfo" class="info">
            Waiting for attendance data...
        </div>

    </section>

</section>


</main>


<!-- =========================================
     FOOTER
========================================= -->

<footer>

    <div class="footer-name">
        LT &nbsp; | &nbsp;
        LAKSHIV TECHNOLOGY &amp; SERVICES PVT LIMITED.
    </div>

    <div class="footer-tagline">
        Together Towards a Smarter Tomorrow...
    </div>

</footer>


<!-- =========================================
     JAVASCRIPT
========================================= -->

<script>

let allEmployees = [];
let allAttendance = [];

let employeeLoading = false;
let attendanceLoading = false;
let summaryLoading = false;


// =====================================================
// DEVICE STATUS
// =====================================================

function setDeviceStatus(online)
{
    const badge =
        document.getElementById('deviceBadge');

    const text =
        document.getElementById('deviceText');

    badge.classList.toggle('online', online);
    badge.classList.toggle('offline', !online);

    text.textContent =
        online ? 'DEVICE CONNECTED' : 'DEVICE OFFLINE';
}


// =====================================================
// CLOCK
// =====================================================

function updateClock()
{
    document.getElementById('deviceClock').textContent =
        '◷  ' + new Date().toLocaleString('en-IN');
}


// =====================================================
// PAGE NAVIGATION
// =====================================================

function showPage(pageId, button)
{
    document
        .querySelectorAll('.page')
        .forEach(page =>
            page.classList.remove('active')
        );

    document
        .querySelectorAll('.nav-btn')
        .forEach(navButton =>
            navButton.classList.remove('active')
        );

    document
        .getElementById(pageId)
        .classList.add('active');

    button.classList.add('active');


    if (pageId === 'employeePage')
    {
        loadEmployees();
    }

    if (pageId === 'attendancePage')
    {
        loadRecords();
    }

    if (pageId === 'dashboardPage')
    {
        loadSummary();
    }
}


// =====================================================
// GET JSON DATA
// =====================================================

async function getJSON(url)
{
    const response = await fetch(
        url,
        { cache: 'no-store' }
    );

    if (!response.ok)
    {
        throw new Error('HTTP ' + response.status);
    }

    return await response.json();
}


// =====================================================
// LOAD DASHBOARD SUMMARY
// =====================================================

async function loadSummary()
{
    if (summaryLoading)
    {
        return;
    }

    summaryLoading = true;

    const info =
        document.getElementById('summaryInfo');

    try
    {
        const data =
            await getJSON('/api/summary');


        document.getElementById('totalEmployees').textContent =
            data.totalEmployees;


        if (data.clockReady)
        {
            document.getElementById('todayAttendance').textContent =
                data.attendanceRecords;

            document.getElementById('employeesIn').textContent =
                data.employeesIn;

            document.getElementById('employeesOut').textContent =
                data.employeesOut;

            info.textContent =
                'Dashboard updated: ' +
                new Date().toLocaleTimeString();
        }
        else
        {
            document.getElementById('todayAttendance').textContent = '--';
            document.getElementById('employeesIn').textContent = '--';
            document.getElementById('employeesOut').textContent = '--';

            info.textContent =
                'Attendance counts are unavailable because the ESP8266 clock is not synchronized.';
        }

        setDeviceStatus(true);
    }
    catch(error)
    {
        info.textContent =
            'Could not load dashboard: ' + error.message;

        setDeviceStatus(false);
    }

    summaryLoading = false;
}


// =====================================================
// CREATE TABLE CELL
// =====================================================

function createCell(row, value)
{
    const cell = document.createElement('td');

    cell.textContent =
        value == null ? '' : String(value);

    row.appendChild(cell);
}


// =====================================================
// EMPLOYEE TABLE RENDERING
// =====================================================

function renderEmployeeTable(targetId, query)
{
    const tbody =
        document.getElementById(targetId);

    const search =
        (query || '').trim().toLowerCase();

    const filtered =
        allEmployees.filter(employee =>
        {
            return (
                String(employee.employeeName) +
                ' ' +
                String(employee.employeeId)
            )
            .toLowerCase()
            .includes(search);
        });

    tbody.innerHTML = '';

    if (filtered.length === 0)
    {
        tbody.innerHTML =
            '<tr><td class="empty" colspan="3">No registered employees found.</td></tr>';

        return;
    }

    filtered.forEach(employee =>
    {
        const row = document.createElement('tr');

        createCell(row, employee.employeeName);
        createCell(row, employee.employeeId);
        createCell(row, employee.fingerprintSlot);

        tbody.appendChild(row);
    });
}


// =====================================================
// DISPLAY EMPLOYEES
// =====================================================

function renderEmployees()
{
    renderEmployeeTable(
        'employeeRows',
        document.getElementById('employeeSearch').value
    );
}


// =====================================================
// LOAD EMPLOYEES
// =====================================================

async function loadEmployees()
{
    if (employeeLoading)
    {
        return;
    }

    employeeLoading = true;

    const info =
        document.getElementById('employeeInfo');

    try
    {
        allEmployees =
            await getJSON('/api/employees');

        renderEmployees();

        document.getElementById('totalEmployees').textContent =
            allEmployees.length;

        info.textContent =
            'Registered employees: ' +
            allEmployees.length +
            ' | Updated: ' +
            new Date().toLocaleTimeString();

        setDeviceStatus(true);
    }
    catch(error)
    {
        info.textContent =
            'Could not load employees: ' + error.message;

        setDeviceStatus(false);
    }

    employeeLoading = false;
}


// =====================================================
// DURATION CALCULATION
// =====================================================

function durationText(seconds)
{
    seconds = Number(seconds || 0);

    const hours =
        Math.floor(seconds / 3600);

    const minutes =
        Math.floor((seconds % 3600) / 60);

    return hours + 'h ' + minutes + 'm';
}


// =====================================================
// ATTENDANCE STATUS
// =====================================================

function createStatusCell(row, record)
{
    const cell = document.createElement('td');

    const pill = document.createElement('span');

    const completed =
        record.outTime && record.outTime !== '--';

    pill.className =
        'status-pill ' +
        (completed ? 'status-complete' : 'status-progress');

    pill.textContent =
        completed ? 'EMPLOYEE OUT' : 'EMPLOYEE IN';

    cell.appendChild(pill);

    row.appendChild(cell);
}


// =====================================================
// RENDER ATTENDANCE TABLE
// =====================================================

function renderAttendance()
{
    const query =
        document
            .getElementById('attendanceSearch')
            .value
            .trim()
            .toLowerCase();

    const tbody =
        document.getElementById('attendanceRows');

    const filtered =
        allAttendance.filter(record =>
        {
            return (
                String(record.employeeName) +
                ' ' +
                String(record.employeeId)
            )
            .toLowerCase()
            .includes(query);
        });

    tbody.innerHTML = '';

    if (filtered.length === 0)
    {
        tbody.innerHTML =
            '<tr><td class="empty" colspan="7">No attendance records found.</td></tr>';

        return;
    }

    filtered.forEach(record =>
    {
        const row = document.createElement('tr');

        createCell(row, record.employeeName);
        createCell(row, record.employeeId);
        createCell(row, record.date);
        createCell(row, record.inTime);
        createCell(row, record.outTime);

        createStatusCell(row, record);

        createCell(
            row,
            durationText(record.durationSeconds)
        );

        tbody.appendChild(row);
    });
}


// =====================================================
// LOAD ATTENDANCE RECORDS
// =====================================================

async function loadRecords()
{
    if (attendanceLoading)
    {
        return;
    }

    attendanceLoading = true;

    const info =
        document.getElementById('attendanceInfo');

    try
    {
        allAttendance =
            await getJSON('/api/attendance');

        renderAttendance();

        info.textContent =
            'Attendance records updated: ' +
            new Date().toLocaleTimeString();

        setDeviceStatus(true);
    }
    catch(error)
    {
        info.textContent =
            'Could not load attendance: ' + error.message;

        setDeviceStatus(false);
    }

    attendanceLoading = false;
}


// =====================================================
// SEARCH EVENTS
// =====================================================

document
    .getElementById('employeeSearch')
    .addEventListener('input', renderEmployees);

document
    .getElementById('attendanceSearch')
    .addEventListener('input', renderAttendance);


// =====================================================
// REFRESH DATA
// =====================================================

async function refreshAll()
{
    await Promise.all([
        loadSummary(),
        loadEmployees(),
        loadRecords()
    ]);
}


// =====================================================
// INITIALIZE WEBSITE
// =====================================================

updateClock();

setInterval(updateClock, 1000);

refreshAll();

setInterval(refreshAll, 5000);

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

    if (input == nullptr)
    {
        return output;
    }

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
// GET TODAY'S DATE KEY: YYYYMMDD
// =====================================================

static bool getTodayKey(uint32_t &todayKey)
{
    const time_t now = time(nullptr);

    if (now < 1700000000)
    {
        return false;
    }

    struct tm localTime;

    if (localtime_r(&now, &localTime) == nullptr)
    {
        return false;
    }

    todayKey =
        (uint32_t)(localTime.tm_year + 1900) * 10000UL +
        (uint32_t)(localTime.tm_mon + 1) * 100UL +
        (uint32_t)localTime.tm_mday;

    return true;
}


// =====================================================
// FORMAT TIME
// =====================================================

static String formatTime(uint32_t epoch)
{
    if (epoch == 0)
    {
        return "--";
    }

    time_t value = (time_t)epoch;

    struct tm localTime;
    char buffer[20];

    if (localtime_r(&value, &localTime) == nullptr)
    {
        return "--";
    }

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
    const uint32_t year =
        dayKey / 10000UL;

    const uint32_t month =
        (dayKey / 100UL) % 100UL;

    const uint32_t day =
        dayKey % 100UL;

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
// DASHBOARD SUMMARY API
// URL: /api/summary
// =====================================================

static void handleSummaryApi()
{
    uint16_t totalEmployees = 0;


    // Count registered employees

    for (uint16_t i = 0; i < MAX_EMPLOYEES; i++)
    {
        EmployeeRecord employee = {};

        if (
            storageReadEmployee(i, employee) &&
            employee.active
        )
        {
            totalEmployees++;
        }
    }


    uint16_t attendanceRecords = 0;
    uint16_t employeesIn = 0;
    uint16_t employeesOut = 0;

    uint32_t todayKey = 0;

    const bool clockReady =
        getTodayKey(todayKey);


    if (clockReady)
    {
        const uint16_t count =
            storageAttendanceCount();

        const uint16_t next =
            storageNextAttendance();


        if (
            count > MAX_ATTENDANCE_RECORDS ||
            next >= MAX_ATTENDANCE_RECORDS
        )
        {
            server.send(
                500,
                "application/json",
                "{\"error\":\"Invalid attendance storage index\"}"
            );

            return;
        }


        bool counted[MAX_EMPLOYEES] = {};


        // Count each employee once for today

        for (uint16_t offset = 0; offset < count; offset++)
        {
            const uint16_t index =
                (
                    next +
                    MAX_ATTENDANCE_RECORDS -
                    1 -
                    offset
                ) % MAX_ATTENDANCE_RECORDS;


            AttendanceRecord record = {};


            if (
                !storageReadAttendance(index, record) ||
                !record.active
            )
            {
                continue;
            }


            if (
                record.dayKey != todayKey ||
                record.inEpoch == 0
            )
            {
                continue;
            }


            if (record.employeeIndex >= MAX_EMPLOYEES)
            {
                continue;
            }


            if (counted[record.employeeIndex])
            {
                continue;
            }


            EmployeeRecord employee = {};


            if (
                !storageReadEmployee(
                    record.employeeIndex,
                    employee
                ) ||
                !employee.active
            )
            {
                continue;
            }


            counted[record.employeeIndex] = true;

            attendanceRecords++;


            if (record.outEpoch > 0)
            {
                employeesOut++;
            }
            else
            {
                employeesIn++;
            }
        }
    }


    // Build JSON response

    String json;

    json.reserve(180);

    json =
        "{\"totalEmployees\":" +
        String(totalEmployees) +

        ",\"attendanceRecords\":" +
        String(attendanceRecords) +

        ",\"employeesIn\":" +
        String(employeesIn) +

        ",\"employeesOut\":" +
        String(employeesOut) +

        ",\"clockReady\":" +
        String(clockReady ? "true" : "false") +

        "}";


    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}


// =====================================================
// EMPLOYEE DETAILS API
// URL: /api/employees
// =====================================================

static void handleEmployeesApi()
{
    String json;

    json.reserve(10000);

    json = "[";

    uint16_t sent = 0;


    for (uint16_t i = 0; i < MAX_EMPLOYEES; i++)
    {
        EmployeeRecord employee = {};


        if (
            !storageReadEmployee(i, employee) ||
            !employee.active
        )
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
// ATTENDANCE DATABASE API
// URL: /api/attendance
// =====================================================

static void handleAttendanceApi()
{
    String json;

    json.reserve(14000);

    json = "[";


    const uint16_t count =
        storageAttendanceCount();

    const uint16_t next =
        storageNextAttendance();


    if (
        count > MAX_ATTENDANCE_RECORDS ||
        next >= MAX_ATTENDANCE_RECORDS
    )
    {
        server.send(
            500,
            "application/json",
            "{\"error\":\"Invalid attendance storage index\"}"
        );

        return;
    }


    const time_t now = time(nullptr);

    uint16_t sent = 0;


    // Read latest records first

    for (
        uint16_t offset = 0;
        offset < count && sent < WEB_MAX_ROWS;
        offset++
    )
    {
        const uint16_t index =
            (
                next +
                MAX_ATTENDANCE_RECORDS -
                1 -
                offset
            ) % MAX_ATTENDANCE_RECORDS;


        AttendanceRecord record = {};


        if (
            !storageReadAttendance(index, record) ||
            !record.active
        )
        {
            continue;
        }


        if (record.employeeIndex >= MAX_EMPLOYEES)
        {
            continue;
        }


        EmployeeRecord employee = {};


        if (
            !storageReadEmployee(
                record.employeeIndex,
                employee
            ) ||
            !employee.active
        )
        {
            continue;
        }


        // Calculate employee duration

        uint32_t duration = 0;


        if (record.inEpoch > 0)
        {
            if (
                record.outEpoch >= record.inEpoch &&
                record.outEpoch > 0
            )
            {
                duration =
                    record.outEpoch - record.inEpoch;
            }
            else if (now > (time_t)record.inEpoch)
            {
                duration =
                    (uint32_t)now - record.inEpoch;
            }
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
// BUILDING BACKGROUND IMAGE
// File: data/building.jpg
// =====================================================

static void handleBuildingImage()
{
    if (
        !littleFsReady ||
        !LittleFS.exists("/building.jpg")
    )
    {
        server.send(
            404,
            "text/plain",
            "building.jpg not found in LittleFS"
        );

        return;
    }


    File imageFile =
        LittleFS.open("/building.jpg", "r");


    if (!imageFile)
    {
        server.send(
            500,
            "text/plain",
            "Could not open building.jpg"
        );

        return;
    }


    server.sendHeader(
        "Cache-Control",
        "public, max-age=86400"
    );


    server.streamFile(
        imageFile,
        "image/jpeg"
    );


    imageFile.close();
}


// =====================================================
// START WEBSITE SERVER
// =====================================================

void webServerBegin()
{
    // Initialize LittleFS

    littleFsReady = LittleFS.begin();

    if (littleFsReady)
    {
        Serial.println("LittleFS mounted successfully.");
        Serial.println("Building background ready.");
    }
    else
    {
        Serial.println(
            "LittleFS mount failed. Building background unavailable."
        );
    }


    // Main website

    server.on(
        "/",
        HTTP_GET,
        []()
        {
            server.send_P(
                200,
                "text/html; charset=utf-8",
                DASHBOARD_PAGE
            );
        }
    );


    // Building image

    server.on(
        "/building.jpg",
        HTTP_GET,
        handleBuildingImage
    );


    // Dashboard summary

    server.on(
        "/api/summary",
        HTTP_GET,
        handleSummaryApi
    );


    // Employee database

    server.on(
        "/api/employees",
        HTTP_GET,
        handleEmployeesApi
    );


    // Attendance database

    server.on(
        "/api/attendance",
        HTTP_GET,
        handleAttendanceApi
    );


    // Unknown URL

    server.onNotFound(
        []()
        {
            server.send(
                404,
                "text/plain",
                "Not found"
            );
        }
    );


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
