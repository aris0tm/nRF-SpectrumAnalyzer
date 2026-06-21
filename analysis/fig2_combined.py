"""
fig2_combined.py — Fig. 2: Combined idle-vs-attack channel occupancy
across all four interference modes (BT sweep, BLE selective, Wi-Fi
targeted, FULL band), each panel with mean +/- 95% CI error bars.

Usage:
    python3 fig2_combined.py
Output:
    fig2_idle_vs_attack.tiff
    fig2_idle_vs_attack.png
"""

import numpy as np
import matplotlib.pyplot as plt
from common import apply_style, load_scan_cleaned, load_idle_baseline, \
    BLUE, RED, ORANGE, THRESH_COLOR

apply_style()

sc = load_scan_cleaned()
idle_mean, _ = load_idle_baseline()

bt_atk   = sc[sc['mode'] == 'BT']
ble_atk  = sc[sc['mode'] == 'BLE']
wifi_atk = sc[sc['mode'] == 'WIFI']
full_atk = sc[sc['mode'] == 'FULL']

panels = [
    ('BT sweep attack',  bt_atk,   'BT/Channel-Sweep Scan (ch. 2–78)',   range(0, 126)),
    ('BLE selective',    ble_atk,  'BLE Selective Scan (ch. 2, 26, 80)', [2, 26, 80]),
    ('Wi-Fi targeted',   wifi_atk, 'Wi-Fi Targeted Scan (ch. 12–72)',    range(12, 73)),
    ('FULL scan',        full_atk, 'FULL Band Scan (ch. 0–125)',        range(0, 126)),
]

fig, axes = plt.subplots(2, 2, figsize=(10, 7))
axes = axes.flatten()

for ax, (label, atk_df, title, ch_range) in zip(axes, panels):
    chs = sorted(set(atk_df['channel'].unique()) & set(ch_range))
    idle_vals = [idle_mean.get(c, np.nan) for c in chs]
    atk_vals  = [atk_df[atk_df['channel'] == c]['occupancy_pct'].mean() for c in chs]
    atk_ci    = [atk_df[atk_df['channel'] == c]['occupancy_pct'].sem() * 1.96
                 for c in chs]

    ax.plot(chs, idle_vals, color=BLUE, lw=1.2, alpha=0.85, label='Idle')
    ax.errorbar(chs, atk_vals, yerr=atk_ci, color=RED, lw=1.2,
                elinewidth=0.5, capsize=1.5, alpha=0.9, label='Under attack')
    ax.axhline(40, color=THRESH_COLOR, lw=1, ls='--', label=r'$\theta$ = 40 %')
    ax.fill_betweenx([0, 105], 52, 72, color=ORANGE, alpha=0.10)
    ax.set_title(title, fontsize=9)
    ax.set_xlabel('RF Channel')
    ax.set_ylabel('Occupancy (%)')
    ax.set_ylim(0, 105)
    ax.legend(fontsize=7)

fig.suptitle('Fig. 2 — Combined Idle-vs.-Attack Channel Occupancy '
              '(all four modes)', fontsize=11, y=1.01)
plt.tight_layout()
plt.savefig('fig2_idle_vs_attack.tiff')
plt.savefig('fig2_idle_vs_attack.png')
plt.close()

print('Fig 2 saved: fig2_idle_vs_attack.tiff / .png')
