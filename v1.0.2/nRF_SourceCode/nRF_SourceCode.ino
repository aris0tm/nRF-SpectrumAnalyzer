#include <SPI.h>
#include <RF24.h>

RF24 radio(9, 10); // CE, CSN pins

// ===== BUTTON CONFIGURATION =====
const int buttonPin = 2;           // Button connected to pin 2 (with pullup)
unsigned long lastButtonPress = 0;
const int debounceDelay = 300;     // 300ms debounce
// ================================

// ===== DETECTION MODES =====
enum ScanMode {
  MODE_FULL,      // All channels (0-125)
  MODE_BLE,       // BLE channels only
  MODE_BLUETOOTH, // Bluetooth Classic channels
  MODE_WIFI       // Wi-Fi channels only
};

ScanMode currentMode = MODE_FULL;
// ===========================

// ===== DETECTION TUNING =====
const int samplesPerChannel = 50;
const int busyThresholdPercent = 20;
const int scanDelayMs = 1000;
const bool showOnlyActive = true;
const bool showVisualBars = true;
// ============================

// ===== CHANNEL DEFINITIONS =====
// BLE uses 40 channels, but these 3 are advertising channels (most active)
const int bleChannels[] = {2, 26, 80};        // 2402, 2426, 2480 MHz
const int bleChannelCount = 3;

// Bluetooth Classic uses channels 0-78 (2402-2480 MHz, 1 MHz spacing)
const int btClassicStart = 2;   // 2402 MHz
const int btClassicEnd = 78;    // 2478 MHz

// Wi-Fi uses channels with 5 MHz spacing (but overlapping)
// These are the center frequencies of Wi-Fi channels 1-13
const int wifiChannels[] = {12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72}; 
const int wifiChannelCount = 13;
// ================================

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  // Setup button with internal pullup
  pinMode(buttonPin, INPUT_PULLUP);
  
  Serial.println("\n=================================");
  Serial.println(" 2.4 GHz RF Monitor v3.0");
  Serial.println(" Multi-Mode Detection System");
  Serial.println("=================================");
  
  // Initialize radio
  if (!radio.begin()) {
    Serial.println("ERROR: NRF24 not detected!");
    Serial.println("Check wiring and power (3.3V only!)");
    while (1) { delay(1000); }
  }
  
  // Configure for maximum detection sensitivity
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MIN);
  radio.setAddressWidth(5);
  
  Serial.println("\nRadio initialized successfully!");
  Serial.println("\n*** BUTTON CONTROLS ***");
  Serial.println("Press button to cycle modes:");
  Serial.println("  1. FULL SCAN (all channels)");
  Serial.println("  2. BLE ONLY (advertising channels)");
  Serial.println("  3. BLUETOOTH CLASSIC");
  Serial.println("  4. WI-FI ONLY");
  Serial.println("***********************\n");
  
  printCurrentMode();
  delay(2000);
}

void loop() {
  // Check for button press
  checkButton();
  
  // Run scan based on current mode
  switch (currentMode) {
    case MODE_FULL:
      scanFullSpectrum();
      break;
    case MODE_BLE:
      scanBLE();
      break;
    case MODE_BLUETOOTH:
      scanBluetoothClassic();
      break;
    case MODE_WIFI:
      scanWiFi();
      break;
  }
  
  delay(scanDelayMs);
}

// ===== BUTTON HANDLER =====
void checkButton() {
  if (digitalRead(buttonPin) == LOW) {
    unsigned long currentTime = millis();
    
    // Debounce check
    if (currentTime - lastButtonPress > debounceDelay) {
      lastButtonPress = currentTime;
      
      // Cycle to next mode
      currentMode = (ScanMode)((currentMode + 1) % 4);
      
      // Print new mode
      Serial.println("\n\n*** MODE CHANGED ***");
      printCurrentMode();
      Serial.println("********************\n");
      
      delay(500); // Brief pause after mode change
    }
  }
}

void printCurrentMode() {
  Serial.print("Current Mode: ");
  switch (currentMode) {
    case MODE_FULL:
      Serial.println("FULL SCAN (All 126 channels)");
      break;
    case MODE_BLE:
      Serial.println("BLE ONLY (3 advertising channels)");
      break;
    case MODE_BLUETOOTH:
      Serial.println("BLUETOOTH CLASSIC (77 channels)");
      break;
    case MODE_WIFI:
      Serial.println("WI-FI ONLY (13 channels)");
      break;
  }
}

// ===== SCAN FUNCTIONS =====

void scanFullSpectrum() {
  Serial.println("========== FULL SPECTRUM SCAN ==========");
  unsigned long scanStart = millis();
  int totalAlerts = 0;
  int activeChannels = 0;
  
  for (int channel = 0; channel <= 125; channel++) {
    int busyPercent = scanChannel(channel);
    float freqMHz = 2400.0 + channel;
    
    if (busyPercent > 0) activeChannels++;
    if (busyPercent >= busyThresholdPercent) totalAlerts++;
    
    if (!showOnlyActive || busyPercent > 0) {
      printChannelResult(freqMHz, busyPercent, "");
    }
  }
  
  printSummary(millis() - scanStart, activeChannels, totalAlerts);
}

void scanBLE() {
  Serial.println("========== BLE ADVERTISING SCAN ==========");
  unsigned long scanStart = millis();
  int totalAlerts = 0;
  int activeChannels = 0;
  
  const char* bleNames[] = {"BLE Ch 37", "BLE Ch 38", "BLE Ch 39"};
  
  for (int i = 0; i < bleChannelCount; i++) {
    int channel = bleChannels[i];
    int busyPercent = scanChannel(channel);
    float freqMHz = 2400.0 + channel;
    
    if (busyPercent > 0) activeChannels++;
    if (busyPercent >= busyThresholdPercent) totalAlerts++;
    
    printChannelResult(freqMHz, busyPercent, bleNames[i]);
  }
  
  printSummary(millis() - scanStart, activeChannels, totalAlerts);
  
  if (totalAlerts > 0) {
    Serial.println("*** BLE DEVICES DETECTED ***");
  }
}

void scanBluetoothClassic() {
  Serial.println("========== BLUETOOTH CLASSIC SCAN ==========");
  unsigned long scanStart = millis();
  int totalAlerts = 0;
  int activeChannels = 0;
  
  for (int channel = btClassicStart; channel <= btClassicEnd; channel++) {
    int busyPercent = scanChannel(channel);
    float freqMHz = 2400.0 + channel;
    
    if (busyPercent > 0) activeChannels++;
    if (busyPercent >= busyThresholdPercent) totalAlerts++;
    
    if (!showOnlyActive || busyPercent > 0) {
      printChannelResult(freqMHz, busyPercent, "BT");
    }
  }
  
  printSummary(millis() - scanStart, activeChannels, totalAlerts);
  
  if (totalAlerts > 0) {
    Serial.println("*** BLUETOOTH ACTIVITY DETECTED ***");
  }
}

void scanWiFi() {
  Serial.println("========== WI-FI SCAN ==========");
  unsigned long scanStart = millis();
  int totalAlerts = 0;
  int activeChannels = 0;
  
  for (int i = 0; i < wifiChannelCount; i++) {
    int channel = wifiChannels[i];
    int busyPercent = scanChannel(channel);
    float freqMHz = 2400.0 + channel;
    
    if (busyPercent > 0) activeChannels++;
    if (busyPercent >= busyThresholdPercent) totalAlerts++;
    
    char label[20];
    sprintf(label, "WiFi Ch %d", i + 1);
    printChannelResult(freqMHz, busyPercent, label);
  }
  
  printSummary(millis() - scanStart, activeChannels, totalAlerts);
  
  if (totalAlerts > 0) {
    Serial.println("*** WI-FI NETWORKS DETECTED ***");
  }
}

// ===== HELPER FUNCTIONS =====

int scanChannel(int channel) {
  radio.setChannel(channel);
  delay(2); // Allow radio to settle
  
  int busyCount = 0;
  
  for (int i = 0; i < samplesPerChannel; i++) {
    radio.startListening();
    delayMicroseconds(200);
    
    if (radio.testRPD()) {
      busyCount++;
    }
    
    radio.stopListening();
    delayMicroseconds(10);
  }
  
  return (busyCount * 100) / samplesPerChannel;
}

void printChannelResult(float freqMHz, int busyPercent, const char* label) {
  // Frequency
  Serial.print(freqMHz, 1);
  Serial.print(" MHz");
  
  // Label (if provided)
  if (strlen(label) > 0) {
    Serial.print(" (");
    Serial.print(label);
    Serial.print(")");
  }
  
  Serial.print(" -> ");
  
  // Percentage
  if (busyPercent < 10) Serial.print(" ");
  Serial.print(busyPercent);
  Serial.print("%");
  
  // Visual bar
  if (showVisualBars && busyPercent > 0) {
    Serial.print(" [");
    int bars = busyPercent / 5;
    for (int b = 0; b < bars && b < 20; b++) {
      Serial.print("#");
    }
    Serial.print("]");
  }
  
  // Alert
  if (busyPercent >= busyThresholdPercent) {
    Serial.print(" <<< ALERT");
  }
  
  Serial.println();
}

void printSummary(unsigned long scanTime, int activeChannels, int totalAlerts) {
  Serial.println("========================================");
  Serial.print("Scan time: ");
  Serial.print(scanTime);
  Serial.println(" ms");
  Serial.print("Active channels: ");
  Serial.println(activeChannels);
  Serial.print("Alert count: ");
  Serial.println(totalAlerts);
  
  if (totalAlerts == 0) {
    Serial.println("Status: No significant activity");
  } else if (totalAlerts < 5) {
    Serial.println("Status: Normal activity");
  } else if (totalAlerts < 20) {
    Serial.println("Status: High activity detected");
  } else {
    Serial.println("Status: *** FLOODING DETECTED ***");
  }
  
  Serial.println("========================================\n");
}
