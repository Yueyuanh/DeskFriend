"""Bounded UART capture, leaving DTR/RTS inactive (does not reset ESP32)."""
import argparse
from pathlib import Path
import time
import serial

p=argparse.ArgumentParser();p.add_argument('--port',default='COM10');p.add_argument('--seconds',type=float,default=25);p.add_argument('--out',default='.pio/serial_lvgl.log');args=p.parse_args()
uart=serial.Serial();uart.port=args.port;uart.baudrate=115200;uart.timeout=.2;uart.dtr=False;uart.rts=False;uart.open()
end=time.monotonic()+args.seconds
with Path(args.out).open('wb') as output:
    while time.monotonic()<end:
        data=uart.read(uart.in_waiting or 1)
        if data:
            output.write(data);output.flush();print(data.decode('utf-8','replace'),end='',flush=True)
uart.close()
