# Changelog


All notable changes to this project are documented in this file.

The project follows semantic versioning principles where practical.

---

## [3.1.0] - Manuscript Experimental Release

Version used for the experiments and results reported in the accompanying manuscript.

### Added

* RSSI estimation derived from RPD occupancy density.
* Automatic per-channel noise-floor calibration during startup.
* Signal-to-noise ratio (SNR) estimation.
* Per-channel utilization history ring buffer.
* Channel activity trend indicators.
* Congestion scoring for neighboring channels.
* GNU Radio serial bridge mode.
* CSV-compatible output stream for external processing.
* Scan history tracking.
* Enhanced channel occupancy analytics.

### Improved

* Occupancy characterization accuracy.
* Channel utilization reporting.
* Experimental reproducibility support.
* Serial interface readability.
* Data export capabilities.

### Research Features

* Dataset generation support.
* Figure-generation workflow support.
* Occupancy baseline measurement support.
* Detection-boundary characterization support.
* Statistical analysis integration.

---

## [2.0.1] - Multi-Mode Detection Release

### Added

* Multi-mode scanning system.
* BLE-specific scanning mode.
* Bluetooth Classic scanning mode.
* Wi-Fi-focused scanning mode.
* Peak occupancy tracking.
* Spike detection.
* ANSI terminal interface.
* Real-time status bar.
* Per-channel occupancy visualization.

### Improved

* Channel classification.
* User interface responsiveness.
* Terminal readability.
* Alert visibility.

### Changed

* Static channel labels introduced.
* Screen updates optimized to reduce redraw overhead.

---

## [1.0.0] - Proof-of-Concept Release

Initial public implementation.

### Added

* NRF24L01+ occupancy monitoring.
* Full-spectrum scanning.
* Button-based mode selection.
* Busy-channel threshold detection.
* Serial monitoring output.
* BLE monitoring support.
* Bluetooth monitoring support.
* Wi-Fi monitoring support.
* Visual occupancy bars.

### Purpose

Demonstrate low-cost 2.4 GHz spectrum occupancy monitoring using commodity hardware.

---

## Research Notes

The experimental results reported in the manuscript were obtained using version 3.1.0.

Earlier releases are preserved for historical reference and development traceability.
