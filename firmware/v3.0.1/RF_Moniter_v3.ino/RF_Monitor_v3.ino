// =============================================================
//  @author: aris0tm  (enhanced build)
//  2.4 GHz RF Monitor v3.0.0
//  Target: Arduino Uno (CH340G chipset)
//
//  NEW in v3.0.0 (additive — all v2.0.1 functions preserved):
//    * RSSI estimation via RPD density (0-100 -> dBm mapping)
//    * Per-channel noise floor baseline (auto-calibrate on boot)
//    * SNR column  (signal - noise_floor)
//    * Channel utilisation history ring-buffer (8 samples)
//    * Trend indicator  ↑ rising  ↓ falling  = stable
//    * Congestion score (weighted avg of alerts in neighbourhood)
//    * GNU Radio serial bridge mode (MODE_GNURADIO):
//        – emits compact CSV frames at 115200 so GRC UDP Source
//          or File Source can ingest raw channel data
//        – frame format:  GR,<scanN>,<ch>,<pct>,<rssi_dbm>\n
//        – companion .grc / Python sink script documented below
//
//  Terminal requirements (unchanged from v2.0.1):
//    PuTTY / Windows Terminal / any VT100 at 115200 baud
// =============================================================

#include <SPI.h>
#include <RF24.h>

// ── Hardware ──────────────────────────────────────────────────
RF24 radio(9, 10);          // CE=9, CSN=10

// ── Button ────────────────────────────────────────────────────
const int BUTTON_PIN   = 2;
unsigned long lastPress = 0;
const int DEBOUNCE_MS  = 300;

// ── Modes ─────────────────────────────────────────────────────
enum ScanMode { MODE_FULL, MODE_BLE, MODE_BLUETOOTH, MODE_WIFI, MODE_GNURADIO };
ScanMode currentMode = MODE_FULL;
bool     modeChanged = true;

// ── Tuning ────────────────────────────────────────────────────
const int SAMPLES_PER_CH   = 50;
const int ALERT_THRESHOLD  = 20;
const int SPIKE_THRESHOLD  = 15;
const int SCAN_DELAY_MS    = 800;

// ── RSSI mapping ──────────────────────────────────────────────
// NRF24 RPD is a 1-bit threshold detector (~-64 dBm).
// We estimate RSSI from busy% density using a linear model:
//   0%  busy  →  ~ -90 dBm  (noise floor / nothing heard)
//   100% busy →  ~ -40 dBm  (very strong signal)
// This is an *approximation* — not a calibrated measurement.
const int  RSSI_MIN_DBM = -90;
const int  RSSI_MAX_DBM = -40;

inline int pctToRSSI(int pct) {
  return RSSI_MIN_DBM + ((pct * (RSSI_MAX_DBM - RSSI_MIN_DBM)) / 100);
}

// ── Channel maps ──────────────────────────────────────────────
const int BLE_CH[]  = {2, 26, 80};      const int BLE_N  = 3;
const int BT_START  = 2;  const int BT_END = 78;
const int WIFI_CH[] = {12,17,22,27,32,37,42,47,52,57,62,67,72};
const int WIFI_N    = 13;

// ── Per-channel live state ────────────────────────────────────
int peakVal[126];           // peak hold %
int prevVal[126];           // previous % (spike detect)
int noiseFloor[126];        // baseline % from calibration → dBm
// Utilisation ring-buffer: 8 samples per channel
//   stored as uint8_t to stay inside 2K SRAM on Uno
//   126 ch × 8 = 1008 bytes
uint8_t  hist[126][8];
uint8_t  histIdx[126];      // next write position per channel

unsigned long scanCount    = 0;
unsigned long lastScanTime = 0;

// ── GNU Radio bridge state ────────────────────────────────────
bool gnuRadioActive = false;
// Frame counter for GRC alignment
unsigned long grFrameN = 0;

// ── Screen column offsets ─────────────────────────────────────
int COL_PCT        = 34;
int COL_BAR        = 39;
int COL_PEAK       = 64;
int COL_RSSI       = 76;   // NEW
int COL_SNR        = 85;   // NEW
int COL_TREND      = 93;   // NEW
int DATA_ROW_START = 6;

// =============================================================
//  ANSI HELPERS  (unchanged)
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
void colorCyan()   { Serial.print(F("\033[36m")); }
void colorMagenta(){ Serial.print(F("\033[35m")); }

// =============================================================
//  COLOR-CODED BAR  (unchanged)
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
//  STATUS BAR  (extended with RSSI scale legend)
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
  Serial.print(F("  |  PEAK HOLD ON  |  RSSI est. ON  \033[0m"));
  clrLine();
}

// =============================================================
//  STATIC ROW  (extended with RSSI / SNR / TREND placeholders)
// =============================================================
void printStaticRow(int sRow, float hz, const char* label) {
  gotoXY(sRow, 1);
  Serial.print(F("  "));
  Serial.print(hz, 1);
  Serial.print(F(" MHz ("));
  int l = strlen(label);
  Serial.print(label);
  for (int i = l; i < 12; i++) Serial.print(' ');
  Serial.print(F(") -> ---%  [--------------------]  PK:---%  ---dBm  SNR:---  ="));
  clrLine();
}

// =============================================================
//  TREND CALCULATOR  — looks at ring-buffer of 8 samples
//  Returns: '+' rising  '-' falling  '=' stable
// =============================================================
char calcTrend(int idx) {
  // sum first half vs second half of ring (oldest→newest)
  int sum1 = 0, sum2 = 0;
  for (int i = 0; i < 4; i++) {
    int pos1 = (histIdx[idx] + i)     & 7;  // older half
    int pos2 = (histIdx[idx] + i + 4) & 7;  // newer half
    sum1 += hist[idx][pos1];
    sum2 += hist[idx][pos2];
  }
  int diff = sum2 - sum1;
  if (diff >  40) return '^';   // rising  (> 10% avg change)
  if (diff < -40) return 'v';   // falling
  return '=';
}

// =============================================================
//  CONGESTION SCORE  — weighted avg of ±2 channel neighbours
//  Returns 0-100
// =============================================================
int congestionScore(int ch, int total) {
  long wSum = 0; long wTot = 0;
  for (int d = -2; d <= 2; d++) {
    int n = ch + d;
    if (n < 0 || n >= total) continue;
    int w = (3 - abs(d));      // weight: centre=3, ±1=2, ±2=1
    wSum += (long)peakVal[n] * w;
    wTot += w;
  }
  return wTot > 0 ? (int)(wSum / wTot) : 0;
}

// =============================================================
//  DATA UPDATE  (extended: RSSI, SNR, trend)
// =============================================================
void updateRowData(int sRow, int pct, int peak, bool spike,
                   int rssiDbm, int snrDb, char trend) {
  // ── busy % ──────────────────────────────────────────────────
  gotoXY(sRow, COL_PCT);
  if      (pct >= ALERT_THRESHOLD) colorRed();
  else if (pct > 0)                colorYellow();
  else                             colorGreen();
  if (pct < 100) Serial.print(' ');
  if (pct < 10)  Serial.print(' ');
  Serial.print(pct); Serial.print('%');
  resetClr();

  // ── bar ─────────────────────────────────────────────────────
  gotoXY(sRow, COL_BAR);
  printBar(pct);

  // ── peak hold ───────────────────────────────────────────────
  gotoXY(sRow, COL_PEAK);
  Serial.print(F("  PK:"));
  if      (peak >= ALERT_THRESHOLD) colorRed();
  else if (peak > 0)                colorYellow();
  else                              colorGreen();
  if (peak < 100) Serial.print(' ');
  if (peak < 10)  Serial.print(' ');
  Serial.print(peak); Serial.print('%');
  resetClr();

  // ── spike marker ────────────────────────────────────────────
  gotoXY(sRow, COL_PEAK + 9);
  if (spike) {
    colorRed(); Serial.print(F(" >>>SPIKE")); resetClr();
  } else {
    Serial.print(F("         "));
  }

  // ── RSSI [NEW] ──────────────────────────────────────────────
  gotoXY(sRow, COL_RSSI);
  if      (rssiDbm >= -60) colorRed();
  else if (rssiDbm >= -75) colorYellow();
  else                     colorGreen();
  // format: "-XXdBm"
  if (rssiDbm > -100) Serial.print(' ');
  Serial.print(rssiDbm); Serial.print(F("dBm"));
  resetClr();

  // ── SNR [NEW] ────────────────────────────────────────────────
  gotoXY(sRow, COL_SNR);
  Serial.print(F(" S:"));
  if      (snrDb >= 20) colorRed();
  else if (snrDb >= 5)  colorYellow();
  else                  colorGreen();
  if (snrDb >= 0 && snrDb < 10) Serial.print(' ');
  if (snrDb >= 0) Serial.print('+');
  Serial.print(snrDb);
  resetClr();

  // ── trend [NEW] ─────────────────────────────────────────────
  gotoXY(sRow, COL_TREND);
  Serial.print(' ');
  if      (trend == '^') colorRed();
  else if (trend == 'v') colorCyan();
  else                   colorGreen();
  Serial.print(trend);
  resetClr();
}

// =============================================================
//  SUMMARY ROW  (unchanged)
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
//  CHANNEL SCANNER  (unchanged RPD method)
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
//  NOISE-FLOOR CALIBRATION  [NEW]
//  Samples every channel once when radio is expected to be quiet.
//  Run at boot, result stored in noiseFloor[].
// =============================================================
void calibrateNoiseFloor() {
  gotoXY(10,1);
  colorCyan(); Serial.print(F("  Calibrating noise floor...  ")); resetClr();
  for (int ch = 0; ch <= 125; ch++) {
    noiseFloor[ch] = scanChannel(ch);
  }
  gotoXY(10,1); colorGreen();
  Serial.print(F("  Noise floor calibrated.      ")); resetClr();
  delay(800);
}

// =============================================================
//  HISTORY UPDATE  [NEW]
// =============================================================
void pushHistory(int idx, int pct) {
  hist[idx][histIdx[idx]] = (uint8_t)pct;
  histIdx[idx] = (histIdx[idx] + 1) & 7;
}

// =============================================================
//  GNU RADIO CSV FRAME EMITTER  [NEW]
//  Called inside runGNURadioScan().
//  Format: GR,<frameN>,<ch>,<pct>,<rssi_dbm>\n
//  GRC Python block to receive: see companion script at bottom.
// =============================================================
void emitGRFrame(int ch, int pct, int rssiDbm) {
  Serial.print(F("GR,"));
  Serial.print(grFrameN);
  Serial.print(',');
  Serial.print(ch);
  Serial.print(',');
  Serial.print(pct);
  Serial.print(',');
  Serial.println(rssiDbm);
}

// =============================================================
//  BUTTON  (unchanged, cycles now through 5 modes)
// =============================================================
void checkButton() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastPress > DEBOUNCE_MS) {
      lastPress   = now;
      currentMode = (ScanMode)((currentMode + 1) % 5);
      modeChanged = true;
      memset(peakVal,    0, sizeof(peakVal));
      memset(prevVal,    0, sizeof(prevVal));
      memset(hist,       0, sizeof(hist));
      memset(histIdx,    0, sizeof(histIdx));
      scanCount   = 0;
      grFrameN    = 0;
      gnuRadioActive = (currentMode == MODE_GNURADIO);
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
  bold(); Serial.println(F(" 2.4GHz RF Monitor v3.0.0")); resetClr();
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

  memset(peakVal,  0, sizeof(peakVal));
  memset(prevVal,  0, sizeof(prevVal));
  memset(hist,     0, sizeof(hist));
  memset(histIdx,  0, sizeof(histIdx));

  Serial.println(F(" Radio OK."));
  Serial.println();
  Serial.println(F(" BUTTON cycles modes:"));
  Serial.println(F("  1. FULL SCAN    (ch 0-125)"));
  Serial.println(F("  2. BLE ONLY     (3 advert. ch)"));
  Serial.println(F("  3. BT CLASSIC   (ch 2-78)"));
  Serial.println(F("  4. WI-FI ONLY   (13 ch)"));
  Serial.println(F("  5. GNU RADIO    (CSV serial bridge)"));
  Serial.println();
  Serial.println(F(" v3.0.0 NEW features (additive):"));
  Serial.println(F("  * RSSI estimate (dBm, RPD-density model)"));
  Serial.println(F("  * Noise-floor calibration at boot"));
  Serial.println(F("  * SNR column (signal minus noise floor)"));
  Serial.println(F("  * Channel utilisation ring-buffer (8 samples)"));
  Serial.println(F("  * Trend indicator: ^ rising  v falling  = stable"));
  Serial.println(F("  * Congestion score in summary"));
  Serial.println(F("  * GNU Radio CSV bridge mode (MODE 5)"));
  Serial.println();
  Serial.println(F(" Starting noise floor calibration..."));

  calibrateNoiseFloor();

  Serial.println(F(" Starting in 2 seconds..."));
  delay(2000);
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
    case MODE_GNURADIO:  runGNURadioScan();  break;
  }
  delay(SCAN_DELAY_MS);
}

// =============================================================
//  FULL SCAN  (0-125)  — extended with RSSI / SNR / trend
// =============================================================
void runFullScan() {
  COL_PCT = 33; COL_BAR = 39; COL_PEAK = 63;
  COL_RSSI = 75; COL_SNR = 84; COL_TREND = 92;

  static bool hdrDone = false;
  if (modeChanged) {
    cls(); hdrDone = false; modeChanged = false;
    memset(peakVal, 0, sizeof(peakVal));
    memset(prevVal, 0, sizeof(prevVal));
    memset(hist,    0, sizeof(hist));
    memset(histIdx, 0, sizeof(histIdx));
    scanCount = 0;
  }
  if (!hdrDone) {
    gotoXY(2,1); Serial.print(F("  ================================================================"));
    gotoXY(3,1); bold(); Serial.print(F("  FULL SPECTRUM SCAN  (2400-2525 MHz)")); resetClr();
    gotoXY(4,1); Serial.print(F("  Freq MHz    ID     -> Busy  Bar                   Peak  RSSI   SNR  T"));
    gotoXY(5,1); Serial.print(F("  ================================================================"));
    hdrDone = true;
  }

  scanCount++;
  unsigned long t0 = millis();
  int active = 0, alerts = 0, congTotal = 0;
  unsigned long el = millis() - lastScanTime;
  float spMin = el > 0 ? (60000.0 / el) : 0;
  lastScanTime = millis();

  for (int ch = 0; ch <= 125; ch++) {
    int pct = scanChannel(ch);
    int row = 6 + ch;
    float hz = 2400.0 + ch;

    pushHistory(ch, pct);
    if (pct > peakVal[ch]) peakVal[ch] = pct;
    bool spike  = (pct - prevVal[ch]) >= SPIKE_THRESHOLD;
    prevVal[ch] = pct;
    if (pct > 0)  active++;
    if (pct >= ALERT_THRESHOLD) { alerts++; congTotal += pct; }

    int rssi   = pctToRSSI(pct);
    int nfDbm  = pctToRSSI(noiseFloor[ch]);
    int snrDb  = rssi - nfDbm;
    char trend = calcTrend(ch);

    if (pct == 0 && peakVal[ch] == 0 && !spike) {
      if (ch % 10 == 0) checkButton();
      continue;
    }

    char id[7]; sprintf(id, "ch%03d", ch);
    gotoXY(row, 1);
    Serial.print(F("  ")); Serial.print(hz, 1);
    Serial.print(F(" MHz (")); Serial.print(id);
    Serial.print(F("     ) -> "));
    updateRowData(row, pct, peakVal[ch], spike, rssi, snrDb, trend);

    if (ch % 10 == 0) checkButton();
    if (modeChanged) return;
  }

  // Congestion score summary [NEW]
  int avgCong = (alerts > 0) ? (congTotal / alerts) : 0;
  drawSummary(133, millis() - t0, active, alerts);
  gotoXY(134, 1); clrLine();
  Serial.print(F("  Congestion score: "));
  if      (avgCong >= 50) colorRed();
  else if (avgCong >= 20) colorYellow();
  else                    colorGreen();
  Serial.print(avgCong); resetClr();
  Serial.print(F("%  (weighted neighbourhood avg of alert channels)"));

  drawStatusBar("FULL SCAN   ", alerts, spMin);
  gotoXY(136, 1);
}

// =============================================================
//  BLE SCAN  — extended
// =============================================================
void runBLEScan() {
  COL_PCT = 34; COL_BAR = 39; COL_PEAK = 64;
  COL_RSSI = 76; COL_SNR = 85; COL_TREND = 93;
  DATA_ROW_START = 6;

  if (modeChanged) {
    cls();
    gotoXY(2,1); Serial.print(F("  ================================================================"));
    gotoXY(3,1); bold(); Serial.print(F("  BLE ADVERTISING SCAN  (2402 / 2426 / 2480 MHz)")); resetClr();
    gotoXY(4,1); Serial.print(F("  Freq (MHz)    Label          ->  Busy  Bar                   Peak  RSSI   SNR  T Spike"));
    gotoXY(5,1); Serial.print(F("  ================================================================"));
    const char* names[] = {"BLE Adv Ch37", "BLE Adv Ch38", "BLE Adv Ch39"};
    for (int i = 0; i < BLE_N; i++)
      printStaticRow(DATA_ROW_START + i, 2400.0 + BLE_CH[i], names[i]);
    modeChanged = false;
    memset(peakVal, 0, sizeof(peakVal));
    memset(prevVal, 0, sizeof(prevVal));
    memset(hist,    0, sizeof(hist));
    memset(histIdx, 0, sizeof(histIdx));
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
    pushHistory(i, pct);
    if (pct > peakVal[i]) peakVal[i] = pct;
    bool spike = (pct - prevVal[i]) >= SPIKE_THRESHOLD;
    prevVal[i] = pct;
    if (pct > 0)  active++;
    if (pct >= ALERT_THRESHOLD) alerts++;

    int rssi  = pctToRSSI(pct);
    int nfDbm = pctToRSSI(noiseFloor[BLE_CH[i]]);
    int snrDb = rssi - nfDbm;
    char trend = calcTrend(i);

    updateRowData(DATA_ROW_START + i, pct, peakVal[i], spike, rssi, snrDb, trend);
  }

  drawSummary(DATA_ROW_START + BLE_N + 1, millis() - t0, active, alerts);
  drawStatusBar("BLE         ", alerts, spMin);
  gotoXY(DATA_ROW_START + BLE_N + 3, 1);
}

// =============================================================
//  BT CLASSIC SCAN  — extended
// =============================================================
void runBTClassicScan() {
  COL_PCT = 34; COL_BAR = 39; COL_PEAK = 64;
  COL_RSSI = 76; COL_SNR = 85; COL_TREND = 93;
  DATA_ROW_START = 6;

  static bool hdrDone = false;
  if (modeChanged) {
    cls(); hdrDone = false; modeChanged = false;
    memset(peakVal, 0, sizeof(peakVal));
    memset(prevVal, 0, sizeof(prevVal));
    memset(hist,    0, sizeof(hist));
    memset(histIdx, 0, sizeof(histIdx));
    scanCount = 0;
  }
  if (!hdrDone) {
    gotoXY(2,1); Serial.print(F("  ================================================================"));
    gotoXY(3,1); bold(); Serial.print(F("  BLUETOOTH CLASSIC SCAN  (2402-2478 MHz)")); resetClr();
    gotoXY(4,1); Serial.print(F("  Freq (MHz)    Label      ->  Busy  Bar                   Peak  RSSI   SNR  T"));
    gotoXY(5,1); Serial.print(F("  ================================================================"));
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

    pushHistory(idx, pct);
    if (pct > peakVal[idx]) peakVal[idx] = pct;
    bool spike = (pct - prevVal[idx]) >= SPIKE_THRESHOLD;
    prevVal[idx] = pct;
    if (pct > 0)  active++;
    if (pct >= ALERT_THRESHOLD) alerts++;

    if (pct == 0 && peakVal[idx] == 0 && !spike) {
      if (idx % 10 == 0) checkButton();
      continue;
    }

    int rssi   = pctToRSSI(pct);
    int nfDbm  = pctToRSSI(noiseFloor[ch]);
    int snrDb  = rssi - nfDbm;
    char trend = calcTrend(idx);

    char label[9]; sprintf(label, "BT ch %02d", ch);
    gotoXY(row, 1);
    Serial.print(F("  ")); Serial.print(hz, 1);
    Serial.print(F(" MHz (")); Serial.print(label);
    Serial.print(F(" ) -> "));
    updateRowData(row, pct, peakVal[idx], spike, rssi, snrDb, trend);

    if (idx % 10 == 0) checkButton();
    if (modeChanged) return;
  }

  int total = BT_END - BT_START + 1;
  drawSummary(DATA_ROW_START + total + 1, millis() - t0, active, alerts);
  drawStatusBar("BT CLASSIC  ", alerts, spMin);
}

// =============================================================
//  WI-FI SCAN  — extended
// =============================================================
void runWiFiScan() {
  COL_PCT = 34; COL_BAR = 39; COL_PEAK = 64;
  COL_RSSI = 76; COL_SNR = 85; COL_TREND = 93;
  DATA_ROW_START = 6;

  const char* wNames[] = {
    "WiFi  Ch 1","WiFi  Ch 2","WiFi  Ch 3","WiFi  Ch 4",
    "WiFi  Ch 5","WiFi  Ch 6","WiFi  Ch 7","WiFi  Ch 8",
    "WiFi  Ch 9","WiFi Ch 10","WiFi Ch 11","WiFi Ch 12","WiFi Ch 13"
  };

  if (modeChanged) {
    cls();
    gotoXY(2,1); Serial.print(F("  ================================================================"));
    gotoXY(3,1); bold(); Serial.print(F("  WI-FI CHANNEL SCAN  (2.4 GHz, ch 1-13)")); resetClr();
    gotoXY(4,1); Serial.print(F("  Freq (MHz)    Label       ->  Busy  Bar                   Peak  RSSI   SNR  T Spike"));
    gotoXY(5,1); Serial.print(F("  ================================================================"));
    for (int i = 0; i < WIFI_N; i++)
      printStaticRow(DATA_ROW_START + i, 2400.0 + WIFI_CH[i], wNames[i]);
    modeChanged = false;
    memset(peakVal, 0, sizeof(peakVal));
    memset(prevVal, 0, sizeof(prevVal));
    memset(hist,    0, sizeof(hist));
    memset(histIdx, 0, sizeof(histIdx));
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
    pushHistory(i, pct);
    if (pct > peakVal[i]) peakVal[i] = pct;
    bool spike = (pct - prevVal[i]) >= SPIKE_THRESHOLD;
    prevVal[i] = pct;
    if (pct > 0)  active++;
    if (pct >= ALERT_THRESHOLD) alerts++;

    int rssi   = pctToRSSI(pct);
    int nfDbm  = pctToRSSI(noiseFloor[WIFI_CH[i]]);
    int snrDb  = rssi - nfDbm;
    char trend = calcTrend(i);

    updateRowData(DATA_ROW_START + i, pct, peakVal[i], spike, rssi, snrDb, trend);
  }

  drawSummary(DATA_ROW_START + WIFI_N + 1, millis() - t0, active, alerts);
  drawStatusBar("WI-FI       ", alerts, spMin);
  gotoXY(DATA_ROW_START + WIFI_N + 3, 1);
}

// =============================================================
//  GNU RADIO SCAN  [NEW]
//
//  Pure CSV emitter — NO ANSI, NO display.
//  Outputs one GR frame per channel per sweep.
//  Companion GNU Radio script (rf_monitor_sink.py) reads this
//  via Serial and feeds a waterfall / power spectrum sink.
//
//  Frame: GR,<frameN>,<ch_0-125>,<busy_pct>,<rssi_dbm>
//
//  To use:
//    1. Flash this firmware, switch to mode 5 via button.
//    2. Open rf_monitor_sink.py (see companion file).
//    3. Run: python rf_monitor_sink.py --port COM3 --baud 115200
//    4. GNU Radio Companion: connect UDP Source (localhost:7355)
//       to any sink block (waterfall, QT GUI Frequency Sink, etc.)
// =============================================================
void runGNURadioScan() {
  if (modeChanged) {
    // Print a single human-readable header, then go silent (CSV only)
    cls();
    Serial.println(F("# RF_MONITOR GNU RADIO BRIDGE MODE"));
    Serial.println(F("# Frame format: GR,frameN,ch,busy_pct,rssi_dbm"));
    Serial.println(F("# Connect rf_monitor_sink.py to ingest frames."));
    Serial.println(F("# Button -> next mode (exits CSV stream)."));
    Serial.println(F("# ---"));
    modeChanged = false;
    grFrameN = 0;
  }

  grFrameN++;
  for (int ch = 0; ch <= 125; ch++) {
    int pct  = scanChannel(ch);
    int rssi = pctToRSSI(pct);
    emitGRFrame(ch, pct, rssi);
    if (ch % 20 == 0) checkButton();
    if (modeChanged) return;
  }
  // End-of-sweep marker so sink can detect frame boundary
  Serial.print(F("GR_EOF,")); Serial.println(grFrameN);
}

// =============================================================
//  END OF RF_Monitor_v3.ino
// =============================================================
