#!/usr/bin/env python3
"""
rf_logger.py  —  RF Monitor v3 Data Logger & Labeller
======================================================
Reads GNU Radio CSV frames from the Arduino (MODE 5),
saves every sweep as a labeled row, and lets you
hot-key label attacks in real time.

USAGE
-----
    python rf_logger.py --port COM3 --out dataset.csv

HOT KEYS (press while running, then Enter)
------------------------------------------
    n  — normal / baseline
    j  — RF jamming / DoS
    w  — WiFi deauth / flood
    b  — BLE advertising flood
    t  — Bluetooth interference
    x  — unknown / discard (row saved but label=unknown)
    q  — quit and save

OUTPUT CSV COLUMNS
------------------
    timestamp_ms, label,
    ch0_pct .. ch125_pct,       (busy % per channel)
    ch0_rssi .. ch125_rssi,     (estimated dBm)
    active_count,               (channels with pct > 0)
    alert_count,                (channels with pct >= 20)
    max_pct, mean_pct,
    wifi_band_energy,           (sum of 13 WiFi sub-channels)
    ble_band_energy,            (sum of 3 BLE advert channels)
    bt_band_energy,             (sum of BT classic band ch2-78)
    spike_count,                (channels that jumped >15% vs prev sweep)
    band_variance               (variance of pct across all channels)
"""

import argparse, csv, os, sys, time, threading, collections
from datetime import datetime

try:
    import serial
except ImportError:
    print("[ERROR] pip install pyserial"); sys.exit(1)

# ── channel maps (mirror firmware) ───────────────────────────
BLE_CHANNELS  = [2, 26, 80]
WIFI_CHANNELS = [12,17,22,27,32,37,42,47,52,57,62,67,72]
BT_RANGE      = range(2, 79)
NUM_CH        = 126
ALERT_THR     = 20
SPIKE_THR     = 15
RSSI_MIN, RSSI_MAX = -90, -40

LABELS = {
    'n': 'normal',
    'j': 'jamming',
    'w': 'wifi_deauth',
    'b': 'ble_flood',
    't': 'bt_interference',
    'x': 'unknown',
}

def pct_to_rssi(pct):
    return RSSI_MIN + (pct * (RSSI_MAX - RSSI_MIN) / 100)

def extract_features(sweep_pct, prev_pct):
    pcts  = [sweep_pct.get(c, 0) for c in range(NUM_CH)]
    rssis = [pct_to_rssi(p) for p in pcts]

    active      = sum(1 for p in pcts if p > 0)
    alerts      = sum(1 for p in pcts if p >= ALERT_THR)
    max_pct     = max(pcts)
    mean_pct    = sum(pcts) / NUM_CH
    wifi_energy = sum(pcts[c] for c in WIFI_CHANNELS)
    ble_energy  = sum(pcts[c] for c in BLE_CHANNELS)
    bt_energy   = sum(pcts[c] for c in BT_RANGE)
    spikes      = sum(1 for c in range(NUM_CH)
                      if pcts[c] - prev_pct.get(c, 0) >= SPIKE_THR)
    mean2 = mean_pct
    variance = sum((p - mean2)**2 for p in pcts) / NUM_CH

    return pcts, rssis, {
        'active_count':      active,
        'alert_count':       alerts,
        'max_pct':           max_pct,
        'mean_pct':          round(mean_pct, 3),
        'wifi_band_energy':  wifi_energy,
        'ble_band_energy':   ble_energy,
        'bt_band_energy':    bt_energy,
        'spike_count':       spikes,
        'band_variance':     round(variance, 3),
    }

def build_header():
    h  = ['timestamp_ms', 'label']
    h += [f'ch{c}_pct'  for c in range(NUM_CH)]
    h += [f'ch{c}_rssi' for c in range(NUM_CH)]
    h += ['active_count','alert_count','max_pct','mean_pct',
          'wifi_band_energy','ble_band_energy','bt_band_energy',
          'spike_count','band_variance']
    return h

# ── label thread ─────────────────────────────────────────────
current_label  = 'normal'
label_lock     = threading.Lock()
running        = True

def label_thread():
    global current_label, running
    print("\n  Hot-keys: n=normal  j=jamming  w=wifi_deauth  "
          "b=ble_flood  t=bt_interference  x=unknown  q=quit")
    print(f"  Current label: [{current_label}]\n")
    while running:
        try:
            key = input("  label> ").strip().lower()
        except EOFError:
            break
        if key == 'q':
            running = False
            break
        if key in LABELS:
            with label_lock:
                current_label = LABELS[key]
            print(f"  ✓  Label set to [{current_label}]")
        else:
            print(f"  Unknown key '{key}'. Options: {list(LABELS.keys())}")

def open_serial(port, baud):
    for attempt in range(5):
        try:
            s = serial.Serial(port, baud, timeout=2)
            print(f"[OK]  Serial {port} @ {baud}")
            return s
        except serial.SerialException as e:
            print(f"[WARN] {e}"); time.sleep(2)
    print("[ERROR] Cannot open port."); sys.exit(1)

def main():
    global running
    ap = argparse.ArgumentParser(description="RF Monitor Logger")
    ap.add_argument('--port', default='COM3')
    ap.add_argument('--baud', default=115200, type=int)
    ap.add_argument('--out',  default='dataset.csv')
    args = ap.parse_args()

    file_exists = os.path.exists(args.out)
    outf  = open(args.out, 'a', newline='')
    writer = csv.writer(outf)
    if not file_exists:
        writer.writerow(build_header())
        print(f"[NEW] Created {args.out}")
    else:
        print(f"[APP] Appending to {args.out}")

    ser = open_serial(args.port, args.baud)

    lt = threading.Thread(target=label_thread, daemon=True)
    lt.start()

    sweep      = {}
    prev_sweep = {}
    sweep_n    = 0
    t_start    = int(time.time() * 1000)

    print("\n[RUNNING] Waiting for GNU Radio frames...\n")

    try:
        while running:
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
                pcts, rssis, feats = extract_features(sweep, prev_sweep)

                with label_lock:
                    lbl = current_label

                row = [ts, lbl] + pcts + [round(r,1) for r in rssis]
                row += [feats['active_count'], feats['alert_count'],
                        feats['max_pct'],      feats['mean_pct'],
                        feats['wifi_band_energy'], feats['ble_band_energy'],
                        feats['bt_band_energy'],   feats['spike_count'],
                        feats['band_variance']]
                writer.writerow(row)
                outf.flush()

                prev_sweep = dict(sweep)
                sweep = {}

                # Console mini-summary
                sys.stdout.write(
                    f"\r  [{sweep_n:05d}] lbl={lbl:<16s} "
                    f"active={feats['active_count']:3d}  "
                    f"alerts={feats['alert_count']:3d}  "
                    f"max={feats['max_pct']:3d}%  "
                    f"spikes={feats['spike_count']:2d}   "
                )
                sys.stdout.flush()

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
        print(f"\n[DONE] {sweep_n} sweeps logged to {args.out}")
    finally:
        running = False
        ser.close()
        outf.close()

if __name__ == '__main__':
    main()
