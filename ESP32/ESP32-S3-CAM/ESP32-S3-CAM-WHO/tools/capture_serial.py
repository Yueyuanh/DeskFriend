"""Reset the board over DTR/RTS and capture its UART output for N seconds.

Usage: python tools/capture_serial.py [PORT] [SECONDS]
"""
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM12"
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 25.0

with serial.Serial(port, 115200, timeout=0.2) as ser:
    # IO0 high (DTR deasserted), pulse EN low to reboot into the application.
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.15)
    ser.setRTS(False)
    time.sleep(0.05)

    deadline = time.time() + seconds
    chunks = []
    while time.time() < deadline:
        data = ser.read(4096)
        if data:
            chunks.append(data)
    sys.stdout.buffer.write(b"".join(chunks))
