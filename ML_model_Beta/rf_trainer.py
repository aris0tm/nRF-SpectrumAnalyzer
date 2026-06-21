#!/usr/bin/env python3
"""
rf_trainer.py  —  RF Monitor Attack Pattern Classifier Trainer
==============================================================
Reads labeled CSV from rf_logger.py, engineers features,
trains a multi-class Random Forest + Isolation Forest ensemble,
evaluates, and saves the model bundle to rf_model.pkl.

USAGE
-----
    python rf_trainer.py --data dataset.csv --out rf_model.pkl

WHAT IT TRAINS
--------------
  1. IsolationForest     — unsupervised anomaly baseline
                           (works even with few labeled samples)
  2. RandomForestClassifier  — supervised multi-class
                           (normal / jamming / wifi_deauth /
                            ble_flood / bt_interference)
  3. StandardScaler      — feature normalisation (saved with model)

The two models are used together at inference time:
  - IF flags "something weird happened"
  - RF identifies *what* it is
  - If RF confidence < threshold, falls back to IF "anomaly" label

FEATURES USED (engineered from raw sweep)
------------------------------------------
  Band energies    : wifi, ble, bt (summed busy%)
  Global stats     : active_count, alert_count, max_pct, mean_pct,
                     band_variance, spike_count
  Derived ratios   : wifi/total, ble/total, bt/total
  Spectral moments : skewness, kurtosis of pct distribution
  Neighbour std    : rolling std of 5-channel windows (×3 bands)
  Delta features   : change in above from previous sweep
                     (only at training time; detector tracks state)

DEPENDENCIES
------------
    pip install scikit-learn pandas numpy joblib
"""

import argparse, sys, warnings
warnings.filterwarnings('ignore')

try:
    import numpy as np
    import pandas as pd
    from sklearn.ensemble import RandomForestClassifier, IsolationForest
    from sklearn.preprocessing import StandardScaler, LabelEncoder
    from sklearn.model_selection import StratifiedKFold, cross_val_score
    from sklearn.metrics import classification_report, confusion_matrix
    import joblib
except ImportError as e:
    print(f"[ERROR] Missing dependency: {e}")
    print("  Run: pip install scikit-learn pandas numpy joblib")
    sys.exit(1)

# ── channel maps ─────────────────────────────────────────────
BLE_CHANNELS  = [2, 26, 80]
WIFI_CHANNELS = [12,17,22,27,32,37,42,47,52,57,62,67,72]
BT_RANGE      = list(range(2, 79))
NUM_CH        = 126

def load_data(path):
    print(f"[LOAD] Reading {path} ...")
    df = pd.read_csv(path)
    print(f"       {len(df)} rows, {len(df.columns)} columns")
    print(f"       Label distribution:\n{df['label'].value_counts().to_string()}")
    # Drop unknowns
    df = df[df['label'] != 'unknown'].copy()
    print(f"       After dropping 'unknown': {len(df)} rows")
    return df

def engineer_features(df):
    """
    Build a clean feature matrix from the raw CSV.
    Uses pre-computed band energies + adds derived/ratio features.
    """
    pct_cols  = [f'ch{c}_pct' for c in range(NUM_CH)]

    # Raw features already present in CSV
    base_feats = [
        'active_count', 'alert_count', 'max_pct', 'mean_pct',
        'wifi_band_energy', 'ble_band_energy', 'bt_band_energy',
        'spike_count', 'band_variance'
    ]

    X = df[base_feats].copy().astype(float)

    # Derived ratios
    total_energy = X['wifi_band_energy'] + X['ble_band_energy'] + X['bt_band_energy'] + 1e-6
    X['wifi_ratio'] = X['wifi_band_energy'] / total_energy
    X['ble_ratio']  = X['ble_band_energy']  / total_energy
    X['bt_ratio']   = X['bt_band_energy']   / total_energy

    # Spectral moments from per-channel pct values
    pct_matrix = df[pct_cols].values.astype(float)  # (N, 126)

    # Skewness (manual — avoids scipy dep)
    mean  = pct_matrix.mean(axis=1, keepdims=True)
    std   = pct_matrix.std(axis=1,  keepdims=True) + 1e-6
    skew  = ((pct_matrix - mean) ** 3).mean(axis=1) / (std.squeeze() ** 3)
    kurt  = ((pct_matrix - mean) ** 4).mean(axis=1) / (std.squeeze() ** 4)

    X['spectral_skew'] = skew
    X['spectral_kurt'] = kurt

    # Band-specific max
    X['wifi_max'] = pct_matrix[:, WIFI_CHANNELS].max(axis=1)
    X['ble_max']  = pct_matrix[:, BLE_CHANNELS].max(axis=1)
    X['bt_max']   = pct_matrix[:, BT_RANGE].max(axis=1)

    # Neighbour std — 5-channel rolling std in three bands
    #   captures "how spiky" a band is
    wifi_band = pct_matrix[:, 12:73]   # approx WiFi overlap region
    ble_band  = pct_matrix[:, 0:83]
    X['wifi_band_std'] = wifi_band.std(axis=1)
    X['ble_band_std']  = ble_band.std(axis=1)
    X['bt_band_std']   = pct_matrix[:, 2:79].std(axis=1)

    # Delta features (row-to-row diff — captures transient attacks)
    for col in ['active_count','alert_count','max_pct','spike_count',
                'wifi_band_energy','ble_band_energy','bt_band_energy']:
        X[f'd_{col}'] = X[col].diff().fillna(0)

    print(f"[FEAT] Feature matrix: {X.shape[0]} rows × {X.shape[1]} features")
    return X

def train(args):
    df = load_data(args.data)

    if len(df) < 50:
        print("[WARN] Very few samples. Collect more data for reliable training.")
        print("       Proceeding with what's available...")

    X = engineer_features(df)
    y_raw = df['label'].values

    le = LabelEncoder()
    y  = le.fit_transform(y_raw)
    print(f"\n[LABELS] Classes: {list(le.classes_)}")

    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X)

    # ── 1. Isolation Forest (unsupervised anomaly) ────────────
    print("\n[TRAIN] Fitting IsolationForest ...")
    normal_mask = (y_raw == 'normal')
    X_normal    = X_scaled[normal_mask]
    iso = IsolationForest(
        n_estimators=200,
        contamination=0.05,
        random_state=42,
        n_jobs=-1
    )
    iso.fit(X_normal if len(X_normal) > 10 else X_scaled)
    print(f"        Trained on {len(X_normal)} normal samples.")

    # ── 2. Random Forest (supervised multi-class) ─────────────
    print("\n[TRAIN] Fitting RandomForestClassifier ...")
    rf = RandomForestClassifier(
        n_estimators=300,
        max_depth=None,
        min_samples_leaf=2,
        class_weight='balanced',
        random_state=42,
        n_jobs=-1
    )

    # Cross-validation if enough samples
    n_classes = len(le.classes_)
    min_per_class = min(np.bincount(y))

    if min_per_class >= 5:
        n_splits = min(5, min_per_class)
        cv = StratifiedKFold(n_splits=n_splits, shuffle=True, random_state=42)
        scores = cross_val_score(rf, X_scaled, y, cv=cv,
                                 scoring='f1_weighted', n_jobs=-1)
        print(f"        CV F1 (weighted): {scores.mean():.3f} ± {scores.std():.3f}")
    else:
        print(f"[WARN]  Skipping CV — need ≥5 samples per class "
              f"(min={min_per_class}). Collect more labeled data.")

    rf.fit(X_scaled, y)
    y_pred = rf.predict(X_scaled)

    print("\n[EVAL] Training-set classification report:")
    print(classification_report(y, y_pred, target_names=le.classes_))

    print("[EVAL] Confusion matrix:")
    cm = confusion_matrix(y, y_pred)
    col_w = max(len(c) for c in le.classes_) + 2
    header = " " * col_w + "".join(f"{c:>{col_w}}" for c in le.classes_)
    print(header)
    for i, row_label in enumerate(le.classes_):
        row = f"{row_label:>{col_w}}" + "".join(f"{v:>{col_w}}" for v in cm[i])
        print(row)

    # ── Feature importances ───────────────────────────────────
    feat_names = X.columns.tolist()
    importances = rf.feature_importances_
    top_idx = np.argsort(importances)[::-1][:10]
    print("\n[INFO] Top-10 feature importances:")
    for rank, idx in enumerate(top_idx, 1):
        print(f"  {rank:2d}. {feat_names[idx]:<30s} {importances[idx]:.4f}")

    # ── Save bundle ───────────────────────────────────────────
    bundle = {
        'rf':        rf,
        'iso':       iso,
        'scaler':    scaler,
        'label_enc': le,
        'features':  feat_names,
        'version':   '3.0.0',
        'trained_at': str(pd.Timestamp.now()),
        'classes':   list(le.classes_),
        'n_samples': len(df),
    }
    joblib.dump(bundle, args.out)
    print(f"\n[SAVE] Model bundle saved to {args.out}")
    print(f"       Classes : {list(le.classes_)}")
    print(f"       Features: {len(feat_names)}")
    print(f"       Samples : {len(df)}")
    print("\n  Run rf_detector.py to start live detection.")

def main():
    ap = argparse.ArgumentParser(description="RF Monitor Trainer")
    ap.add_argument('--data', default='dataset.csv', help='Labeled CSV from rf_logger.py')
    ap.add_argument('--out',  default='rf_model.pkl', help='Output model bundle')
    train(ap.parse_args())

if __name__ == '__main__':
    main()
