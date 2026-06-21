"""
fig3_threshold.py — Fig. 3: Alert rate vs. occupancy threshold theta,
for all four scan modes plus the idle false-positive baseline.

Usage:
    python3 fig3_threshold.py
Output:
    fig3_alert_vs_threshold.tiff
    fig3_alert_vs_threshold.png
"""

import matplotlib.pyplot as plt
from common import apply_style, load_scan_cleaned, load_idle_baseline, \
    BLUE, RED, ORANGE, GRAY, THRESH_COLOR

apply_style()

sc = load_scan_cleaned()
_, idle_full = load_idle_baseline()   # idle_full = the raw idle-portion dataframe

thresholds = list(range(5, 75, 5))
modes_map = {'FULL': BLUE, 'BLE': RED, 'BT': ORANGE, 'WIFI': 'green'}

fig, ax = plt.subplots(figsize=(6, 3.8))

for mode, col in modes_map.items():
    sub = sc[sc['mode'] == mode]
    alert_rates = []
    for th in thresholds:
        per_ch = sub.groupby('channel')['occupancy_pct'].mean()
        rate = (per_ch > th).mean() * 100
        alert_rates.append(rate)
    ax.plot(thresholds, alert_rates, color=col, marker='o', ms=3, lw=1.4,
             label=mode)

# Idle false-positive rate vs threshold
idle_fp = []
for th in thresholds:
    per_ch = idle_full.groupby('channel')['occupancy_pct'].mean()
    rate = (per_ch > th).mean() * 100
    idle_fp.append(rate)
ax.plot(thresholds, idle_fp, color=GRAY, lw=1.2, ls='--', marker='s', ms=3,
         label='Idle FP')

ax.axvline(40, color=THRESH_COLOR, lw=0.8, ls=':', alpha=0.7)
ax.set_xlabel(r'Occupancy Threshold $\theta$ (%)')
ax.set_ylabel('Alert Rate (%)')
ax.set_title(r'Fig. 3 — Alert Rate vs. Threshold $\theta$ (all scan modes)',
             fontsize=10)
ax.legend(fontsize=8)
ax.set_xlim(5, 70)
ax.set_ylim(0, 100)

plt.tight_layout()
plt.savefig('fig3_alert_vs_threshold.tiff')
plt.savefig('fig3_alert_vs_threshold.png')
plt.close()

print('Fig 3 saved: fig3_alert_vs_threshold.tiff / .png')
