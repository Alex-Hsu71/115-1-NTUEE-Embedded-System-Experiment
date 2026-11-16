"""Live plot of accelerometer data: raw vs FIR-filtered, from the board.

Usage:
    uv run live_plot.py --tcp 8002         # WiFi: TCP server, the board connects to us
    uv run live_plot.py --port COM7        # UART (ST-LINK VCP)
    add --save out/sensor_capture.csv to also log the samples to CSV

In WiFi mode the board must have this PC's IP in App/Inc/wifi_config.h.

Shake / tap the board: the raw trace is noisy, the filtered trace is smooth and
lags by (taps-1)/2 = 15 samples (150 ms).

Keys (plot window focused):
    space   pause / resume the display (data keeps logging)
    s       save a snapshot PNG to tools/out/
    q       close the window (also saves out/live_plot.png)
Ctrl+C in the terminal also saves out/live_plot.png before exiting.
"""
import argparse
import signal
import socket
import threading
from collections import deque
from datetime import datetime

import matplotlib.animation as animation
import matplotlib.pyplot as plt
import numpy as np

from common import OUT_DIR, list_ports, load_coeffs, open_serial

WINDOW = 500  # samples shown (5 s at 100 Hz)


def serial_lines(port, baud, stop):
    ser = open_serial(port, baud)
    try:
        while not stop.is_set():
            yield ser.readline().decode("ascii", errors="ignore")
    finally:
        ser.close()


def tcp_lines(port, stop):
    """TCP server: accept the board, yield its lines, re-accept after a drop."""
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen(1)
    srv.settimeout(1.0)
    print(f"TCP server listening on port {port}, waiting for the board...")
    try:
        while not stop.is_set():
            try:
                conn, addr = srv.accept()
            except socket.timeout:
                continue
            print(f"board connected from {addr[0]}:{addr[1]}")
            conn.settimeout(1.0)
            pending = b""
            with conn:
                while not stop.is_set():
                    try:
                        chunk = conn.recv(2048)
                    except socket.timeout:
                        continue
                    except OSError:
                        break
                    if not chunk:
                        break
                    pending += chunk
                    *done, pending = pending.split(b"\n")
                    for raw in done:
                        yield raw.decode("ascii", errors="ignore")
            print("board disconnected, waiting for reconnect...")
    finally:
        srv.close()


def reader(source, buf, lock, stop, log):
    for line in source:
        line = line.strip()
        if line.startswith("S,"):
            try:
                vals = [float(v) for v in line.split(",")[1:]]
            except ValueError:
                continue
            if len(vals) == 7:
                with lock:
                    if buf and vals[0] < buf[-1][0]:   # board restarted / test re-run
                        buf.clear()
                    buf.append(vals)
                if log:
                    log.write(line[2:] + "\n")
        elif line.startswith("#"):
            print(line)


def main() -> None:
    ap = argparse.ArgumentParser()
    src = ap.add_mutually_exclusive_group()
    src.add_argument("--port", help="serial port, e.g. COM7")
    src.add_argument("--tcp", type=int, metavar="PORT", help="listen for the board on this TCP port (WiFi)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--save", help="CSV file to log sensor samples")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()
    if args.list or not (args.port or args.tcp):
        print(list_ports())
        return

    h, fs, fc = load_coeffs()
    buf, lock, stop = deque(maxlen=WINDOW), threading.Lock(), threading.Event()
    source = tcp_lines(args.tcp, stop) if args.tcp else serial_lines(args.port, args.baud, stop)
    via = f"WiFi (TCP :{args.tcp})" if args.tcp else f"UART ({args.port})"
    log = open(args.save, "w", encoding="utf-8") if args.save else None
    if log:
        log.write("n,x,y,z,fx,fy,fz\n")
    threading.Thread(target=reader, args=(source, buf, lock, stop, log), daemon=True).start()

    fig, axes = plt.subplots(3, 1, sharex=True, figsize=(10, 8), constrained_layout=True)
    fig.suptitle(f"LSM6DSL accelerometer via {via} — raw vs FIR low-pass ({len(h)} taps, fc={fc:g} Hz)")
    lines = []
    for ax, name in zip(axes, "XYZ"):
        raw, = ax.plot([], [], color="0.6", lw=1, label="raw")
        flt, = ax.plot([], [], "C0", lw=2, label="filtered")
        ax.set_ylabel(f"{name} [mg]")
        ax.legend(loc="upper left", fontsize=8)
        lines.append((raw, flt))
    axes[-1].set_xlabel("time [s]")

    OUT_DIR.mkdir(exist_ok=True)
    plt.rcParams["keymap.save"] = ["ctrl+s"]      # free 's' for our snapshot key
    state = {"paused": False, "saved": False}

    def save(path):
        fig.savefig(path, dpi=150)
        print(f"saved {path}")

    def save_final():
        if not state["saved"]:
            state["saved"] = True
            save(OUT_DIR / "live_plot.png")

    def on_key(event):
        if event.key == " ":
            state["paused"] = not state["paused"]
            fig.suptitle(fig._suptitle.get_text().replace("  [PAUSED]", "")
                         + ("  [PAUSED]" if state["paused"] else ""))
            fig.canvas.draw_idle()
        elif event.key == "s":
            save(OUT_DIR / f"live_{datetime.now():%H%M%S}.png")

    fig.canvas.mpl_connect("key_press_event", on_key)
    fig.canvas.mpl_connect("close_event", lambda _e: save_final())

    # Ctrl+C in the terminal: save, then close the window cleanly
    def on_sigint(*_):
        save_final()
        plt.close(fig)

    signal.signal(signal.SIGINT, on_sigint)

    def update(_):
        if state["paused"]:
            return []
        with lock:
            if not buf:
                return []
            d = np.array(buf)
        t = d[:, 0] / fs
        for i, (ax, (raw, flt)) in enumerate(zip(axes, lines)):
            raw.set_data(t, d[:, 1 + i])
            flt.set_data(t, d[:, 4 + i])
            ax.set_xlim(t[0], max(t[-1], t[0] + WINDOW / fs))
            lo, hi = d[:, [1 + i, 4 + i]].min(), d[:, [1 + i, 4 + i]].max()
            pad = max(20.0, 0.1 * (hi - lo))
            ax.set_ylim(lo - pad, hi + pad)
        return [l for pair in lines for l in pair]

    _anim = animation.FuncAnimation(fig, update, interval=100, cache_frame_data=False)
    try:
        plt.show()
    finally:
        stop.set()
        if log:
            log.close()


if __name__ == "__main__":
    main()
