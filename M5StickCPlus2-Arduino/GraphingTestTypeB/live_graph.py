#!/usr/bin/env python3
"""
live_graph.py  —  Live weight graph for M5StickC Plus2 + UNIT_SCALES
─────────────────────────────────────────────────────────────────────
Reads CSV lines from Serial:
    DATA,<millis_ms>,<weight_g>,<boltStart>,<boltGoal>

    boltStart / boltGoal:  0 = touching (active),  1 = not present

Draws a live matplotlib window showing:
  • Weight curve (colour changes at warning/max thresholds)
  • Blue  background band when boltStart == 0
  • Green background band when boltGoal  == 0
  • Dashed threshold lines (warning = orange, max = red)
  • Press 's' or the Save button on the toolbar to save as PNG/SVG/etc.

Usage:
    python live_graph.py
    python live_graph.py --port COM3
    python live_graph.py --port /dev/ttyUSB0 --baud 115200
    python live_graph.py --window 60          # show last 60 seconds
    python live_graph.py --max 300 --warn 150 # override thresholds

Requirements:
    pip install pyserial matplotlib
"""

import argparse
import collections
import sys
import threading
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("[ERROR] pyserial not found — install with:  pip install pyserial")

try:
    import matplotlib
    matplotlib.use("TkAgg")          # works on most systems; change if needed
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    from matplotlib.animation import FuncAnimation
except ImportError:
    sys.exit("[ERROR] matplotlib not found — install with:  pip install matplotlib")


# ── defaults (can be overridden via CLI) ─────────────────────────────────────
MAX_WEIGHT = 1000.0
WARNING_WEIGHT = 750.0
WINDOW_SECONDS = 30          # how many seconds of data to show at once
BAUD_RATE = 115200

# ── data store (filled by the reader thread) ──────────────────────────────────
# Each entry: (time_s, weight, bolt_start, bolt_goal)
data: collections.deque = collections.deque()
data_lock = threading.Lock()
tare_events: list[float] = []     # timestamps of TARE events
t0: float | None = None           # time of first sample (seconds)
last_status = "Waiting for device…"


# ── serial reader thread ──────────────────────────────────────────────────────

def serial_reader(port: str, baud: int):
    global t0, last_status

    while True:
        try:
            with serial.Serial(port, baud, timeout=1) as ser:
                last_status = f"Connected on {port}"
                while True:
                    raw = ser.readline()
                    if not raw:
                        continue
                    line = raw.decode("ascii", errors="ignore").strip()

                    if line == "READY":
                        last_status = "Device ready"
                        continue

                    if line == "TARE":
                        ts = time.time() - (t0 or time.time())
                        tare_events.append(ts)
                        last_status = "Scale zeroed (TARE)"
                        continue

                    if line.startswith("DATA,"):
                        parts = line.split(",")
                        if len(parts) != 5:
                            continue
                        try:
                            millis = int(parts[1])
                            weight = float(parts[2])
                            bolt_start = int(parts[3])
                            bolt_goal = int(parts[4])
                        except ValueError:
                            continue

                        ts = millis / 1000.0   # convert ms → seconds
                        if t0 is None:
                            t0 = time.time() - ts   # anchor wall-clock time

                        with data_lock:
                            data.append((ts, weight, bolt_start, bolt_goal))

        except serial.SerialException as e:
            last_status = f"Serial error: {e} — retrying…"
            time.sleep(2)


# ── helper: split a list of (x, y) into colour-coded segments ────────────────

def colour_segments(times, weights, warn, maxw):
    """
    Returns three lists of (x_array, y_array):
      normal_segs, warning_segs, maxed_segs
    Each segment is a continuous run of the same colour so we can plot them
    with a single colour each.
    """
    normal_segs, warning_segs, maxed_segs = [], [], []

    def category(w):
        if w >= maxw:
            return 2
        if w >= warn:
            return 1
        return 0

    if not times:
        return normal_segs, warning_segs, maxed_segs

    seg_x, seg_y = [times[0]], [weights[0]]
    cur_cat = category(weights[0])

    for x, y in zip(times[1:], weights[1:]):
        cat = category(y)
        if cat == cur_cat:
            seg_x.append(x)
            seg_y.append(y)
        else:
            [normal_segs, warning_segs, maxed_segs][cur_cat].append(
                (seg_x, seg_y))
            # overlap for continuity
            seg_x, seg_y = [seg_x[-1], x], [seg_y[-1], y]
            cur_cat = cat

    [normal_segs, warning_segs, maxed_segs][cur_cat].append((seg_x, seg_y))
    return normal_segs, warning_segs, maxed_segs


# ── matplotlib setup ─────────────────────────────────────────────────────────

def build_plot(window_s, warn, maxw):
    fig, ax = plt.subplots(figsize=(12, 5))
    fig.patch.set_facecolor("#1a1a2e")
    ax.set_facecolor("#16213e")

    for spine in ax.spines.values():
        spine.set_edgecolor("#444466")

    ax.tick_params(colors="#aaaacc")
    ax.xaxis.label.set_color("#aaaacc")
    ax.yaxis.label.set_color("#aaaacc")
    ax.set_title("Weight over Time", color="#ddddff", fontsize=14, pad=10)
    ax.set_xlabel("Time (s)", color="#aaaacc")
    ax.set_ylabel("Weight (g)", color="#aaaacc")
    ax.grid(True, color="#2a2a4a", linestyle="--", linewidth=0.5)

    # Threshold lines (drawn once)
    ax.axhline(warn, color="#ffaa00", linewidth=1.2, linestyle="--",
               label=f"Warning ({warn:.0f} g)")
    ax.axhline(maxw, color="#ff4444", linewidth=1.2, linestyle="--",
               label=f"Max ({maxw:.0f} g)")

    # Legend patches
    legend_elements = [
        mpatches.Patch(facecolor="#2244aa", alpha=0.3,
                       label="Bolt Start active"),
        mpatches.Patch(facecolor="#226622", alpha=0.3,
                       label="Bolt Goal active"),
        plt.Line2D([0], [0], color="#00ccff", linewidth=2, label="Normal"),
        plt.Line2D([0], [0], color="#ffaa00", linewidth=2, label="Warning"),
        plt.Line2D([0], [0], color="#ff4444", linewidth=2, label="Maxed"),
        plt.Line2D([0], [0], color="#ffaa00", linewidth=1.2,
                   linestyle="--", label=f"Warning limit ({warn:.0f} g)"),
        plt.Line2D([0], [0], color="#ff4444", linewidth=1.2,
                   linestyle="--", label=f"Max limit ({maxw:.0f} g)"),
    ]
    ax.legend(handles=legend_elements, loc="upper left",
              facecolor="#1a1a2e", edgecolor="#444466",
              labelcolor="#ccccee", fontsize=8)

    status_text = ax.text(0.99, 0.02, "", transform=ax.transAxes,
                          ha="right", va="bottom", fontsize=8,
                          color="#88aacc")

    fig.tight_layout()
    return fig, ax, status_text


def animate(frame, ax, status_text, window_s, warn, maxw):
    with data_lock:
        snapshot = list(data)

    if not snapshot:
        status_text.set_text(last_status)
        return

    times = [d[0] for d in snapshot]
    weights = [d[1] for d in snapshot]
    bolts = [(d[2], d[3]) for d in snapshot]

    # Scroll window
    t_end = times[-1]
    t_start = max(times[0], t_end - window_s)

    # Filter to visible window
    vis = [(t, w, bs, bg) for t, w, bs, bg in snapshot if t >= t_start]
    if not vis:
        return

    vis_t = [v[0] for v in vis]
    vis_w = [v[1] for v in vis]
    vis_bs = [v[2] for v in vis]
    vis_bg = [v[3] for v in vis]

    ax.cla()
    ax.set_facecolor("#16213e")
    for spine in ax.spines.values():
        spine.set_edgecolor("#444466")
    ax.tick_params(colors="#aaaacc")
    ax.set_xlabel("Time (s)", color="#aaaacc")
    ax.set_ylabel("Weight (g)", color="#aaaacc")
    ax.set_title("Weight over Time", color="#ddddff", fontsize=14, pad=10)
    ax.grid(True, color="#2a2a4a", linestyle="--", linewidth=0.5)

    # ── Background bands for bolt states ────────────────────────────────────
    # Walk through samples and paint axvspan for each active run
    def paint_bands(state_series, times_series, colour, alpha=0.25):
        in_band = False
        band_start = None
        for i, (t, s) in enumerate(zip(times_series, state_series)):
            if s == 0 and not in_band:       # 0 = active
                in_band = True
                band_start = t
            elif s != 0 and in_band:
                ax.axvspan(band_start, t, color=colour,
                           alpha=alpha, linewidth=0)
                in_band = False
        if in_band:
            ax.axvspan(band_start, times_series[-1], color=colour,
                       alpha=alpha, linewidth=0)

    paint_bands(vis_bs, vis_t, "#97d0ff")    # blue  — bolt start
    paint_bands(vis_bg, vis_t, "#7BE45C")    # green — bolt goal

    # ── Threshold lines ──────────────────────────────────────────────────────
    ax.axhline(warn, color="#ffaa00", linewidth=1.2, linestyle="--")
    ax.axhline(maxw, color="#ff4444", linewidth=1.2, linestyle="--")

    # ── Tare markers ────────────────────────────────────────────────────────
    for te in tare_events:
        if t_start <= te <= t_end:
            ax.axvline(te, color="#ffffff", linewidth=1, linestyle=":",
                       label="TARE" if te == tare_events[0] else "")
            ax.text(te, ax.get_ylim()[1] if ax.get_ylim()[1] != 0 else maxw,
                    " TARE", color="#ffffff", fontsize=7, va="top")

    # ── Weight curve (colour-coded) ──────────────────────────────────────────
    seg_colours = ["#00ccff", "#ffaa00", "#ff4444"]
    normal_segs, warning_segs, maxed_segs = colour_segments(
        vis_t, vis_w, warn, maxw)
    for segs, col in zip([normal_segs, warning_segs, maxed_segs], seg_colours):
        for sx, sy in segs:
            ax.plot(sx, sy, color=col, linewidth=2)

    # ── Axes limits ──────────────────────────────────────────────────────────
    ax.set_xlim(t_start, t_end + 0.5)
    y_min = min(0, min(vis_w) - 5)
    y_max = max(maxw * 1.05, max(vis_w) * 1.1)
    ax.set_ylim(y_min, y_max)

    # ── Status text ──────────────────────────────────────────────────────────
    current_w = vis_w[-1]
    bs, bg = vis_bs[-1], vis_bg[-1]
    bolt_info = []
    if bs == 0:
        bolt_info.append("Bolt Start")
    if bg == 0:
        bolt_info.append("Bolt Goal")
    bolt_str = "  ".join(bolt_info) if bolt_info else ""
    ax.text(0.99, 0.02,
            f"{last_status}  |  {current_w:.1f} g  {bolt_str}",
            transform=ax.transAxes, ha="right", va="bottom",
            fontsize=14, color="#88aacc")


# ── CLI ──────────────────────────────────────────────────────────────────────

def auto_detect_port():
    keywords = ("CP210", "CH340", "CH9102", "FTDI",
                "USB Serial", "ESP32", "M5")
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "") + (p.manufacturer or "")
        if any(k.lower() in desc.lower() for k in keywords):
            return p.device
    ports = serial.tools.list_ports.comports()
    return ports[0].device if ports else None


def main():
    parser = argparse.ArgumentParser(
        description="Live weight graph for M5StickC Plus2")
    parser.add_argument("--port",   default=None,          help="Serial port")
    parser.add_argument("--baud",   default=BAUD_RATE,     type=int)
    parser.add_argument("--window", default=WINDOW_SECONDS, type=int,
                        help="Seconds of data to display (default 30)")
    parser.add_argument("--warn",   default=WARNING_WEIGHT, type=float,
                        help=f"Warning threshold in grams (default {WARNING_WEIGHT})")
    parser.add_argument("--max",    default=MAX_WEIGHT,     type=float,
                        help=f"Max threshold in grams (default {MAX_WEIGHT})")
    args = parser.parse_args()

    port = args.port or auto_detect_port()
    if port is None:
        sys.exit("[ERROR] No serial port found — use --port to specify one.")

    print(f"[INFO] Connecting to {port} @ {args.baud} baud")
    print(
        f"[INFO] Warning: {args.warn} g  |  Max: {args.max} g  |  Window: {args.window} s")
    print("[INFO] Close the plot window or press Ctrl-C to quit.")

    # Start background reader thread
    t = threading.Thread(target=serial_reader,
                         args=(port, args.baud), daemon=True)
    t.start()

    # Build plot and start animation
    fig, ax, status_text = build_plot(args.window, args.warn, args.max)
    ani = FuncAnimation(
        fig,
        animate,
        fargs=(ax, status_text, args.window, args.warn, args.max),
        interval=250,       # refresh every 250 ms
        cache_frame_data=False,
    )

    plt.show()


if __name__ == "__main__":
    main()
