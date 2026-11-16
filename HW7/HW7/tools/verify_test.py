"""Capture the known-signal test run from the board and verify the FIR output.

The firmware runs a 300-sample test at boot and every time the blue USER
button is pressed:
    X = sin(2*pi*2t) + 0.5*sin(2*pi*30t)   (30 Hz should be removed)
    Y = unit impulse                       (output should equal the coefficients)
    Z = unit step                          (output should settle to DC gain = 1)

This script recomputes the filter with numpy (float64) from the same input
and the coefficients in App/Inc/fir_coeffs.h, reports the error, and saves
plots to tools/out/ for the report.

Usage:
    uv run verify_test.py --port COM5          # capture live (press RESET or USER button)
    uv run verify_test.py --csv out/test_capture.csv   # re-analyse a saved capture
    uv run verify_test.py --list               # list serial ports
"""
import argparse
import sys

import matplotlib.pyplot as plt
import numpy as np

from common import OUT_DIR, list_ports, load_coeffs, open_serial

COLS = ["n", "x", "y", "z", "fx", "fy", "fz"]


def capture(port: str, baud: int) -> tuple[np.ndarray, str]:
    ser = open_serial(port, baud)
    print(f"Listening on {port} @ {baud}. Press the board's RESET or blue USER button...")
    rows, summary, started = [], "", False
    while True:
        line = ser.readline().decode("ascii", errors="ignore").strip()
        if not line:
            continue
        if line.startswith("#TEST_BEGIN"):
            print(line)
            rows, started = [], True
        elif started and line.startswith("T,"):
            rows.append([float(v) for v in line.split(",")[1:]])
        elif started and line.startswith("#TEST_END"):
            summary = line
            print(line)
            break
    ser.close()
    return np.array(rows), summary


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--csv", help="analyse a saved capture instead of the serial port")
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    args = ap.parse_args()

    if args.list:
        print(list_ports())
        return
    OUT_DIR.mkdir(exist_ok=True)

    if args.csv:
        data = np.loadtxt(args.csv, delimiter=",", skiprows=1)
        summary = ""
    elif args.port:
        data, summary = capture(args.port, args.baud)
        np.savetxt(OUT_DIR / "test_capture.csv", data, delimiter=",",
                   header=",".join(COLS), comments="", fmt="%.6f")
        print(f"saved {OUT_DIR / 'test_capture.csv'}")
    else:
        ap.error("give --port COMx or --csv file (use --list to see ports)")

    h, fs, fc = load_coeffs()
    n = data[:, 0].astype(int)
    x, y, z, fx, fy, fz = (data[:, i] for i in range(1, 7))
    N, taps = len(n), len(h)
    delay = (taps - 1) // 2

    # ---- numerical comparison against numpy float64 reference -------------
    ok = True
    print(f"\nFIR: {taps} taps, fs={fs:g} Hz, fc={fc:g} Hz, group delay={delay} samples")
    print(f"{'axis':<6}{'signal':<24}{'max |board - numpy|':>22}")
    for name, desc, sig, out in (("X", "2 Hz + 30 Hz sines", x, fx),
                                 ("Y", "unit impulse", y, fy),
                                 ("Z", "unit step", z, fz)):
        ref = np.convolve(sig, h)[:N]
        err = np.max(np.abs(out - ref))
        ok &= err < 1e-5          # board prints 6 decimals -> ~5e-7 rounding
        print(f"{name:<6}{desc:<24}{err:>22.3e}")
    imp_err = np.max(np.abs(fy[:taps] - h))
    print(f"impulse response vs coefficients : max err = {imp_err:.3e}")
    print(f"step response final value        : {fz[-1]:.6f} (expected sum(h) = {h.sum():.6f})")

    # amplitude of each tone after the filter: skip the start-up transient and
    # use a whole number of 2 Hz periods (50 samples = 1 x 2 Hz = 15 x 30 Hz)
    period = int(round(fs / 2.0))
    ss = slice(N - ((N - taps) // period) * period, N)
    t = n[ss] / fs
    for f0 in (2.0, 30.0):
        basis = np.exp(-2j * np.pi * f0 * t)
        a_in = 2 * abs(np.mean(x[ss] * basis))
        a_out = 2 * abs(np.mean(fx[ss] * basis))
        print(f"{f0:>4g} Hz tone: in {a_in:.4f} -> out {a_out:.4f}  ({20*np.log10(a_out/a_in):6.1f} dB)")
    if summary:
        print(f"board summary: {summary}")
    print("RESULT:", "PASS" if ok else "FAIL")

    # ---- plots --------------------------------------------------------------
    fig, ax = plt.subplots(3, 1, figsize=(10, 9), constrained_layout=True)
    ax[0].plot(n, x, color="0.6", lw=1, label="input x[n] = sin(2·2πt) + 0.5·sin(2·30πt)")
    ax[0].plot(n, fx, "C0", lw=2, label="board output (arm_fir_f32)")
    ax[0].plot(n, np.convolve(x, h)[:N], "C1--", lw=1, label="numpy reference")
    ax[0].plot(n, np.sin(2 * np.pi * 2 * (n - delay) / fs), "k:", lw=1,
               label=f"ideal 2 Hz, delayed {delay} samples")
    ax[0].set(title="Test signal X: 30 Hz component removed", xlabel="sample n")
    ax[0].legend(fontsize=8, loc="upper right")

    ax[1].stem(np.arange(taps), h, linefmt="C1-", markerfmt="C1o", basefmt=" ", label="designed h[k]")
    ax[1].plot(n[: taps + 10], fy[: taps + 10], "C0x", ms=8, label="board impulse response")
    ax[1].set(title="Test signal Y: impulse response = FIR coefficients", xlabel="sample n")
    ax[1].legend(fontsize=8)

    ax[2].plot(n, z, color="0.6", label="input u[n]")
    ax[2].plot(n, fz, "C0", label="board step response")
    ax[2].set(title="Test signal Z: step response (DC gain 1)", xlabel="sample n", ylim=(-0.2, 1.2))
    ax[2].legend(fontsize=8)
    fig.savefig(OUT_DIR / "test_time.png", dpi=150)

    # spectrum before/after + designed frequency response
    fig2, ax2 = plt.subplots(figsize=(10, 4.5), constrained_layout=True)
    w = np.hanning(N - taps)
    f = np.fft.rfftfreq(N - taps, 1 / fs)
    for sig, lab, c in ((x, "input X", "0.5"), (fx, "board output", "C0")):
        spec = np.abs(np.fft.rfft(sig[taps:] * w)) / w.sum() * 2
        ax2.plot(f, 20 * np.log10(spec + 1e-9), c, label=lab)
    fr = np.linspace(0, fs / 2, 512)
    H = np.abs(np.exp(-2j * np.pi * np.outer(fr, np.arange(taps)) / fs) @ h)
    ax2.plot(fr, 20 * np.log10(H + 1e-9), "C1--", label="|H(f)| designed")
    ax2.axvline(fc, color="k", ls=":", lw=1)
    ax2.set(title="Spectrum before / after filtering", xlabel="frequency [Hz]",
            ylabel="magnitude [dB]", ylim=(-100, 5), xlim=(0, fs / 2))
    ax2.legend()
    fig2.savefig(OUT_DIR / "test_spectrum.png", dpi=150)
    print(f"plots saved to {OUT_DIR}")
    plt.show()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
