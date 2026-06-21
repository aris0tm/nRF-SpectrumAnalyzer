// =============================================================
//  @author: aris0tm
//  D&T:s&E   : 15-17/02/26:08-11pm +5.30
//  2.4 GHz RF Monitor v2.0.1
//  Target: Arduino Uno (CH340 chipset --Cheap knockoff on inet which fully satisfies the intention)
//  Key change v2.0.1: Static channel labels, ONLY % values redraw 
//
//  Requires ANSI terminal for in-place updates:
//    - Arduino IDE 2.x Serial Monitor (ANSI support built in) which is not working 
//    - PuTTY: Connection > Terminal type = ANSI, 115200 baud (Recmmended)
//    - Any VT100/ANSI terminal emulator at 115200 baud eg:Windows terminal 
//
//  Without ANSI support it still works but prints linearly.
//  See document ANSI_SerialMoniter.md for Monitering on WinTerminel
// =============================================================
#include <SPI.h>
#include <RF24.h>

// CE=9, CSN=10
RF24 radio(9, 10); 

// ===== BUTTON =====
const int  BUTTON_PIN    = 2;//NRF24-1 HSPI=SCK = 14, MISO = 12, MOSI = 13, CS = 15 , CE = 26

unsigned long lastPress  = 0;
const int  DEBOUNCE_MS   = 300;

// ===== MODES =====
enum ScanMode { MODE_FULL, MODE_BLE, MODE_BLUETOOTH, MODE_WIFI };
ScanMode currentMode     = MODE_FULL;
bool     modeChanged     = true;  // true = force full screen redraw

// ===== TUNING =====
const int SAMPLES_PER_CH   = 50;  // RPD samples per channel
const int ALERT_THRESHOLD  = 20;  // % busy = alert (red)
const int SPIKE_THRESHOLD  = 15;  // [NEW] % one-cycle jump = spike marker
const int SCAN_DELAY_MS    = 800; // ms between sweeps

// ===== CHANNEL MAPS =====
const int BLE_CH[]    = {2, 26, 80};     const int BLE_N  = 3;
const int BT_START    = 2; const int BT_END = 78;
const int WIFI_CH[]   = {12,17,22,27,32,37,42,47,52,57,62,67,72};
const int WIFI_N      = 13;

// ===== LIVE STATE =====
int peakVal[126];   // [NEW] Peak hold - max % seen since last mode change
int prevVal[126];   // [NEW] Previous % - used for spike detection

unsigned long scanCount    = 0;
unsigned long lastScanTime = 0;   // [NEW] For scans/min calculation

// ===== SCREEN COLUMN OFFSETS (set per mode) =====
int COL_PCT        = 34;
int COL_BAR        = 39;
int COL_PEAK       = 64;
int DATA_ROW_START = 6;

// =============================================================
//  ANSI HELPERS
// =============================================================
void gotoXY(int r, int c) {
  Serial.print(F("\033[")); Serial.print(r);
  Serial.print(F(";"));    Serial.print(c);
  Serial.print(F("H"));
}
void cls()         { Serial.print(F("\033[2J\033[H")); }
void clrLine()     { Serial.print(F("\033[K")); }
void resetClr()    { Serial.print(F("\033[0m")); }
void bold()        { Serial.print(F("\033[1m")); }
void reverse()     { Serial.print(F("\033[7m")); }
void colorRed()    { Serial.print(F("\033[31m")); }
void colorYellow() { Serial.print(F("\033[33m")); }
void colorGreen()  { Serial.print(F("\033[32m")); }

// =============================================================
//  COLOR-CODED BAR — always exactly 22 chars: [##########     ]
// =============================================================
void printBar(int pct) {
  int bars = (pct * 20) / 100;
  if      (pct >= 50) colorRed();
  else if (pct >= 20) colorYellow();
  else                colorGreen();
  Serial.print(F("["));
  for (int b = 0; b < 20; b++) Serial.print(b < bars ? '#' : ' ');
  Serial.print(F("]"));
  resetClr();
}

// =============================================================
//  STATUS BAR — row 1, updated every sweep
//  [NEW] mode | alerts | scans/min | peak hold indicator
// =============================================================
void drawStatusBar(const char* modeName, int alerts, float spMin) {
  gotoXY(1, 1);
  reverse();
  Serial.print(F(" MODE: ")); Serial.print(modeName);
  Serial.print(F("  |  ALERTS: "));
  if (alerts > 0) colorRed();
  if (alerts < 100) Serial.print(' ');
  if (alerts < 10)  Serial.print(' ');
  Serial.print(alerts);
  reverse();
  Serial.print(F("  |  SCANS/MIN: "));
  if (spMin < 10.0) Serial.print(' ');
  Serial.print(spMin, 1);
  Serial.print(F("  |  PEAK HOLD ON  \033[0m"));
}

// =============================================================
//  STATIC ROW — printed ONCE per mode change
//  Layout (label padded to 12 chars for alignment):
//  "  FFFF.F MHz (LLLLLLLLLLLL) -> ---% [--------------------]  PK:---%"
// =============================================================
void printStaticRow(int sRow, float hz, const char* label) {
  gotoXY(sRow, 1);
  Serial.print(F("  "));
  Serial.print(hz, 1);
  Serial.print(F(" MHz ("));
  int l = strlen(label);
  Serial.print(label);
  for (int i = l; i < 12; i++) Serial.print(' ');
  Serial.print(F(") -> "));
  Serial.print(F("---%  [--------------------]  PK:---%          "));
  clrLine();
}

// =============================================================
//  DATA UPDATE — overwrites ONLY busy%, bar, peak%, spike field
//  Labels are never touched
// =============================================================
void updateRowData(int sRow, int pct, int peak, bool spike) {
  // busy %
  gotoXY(sRow, COL_PCT);
  if      (pct >= ALERT_THRESHOLD) colorRed();
  else if (pct > 0)                colorYellow();
  else                             colorGreen();
  if (pct < 100) Serial.print(' ');
  if (pct < 10)  Serial.print(' ');
  Serial.print(pct); Serial.print('%');
  resetClr();

  // bar
  gotoXY(sRow, COL_BAR);
  printBar(pct);

  // peak hold [NEW]
  gotoXY(sRow, COL_PEAK);
  Serial.print(F("  PK:"));
  if      (peak >= ALERT_THRESHOLD) colorRed();
  else if (peak > 0)                colorYellow();
  else                              colorGreen();
  if (peak < 100) Serial.print(' ');
  if (peak < 10)  Serial.print(' ');
  Serial.print(peak); Serial.print('%');
  resetClr();

  // spike marker [NEW]
  gotoXY(sRow, COL_PEAK + 9);
  if (spike) {
    colorRed(); Serial.print(F(" >>> SPIKE")); resetClr();
  } else {
    Serial.print(F("          ")); // erase old marker
  }
}

// =============================================================
//  SUMMARY ROW
// =============================================================
void drawSummary(int sRow, unsigned long ms, int active, int alerts) {
  gotoXY(sRow, 1); clrLine();
  Serial.print(F("  Scan: ")); Serial.print(ms); Serial.print(F("ms"));
  Serial.print(F("  |  Active ch: ")); Serial.print(active);
  Serial.print(F("  |  Alerts: "));
  if (alerts > 0) colorRed();
  Serial.print(alerts); resetClr();
  Serial.print(F("  |  "));
  if      (alerts == 0) { colorGreen();  Serial.print(F("Clear                    ")); }
  else if (alerts < 5)  { colorYellow(); Serial.print(F("Normal activity          ")); }
  else if (alerts < 20) { colorRed();    Serial.print(F("High activity!           ")); }
  else                  { colorRed(); bold(); Serial.print(F("*** FLOODING DETECTED ***")); }
  resetClr(); clrLine();
}

// =============================================================
//  CHANNEL SCANNER — same RPD method as v3
// =============================================================
int scanChannel(int ch) {
  radio.setChannel(ch);
  delay(2);
  int busy = 0;
  for (int i = 0; i < SAMPLES_PER_CH; i++) {
    radio.startListening();
    delayMicroseconds(200);
    if (radio.testRPD()) busy++;
    radio.stopListening();
    delayMicroseconds(10);
  }
  return (busy * 100) / SAMPLES_PER_CH;
}

// =============================================================
//  BUTTON
// =============================================================
void checkButton() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastPress > DEBOUNCE_MS) {
      lastPress = now;
      currentMode = (ScanMode)((currentMode + 1) % 4);
      modeChanged = true;
      memset(peakVal, 0, sizeof(peakVal));
      memset(prevVal, 0, sizeof(prevVal));
      scanCount = 0;
    }
  }
}

// =============================================================
//  SETUP
// =============================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  cls();
  bold(); Serial.println(F(" 2.4GHz RF Monitor v4.0")); resetClr();
  Serial.println(F(" Initializing NRF24L01..."));

  if (!radio.begin()) {
    colorRed();
    Serial.println(F(" ERROR: NRF24 not found! Check 3.3V & SPI wiring."));
    resetClr();
    while (1) delay(1000);
  }

  radio.setAutoAck(false);
  radio.stopListening();
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MIN);
  radio.setAddressWidth(5);

  memset(peakVal, 0, sizeof(peakVal));
  memset(prevVal, 0, sizeof(prevVal));

  Serial.println(F(" Radio OK."));
  Serial.println();
  Serial.println(F(" BUTTON cycles modes:"));
  Serial.println(F("  1. FULL SCAN    (ch 0-125)"));
  Serial.println(F("  2. BLE ONLY     (3 advert. ch)"));
  Serial.println(F("  3. BT CLASSIC   (ch 2-78)"));
  Serial.println(F("  4. WI-FI ONLY   (13 ch)"));
  Serial.println();
  Serial.println(F(" v4 NEW features:"));
  Serial.println(F("  * Static labels -- only % values redraw each sweep"));
  Serial.println(F("  * Peak hold per channel (resets on mode change)"));
  Serial.println(F("  * >>>SPIKE if channel jumps >15% in one cycle"));
  Serial.println(F("  * Scans/min in status bar"));
  Serial.println(F("  * Color: green=idle  yellow=active  red=alert"));
  Serial.println();
  Serial.println(F(" Use ANSI terminal for in-place updates."));
  Serial.println(F(" Starting in 3 seconds..."));
  delay(3000);
}

// =============================================================
//  LOOP
// =============================================================
void loop() {
  checkButton();
  switch (currentMode) {
    case MODE_FULL:      runFullScan();      break;
    case MODE_BLE:       runBLEScan();       break;
    case MODE_BLUETOOTH: runBTClassicScan(); break;
    case MODE_WIFI:      runWiFiScan();      break;
  }
  delay(SCAN_DELAY_MS);
}

// =============================================================
//  FULL SCAN (0-125)
//  126 rows can't all stay static on a small terminal, so we
//  redraw per cycle but SKIP zero-to-zero unchanged channels
//  entirely — no serial output for silent channels at all.
// =============================================================
void runFullScan() {
  COL_PCT = 33; COL_BAR = 39; COL_PEAK = 63;

  static bool hdrDone = false;
  if (modeChanged) {
    cls(); hdrDone = false; modeChanged = false;
    memset(peakVal, 0, sizeof(peakVal));
    memset(prevVal, 0, sizeof(prevVal));
    scanCount = 0;
  }
  if (!hdrDone) {
    gotoXY(2,1); Serial.print(F("  ================================================"));
    gotoXY(3,1); bold(); Serial.print(F("  FULL SPECTRUM SCAN  (2400-2525 MHz)")); resetClr();
    gotoXY(4,1); Serial.print(F("  Freq MHz      ID       ->  Busy  Bar                    Peak      Spike"));
    gotoXY(5,1); Serial.print(F("  ================================================"));
    hdrDone = true;
  }

  scanCount++;
  unsigned long t0 = millis();
  int active = 0, alerts = 0;
  unsigned long el = millis() - lastScanTime;
  float spMin = el > 0 ? (60000.0 / el) : 0;
  lastScanTime = millis();

  for (int ch = 0; ch <= 125; ch++) {
    int pct = scanChannel(ch);
    int row = 6 + ch;
    float hz = 2400.0 + ch;

    if (pct > peakVal[ch]) peakVal[ch] = pct;
    bool spike = (pct - prevVal[ch]) >= SPIKE_THRESHOLD;
    prevVal[ch] = pct;
    if (pct > 0) active++;
    if (pct >= ALERT_THRESHOLD) alerts++;

    // Zero-skip: don't waste serial time on silent unchanged channels
    if (pct == 0 && peakVal[ch] == 0 && !spike) {
      if (ch % 10 == 0) checkButton();
      continue;
    }

    char id[7]; sprintf(id, "ch%03d", ch);
    gotoXY(row, 1);
    Serial.print(F("  ")); Serial.print(hz, 1);
    Serial.print(F(" MHz (")); Serial.print(id);
    Serial.print(F("     ) -> "));
    updateRowData(row, pct, peakVal[ch], spike);

    if (ch % 10 == 0) checkButton();
    if (modeChanged) return;
  }

  drawSummary(133, millis() - t0, active, alerts);
  drawStatusBar("FULL SCAN   ", alerts, spMin);
  gotoXY(135, 1);
}

// =============================================================
//  BLE SCAN (3 advertising channels — fully static labels)
// =============================================================
void runBLEScan() {
  COL_PCT = 34; COL_BAR = 39; COL_PEAK = 64;
  DATA_ROW_START = 6;

  if (modeChanged) {
    cls();
    gotoXY(2,1); Serial.print(F("  ===================================================="));
    gotoXY(3,1); bold(); Serial.print(F("  BLE ADVERTISING SCAN  (2402 / 2426 / 2480 MHz)")); resetClr();
    gotoXY(4,1); Serial.print(F("  Freq (MHz)    Label          ->  Busy  Bar                    Peak    Spike"));
    gotoXY(5,1); Serial.print(F("  ===================================================="));
    const char* names[] = {"BLE Adv Ch37", "BLE Adv Ch38", "BLE Adv Ch39"};
    for (int i = 0; i < BLE_N; i++)
      printStaticRow(DATA_ROW_START + i, 2400.0 + BLE_CH[i], names[i]);
    modeChanged = false;
    memset(peakVal, 0, sizeof(peakVal));
    memset(prevVal, 0, sizeof(prevVal));
    scanCount = 0;
  }

  scanCount++;
  unsigned long t0 = millis();
  int active = 0, alerts = 0;
  unsigned long el = millis() - lastScanTime;
  float spMin = el > 0 ? (60000.0 / el) : 0;
  lastScanTime = millis();

  for (int i = 0; i < BLE_N; i++) {
    int pct = scanChannel(BLE_CH[i]);
    if (pct > peakVal[i]) peakVal[i] = pct;
    bool spike = (pct - prevVal[i]) >= SPIKE_THRESHOLD;
    prevVal[i] = pct;
    if (pct > 0) active++;
    if (pct >= ALERT_THRESHOLD) alerts++;
    updateRowData(DATA_ROW_START + i, pct, peakVal[i], spike);
  }

  drawSummary(DATA_ROW_START + BLE_N + 1, millis() - t0, active, alerts);
  drawStatusBar("BLE         ", alerts, spMin);
  gotoXY(DATA_ROW_START + BLE_N + 3, 1);
}

// =============================================================
//  BT CLASSIC SCAN (channels 2-78)
// =============================================================
void runBTClassicScan() {
  COL_PCT = 34; COL_BAR = 39; COL_PEAK = 64;
  DATA_ROW_START = 6;

  static bool hdrDone = false;
  if (modeChanged) {
    cls(); hdrDone = false; modeChanged = false;
    memset(peakVal, 0, sizeof(peakVal));
    memset(prevVal, 0, sizeof(prevVal));
    scanCount = 0;
  }
  if (!hdrDone) {
    gotoXY(2,1); Serial.print(F("  ================================================"));
    gotoXY(3,1); bold(); Serial.print(F("  BLUETOOTH CLASSIC SCAN  (2402-2478 MHz)")); resetClr();
    gotoXY(4,1); Serial.print(F("  Freq (MHz)    Label      ->  Busy  Bar                    Peak    Spike"));
    gotoXY(5,1); Serial.print(F("  ================================================"));
    hdrDone = true;
  }

  scanCount++;
  unsigned long t0 = millis();
  int active = 0, alerts = 0;
  unsigned long el = millis() - lastScanTime;
  float spMin = el > 0 ? (60000.0 / el) : 0;
  lastScanTime = millis();

  int idx = 0;
  for (int ch = BT_START; ch <= BT_END; ch++, idx++) {
    int pct = scanChannel(ch);
    int row = DATA_ROW_START + idx;
    float hz = 2400.0 + ch;

    if (pct > peakVal[idx]) peakVal[idx] = pct;
    bool spike = (pct - prevVal[idx]) >= SPIKE_THRESHOLD;
    prevVal[idx] = pct;
    if (pct > 0) active++;
    if (pct >= ALERT_THRESHOLD) alerts++;

    if (pct == 0 && peakVal[idx] == 0 && !spike) {
      if (idx % 10 == 0) checkButton();
      continue;
    }

    char label[9]; sprintf(label, "BT ch %02d", ch);
    gotoXY(row, 1);
    Serial.print(F("  ")); Serial.print(hz, 1);
    Serial.print(F(" MHz (")); Serial.print(label);
    Serial.print(F(" ) -> "));
    updateRowData(row, pct, peakVal[idx], spike);

    if (idx % 10 == 0) checkButton();
    if (modeChanged) return;
  }

  int total = BT_END - BT_START + 1;
  drawSummary(DATA_ROW_START + total + 1, millis() - t0, active, alerts);
  drawStatusBar("BT CLASSIC  ", alerts, spMin);
}

// =============================================================
//  WI-FI SCAN (13 channels — fully static labels)
// =============================================================
void runWiFiScan() {
  COL_PCT = 34; COL_BAR = 39; COL_PEAK = 64;
  DATA_ROW_START = 6;

  const char* wNames[] = {
    "WiFi  Ch 1","WiFi  Ch 2","WiFi  Ch 3","WiFi  Ch 4",
    "WiFi  Ch 5","WiFi  Ch 6","WiFi  Ch 7","WiFi  Ch 8",
    "WiFi  Ch 9","WiFi Ch 10","WiFi Ch 11","WiFi Ch 12","WiFi Ch 13"
  };

  if (modeChanged) {
    cls();
    gotoXY(2,1); Serial.print(F("  ===================================================="));
    gotoXY(3,1); bold(); Serial.print(F("  WI-FI CHANNEL SCAN  (2.4 GHz, ch 1-13)")); resetClr();
    gotoXY(4,1); Serial.print(F("  Freq (MHz)    Label       ->  Busy  Bar                    Peak    Spike"));
    gotoXY(5,1); Serial.print(F("  ===================================================="));
    for (int i = 0; i < WIFI_N; i++)
      printStaticRow(DATA_ROW_START + i, 2400.0 + WIFI_CH[i], wNames[i]);
    modeChanged = false;
    memset(peakVal, 0, sizeof(peakVal));
    memset(prevVal, 0, sizeof(prevVal));
    scanCount = 0;
  }

  scanCount++;
  unsigned long t0 = millis();
  int active = 0, alerts = 0;
  unsigned long el = millis() - lastScanTime;
  float spMin = el > 0 ? (60000.0 / el) : 0;
  lastScanTime = millis();

  for (int i = 0; i < WIFI_N; i++) {
    int pct = scanChannel(WIFI_CH[i]);
    if (pct > peakVal[i]) peakVal[i] = pct;
    bool spike = (pct - prevVal[i]) >= SPIKE_THRESHOLD;
    prevVal[i] = pct;
    if (pct > 0) active++;
    if (pct >= ALERT_THRESHOLD) alerts++;
    updateRowData(DATA_ROW_START + i, pct, peakVal[i], spike);
  }

  drawSummary(DATA_ROW_START + WIFI_N + 1, millis() - t0, active, alerts);
  drawStatusBar("WI-FI       ", alerts, spMin);
  gotoXY(DATA_ROW_START + WIFI_N + 3, 1);
}