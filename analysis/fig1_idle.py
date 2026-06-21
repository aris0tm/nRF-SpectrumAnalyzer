"""
fig1_idle.py — Fig. 1: Measured idle baseline channel occupancy
across all 126 NRF24L01+ channels (FULL scan mode, N=50 samples/channel).

Usage:
    python3 fig1_idle.py
Output:
    fig1_baseline_occupancy.tiff  (300 DPI, for PLOS ONE upload)
    fig1_baseline_occupancy.png   (quick preview)
"""

import matplotlib.pyplot as plt
from common import apply_style, load_idle_baseline, BLUE, RED, ORANGE

apply_style()

idle_mean, idle_df = load_idle_baseline()
ch = sorted(idle_mean.index)

fig, ax = plt.subplots(figsize=(7.5, 3.2))
ax.bar(ch, idle_mean[ch], width=0.9, color=BLUE, alpha=0.75,
       label='Idle occupancy')
ax.axhline(40, color=RED, lw=1.2, ls='--', label=r'$\theta$ = 40 %')
ax.fill_betweenx([0, 100], 52, 72, color=ORANGE, alpha=0.12,
                  label='Wi-Fi Ch.11 + leakage')

ax.set_xlabel('RF Channel Number')
ax.set_ylabel('Occupancy (%)')
ax.set_title('Fig. 1 — Measured Idle Baseline Channel Occupancy '
              '(FULL scan, N=50)', fontsize=10)
ax.set_xlim(-1, 126)
ax.set_ylim(0, 75)
ax.legend(fontsize=8, loc='upper left')

plt.tight_layout()
plt.savefig('fig1_baseline_occupancy.tiff')
plt.savefig('fig1_baseline_occupancy.png')
plt.close()

print('Fig 1 saved: fig1_baseline_occupancy.tiff / .png')
