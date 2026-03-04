---

## **Upload Instructions**

1. **Install RF24 library** (if not already installed):
   - Arduino IDE → Tools → Manage Libraries
   - Search "RF24"
   - Install "RF24 by TMRh20"

2. **Wire your NRF24+PA+LNA module**:
```
   NRF24    Arduino
   ----------------------
   VCC   →  3.3V (IMPORTANT!)
   GND   →  GND
   CE    →  Pin 9
   CSN   →  Pin 10
   SCK   →  Pin 13
   MOSI  →  Pin 11
   MISO  →  Pin 12
```

3. **Add capacitor** (recommended for PA+LNA):
   - 10μF or 100μF capacitor between VCC and GND at the module

4. **Upload and open Serial Monitor**:
   - Set baud rate to **115200**

---

## **What You Should See**

### **No Activity (quiet environment)**
```
========== NEW SCAN ==========
==============================
Scan time: 15234 ms
Active channels: 0
Alert count: 0
Status: No significant activity
==============================
```

### **Normal Environment (Wi-Fi router nearby)**
```
========== NEW SCAN ==========
2412.0 MHz -> 12% [##]
2437.0 MHz -> 87% [#################] <<< ALERT
2462.0 MHz -> 35% [#######] <<< ALERT
==============================
Scan time: 15298 ms
Active channels: 3
Alert count: 2
Status: Normal (Wi-Fi/BT devices)
==============================
```

### **Bluetooth Device Active**
```
========== NEW SCAN ==========
2402.0 MHz -> 28% [#####] <<< ALERT
2426.0 MHz -> 32% [######] <<< ALERT
2440.0 MHz -> 22% [####] <<< ALERT
2450.0 MHz -> 25% [#####] <<< ALERT
2480.0 MHz -> 35% [#######] <<< ALERT
==============================
Active channels: 5
Alert count: 5
Status: High activity detected
==============================
```

### **Flooding Attack (Your Test Module)**
```
========== NEW SCAN ==========
2400.0 MHz -> 96% [###################] <<< ALERT
2401.0 MHz -> 94% [##################] <<< ALERT
2402.0 MHz -> 98% [###################] <<< ALERT
2403.0 MHz -> 95% [###################] <<< ALERT
... (continues for many channels)
==============================
Active channels: 68
Alert count: 68
Status: *** FLOODING DETECTED ***
==============================