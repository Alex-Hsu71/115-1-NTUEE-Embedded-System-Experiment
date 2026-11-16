"""Shared helpers: read FIR coefficients from the C header, open the serial port."""
import re
from pathlib import Path

import numpy as np

HEADER = Path(__file__).resolve().parent.parent / "App" / "Inc" / "fir_coeffs.h"
OUT_DIR = Path(__file__).resolve().parent / "out"


def load_coeffs(path: Path = HEADER) -> tuple[np.ndarray, float, float]:
    text = path.read_text(encoding="utf-8")
    fs = float(re.search(r"FIR_FS_HZ\s+([\d.]+)f", text).group(1))
    fc = float(re.search(r"FIR_FC_HZ\s+([\d.]+)f", text).group(1))
    body = text[text.index("{") + 1 : text.index("}")]
    h = np.array([float(v.strip().rstrip("f")) for v in body.split(",") if v.strip()])
    return h, fs, fc


def open_serial(port: str, baud: int):
    import serial  # pyserial

    return serial.Serial(port, baud, timeout=1)


def list_ports() -> str:
    from serial.tools import list_ports as lp

    ports = [f"{p.device} ({p.description})" for p in lp.comports()]
    return "\n".join(ports) if ports else "(no serial ports found)"
