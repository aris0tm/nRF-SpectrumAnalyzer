# analysis/ — Figure generation scripts

Reproduces all 5 manuscript figures from raw CSV data. Each script is
independent and can be run on its own; all share `common.py` for data
loading and plot styling so there's one place to fix things, not five.

## Setup

```
pip install pandas numpy matplotlib --break-system-packages
```

Place your raw CSVs in `analysis/data/`:
- `1781787648311_scan_cleaned.csv` — all 4 modes, full 126-channel sweep
- `Rf_data_with_BT_atk.csv` — contains FULL-mode rows used as idle baseline

(Edit the paths at the top of `common.py` if your filenames differ.)

## Run

```
cd analysis
python3 fig1_idle.py
python3 fig2_combined.py
python3 fig3_threshold.py
python3 fig4_profile.py
python3 fig5_validation.py
```

Each script prints what it saved. Output is `.tiff` (300 DPI, for PLOS ONE
upload) and `.png` (quick preview) in the working directory.

## File map

| Script | Manuscript figure | What it shows |
|---|---|---|
| `fig1_idle.py` | Fig. 1 | Idle baseline occupancy, all 126 channels |
| `fig2_combined.py` | Fig. 2 | Idle-vs-attack, 2×2 panel, all 4 modes, with CI |
| `fig3_threshold.py` | Fig. 3 | Alert rate vs. threshold θ, all modes + idle FP |
| `fig4_profile.py` | Fig. 4 | Per-channel alert profile, BT sweep mode |
| `fig5_validation.py` | Fig. 5 | DIY jammer validation, detection rate ± Wilson CI |

## Where the numbers come from

`fig1`–`fig4` compute everything directly from the CSV `occupancy_pct`
column via `groupby('channel').mean()` / `.sem()` — no manual numbers
anywhere in those four scripts.

`fig5_validation.py` is the one exception: the `events`/`detected` arrays
are typed in from the structured validation-run log (a separate counted
record of jammer activation events, not derived from the sweep CSVs).
If your validation counts change, edit those two arrays directly — see
the comment at the top of the script.

## Wilson CI

`common.wilson_ci(k, n)` implements the Wilson score interval (Eq. 11 in
the manuscript), the same formula used throughout the paper for all
proportion statistics. Don't substitute a Wald/normal-approximation CI
elsewhere — reviewers will expect consistency with what's stated in
Methods.
