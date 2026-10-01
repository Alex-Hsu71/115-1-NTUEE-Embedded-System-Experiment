#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
server.py -- 電腦端 (host) 的 TCP server + 即時繪圖
"""

import socket
import threading
import collections
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# ---------- 可調參數 ----------
HOST = "0.0.0.0"
PORT = 8002
MAXLEN = 200 
# --------------------------------


buf_x = collections.deque([0] * MAXLEN, maxlen=MAXLEN)
buf_y = collections.deque([0] * MAXLEN, maxlen=MAXLEN)
buf_z = collections.deque([0] * MAXLEN, maxlen=MAXLEN)

lock = threading.Lock()


def recv_loop(conn, addr):
    """背景執行緒:不斷從一個已連線的 socket 收資料、切行、解析、存進 buffer。"""
    print(f"[+] Client connected: {addr[0]}:{addr[1]}")
    leftover = ""

    while True:
        try:
            data = conn.recv(1024)
        except OSError:
            break
        if not data:
            break

        leftover += data.decode("utf-8", errors="ignore")
        lines = leftover.split("\n")
        leftover = lines.pop()

        for line in lines:
            line = line.strip()
            if not line:
                continue
            parts = line.split(",")
            if len(parts) != 3:
                continue
            try:
                x, y, z = (int(float(p)) for p in parts)
            except ValueError:
                continue
            with lock:
                buf_x.append(x)
                buf_y.append(y)
                buf_z.append(z)

    print(f"[-] Client disconnected: {addr[0]}:{addr[1]}")
    conn.close()


def accept_loop(server):
    while True:
        conn, addr = server.accept()
        threading.Thread(target=recv_loop, args=(conn, addr), daemon=True).start()


def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, PORT))
    server.listen(1)
    print(f"[*] TCP server listening on {HOST}:{PORT} ... (Ctrl+C 結束)")

    threading.Thread(target=accept_loop, args=(server,), daemon=True).start()

    fig, ax = plt.subplots()
    (line_x,) = ax.plot([], [], label="X")
    (line_y,) = ax.plot([], [], label="Y")
    (line_z,) = ax.plot([], [], label="Z")
    ax.set_xlim(0, MAXLEN)
    ax.set_ylim(-2000, 2000)
    ax.set_xlabel("sample")
    ax.set_ylabel("accelerometer raw value")
    ax.set_title("STM32 3-axis accelerometer (live)")
    ax.legend(loc="upper right")
    ax.grid(True)

    xs = range(MAXLEN)

    def update(_frame):
        with lock:
            ax.set_ylim(-2000, 2000)
            line_x.set_data(xs, list(buf_x))
            line_y.set_data(xs, list(buf_y))
            line_z.set_data(xs, list(buf_z))
        return line_x, line_y, line_z

    _ani = FuncAnimation(fig, update, interval=50, blit=False, cache_frame_data=False)
    plt.show()


if __name__ == "__main__":
    main()
