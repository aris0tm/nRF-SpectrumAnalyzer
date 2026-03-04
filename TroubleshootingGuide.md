## Troubleshooting Guide

### 7.1 Common Issues

**Issue 1: "NRF24 not detected!" Error**

**Symptoms:**
```
ERROR: NRF24 not detected!
Check wiring and power (3.3V only!)
```

**Root Causes & Solutions:**

| Cause | Diagnostic | Solution |
|-------|-----------|----------|
| Wiring error | Check continuity | Re-verify all 7 connections |
| Wrong voltage | Measure VCC pin | Must be 3.3V, NOT 5V! |
| Damaged module | Swap module | Try known-good NRF24 |
| CE/CSN swapped | Check pin assignments | Verify CE=9, CSN=10 |

**Debug Procedure:**
```
1. Power off Arduino
2. Disconnect all wires from NRF24
3. Measure Arduino 3.3V pin: Should read 3.2-3.4V
4. If reads 5V: Check you're using 3.3V pin, not 5V pin
5. Reconnect only VCC and GND
6. Measure voltage at NRF24 VCC pin
7. If correct (3.3V), proceed with signal wires
8. If incorrect, trace wiring error
```

---

**Issue 2: All Channels Show 0%**

**Symptoms:**
```
2400.0 MHz -> 0%
2401.0 MHz -> 0%
...
Active channels: 0
```

**Solutions:**

**A. Increase RPD Update Time**
```cpp
// In scanChannel() function, change:
delayMicroseconds(200);  // Try 500 or 1000
```

**B. Add Larger Capacitor**
- Replace 10µF with 100µF
- Power instability can cause detection failure

**C. Test with Known Source**
```cpp
// Simple test code:
void loop() {
  radio.setChannel(40);
  radio.startListening();
  delay(1);
  Serial.println(radio.testRPD() ? "DETECT" : "Nothing");
  radio.stopListening();
  delay(100);
}
// Place phone/laptop nearby broadcasting Wi-Fi
// Should see frequent "DETECT" messages
```

---

**Issue 3: Button Doesn't Work**

**Symptoms:**
- Pressing button has no effect
- Mode never changes

**Solutions:**

**A. Verify Wiring**
```
Correct:  Pin 2 ──┬── Button ──┬── GND
Incorrect: Pin 2 ──┬── Button ──┬── 5V (damages Arduino!)
```

**B. Test Button Hardware**
```cpp
// Add to loop() for debugging:
Serial.print("Button state: ");
Serial.println(digitalRead(buttonPin));
// Should read 1 (HIGH) when not pressed
// Should read 0 (LOW) when pressed
```

**C. Reduce Debounce Time**
```cpp
const int debounceDelay = 100;  // Was 300, try lower
```

---

### 7.2 Performance Optimization

**Faster Scanning:**
```cpp
// Aggressive timing (may reduce reliability)
const int samplesPerChannel = 25;    // Was 50
delayMicroseconds(100);              // Was 200
delay(1);                            // Was delay(2)

Result: ~7 second full scan (vs 15 seconds)
Trade-off: ~10% lower detection probability
```

**Higher Accuracy:**
```cpp
// Conservative timing (slower but more reliable)
const int samplesPerChannel = 100;   // Was 50
delay(5);                            // Was delay(2)

Result: ~30 second full scan
Benefit: >98% detection probability
```

### 7.3 Calibration

**Baseline Establishment:**

Establish RF environment baseline for anomaly detection:

```
1. Go to location with minimal RF activity (rural area ideal)
2. Run full spectrum scan
3. Record typical activity levels
4. Expected result:
   - 90%+ channels at 0-5%
   - Few channels at 10-20%
   - No channels >30%
5. If higher: RF pollution from nearby sources
```

**Threshold Adjustment:**

Adapt threshold to environment:

```
Procedure:
1. Run full scan in target environment
2. Note typical "busy" channel percentages
3. Set threshold 5-10% above typical background
4. Monitor false alarm rate
5. Increase if too many false positives
6. Decrease if missing known devices

Example:
Office with 3 Wi-Fi APs:
  Typical background: 5-15%
  Set threshold: 20-25%
```

---
