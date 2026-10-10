#!/usr/bin/env python3

import argparse
import asyncio
import struct
import sys
from datetime import datetime

from bleak import BleakClient, BleakScanner

# 128-bit UUIDs. BlueNRG stores UUID bytes little-endian, so the string form is the
# byte array reversed. Check these against what nRF Connect shows and edit if needed.
ACC_SERV_UUID  = "00000000-0001-11e1-9ab4-0002a5d5c51b"
ACC_CHAR_UUID  = "00000000-0002-11e1-9ab4-0002a5d5c51b"   # (a) acceleration, NOTIFY
FREQ_CHAR_UUID = "00000000-0003-11e1-9ab4-0002a5d5c51b"   # (b) frequency, WRITE
SM_CHAR_UUID   = "00000000-0004-11e1-9ab4-0002a5d5c51b"   # (c) significant motion, NOTIFY

FREQ_MIN, FREQ_MAX = 1, 100


def on_acc_notify(_sender, data: bytearray):
    """Called every time the STM32 notifies new acceleration values."""
    if len(data) < 6:
        print(f"short packet ({len(data)} bytes): {data.hex()}")
        return
    x, y, z = struct.unpack("<hhh", data[:6])
    t = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{t}] X={x:6d} mg  Y={y:6d} mg  Z={z:6d} mg")


def on_sm_notify(_sender, data: bytearray):
    """Called when the STM32 reports a significant motion event."""
    count = struct.unpack("<H", data[:2])[0] if len(data) >= 2 else data[0]
    t = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{t}] *** SIGNIFICANT MOTION detected (event #{count}) ***")


async def write_freq(client: BleakClient, freq: int):
    freq = max(FREQ_MIN, min(FREQ_MAX, freq))
    await client.write_gatt_char(FREQ_CHAR_UUID, struct.pack("<H", freq), response=True)
    print(f"--> sampling frequency set to {freq} Hz")


async def find_device(name: str | None, address: str | None, timeout: float):
    if address:
        print(f"Looking for {address} ...")
        return await BleakScanner.find_device_by_address(address, timeout=timeout)
    print(f"Scanning for a device named '{name}' ...")
    return await BleakScanner.find_device_by_name(name, timeout=timeout)


async def read_user_input(client: BleakClient, stop: asyncio.Event):
    """Read frequencies typed by the user without blocking BLE notifications."""
    loop = asyncio.get_running_loop()
    while not stop.is_set():
        line = await loop.run_in_executor(None, sys.stdin.readline)
        if not line:                      # stdin closed
            stop.set()
            break
        line = line.strip()
        if line.lower() in ("q", "quit", "exit"):
            stop.set()
            break
        if not line:
            continue
        try:
            await write_freq(client, int(line))
        except ValueError:
            print(f"Enter an integer {FREQ_MIN}-{FREQ_MAX}, or q to quit")


async def main():
    parser = argparse.ArgumentParser(description="STM32 BLE accelerometer client")
    parser.add_argument("--name", default="BlueNRG", help="advertised name of the STM32 board")
    parser.add_argument("--address", help="MAC address (overrides --name)")
    parser.add_argument("--freq", type=int, default=10, help="initial sampling frequency in Hz")
    parser.add_argument("--timeout", type=float, default=10.0, help="scan timeout in seconds")
    args = parser.parse_args()

    device = await find_device(args.name, args.address, args.timeout)
    if device is None:
        print("Device not found. Check that the board is advertising and not connected to your phone.")
        return

    stop = asyncio.Event()
    ready = False   # becomes True only after the connection is fully set up

    def on_disconnect(_client):
        # BlueZ sometimes reports a disconnect while the connection is still being
        # established. Ignore those; only a disconnect after setup ends the program.
        if ready:
            print("Disconnected from STM32")
            stop.set()
        else:
            print("(ignored a disconnect event during connection setup)")

    async with BleakClient(device, disconnected_callback=on_disconnect) as client:
        print(f"Connected to {device.name} ({device.address})")

        await write_freq(client, args.freq)
        await client.start_notify(ACC_CHAR_UUID, on_acc_notify)
        await client.start_notify(SM_CHAR_UUID, on_sm_notify)
        ready = True
        print(f"Receiving data. Type a frequency ({FREQ_MIN}-{FREQ_MAX}) and press Enter to change it, q to quit.")

        input_task = asyncio.create_task(read_user_input(client, stop))
        await stop.wait()
        input_task.cancel()

        if client.is_connected:
            await client.stop_notify(ACC_CHAR_UUID)
            await client.stop_notify(SM_CHAR_UUID)
    print("Bye")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
