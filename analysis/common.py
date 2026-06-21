"""
common.py — shared data loading and plot styling for all figure scripts.

Import this from fig1_idle.py ... fig5_validation.py so plot style and
data-loading logic live in exactly one place. Edit CSV paths here if your
file locations change.
"""

import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# ── File paths — adjust if your CSVs live elsewhere ─────────────────────────
SCAN_CLEANED   = 'data/1781787648311_scan_cleaned.csv'   # all 4 modes, full sweep
BT_ATTACK      = 'data/Rf_data_with_BT_atk.csv'          # contains FULL rows used as idle proxy
RF_BASELINE    = 'data/1781787462740_rf_data.csv'        # short baseline run

# ── Shared matplotlib style (PLOS ONE friendly: serif, 300 DPI, light grid) ─
PLOS_STYLE = {
    'font.family': 'serif', 'font.size': 10,
    'axes.linewidth': 0.8, 'axes.grid': True,
    'grid.alpha': 0.3, 'grid.linestyle': '--',
    'figure.dpi': 300, 'savefig.dpi': 300,
    'savefig.bbox': 'tight', 'savefig.pad_inches': 0.05,
}

# ── Shared colour palette ────────────────────────────────────────────────────
BLUE   = '#1f4e79'   # idle / primary series
RED    = '#c00000'   # attack / alert series
GRAY   = '#888888'   # neutral / false-positive reference
ORANGE = '#e07b00'   # Wi-Fi-adjacent zone highlight
THRESH_COLOR = '#555555'   # threshold lines


def apply_style():
    """Call once at the top of each fig*.py script."""
    plt.rcParams.update(PLOS_STYLE)


def load_scan_cleaned():
    """Full 4-mode sweep dataset: columns = time_ms, channel, freq_mhz,
    samples, detected, occupancy_pct, alert, mode."""
    return pd.read_csv(SCAN_CLEANED)


def load_bt_attack():
    """BT-attack dataset; also contains FULL-mode rows used as the idle
    baseline proxy (first half of FULL rows, before jammer activation)."""
    return pd.read_csv(BT_ATTACK)


def load_idle_baseline():
    """Returns mean occupancy per channel for the idle baseline, derived
    from the first half of FULL-mode rows in the BT-attack dataset
    (interference node OFF for that portion of the run)."""
    bt = load_bt_attack()
    full_rows = bt[bt['mode'] == 'FULL'].copy().reset_index(drop=True)
    n_idle = len(full_rows) // 2
    idle_df = full_rows.iloc[:n_idle]
    return idle_df.groupby('channel')['occupancy_pct'].mean(), idle_df


def wilson_ci(k, n, z=1.96):
    """Wilson score 95% CI for a binomial proportion k/n.
    Returns (centre_pct, half_width_pct). See Eq. 11 in the manuscript."""
    p = k / n
    denom = 1 + z**2 / n
    centre = (p + z**2 / (2 * n)) / denom
    half = z * np.sqrt(p * (1 - p) / n + z**2 / (4 * n**2)) / denom
    return centre * 100, half * 100
