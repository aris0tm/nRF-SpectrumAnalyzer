# RF Monitor v3 — ML Attack Detection Pipeline
## Full pipeline: Logger → Trainer → Live Detector + Dashboard

```
Arduino (MODE 5)
    │  serial CSV frames
    ▼
rf_logger.py      ──►  dataset.csv        (labeled sweeps)
    │
    ▼
rf_trainer.py     ──►  rf_model.pkl       (trained model bundle)
    │
    ▼
rf_detector.py    ──►  detections.jsonl   (live detection log)
    │                      +
    └──► ws://localhost:8765  ──►  rf_dashboard.html  (browser)
```

---

## INSTALL

```bash
pip install pyserial scikit-learn numpy pandas joblib websockets
```

---

## STEP 1 — Collect Labeled Data

Flash RF_Monitor_v3.ino. Press button to **Mode 5 (GNU RADIO BRIDGE)**.

```bash
python rf_logger.py --port COM3 --out dataset.csv
```

**Hot-keys while logging** (type key + Enter):
| Key | Label            | Simulate by...                               |
|-----|------------------|----------------------------------------------|
| `n` | normal           | idle environment                             |
| `j` | jamming          | run a 2.4 GHz jammer or microwave nearby     |
| `w` | wifi_deauth      | use a WiFi deauth tool (lab only)            |
| `b` | ble_flood        | run BLE scanner flood script                 |
| `t` | bt_interference  | pair/un-pair many BT devices simultaneously  |
| `x` | unknown          | discard row                                  |
| `q` | quit             |                                              |

**Minimum recommended samples per class: 50–200**
More data = better accuracy. 500+ per class is ideal.

---

## STEP 2 — Train the Model

```bash
python rf_trainer.py --data dataset.csv --out rf_model.pkl
```

Output:
- Cross-validation F1 score
- Per-class classification report
- Confusion matrix
- Top-10 feature importances
- Saved model bundle: `rf_model.pkl`

**Two models inside the bundle:**
1. `IsolationForest` — unsupervised anomaly baseline (works with 0 labels)
2. `RandomForestClassifier` — supervised multi-class (5 attack types)

**Retrain any time** you collect more data. Just re-run rf_trainer.py.

---

## STEP 3 — Live Detection

Arduino must still be in **Mode 5**.

```bash
python rf_detector.py --port COM3 --model rf_model.pkl
```

Then open **rf_dashboard.html** in your browser.

### Terminal alert levels:
| Level   | Meaning                                    | Color  |
|---------|--------------------------------------------|--------|
| CLEAR   | Normal baseline                            | Green  |
| ANOMALY | IsolationForest flagged, RF not confident  | Yellow |
| WARNING | RF ≥60% confident, non-normal class        | Orange |
| ATTACK  | RF ≥85% confident, non-normal class        | Red    |

### Dashboard panels:
- **Spectrum** — live 126-channel power bar chart with WiFi/BLE overlays
- **Waterfall** — scrolling history (last 60 sweeps)
- **Classifier Probabilities** — live bar chart per attack class
- **Isolation Forest Score** — anomaly gauge
- **Band Activity** — WiFi / BLE / BT energy mini-chart
- **Detection Log** — timestamped event stream

---

## HOW THE DETECTION WORKS

### Features engineered from each sweep (31 total):
- Band energies (WiFi sum, BLE sum, BT sum)
- Global stats: active count, alert count, max%, mean%, variance
- Derived ratios: wifi/total, ble/total, bt/total
- Spectral moments: skewness, kurtosis
- Band-specific max and std
- Delta features: change from previous sweep (catches transient attacks)

### Attack signatures the model learns:
| Attack           | Signature in features                                      |
|------------------|------------------------------------------------------------|
| Jamming          | High mean_pct, high alert_count, elevated band_variance    |
| WiFi deauth      | Burst on WIFI_CHANNELS, high d_wifi_band_energy (delta)    |
| BLE flood        | Sustained ble_band_energy spike on ch 2/26/80              |
| BT interference  | Elevated bt_band_energy across ch 2-78, erratic variance   |
| Anomaly (any)    | IsolationForest deviation from learned normal baseline     |

---

## FILES

| File                | Purpose                                |
|---------------------|----------------------------------------|
| RF_Monitor_v3.ino   | Arduino firmware (all modes + Mode 5) |
| rf_logger.py        | Sweep logger + real-time labeller      |
| rf_trainer.py       | Model trainer (RF + IsoForest)         |
| rf_detector.py      | Live inference + WS server             |
| rf_dashboard.html   | Browser dashboard (open locally)       |
| rf_model.pkl        | Trained model bundle (after training)  |
| dataset.csv         | Labeled training data                  |
| detections.jsonl    | Live detection log (JSON lines)        |

---

## NOTES ON NRF24 RSSI ACCURACY

The nRF24L01 uses a 1-bit RPD (Received Power Detector) threshold
at approximately -64 dBm. It does NOT have a true RSSI ADC.

The "dBm" values in this system are **estimated** from busy-% density
using a linear model (0% → -90 dBm, 100% → -40 dBm).

This is sufficient for **relative comparison and ML pattern detection**
but should not be treated as calibrated RF measurement.

For true RSSI measurements, use an RTL-SDR or HackRF with GNU Radio.
