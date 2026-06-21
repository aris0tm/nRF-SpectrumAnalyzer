#!/usr/bin/env python3
"""
rf_monitor_sink.py  —  GNU Radio companion sink for RF Monitor v3.0.0
======================================================================
Reads the CSV stream emitted by MODE_GNURADIO (mode 5) on the Arduino,
converts each sweep into a 126-sample power vector, and re-broadcasts
it over UDP so that GNU Radio Companion blocks can consume it.

QUICK START
-----------
1.  Flash RF_Monitor_v3.ino, press button until "GNU RADIO BRIDGE" mode.
2.  Run:
        python rf_monitor_sink.py --port COM3 --baud 115200
    On Linux:
        python rf_monitor_sink.py --port /dev/ttyUSB0 --baud 115200
3.  In GNU Radio Companion open rf_monitor_waterfall.grc (see below)
    or connect any block to UDP Source  host=127.0.0.1  port=7355
    with item_size=float32, MTU=504 (126 floats × 4 bytes).

FRAME PROTOCOL (from Arduino)
------------------------------
    GR,<frameN>,<ch>,<busy_pct>,<rssi_dbm>     — per channel
    GR_EOF,<frameN>                             — end of sweep

OUTPUT UDP PACKET
-----------------
    126 × float32 in network byte order.
    Each float = RSSI in dBm (range -90.0 … -40.0).
    Channels are ordered 0-125 = 2400-2525 MHz.
    Suitable for:
        * QT GUI Frequency Sink  (sample_rate=125e6, center_freq=2462.5e6)
        * Waterfall Sink
        * File Sink (raw float32 IQ-free spectrum snapshot)

DEPENDENCIES
------------
    pip install pyserial
    GNU Radio 3.9+ (for .grc companion, optional)

GNU RADIO COMPANION SNIPPET
----------------------------
Add these blocks to a new flowgraph and connect them:

    [UDP Source]
        Input Type : byte
        Host       : 127.0.0.1
        Port       : 7355
        MTU        : 504
        EOF on done: No

    [Byte To Float]   (or use "Blocks > Type Conversions > Repack Bits"
                       then "Interleaved Float To Complex" if you want
                       a spectrogram-style waterfall)

    Tip: use a "Stream To Vector" of size 126 feeding a
         "Vector Sink" or "QT GUI Vector Sink" for a live bar chart.

LOGGING
-------
    Pass --log spectrum.csv to save every sweep as a CSV row:
        timestamp_ms, ch0_rssi, ch1_rssi, ... ch125_rssi
"""

import argparse
import socket
import struct
import sys
import time
import csv
import os

try:
    import serial
except ImportError:
    print("[ERROR] pyserial not found. Run: pip install pyserial")
    sys.exit(1)

# ── defaults ──────────────────────────────────────────────────
UDP_HOST   = "127.0.0.1"
UDP_PORT   = 7355
NUM_CH     = 126
FLOAT_SIZE = 4
PKT_SIZE   = NUM_CH * FLOAT_SIZE   # 504 bytes

RSSI_IDLE  = -90.0   # value emitted for channels with no data in sweep


def parse_args():
    ap = argparse.ArgumentParser(
        description="RF Monitor v3 → GNU Radio UDP bridge")
    ap.add_argument("--port",  default="COM3",   help="Serial port (e.g. COM3 or /dev/ttyUSB0)")
    ap.add_argument("--baud",  default=115200,   type=int, help="Baud rate (default 115200)")
    ap.add_argument("--udp-host", default=UDP_HOST, help="UDP destination host")
    ap.add_argument("--udp-port", default=UDP_PORT, type=int, help="UDP destination port")
    ap.add_argument("--log",   default=None,     help="Optional CSV log file path")
    ap.add_argument("--quiet", action="store_true", help="Suppress per-frame console output")
    return ap.parse_args()


def open_serial(port, baud, retries=5):
    for attempt in range(retries):
        try:
            s = serial.Serial(port, baud, timeout=2)
            print(f"[OK]  Opened {port} @ {baud} baud")
            return s
        except serial.SerialException as e:
            print(f"[WARN] Attempt {attempt+1}/{retries}: {e}")
            time.sleep(2)
    print("[ERROR] Could not open serial port. Exiting.")
    sys.exit(1)


def build_udp_packet(sweep: dict) -> bytes:
    """
    Convert sweep dict {ch: rssi_dbm} into 126-float32 packet.
    Missing channels default to RSSI_IDLE.
    """
    values = [sweep.get(ch, RSSI_IDLE) for ch in range(NUM_CH)]
    return struct.pack(f"!{NUM_CH}f", *values)


def print_spectrum(sweep: dict, frame_n: int):
    """Simple ASCII mini-spectrum for console feedback."""
    line = []
    for ch in range(0, NUM_CH, 5):
        rssi = sweep.get(ch, RSSI_IDLE)
        # map -90…-40 → 0…10 bars
        bars = int((rssi - RSSI_IDLE) / ((-40 - RSSI_IDLE) / 10))
        bars = max(0, min(10, bars))
        line.append(f"{2400+ch:4d}={'#'*bars+'.'*(10-bars)}")
    print(f"[Frame {frame_n:06d}] " + "  ".join(line[:5]) + " ...")


def main():
    args  = parse_args()
    ser   = open_serial(args.port, args.baud)
    sock  = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    log_writer = None
    log_file   = None
    if args.log:
        log_file   = open(args.log, "w", newline="")
        log_writer = csv.writer(log_file)
        header = ["timestamp_ms"] + [f"ch{c}_rssi" for c in range(NUM_CH)]
        log_writer.writerow(header)
        print(f"[LOG] Writing spectrum log to {args.log}")

    print(f"[OK]  Streaming to UDP {args.udp_host}:{args.udp_port}")
    print(f"[OK]  Packet size: {PKT_SIZE} bytes ({NUM_CH} float32 channels)")
    print("      Press Ctrl+C to stop.\n")

    sweep       = {}       # {ch: rssi_dbm}
    frame_n     = 0
    sweep_count = 0
    t_start     = time.time()

    try:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            try:
                line = raw.decode("ascii", errors="ignore").strip()
            except Exception:
                continue

            # ── skip comment / info lines ──────────────────────
            if line.startswith("#") or line == "":
                continue

            # ── end-of-sweep marker ───────────────────────────
            if line.startswith("GR_EOF"):
                parts = line.split(",")
                frame_n = int(parts[1]) if len(parts) > 1 else frame_n + 1
                if sweep:
                    pkt = build_udp_packet(sweep)
                    sock.sendto(pkt, (args.udp_host, args.udp_port))
                    sweep_count += 1

                    if log_writer:
                        ts = int((time.time() - t_start) * 1000)
                        row = [ts] + [sweep.get(c, RSSI_IDLE) for c in range(NUM_CH)]
                        log_writer.writerow(row)
                        log_file.flush()

                    if not args.quiet:
                        print_spectrum(sweep, frame_n)

                sweep = {}  # reset for next sweep
                continue

            # ── data frame ────────────────────────────────────
            if line.startswith("GR,"):
                parts = line.split(",")
                if len(parts) != 5:
                    continue
                try:
                    # GR,frameN,ch,busy_pct,rssi_dbm
                    ch      = int(parts[2])
                    rssi    = float(parts[4])
                    if 0 <= ch < NUM_CH:
                        sweep[ch] = rssi
                except ValueError:
                    continue

    except KeyboardInterrupt:
        print(f"\n[INFO] Stopped. {sweep_count} sweeps sent.")
    finally:
        ser.close()
        sock.close()
        if log_file:
            log_file.close()


if __name__ == "__main__":
    main()


# =============================================================
#  GNU RADIO COMPANION (.grc) MINIMAL FLOWGRAPH  (paste into GRC)
# =============================================================
#
#  The simplest way to visualise the UDP stream in GRC is:
#
#  Block 1: UDP Source
#    ├─ Input Type  : byte
#    ├─ Host        : 127.0.0.1
#    ├─ Port        : 7355
#    └─ MTU         : 504
#
#  Block 2: Blocks > Type Conversions > Float To Char  (skip if using byte sink)
#    — OR —
#  Block 2: Reinterpret Cast  input=byte, output=float32
#            (use "Packed to Unpacked" then "Chunks to Symbols" trick,
#             or the simpler custom Python block below)
#
#  ── Simplest approach: Python Block sink inside GRC ──────────
#
#  Add a "Python Block" (Misc > Python Block) with:
#
#    import struct, numpy as np
#    class blk(gr.sync_block):
#        def __init__(self): super().__init__("RFMonSink",
#            in_sig=[np.uint8], out_sig=[])
#        def work(self, input_items, _):
#            raw = bytes(input_items[0])
#            if len(raw) >= 504:
#                vals = struct.unpack("!126f", raw[:504])
#                # vals[i] = RSSI dBm for channel i (2400+i MHz)
#                print(vals)
#            return len(input_items[0])
#
#  ── Full waterfall in GRC (recommended) ─────────────────────
#
#  Because nRF24 gives 1-bit RPD (not I/Q samples), the data is
#  a *power vector*, not complex baseband.  Use:
#
#    UDP Source (byte, MTU=504)
#    → Python Block (reinterpret 504 bytes → 126 float32)
#    → QT GUI Vector Sink (vlen=126, x_start=2400, x_step=1,
#                          x_axis_label="Freq (MHz)",
#                          y_axis_label="RSSI (dBm)")
#
#  This gives a live 126-bar spectrum matching your terminal view.
# =============================================================
