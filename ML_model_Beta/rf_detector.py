#!/usr/bin/env python3
"""
rf_detector.py  —  RF Monitor Live Attack Detector
===================================================
Reads GNU Radio CSV frames from the Arduino (MODE 5),
runs the trained model bundle in real time, and:
  • Prints colour-coded alerts to the terminal
  • Serves a live WebSocket dashboard on http://localhost:8765
  • Logs detections to detections.jsonl

USAGE
-----
    python rf_detector.py --port COM3 --model rf_model.pkl

ALERT LEVELS
------------
    CLEAR       — normal baseline
    ANOMALY     — IsolationForest flagged it, RF not confident
    WARNING     — RF confident >= 60%, non-normal class
    ATTACK      — RF confident >= 85%, non-normal class

DEPENDENCIES
------------
    pip install pyserial scikit-learn joblib websockets numpy
"""

import argparse, asyncio, json, sys, time, os, threading
import collections, statistics, warnings
warnings.filterwarnings('ignore')

try:
    import serial
except ImportError:
    print("[ERROR] pip install pyserial"); sys.exit(1)

try:
    import numpy as np
    import joblib
    import websockets
except ImportError as e:
    print(f"[ERROR] Missing: {e}")
    print("  pip install scikit-learn joblib websockets numpy")
    sys.exit(1)

# ── channel maps ─────────────────────────────────────────────
BLE_CHANNELS  = [2, 26, 80]
WIFI_CHANNELS = [12,17,22,27,32,37,42,47,52,57,62,67,72]
BT_RANGE      = list(range(2, 79))
NUM_CH        = 126
ALERT_THR     = 20
SPIKE_THR     = 15
RSSI_MIN, RSSI_MAX = -90, -40

# ── confidence thresholds ─────────────────────────────────────
CONF_ATTACK  = 0.85
CONF_WARNING = 0.60

# ── ANSI colours ─────────────────────────────────────────────
RED    = "\033[91m"
YELLOW = "\033[93m"
GREEN  = "\033[92m"
CYAN   = "\033[96m"
BOLD   = "\033[1m"
RESET  = "\033[0m"

def pct_to_rssi(pct):
    return RSSI_MIN + (pct * (RSSI_MAX - RSSI_MIN) / 100)

# ── shared state between serial thread and WS server ─────────
latest_event  = {}
event_lock    = threading.Lock()
ws_clients    = set()
ws_loop       = None
alert_history = collections.deque(maxlen=200)

def make_feature_vector(sweep_pct, prev_pct, feat_names):
    """
    Build the same feature vector as rf_trainer.py — must stay in sync.
    """
    pcts = [sweep_pct.get(c, 0) for c in range(NUM_CH)]

    active      = sum(1 for p in pcts if p > 0)
    alerts      = sum(1 for p in pcts if p >= ALERT_THR)
    max_pct     = max(pcts)
    mean_pct    = sum(pcts) / NUM_CH
    wifi_energy = sum(pcts[c] for c in WIFI_CHANNELS)
    ble_energy  = sum(pcts[c] for c in BLE_CHANNELS)
    bt_energy   = sum(pcts[c] for c in BT_RANGE)
    spikes      = sum(1 for c in range(NUM_CH)
                      if pcts[c] - prev_pct.get(c, 0) >= SPIKE_THR)
    variance    = statistics.variance(pcts) if len(pcts) > 1 else 0.0

    total_e = wifi_energy + ble_energy + bt_energy + 1e-6
    pct_arr = np.array(pcts, dtype=float)
    mean_a  = pct_arr.mean()
    std_a   = pct_arr.std() + 1e-6
    skew    = float(((pct_arr - mean_a) ** 3).mean() / (std_a ** 3))
    kurt    = float(((pct_arr - mean_a) ** 4).mean() / (std_a ** 4))

    prev_arr = np.array([prev_pct.get(c, 0) for c in range(NUM_CH)], dtype=float)

    base = {
        'active_count':      active,
        'alert_count':       alerts,
        'max_pct':           max_pct,
        'mean_pct':          round(mean_pct, 3),
        'wifi_band_energy':  wifi_energy,
        'ble_band_energy':   ble_energy,
        'bt_band_energy':    bt_energy,
        'spike_count':       spikes,
        'band_variance':     round(variance, 3),
        'wifi_ratio':        wifi_energy / total_e,
        'ble_ratio':         ble_energy  / total_e,
        'bt_ratio':          bt_energy   / total_e,
        'spectral_skew':     skew,
        'spectral_kurt':     kurt,
        'wifi_max':          pct_arr[WIFI_CHANNELS].max(),
        'ble_max':           pct_arr[BLE_CHANNELS].max(),
        'bt_max':            pct_arr[BT_RANGE].max(),
        'wifi_band_std':     pct_arr[12:73].std(),
        'ble_band_std':      pct_arr[0:83].std(),
        'bt_band_std':       pct_arr[2:79].std(),
    }
    # Delta features
    prev_wifi = float(sum(prev_arr[c] for c in WIFI_CHANNELS))
    prev_ble  = float(sum(prev_arr[c] for c in BLE_CHANNELS))
    prev_bt   = float(sum(prev_arr[c] for c in BT_RANGE))
    prev_active = int(sum(1 for c in range(NUM_CH) if prev_arr[c] > 0))
    prev_alerts = int(sum(1 for c in range(NUM_CH) if prev_arr[c] >= ALERT_THR))
    prev_max    = float(prev_arr.max())
    prev_spikes = 0  # can't compute from single snapshot

    base['d_active_count']      = active - prev_active
    base['d_alert_count']       = alerts - prev_alerts
    base['d_max_pct']           = max_pct - prev_max
    base['d_spike_count']       = spikes - prev_spikes
    base['d_wifi_band_energy']  = wifi_energy - prev_wifi
    base['d_ble_band_energy']   = ble_energy  - prev_ble
    base['d_bt_band_energy']    = bt_energy   - prev_bt

    # Build vector in training order
    vec = np.array([base[f] for f in feat_names], dtype=float)
    return vec, pcts

def classify(vec, bundle):
    scaler = bundle['scaler']
    rf     = bundle['rf']
    iso    = bundle['iso']
    le     = bundle['label_enc']

    X = scaler.transform(vec.reshape(1, -1))

    # Isolation Forest score (-1 = anomaly, 1 = normal)
    iso_score  = iso.decision_function(X)[0]   # <0 = more anomalous
    iso_flag   = iso.predict(X)[0] == -1

    # Random Forest probabilities
    proba      = rf.predict_proba(X)[0]
    pred_idx   = proba.argmax()
    pred_label = le.classes_[pred_idx]
    confidence = proba[pred_idx]

    # Combine
    if pred_label == 'normal' and not iso_flag:
        level = 'CLEAR'
        label = 'normal'
        conf  = confidence
    elif iso_flag and confidence < CONF_WARNING:
        level = 'ANOMALY'
        label = 'anomaly'
        conf  = abs(iso_score)
    elif confidence >= CONF_ATTACK and pred_label != 'normal':
        level = 'ATTACK'
        label = pred_label
        conf  = confidence
    elif confidence >= CONF_WARNING and pred_label != 'normal':
        level = 'WARNING'
        label = pred_label
        conf  = confidence
    else:
        level = 'CLEAR'
        label = 'normal'
        conf  = confidence

    return {
        'level':      level,
        'label':      label,
        'confidence': round(float(conf), 3),
        'iso_score':  round(float(iso_score), 4),
        'all_proba':  {le.classes_[i]: round(float(p), 3)
                       for i, p in enumerate(proba)},
    }

def print_alert(sweep_n, result, feats_summary):
    lvl = result['level']
    lbl = result['label']
    conf = result['confidence']

    colour = {
        'CLEAR':   GREEN,
        'ANOMALY': YELLOW,
        'WARNING': YELLOW,
        'ATTACK':  RED + BOLD,
    }.get(lvl, RESET)

    bar = f"[{lvl:<7s}]"
    ts  = time.strftime('%H:%M:%S')
    line = (f"{colour}{bar}{RESET} #{sweep_n:05d} {ts}  "
            f"label={lbl:<18s} conf={conf:.0%}  "
            f"alerts={feats_summary['alert_count']:3d}  "
            f"spikes={feats_summary['spike_count']:2d}  "
            f"wifi={feats_summary['wifi_band_energy']:4d}  "
            f"ble={feats_summary['ble_band_energy']:3d}  "
            f"bt={feats_summary['bt_band_energy']:4d}")
    print(line)

    if lvl in ('ATTACK', 'WARNING'):
        proba = result['all_proba']
        print(f"        Probabilities: " +
              "  ".join(f"{k}={v:.0%}" for k, v in
                        sorted(proba.items(), key=lambda x: -x[1])))

# ── WebSocket server ──────────────────────────────────────────
async def ws_handler(websocket):
    ws_clients.add(websocket)
    try:
        # Send history to new client
        for ev in list(alert_history):
            await websocket.send(json.dumps(ev))
        async for _ in websocket:
            pass
    except Exception:
        pass
    finally:
        ws_clients.discard(websocket)

async def ws_main():
    global ws_loop
    ws_loop = asyncio.get_event_loop()
    async with websockets.serve(ws_handler, "localhost", 8765):
        print(f"[WS]  Dashboard WebSocket on ws://localhost:8765")
        await asyncio.Future()  # run forever

def ws_thread_fn():
    asyncio.run(ws_main())

def broadcast(event):
    if not ws_loop or not ws_clients:
        return
    msg = json.dumps(event)
    for ws in list(ws_clients):
        asyncio.run_coroutine_threadsafe(ws.send(msg), ws_loop)

# ── Detection log ─────────────────────────────────────────────
def log_detection(logf, event):
    logf.write(json.dumps(event) + '\n')
    logf.flush()

# ── Serial reader / main inference loop ─────────────────────
def run_detector(args, bundle):
    feat_names = bundle['features']

    ser = None
    for attempt in range(5):
        try:
            ser = serial.Serial(args.port, args.baud, timeout=2)
            print(f"[OK]  Serial {args.port} @ {args.baud}")
            break
        except serial.SerialException as e:
            print(f"[WARN] {e}"); time.sleep(2)
    if ser is None:
        print("[ERROR] Cannot open port."); sys.exit(1)

    logf = open(args.log, 'a')
    print(f"[LOG]  Detections → {args.log}")
    print(f"[MODEL] Classes: {bundle['classes']}")
    print(f"        Trained: {bundle.get('trained_at','?')}  "
          f"Samples: {bundle.get('n_samples','?')}\n")

    sweep      = {}
    prev_sweep = {}
    sweep_n    = 0
    t_start    = int(time.time() * 1000)

    try:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            try:
                line = raw.decode('ascii', errors='ignore').strip()
            except Exception:
                continue

            if not line or line.startswith('#'):
                continue

            if line.startswith('GR_EOF'):
                if not sweep:
                    continue
                sweep_n += 1
                ts = int(time.time() * 1000) - t_start

                vec, pcts = make_feature_vector(sweep, prev_sweep, feat_names)
                result    = classify(vec, bundle)

                # Build summary dict for broadcast & log
                feat_summary = {
                    'active_count':     int(sum(1 for p in pcts if p > 0)),
                    'alert_count':      int(sum(1 for p in pcts if p >= ALERT_THR)),
                    'spike_count':      int(sum(1 for c in range(NUM_CH)
                                              if pcts[c] - prev_sweep.get(c,0) >= SPIKE_THR)),
                    'wifi_band_energy': int(sum(pcts[c] for c in WIFI_CHANNELS)),
                    'ble_band_energy':  int(sum(pcts[c] for c in BLE_CHANNELS)),
                    'bt_band_energy':   int(sum(pcts[c] for c in BT_RANGE)),
                    'max_pct':          int(max(pcts)),
                }

                event = {
                    'ts':        ts,
                    'sweep_n':   sweep_n,
                    'time_str':  time.strftime('%H:%M:%S'),
                    'result':    result,
                    'feats':     feat_summary,
                    'spectrum':  pcts,   # full 126-ch array for dashboard
                }

                print_alert(sweep_n, result, feat_summary)
                log_detection(logf, event)

                with event_lock:
                    latest_event.update(event)
                alert_history.append(event)
                broadcast(event)

                prev_sweep = dict(sweep)
                sweep = {}

            elif line.startswith('GR,'):
                parts = line.split(',')
                if len(parts) == 5:
                    try:
                        ch  = int(parts[2])
                        pct = int(parts[3])
                        if 0 <= ch < NUM_CH:
                            sweep[ch] = pct
                    except ValueError:
                        pass

    except KeyboardInterrupt:
        print(f"\n[DONE] {sweep_n} sweeps processed.")
    finally:
        ser.close()
        logf.close()

def main():
    ap = argparse.ArgumentParser(description="RF Monitor Live Detector")
    ap.add_argument('--port',  default='COM3')
    ap.add_argument('--baud',  default=115200, type=int)
    ap.add_argument('--model', default='rf_model.pkl')
    ap.add_argument('--log',   default='detections.jsonl')
    args = ap.parse_args()

    if not os.path.exists(args.model):
        print(f"[ERROR] Model not found: {args.model}")
        print("  Run rf_trainer.py first.")
        sys.exit(1)

    print(f"[LOAD] Loading model {args.model} ...")
    bundle = joblib.load(args.model)
    print(f"       OK — {len(bundle['features'])} features, "
          f"classes: {bundle['classes']}")

    # Start WebSocket server in background thread
    wst = threading.Thread(target=ws_thread_fn, daemon=True)
    wst.start()
    time.sleep(0.5)

    print(f"\n[INFO] Open rf_dashboard.html in your browser.")
    print(f"       Dashboard WebSocket: ws://localhost:8765\n")

    run_detector(args, bundle)

if __name__ == '__main__':
    main()
