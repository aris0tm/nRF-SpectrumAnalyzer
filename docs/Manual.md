# 2.4 GHz RF Spectrum Monitor 

**Versions:** 3  
**Last Updated:** February 2026  
**Author:** aris0tm ( Harishkumar M ), Zeus0tm ( Balasubramaniam P D )
**Platform:** Arduino + NRF24L01+PA+LNA  

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [System Overview](#2-system-overview)
3. [Hardware Requirements](#3-hardware-requirements)
4. [Software Architecture](#4-software-architecture)
5. [Installation Guide](#5-installation-guide)
6. [Operation Manual](#6-operation-manual)
7. [Technical Specifications](#7-technical-specifications)
8. [Troubleshooting Guide](#8-troubleshooting-guide)
9. [Performance Analysis](#9-performance-analysis)
10. [Legal and Compliance](#10-legal-and-compliance)
11. [Appendices](#11-appendices)

---

## 1. Executive Summary

### 1.1 Purpose

The 2.4 GHz RF Spectrum Monitor is a continuous surveillance system designed to detect, monitor, and alert on radio frequency activity in the 2.4-2.5 GHz ISM band. This system provides real-time detection of:

- **Bluetooth Low Energy (BLE)** devices
- **Bluetooth Classic** communications
- **Wi-Fi (802.11 b/g/n)** networks
- **RF flooding/jamming attacks**
- **Unauthorized wireless transmissions**

### 1.2 Key Features

| Feature | Capability |
|---------|-----------|
| **Frequency Range** | 2400-2525 MHz (126 channels) |
| **Detection Sensitivity** | -64 dBm (NRF24L01 hardware limit) |
| **Scan Modes** | 4 selectable modes via push-button |
| **Real-time Monitoring** | Continuous operation until power removed |
| **Visual Feedback** | Serial output with ASCII bar graphs |
| **Alert System** | Configurable threshold-based alerts |
| **Interface** | USB serial (115200 baud) |

### 1.3 Use Cases

- **Security Auditing**: Detect rogue wireless devices in secure facilities
- **RF Environment Mapping**: Survey 2.4 GHz spectrum usage
- **Interference Detection**: Identify sources of wireless interference
- **Jamming Detection**: Alert on RF flooding attacks
- **IoT Device Discovery**: Locate BLE beacons and sensors
- **Network Analysis**: Monitor Wi-Fi channel congestion

---

## 2. System Overview

### 2.1 System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    USER INTERFACE                        │
│  (Serial Monitor @ 115200 baud + Push Button Control)   │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│              ARDUINO MICROCONTROLLER                     │
│  ┌──────────────────────────────────────────────────┐   │
│  │         Main Control Loop                        │   │
│  │  • Button handler (mode switching)               │   │
│  │  • Scan mode selector                            │   │
│  │  • Data processing & formatting                  │   │
│  └─────────────────┬────────────────────────────────┘   │
│                    │                                     │
│  ┌─────────────────▼────────────────────────────────┐   │
│  │           SPI Interface                          │   │
│  │  (SCK, MOSI, MISO, CSN, CE)                     │   │
│  └─────────────────┬────────────────────────────────┘   │
└────────────────────┼────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│              NRF24L01+PA+LNA MODULE                     │
│  ┌──────────────────────────────────────────────────┐   │
│  │  RF Frontend (2.4 GHz Transceiver)               │   │
│  │  • Channel switching (0-125)                     │   │
│  │  • RPD (Received Power Detector)                 │   │
│  │  • Power amplifier (+20 dBm TX)                  │   │
│  │  • Low-noise amplifier (RX sensitivity)          │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                     │
                     ▼
              [RF Environment]
         (BLE, Bluetooth, Wi-Fi, etc.)
```

### 2.2 Operational Flow

```
START
  │
  ├─► Initialize Hardware
  │    ├─► Configure NRF24L01
  │    ├─► Setup button input
  │    └─► Verify radio communication
  │
  ├─► Main Loop ◄──────────────┐
  │    │                        │
  │    ├─► Check button press   │
  │    │    └─► Change mode?    │
  │    │                        │
  │    ├─► Select scan mode     │
  │    │    ├─► Full spectrum   │
  │    │    ├─► BLE only        │
  │    │    ├─► Bluetooth       │
  │    │    └─► Wi-Fi only      │
  │    │                        │
  │    ├─► Execute scan         │
  │    │    ├─► For each channel│
  │    │    │    ├─► Set channel│
  │    │    │    ├─► Sample RPD │
  │    │    │    └─► Calculate %│
  │    │    │                   │
  │    │    └─► Display results │
  │    │                        │
  │    ├─► Delay                │
  │    │                        │
  │    └────────────────────────┘
  │
 END (never reached - runs continuously)
```

### 2.3 Detection Methodology

The system employs **Received Power Detection (RPD)** using the NRF24L01's built-in carrier sense capability:

1. **Channel Selection**: Radio tunes to specific frequency
2. **Settling Time**: 2ms delay for PLL stabilization
3. **Sampling**: 50 rapid measurements per channel
4. **Statistical Analysis**: Calculate percentage of "busy" samples
5. **Threshold Comparison**: Alert if exceeds configurable threshold
6. **Aggregation**: Compile results across all channels

**Detection Formula:**
```
Busy% = (Detections / Total Samples) × 100
Alert Triggered = (Busy% ≥ Threshold%)
```

---

## 3. Hardware Requirements

### 3.1 Bill of Materials (BOM)

| Component | Specification | Quantity | Notes |
|-----------|--------------|----------|-------|
| **Microcontroller** | Arduino Uno / Nano / Mega | 1 | Any ATmega328P-based board |
| **RF Module** | NRF24L01+PA+LNA | 1 | Must have external antenna |
| **Antenna** | 2.4 GHz omnidirectional | 1 | Usually included with module |
| **Push Button** | Momentary SPST | 1 | NO (normally open) type |
| **Capacitor** | 10-100µF electrolytic | 1 | Power stabilization |
| **USB Cable** | Type A to B/Mini/Micro | 1 | Matches Arduino connector |
| **Jumper Wires** | Male-to-female | 10 | 20cm length recommended |

**Optional Components:**
- **LEDs (4x)**: Mode indicator lights (white, blue, green, red)
- **Resistors (4x)**: 220Ω for LED current limiting
- **Enclosure**: Project box for permanent installation
- **External Power**: 5V/1A adapter for standalone operation

### 3.2 NRF24L01+PA+LNA Module Specifications

```
┌─────────────────────────────────────────┐
│  NRF24L01+PA+LNA Technical Data         │
├─────────────────────────────────────────┤
│ Operating Voltage:     3.0 - 3.6V       │
│ Current Draw (RX):     45 mA            │
│ Current Draw (TX):     115 mA (max)     │
│ TX Power:              +20 dBm (100mW)  │
│ RX Sensitivity:        -82 dBm @ 2Mbps  │
│ Frequency Range:       2400-2525 MHz    │
│ Channels:              126 (1 MHz step) │
│ Data Rates:            250K/1M/2M bps   │
│ Interface:             SPI (4-wire)     │
│ Operating Temp:        -40°C to +85°C   │
└─────────────────────────────────────────┘
```

### 3.3 Pinout Diagram

```
NRF24L01+PA+LNA Module (Top View)
┌────────────────────────────┐
│         [Antenna]          │
│            │               │
│  ┌─────────┴─────────┐     │
│  │  ╔═══════════════╗ │     │
│  │  ║    NRF24L01   ║ │     │
│  │  ║   +PA +LNA    ║ │     │
│  │  ╚═══════════════╝ │     │
│  └───────────────────┘     │
│                            │
│  1  2  3  4  5  6  7  8    │
│  │  │  │  │  │  │  │  │    │
└──┼──┼──┼──┼──┼──┼──┼──┼────┘
   │  │  │  │  │  │  │  │
   │  │  │  │  │  │  │  └─ IRQ (not used)
   │  │  │  │  │  │  └──── MISO
   │  │  │  │  │  └─────── MOSI
   │  │  │  │  └────────── SCK
   │  │  │  └───────────── CSN
   │  │  └──────────────── CE
   │  └─────────────────── GND
   └────────────────────── VCC (3.3V)
```

### 3.4 Wiring Schematic

```
Arduino Uno                    NRF24L01+PA+LNA
┌──────────────┐              ┌──────────────┐
│              │              │              │
│         3.3V ├──────────┬───┤ VCC          │
│              │          │   │              │
│          GND ├──────────┼───┤ GND          │
│              │          │   │              │
│    Pin 9 (CE)├──────────────┤ CE           │
│              │          │   │              │
│   Pin 10(CSN)├──────────────┤ CSN          │
│              │          │   │              │
│   Pin 13(SCK)├──────────────┤ SCK          │
│              │          │   │              │
│  Pin 11(MOSI)├──────────────┤ MOSI         │
│              │          │   │              │
│  Pin 12(MISO)├──────────────┤ MISO         │
│              │          │   │              │
│    Pin 2     ├──┐       │   └──────────────┘
│              │  │       │
│          GND ├──┼───┐   │   10µF Capacitor
│              │  │   │   │   ┌────┴────┐
└──────────────┘  │   │   └───┤+      - ├
                  │   │       └─────────┘
            Push  │   │
            Button│   │
              [─] │   │
                  └───┘
```

**Critical Wiring Notes:**

⚠️ **VOLTAGE WARNING**: NRF24L01 modules are **3.3V ONLY**. Connecting to 5V will permanently damage the module.

⚠️ **CAPACITOR REQUIRED**: PA+LNA modules draw high current spikes. Place 10-100µF capacitor directly across VCC/GND pins on the module.

⚠️ **SHORT WIRES**: Keep SPI wires under 10cm to minimize signal integrity issues.

### 3.5 Power Considerations

**Power Budget Analysis:**

| Component | Idle | Active | Peak |
|-----------|------|--------|------|
| Arduino Uno | 50 mA | 50 mA | 50 mA |
| NRF24 (RX mode) | 45 mA | 45 mA | 45 mA |
| NRF24 (TX mode) | - | - | 115 mA |
| **Total System** | **95 mA** | **95 mA** | **165 mA** |

**Power Supply Requirements:**
- **USB Powered**: Adequate (500 mA available)
- **External 5V**: 1A adapter recommended for margin
- **Battery Operation**: 3x AA (4.5V) with voltage regulator

---

## 4. Software Architecture

### 4.1 System Requirements

**Development Environment:**
- Arduino IDE 1.8.19 or newer (or Arduino IDE 2.x)
- RF24 library by TMRh20 (version 1.4.5 or newer)

**Compiler Requirements:**
- AVR-GCC compiler (included with Arduino IDE)
- C++11 support (default in modern Arduino IDE)

**Memory Footprint:**
```
Program Storage:  ~10 KB / 32 KB (31%)
Dynamic Memory:   ~800 bytes / 2 KB (39%)
```

### 4.2 Software Modules

#### 4.2.1 Module Hierarchy

```
main.ino
├── Configuration Module
│   ├── Pin definitions
│   ├── Scan parameters
│   └── Channel definitions
│
├── Hardware Abstraction Layer
│   ├── Radio initialization
│   ├── SPI communication
│   └── GPIO control
│
├── User Interface Module
│   ├── Button handler (debouncing)
│   ├── Mode selection logic
│   └── Serial output formatting
│
├── Scanning Engine
│   ├── Full spectrum scan
│   ├── BLE-specific scan
│   ├── Bluetooth Classic scan
│   └── Wi-Fi-specific scan
│
├── Detection Algorithm
│   ├── Channel sampling
│   ├── RPD reading
│   └── Statistical analysis
│
└── Reporting Module
    ├── Channel result formatting
    ├── Visual bar generation
    ├── Alert triggering
    └── Summary statistics
```

#### 4.2.2 State Machine

```
┌─────────────┐
│   STARTUP   │
└──────┬──────┘
       │ Initialize Hardware
       │ Configure Radio
       │ Display Welcome
       ▼
┌─────────────┐
│    IDLE     │◄──────────────┐
└──────┬──────┘               │
       │ Wait for trigger     │
       │                      │
       ▼                      │
┌─────────────┐               │
│ CHECK BUTTON│               │
└──────┬──────┘               │
       │ Button pressed?      │
       ├─ NO ────────────────►│
       │                      │
       ▼ YES                  │
┌─────────────┐               │
│ CHANGE MODE │               │
└──────┬──────┘               │
       │ Cycle mode           │
       │ Update display       │
       ▼                      │
┌─────────────┐               │
│   SCANNING  │               │
└──────┬──────┘               │
       │ Execute selected     │
       │ scan mode            │
       │                      │
       ├─► Full Spectrum      │
       ├─► BLE Only           │
       ├─► Bluetooth Classic  │
       └─► Wi-Fi Only         │
              │               │
              ▼               │
       ┌─────────────┐        │
       │   REPORT    │        │
       └──────┬──────┘        │
              │ Display       │
              │ results       │
              │               │
              ▼               │
       ┌─────────────┐        │
       │    DELAY    │        │
       └──────┬──────┘        │
              │ Wait          │
              │ (1-2 sec)     │
              │               │
              └───────────────┘
```

### 4.3 Key Algorithms

#### 4.3.1 Channel Sampling Algorithm

```cpp
/**
 * @brief Scan a single channel and return activity percentage
 * @param channel Channel number (0-125)
 * @return Activity percentage (0-100)
 */
int scanChannel(int channel) {
    // Step 1: Tune radio to channel
    radio.setChannel(channel);
    delay(2); // PLL settling time
    
    int detectionCount = 0;
    
    // Step 2: Take multiple samples
    for (int sample = 0; sample < SAMPLES_PER_CHANNEL; sample++) {
        // Step 3: Enable receiver
        radio.startListening();
        delayMicroseconds(200); // RPD update time
        
        // Step 4: Read carrier detect flag
        if (radio.testRPD()) {
            detectionCount++;
        }
        
        // Step 5: Disable receiver
        radio.stopListening();
        delayMicroseconds(10); // Brief gap
    }
    
    // Step 6: Calculate percentage
    return (detectionCount * 100) / SAMPLES_PER_CHANNEL;
}
```

**Algorithm Complexity:**
- Time complexity: O(n) where n = samples per channel
- Space complexity: O(1) - constant memory usage
- Execution time: ~30ms per channel (50 samples × 0.6ms)

#### 4.3.2 Button Debouncing Algorithm

```cpp
/**
 * @brief Handle button press with debouncing
 * Implements software debouncing to prevent multiple
 * mode changes from a single physical press
 */
void checkButton() {
    static unsigned long lastPressTime = 0;
    const unsigned long DEBOUNCE_MS = 300;
    
    // Read button state (active LOW due to pullup)
    if (digitalRead(BUTTON_PIN) == LOW) {
        unsigned long currentTime = millis();
        
        // Check if enough time has passed since last press
        if (currentTime - lastPressTime > DEBOUNCE_MS) {
            lastPressTime = currentTime;
            
            // Cycle to next mode
            currentMode = (ScanMode)((currentMode + 1) % 4);
            
            // Provide user feedback
            displayModeChange();
        }
    }
}
```

**Debouncing Strategy:**
- **Method**: Time-based software debouncing
- **Debounce period**: 300ms (adjustable)
- **Advantages**: No external components, reliable
- **Trade-off**: Cannot press button faster than 300ms

#### 4.3.3 Alert Detection Logic

```cpp
/**
 * @brief Determine if channel activity triggers alert
 * @param busyPercent Activity level (0-100%)
 * @return true if alert threshold exceeded
 */
bool isAlertCondition(int busyPercent) {
    // Primary threshold check
    if (busyPercent >= THRESHOLD_PERCENT) {
        return true;
    }
    
    // Future enhancement: Could add secondary conditions
    // - Consecutive channel activity
    // - Sustained activity over multiple scans
    // - Pattern matching for specific protocols
    
    return false;
}
```

### 4.4 Configuration Parameters

```cpp
// ===== USER-CONFIGURABLE PARAMETERS =====

// Sampling configuration
const int samplesPerChannel = 50;
// Range: 10-200
// Lower = faster scanning, less accurate
// Higher = slower scanning, more accurate
// Recommended: 50 (good balance)

// Alert threshold
const int busyThresholdPercent = 20;
// Range: 0-100
// Lower = more sensitive, more false positives
// Higher = less sensitive, may miss weak signals
// Recommended values:
//   - 10-20: High sensitivity (jamming detection)
//   - 30-50: Medium (general monitoring)
//   - 60-80: Low (only strong signals)

// Scan timing
const int scanDelayMs = 1000;
// Range: 0-10000 ms
// Lower = more frequent updates
// Higher = less CPU usage, easier to read
// Recommended: 1000-2000 ms

// Display options
const bool showOnlyActive = true;
// true: Only display channels with activity (cleaner)
// false: Show all channels including 0% (verbose)

const bool showVisualBars = true;
// true: ASCII bar graphs for visual representation
// false: Numbers only (minimal output)

// Hardware pins
const int buttonPin = 2;
// Any digital pin with interrupt capability
// Pins 2 and 3 on Uno support interrupts

const int cePin = 9;
const int csnPin = 10;
// SPI chip select pins for NRF24
// Can be changed if needed for other shields
```

---

## 5. Installation Guide

### 5.1 Prerequisites

**Required Software:**
1. Arduino IDE (download from arduino.cc)
2. USB drivers for your Arduino board
3. RF24 library

**Required Hardware:**
1. Computer with USB port
2. All components from BOM (Section 3.1)
3. Soldering iron (if using bare modules)

### 5.2 Step-by-Step Installation

#### Step 1: Install Arduino IDE

```
1. Download Arduino IDE from https://www.arduino.cc/en/software
2. Run installer and follow prompts
3. Launch Arduino IDE
4. Verify installation: File → Examples → 01.Basics → Blink
```

#### Step 2: Install RF24 Library

**Method A: Library Manager (Recommended)**
```
1. Open Arduino IDE
2. Navigate: Sketch → Include Library → Manage Libraries
3. Search: "RF24"
4. Find: "RF24 by TMRh20"
5. Click: Install
6. Wait for completion
7. Close Library Manager
```

**Method B: Manual Installation**
```
1. Download: https://github.com/nRF24/RF24/archive/master.zip
2. Extract ZIP file
3. Copy folder to: Documents/Arduino/libraries/
4. Rename to: RF24
5. Restart Arduino IDE
```

**Verification:**
```
1. Navigate: File → Examples → RF24 → GettingStarted
2. If example appears, installation successful
```

#### Step 3: Hardware Assembly

**3.1: Prepare Workspace**
- Clean, static-free work surface
- Good lighting
- Organized component layout

**3.2: Solder Pin Headers (if needed)**
```
NRF24 Module:
1. Insert 8-pin header into module
2. Solder all 8 pins
3. Check for cold solder joints
4. Clean flux residue

Arduino:
- If using bare Arduino, solder headers as needed
```

**3.3: Add Capacitor**
```
1. Identify polarity (long leg = positive)
2. Solder across VCC and GND pins on NRF24 module
3. Keep leads short (< 5mm)
4. Position flat against module PCB

   NRF24 Module
   ┌─────────┐
   │ VCC  GND│
   │  │    │ │
   │  └─┬──┘ │
   │    │    │
   │  ──┴──  │  ← 10µF capacitor
   │  ─────  │
   └─────────┘
```

**3.4: Connect Wiring**

Follow this exact sequence to prevent errors:

```
Connection Order:
1. GND connections first (safety)
2. Power connections
3. Signal connections

Detailed Steps:
□ NRF24 GND    → Arduino GND
□ NRF24 VCC    → Arduino 3.3V (NOT 5V!)
□ NRF24 CE     → Arduino Pin 9
□ NRF24 CSN    → Arduino Pin 10
□ NRF24 SCK    → Arduino Pin 13
□ NRF24 MOSI   → Arduino Pin 11
□ NRF24 MISO   → Arduino Pin 12
□ Button leg 1 → Arduino Pin 2
□ Button leg 2 → Arduino GND
```

**3.5: Double-Check Connections**

Use multimeter to verify:
- Continuity on all signal lines
- NO continuity between VCC and GND
- VCC measures 3.3V (not 5V!)

#### Step 4: Load Software

**4.1: Download Code**
```
1. Copy complete code from Section 11.1 (Appendix)
2. Open new Arduino IDE sketch: File → New
3. Delete template code
4. Paste complete code
5. Save: File → Save As → "RF_Monitor_v3"
```

**4.2: Configure Board**
```
1. Connect Arduino via USB
2. Select board: Tools → Board → Arduino Uno (or your model)
3. Select port: Tools → Port → COM# (Windows) or /dev/ttyUSB# (Linux)
```

**4.3: Compile**
```
1. Click: Sketch → Verify/Compile
2. Wait for compilation
3. Check for errors in console
4. Expected output: "Done compiling"
```

**4.4: Upload**
```
1. Click: Sketch → Upload (or Ctrl+U)
2. Watch progress bar
3. Wait for "Done uploading"
4. Arduino will auto-reset
```

#### Step 5: Initial Testing

**5.1: Open Serial Monitor**
```
1. Click: Tools → Serial Monitor (or Ctrl+Shift+M)
2. Set baud rate: 115200 (bottom-right dropdown)
3. Set line ending: "Newline" (optional)
```

**5.2: Expected Startup Output**
```
=================================
 2.4 GHz RF Monitor v3.0
 Multi-Mode Detection System
=================================

Radio initialized successfully!

*** BUTTON CONTROLS ***
Press button to cycle modes:
  1. FULL SCAN (all channels)
  2. BLE ONLY (advertising channels)
  3. BLUETOOTH CLASSIC
  4. WI-FI ONLY
***********************

Current Mode: FULL SCAN (All 126 channels)

========== FULL SPECTRUM SCAN ==========
```

**5.3: Verify Functionality**

Test Checklist:
```
□ Serial output appears
□ Scanning begins automatically
□ At least some channels show activity (Wi-Fi routers)
□ Button press changes mode
□ Mode indicator updates
□ No error messages
```

### 5.3 Common Installation Issues

| Symptom | Cause | Solution |
|---------|-------|----------|
| "NRF24 not detected" | Wiring error | Check all 7 connections, verify 3.3V |
| No serial output | Wrong baud rate | Set to 115200 in Serial Monitor |
| "avrdude: stk500" error | Wrong board/port | Verify Tools → Board and Tools → Port |
| Compilation errors | Missing library | Install RF24 library (Step 2) |
| All channels show 0% | Module damage | Check 3.3V (not 5V!), try new module |
| Random crashes | Power issue | Add/check capacitor on NRF24 |

---

## 6. Operation Manual

### 6.1 Starting the System

**Power-On Sequence:**
1. Connect Arduino to power (USB or external)
2. System performs self-test (3 seconds)
3. Welcome message displays on Serial Monitor
4. Automatic scanning begins in FULL SCAN mode

**Initial Display:**
```
=================================
 2.4 GHz RF Monitor v3.0
 Multi-Mode Detection System
=================================

Radio initialized successfully!
Configuration:
  Samples per channel: 50
  Alert threshold: 20%
  Scan delay: 1000 ms

Starting continuous monitoring...

Current Mode: FULL SCAN (All 126 channels)
```

### 6.2 Operating Modes

#### Mode 1: FULL SPECTRUM SCAN

**Purpose:** Comprehensive survey of entire 2.4 GHz band

**Channels Scanned:** 0-125 (2400-2525 MHz)

**Scan Duration:** ~15 seconds per cycle

**Use Cases:**
- Initial RF environment assessment
- Detecting unknown interference sources
- Comprehensive security audits
- Baseline spectrum mapping

**Sample Output:**
```
========== FULL SPECTRUM SCAN ==========
2400.0 MHz ->  2%
2401.0 MHz ->  0%
2402.0 MHz -> 28% [#####] <<< ALERT
2403.0 MHz ->  5% [#]
2412.0 MHz -> 15% [###]
2437.0 MHz -> 89% [#################] <<< ALERT
2462.0 MHz -> 12% [##]
2480.0 MHz -> 35% [#######] <<< ALERT
========================================
Scan time: 15234 ms
Active channels: 42
Alert count: 15
Status: High activity detected
========================================
```

**Interpretation:**
- **2-10%**: Background noise, distant devices
- **10-30%**: Light activity (BLE beacons, idle devices)
- **30-60%**: Moderate activity (active connections)
- **60-100%**: Heavy activity (Wi-Fi AP, active jamming)

---

## Interference Node

![Fig.4](images/Interferance_node.jpeg)

---

## Pin Mapping & Wiring Diagram

![pinout & wiring](Schematic.md)

---

# Repository Structure

```text
nRF-SpectrumAnalyzer/

├── firmware/
├── datasets/
├── analysis/
├── docs/
├── paper/
├── CHANGELOG.md
├── CITATION.cff
├── LICENSE
└── README.md
```

---

# Reproducing the Published Results

The experimental results reported in the accompanying manuscript were generated using:

```text
Firmware Version: v3.1.0
```

General workflow:

1. Assemble hardware according to documentation.
2. Flash detector firmware.
3. Collect occupancy measurements.
4. Export measurements as CSV.
5. Execute analysis scripts.
6. Compare generated figures with manuscript figures.

---

# Data Availability

Experimental datasets used in the manuscript are archived within this repository and the associated DOI release.

Datasets include:

* Baseline occupancy measurements
* Bluetooth interference measurements
* BLE interference measurements
* Wi-Fi interference measurements
* Combined interference measurements

---

# Code Availability

The following components are publicly available:

* Detector firmware
* Analysis scripts
* Documentation
* Experimental datasets

The interference-generation firmware is not included in the public release because it can be adapted for intentional radio-frequency interference generation.

Excluding this component reduces the potential for misuse while preserving reproducibility through the provided datasets, detector implementation, experimental methodology, and analysis scripts.

---

# Citation

If you use this repository, please cite the archived release.

Citation metadata is available through:

* CITATION.cff
* Zenodo DOI
* Associated manuscript

Example:

```bibtex
@software{harishkumar_nrf_spectrumanalyzer,
  author = {Harishkumar, Aris},
  title = {nRF-SpectrumAnalyzer},
  year = {2026},
  doi = {DOI_HERE}
}
```

---

# License

GPL-3.0

See LICENSE for details.

---

# Disclaimer

This repository is intended for:

* Academic research
* Wireless coexistence studies
* RF occupancy monitoring
* Educational purposes

Users are responsible for ensuring compliance with all applicable radio-frequency regulations and local laws governing wireless transmissions.

---

# Related Publication

Dual-node 2.4 GHz ISM Band Interference Testbed Using Commodity Embedded Hardware: Characterisation of Generation and Detection Boundaries.

Manuscript and supplementary material are available in the DOI archive associated with this repository.
