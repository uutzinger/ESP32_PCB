#!/usr/bin/env python3

import argparse
import os
import select
import re
import termios
import time
import tty


START_MARKER = b"[CDC] Flooding USB CDC for "
END_MARKER = b"[CDC] Sent "


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Measure ESP32-S3 USB CDC flood throughput from a serial port."
    )
    parser.add_argument("port", help="Serial device path, for example /dev/ttyACM0")
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Requested baud rate for the CDC port (symbolic for USB CDC). Default: 115200.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=20.0,
        help="Overall timeout in seconds. Default: 20.",
    )
    return parser.parse_args()


def baud_constant(baud: int) -> int:
    mapping = {
        9600: termios.B9600,
        19200: termios.B19200,
        38400: termios.B38400,
        57600: termios.B57600,
        115200: termios.B115200,
        230400: termios.B230400,
        460800: termios.B460800,
        921600: getattr(termios, "B921600", termios.B115200),
        1000000: getattr(termios, "B1000000", termios.B115200),
        1500000: getattr(termios, "B1500000", termios.B115200),
        2000000: getattr(termios, "B2000000", termios.B115200),
    }
    return mapping.get(baud, termios.B115200)


def set_raw_port(fd: int, baud: int) -> None:
    tty.setraw(fd)
    attrs = termios.tcgetattr(fd)
    speed = baud_constant(baud)
    attrs[4] = speed
    attrs[5] = speed
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)


def strip_one_newline(data: bytes) -> bytes:
    if data.endswith(b"\r\n"):
        return data[:-2]
    if data.endswith(b"\n") or data.endswith(b"\r"):
        return data[:-1]
    return data


def main() -> int:
    args = parse_args()

    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY)
    try:
        set_raw_port(fd, args.baud)

        print(f"[HOST] Listening on {args.port} at requested baud {args.baud}.")
        print("[HOST] Reset the board now, then wait for the CDC flood test to start.")

        boot_buffer = bytearray()
        flood_buffer = bytearray()
        state = "boot"
        flood_start_time = None
        deadline = time.monotonic() + args.timeout

        while time.monotonic() < deadline:
            remaining = max(0.0, deadline - time.monotonic())
            ready, _, _ = select.select([fd], [], [], remaining)
            if not ready:
                break

            chunk = os.read(fd, 4096)
            if not chunk:
                continue

            if state == "boot":
                boot_buffer.extend(chunk)

                while True:
                    newline_index = boot_buffer.find(b"\n")
                    if newline_index < 0:
                        break

                    line = bytes(boot_buffer[:newline_index]).rstrip(b"\r")
                    del boot_buffer[: newline_index + 1]

                    if line:
                        print(line.decode("utf-8", errors="replace"))

                    if line.startswith(START_MARKER):
                        flood_start_time = time.monotonic()
                        state = "flood"
                        flood_buffer.extend(boot_buffer)
                        boot_buffer.clear()
                        break

            if state == "flood":
                if chunk:
                    flood_buffer.extend(chunk)

                marker_index = flood_buffer.find(END_MARKER)
                if marker_index >= 0:
                    payload = strip_one_newline(bytes(flood_buffer[:marker_index]))
                    payload_bytes = len(payload)
                    end_time = time.monotonic()
                    elapsed_s = max(end_time - flood_start_time, 1e-9)
                    throughput_mbps = (payload_bytes * 8.0) / elapsed_s / 1_000_000.0

                    summary_end = flood_buffer.find(b"\n", marker_index)
                    if summary_end >= 0:
                        summary_line = bytes(flood_buffer[marker_index:summary_end]).rstrip(b"\r")
                        print(summary_line.decode("utf-8", errors="replace"))

                    print(f"[HOST] Counted {payload_bytes} bytes in {elapsed_s:.3f} s")
                    print(f"[HOST] Measured throughput: {throughput_mbps:.3f} Mbit/s")
                    return 0

        print("[HOST] Timed out waiting for the CDC flood test to complete.")
        return 1
    finally:
        os.close(fd)


if __name__ == "__main__":
    raise SystemExit(main())