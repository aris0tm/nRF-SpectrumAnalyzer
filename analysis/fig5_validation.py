"""
fig5_validation.py — Fig. 5: DIY jammer validation — per-mode detection
rate at theta=40%, 3 m separation, with 95% Wilson confidence intervals
(see Eq. 11 in the manuscript for the Wilson score formula).

Note: the event/detection counts below (events, detected) come from the
structured validation run log, not from the scan_cleaned.csv sweep data.
Replace these arrays with your own validation-run tallies if they change.

Usage:
    python3 fig5_validation.py
Output:
    fig5_validation_ci.tiff
    fig5_validation_ci.png
"""

import numpy as np
import matplotlib.pyplot as plt
from common import apply_style, wilson_ci, BLUE, RED, GRAY

apply_style()

modes_v  = ['Barrage', 'Selective', 'Reactive', 'Sweep', 'Overall']
events   = [52, 48, 55, 50, 205]
detected = [45, 43, 39, 38, 165]

centres, halves = zip(*[wilson_ci(d, e) for d, e in zip(detected, events)])

fig, ax = plt.subplots(figsize=(6.5, 3.5))
x = np.arange(len(modes_v))
bar_colors = [BLUE, BLUE, BLUE, BLUE, RED]

ax.bar(x, centres, yerr=halves, color=bar_colors, alpha=0.82, capsize=4,
       error_kw={'elinewidth': 1.2})
ax.axhline(centres[-1], color=GRAY, lw=1, ls='--', alpha=0.6,
           label=f'Overall {centres[-1]:.1f}%')

for i, (c, h) in enumerate(zip(centres, halves)):
    ax.text(i, c + h + 1.5, f'{c:.1f}%', ha='center', fontsize=8)

ax.set_xticks(x)
ax.set_xticklabels(modes_v)
ax.set_ylabel('Detection Rate (%)')
ax.set_ylim(0, 105)
ax.set_title('Fig. 5 — DIY Jammer Validation: Detection Rate per Mode '
             r'(3 m, $\theta$=40%)' + '\nwith 95% Wilson Confidence Intervals',
             fontsize=9)
ax.legend(fontsize=8)

plt.tight_layout()
plt.savefig('fig5_validation_ci.tiff')
plt.savefig('fig5_validation_ci.png')
plt.close()

print('Fig 5 saved: fig5_validation_ci.tiff / .png')
print()
print('Per-mode detection rates (Wilson 95% CI):')
for m, c, h, e, d in zip(modes_v, centres, halves, events, detected):
    print(f'  {m:10s}  {d}/{e} = {c:.1f}%  [{c-h:.1f}%, {c+h:.1f}%]')
