### 3.2 Circuit Schematic

```
                    Arduino Uno
              ┌─────────────────────┐
              │                     │
              │    ┌──────────┐     │
   USB ───────┤USB │ATmega328P│     │
   Power      │    │  16 MHz  │     │
              │    └──────────┘     │
              │                     │
 3.3V ────────┤ 3.3V           GND  ├──────┬──── GND
              │                     │      │
 Pin 9 ───────┤ D9 (CE)             │      │
              │                     │      │
 Pin 10 ──────┤ D10 (CSN)           │      │
              │                     │      │
 Pin 13 ──────┤ D13 (SCK)           │      │
              │                     │      │
 Pin 11 ──────┤ D11 (MOSI)          │      │
              │                     │      │
 Pin 12 ──────┤ D12 (MISO)          │      │
              │                     │      │
 Pin 2 ───────┤ D2 (Button)         │      │
              │     │               │      │
              └─────┼───────────────┘      │
                    │                      │
                    └───┐              ┌───┘
                        │              │
                     ┌──┴──┐        ┌──┴──┐
                     │  S  │        │ 10µF│
                     │  W  │        │ CAP │
                     └──┬──┘        └──┬──┘
                        │              │
                        └──────────────┘

            NRF24L01+PA+LNA Module
          ┌──────────────────────────┐
          │      [Antenna]           │
          │         │                │
          │  ┌──────┴──────┐         │
          │  │  nRF24L01+  │         │
          │  │   +PA+LNA   │         │
          │  └─────────────┘         │
          │                          │
          │ 1   2   3   4   5   6   7│
          └─┬───┬───┬───┬───┬───┬───┘
            │   │   │   │   │   │
         VCC│  GND  CE CSN SCK MOSI MISO
          3.3V
```

**Pin Mapping Table:**

| NRF24 Pin | Function | Arduino Pin | Description |
|-----------|----------|-------------|-------------|
| 1 | GND | GND | Ground reference |
| 2 | VCC | 3.3V | Power supply (CRITICAL: 3.3V only!) |
| 3 | CE | D9 | Chip Enable (TX/RX mode control) |
| 4 | CSN | D10 | SPI Chip Select (active low) |
| 5 | SCK | D13 | SPI Clock |
| 6 | MOSI | D11 | SPI Master Out Slave In |
| 7 | MISO | D12 | SPI Master In Slave Out |
| 8 | IRQ | (unused) | Interrupt Request (optional) |

### 3.3 Assembly Instructions

**Step 1: Solder Headers**
- Solder 8-pin male header to NRF24 module
- Ensure joints are shiny and well-formed
- Clean flux residue with isopropyl alcohol

**Step 2: Add Power Capacitor**
```
Critical: This prevents voltage sags during PA transmission

Procedure:
1. Identify capacitor polarity (long leg = +, short leg = -)
2. Bend leads to fit across VCC and GND pins
3. Solder positive leg to VCC pin on module
4. Solder negative leg to GND pin on module
5. Position flat against PCB, secure with kapton tape if needed
```

**Step 3: Wire Connections**

Follow this exact sequence to prevent errors:
```
□ GND connections first (safety ground)
  - NRF24 GND → Arduino GND

□ Power connections
  - NRF24 VCC → Arduino 3.3V
  ⚠️ VERIFY: Measure voltage = 3.2-3.4V with multimeter

□ SPI signal lines
  - NRF24 CE → Arduino Pin 9
  - NRF24 CSN → Arduino Pin 10
  - NRF24 SCK → Arduino Pin 13
  - NRF24 MOSI → Arduino Pin 11
  - NRF24 MISO → Arduino Pin 12

□ Button
  - One leg → Arduino Pin 2
  - Other leg → Arduino GND
```

**Step 4: Physical Installation**
- Keep wires under 10 cm length
- Route power wires separately from signal wires
- Orient antenna vertically for omnidirectional reception
- Secure Arduino to non-conductive surface (wood, plastic)

### 3.4 Power Supply Considerations

**Power Budget:**
| Component | Current Draw | Notes |
|-----------|--------------|-------|
| Arduino (idle) | 50 mA | Includes voltage regulator loss |
| NRF24 (RX mode) | 45 mA | Continuous during scanning |
| NRF24 (TX mode) | 115 mA | Not used in this application |
| **Total (worst case)** | **95 mA** | USB provides 500 mA (adequate) |

**Power Sources:**
- **USB (Recommended):** 5V @ 500 mA, stable and convenient
- **9V Battery + Arduino:** Poor choice (regulator inefficiency)
- **External 5V adapter:** 1A rating recommended for margin

**Voltage Verification:**
```cpp
// Add this to setup() for voltage monitoring
float vcc = readVcc() / 1000.0;
Serial.print("Supply voltage: ");
Serial.print(vcc);
Serial.println(" V");

long readVcc() {
  long result;
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));
  result = ADCL;
  result |= ADCH << 8;
  result = 1126400L / result;
  return result;
}
```
