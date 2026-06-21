"""
fig4_profile.py — Fig. 4: Per-channel alert profile for BT/channel-sweep
interference mode at theta = 40%. Bars above threshold are coloured red
(detected), bars below are blue (not detected).

Usage:
    python3 fig4_profile.py
Output:
    fig4_bt_perchannel.tiff
    fig4_bt_perchannel.png
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from common import apply_style, load_scan_cleaned, BLUE, RED, ORANGE, \
    THRESH_COLOR

apply_style()

sc = load_scan_cleaned()
bt_atk = sc[sc['mode'] == 'BT']

bt_ch = bt_atk.groupby('channel')['occupancy_pct'].mean()
chs = sorted(bt_ch.index)
vals = bt_ch[chs].values
colors = [RED if v > 40 else BLUE for v in vals]

fig, ax = plt.subplots(figsize=(7, 3.2))
ax.bar(chs, vals, color=colors, width=0.9, alpha=0.8)
ax.axhline(40, color=THRESH_COLOR, lw=1.2, ls='--')
ax.axvline(29, color=ORANGE, lw=1, ls=':')

above = mpatches.Patch(color=RED, alpha=0.8, label='Above threshold')
below = mpatches.Patch(color=BLUE, alpha=0.8, label='Below threshold')
thresh_patch = mpatches.Patch(color=THRESH_COLOR, label=r'$\theta$=40%')
onset_patch = mpatches.Patch(color=ORANGE, label='Onset ch. 29')
ax.legend(handles=[above, below, thresh_patch, onset_patch], fontsize=8)

ax.set_xlabel('RF Channel')
ax.set_ylabel('Mean Occupancy (%)')
ax.set_title(r'Fig. 4 — Per-Channel Alert Profile: BT/Channel-Sweep Mode '
             r'($\theta$=40%)', fontsize=10)
ax.set_xlim(0, 82)
ax.set_ylim(0, 75)

plt.tight_layout()
plt.savefig('fig4_bt_perchannel.tiff')
plt.savefig('fig4_bt_perchannel.png')
plt.close()

print('Fig 4 saved: fig4_bt_perchannel.tiff / .png')
