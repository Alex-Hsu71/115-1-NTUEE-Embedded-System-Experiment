import queue
import socket
import threading
import tkinter as tk
from collections import deque
from datetime import datetime
from tkinter import ttk


HOST = "0.0.0.0"
PORT = 8002
SAMPLE_LIMIT = 200


class SensorServer:
    def __init__(self, sample_queue):
        self.sample_queue = sample_queue
        self.stop_event = threading.Event()
        self.server_socket = None

    def start(self):
        thread = threading.Thread(target=self._run, daemon=True)
        thread.start()

    def stop(self):
        self.stop_event.set()
        if self.server_socket is not None:
            self.server_socket.close()

    def _run(self):
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((HOST, PORT))
            self.server_socket.listen(1)
            self.server_socket.settimeout(1.0)
            print(f"TCP server listening on port {PORT}")

            while not self.stop_event.is_set():
                try:
                    connection, address = self.server_socket.accept()
                except socket.timeout:
                    continue
                except OSError:
                    break

                print(f"STM32 connected from {address}")
                self._receive(connection)
                connection.close()
        except OSError as error:
            self.sample_queue.put(("error", str(error)))
        finally:
            if self.server_socket is not None:
                self.server_socket.close()

    def _receive(self, connection):
        connection.settimeout(1.0)
        pending = ""
        while not self.stop_event.is_set():
            try:
                payload = connection.recv(1024)
            except socket.timeout:
                continue
            except OSError:
                break

            if not payload:
                break

            pending += payload.decode("utf-8", errors="ignore")
            lines = pending.split("\n")
            pending = lines.pop()
            for line in lines:
                line = line.strip()
                if line:
                    print("Received:", line)
                if line == "EVENT:MOTION":
                    self.sample_queue.put(("motion", None))
                    continue
                sample = self._parse_line(line)
                if sample is not None:
                    self.sample_queue.put(("sample", sample))
                elif line:
                    self.sample_queue.put(("raw", line))

    @staticmethod
    def _parse_line(line):
        try:
            if line.startswith("DATA,ACC:") and ",GYRO:" in line:
                acceleration_text, gyro_text = line[9:].split(",GYRO:", 1)
                gyroscope = [float(value) for value in gyro_text.split(",")]
            elif line.startswith("ACC:"):
                acceleration_text = line[4:]
                gyroscope = [0.0, 0.0, 0.0]
            else:
                return None

            acceleration = [int(value) for value in acceleration_text.split(",")]
            if len(acceleration) != 3 or len(gyroscope) != 3:
                return None
            return acceleration, gyroscope
        except (TypeError, ValueError):
            return None


class SensorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("STM32 LSM6DSL Sensor Monitor")
        self.root.geometry("1100x760")

        self.sample_queue = queue.Queue()
        self.server = SensorServer(self.sample_queue)
        self.acceleration = [deque(maxlen=SAMPLE_LIMIT) for _ in range(3)]
        self.gyroscope = [deque(maxlen=SAMPLE_LIMIT) for _ in range(3)]
        self.sample_count = 0
        self.motion_markers = deque(maxlen=SAMPLE_LIMIT)
        self.status = tk.StringVar(value=f"Listening on TCP {PORT}")

        toolbar = ttk.Frame(root, padding=8)
        toolbar.pack(fill=tk.X)
        ttk.Label(toolbar, textvariable=self.status).pack(side=tk.LEFT)

        # Full-width banner that flashes red on a motion event, instead of a
        # small corner label, so it is hard to miss during a live demo.
        self.banner = tk.Label(root, text="No motion alert", font=("TkDefaultFont", 14, "bold"),
                                background="#1b2733", foreground="#9fb3c8", pady=6)
        self.banner.pack(fill=tk.X)

        body = ttk.Frame(root)
        body.pack(fill=tk.BOTH, expand=True, padx=8, pady=8)

        self.canvas = tk.Canvas(body, background="#101820", highlightthickness=0)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        log_frame = ttk.Frame(body, width=220)
        log_frame.pack(side=tk.LEFT, fill=tk.Y, padx=(8, 0))
        log_frame.pack_propagate(False)
        ttk.Label(log_frame, text="Motion events").pack(anchor="w")
        self.event_log = tk.Listbox(log_frame, background="#101820", foreground="#f4f7fa",
                                     highlightthickness=0, borderwidth=0)
        self.event_log.pack(fill=tk.BOTH, expand=True)

        self.server.start()
        self.root.after(50, self._process_queue)
        self.root.protocol("WM_DELETE_WINDOW", self._close)

    def _process_queue(self):
        try:
            while True:
                message_type, value = self.sample_queue.get_nowait()
                if message_type == "sample":
                    acceleration, gyroscope = value
                    for index in range(3):
                        self.acceleration[index].append(acceleration[index])
                        self.gyroscope[index].append(gyroscope[index])
                    self.sample_count += 1
                    self.status.set(f"Connected - samples: {self.sample_count}")
                elif message_type == "motion":
                    self._on_motion_event()
                elif message_type == "error":
                    self.status.set(f"Server error: {value}")
                elif message_type == "raw":
                    self.status.set(f"Unrecognized packet: {value[:45]}")
        except queue.Empty:
            pass

        self._redraw()
        self.root.after(50, self._process_queue)

    def _on_motion_event(self):
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.banner.configure(text="ALERT: SIGNIFICANT MOTION DETECTED", background="#d32f2f",
                               foreground="#ffffff")
        self.root.after(1500, self._clear_motion_banner)
        self.event_log.insert(0, f"{timestamp}  motion")
        self.motion_markers.append(self.sample_count)

    def _clear_motion_banner(self):
        self.banner.configure(text="No motion alert", background="#1b2733", foreground="#9fb3c8")

    def _redraw(self):
        self.canvas.delete("all")
        width = max(self.canvas.winfo_width(), 500)
        height = max(self.canvas.winfo_height(), 500)
        panel_height = (height - 36) // 2
        self._draw_plot(8, panel_height, width, "Acceleration (mg)", self.acceleration, mark_motion=True)
        self._draw_plot(20 + panel_height, panel_height, width, "Gyroscope (dps)", self.gyroscope)

    def _draw_plot(self, top, panel_height, width, title, series, mark_motion=False):
        left = 58
        right = width - 18
        bottom = top + panel_height - 28
        colors = ("#ff6b6b", "#ffd166", "#4dd4ac")
        values = [value for channel in series for value in channel]
        if not values:
            minimum, maximum = -1.0, 1.0
        else:
            minimum = min(values)
            maximum = max(values)
            padding = max((maximum - minimum) * 0.15, 1.0)
            minimum -= padding
            maximum += padding
        if maximum == minimum:
            maximum += 1.0

        self.canvas.create_rectangle(left, top, right, bottom, outline="#40566b")
        self.canvas.create_text(left, top + 10, text=title, anchor="w", fill="#f4f7fa")
        self.canvas.create_text(8, top + 10, text=f"{maximum:.1f}", anchor="w", fill="#9fb3c8")
        self.canvas.create_text(8, bottom, text=f"{minimum:.1f}", anchor="w", fill="#9fb3c8")

        for index, channel in enumerate(series):
            samples = list(channel)
            if len(samples) < 2:
                continue
            points = []
            for sample_index, value in enumerate(samples):
                x = left + (right - left) * sample_index / (SAMPLE_LIMIT - 1)
                y = bottom - (value - minimum) * (bottom - top) / (maximum - minimum)
                points.extend((x, y))
            self.canvas.create_line(*points, fill=colors[index], width=2, smooth=True)
            self.canvas.create_text(right - 100 + index * 32, top + 10,
                                    text=("X", "Y", "Z")[index],
                                    fill=colors[index])

        if mark_motion and series and series[0]:
            window_len = len(series[0])
            window_start = self.sample_count - window_len
            for marker in self.motion_markers:
                relative_index = marker - window_start
                if 0 <= relative_index < window_len:
                    x = left + (right - left) * relative_index / (SAMPLE_LIMIT - 1)
                    self.canvas.create_line(x, top, x, bottom, fill="#ff2d55", width=1, dash=(4, 2))

    def _close(self):
        self.server.stop()
        self.root.destroy()


if __name__ == "__main__":
    application = tk.Tk()
    SensorApp(application)
    application.mainloop()