# -*- coding: utf-8 -*-
"""
Created on Sun Jun 14 20:12:26 2026

@author: C_Bhavana_Singh
"""

import serial, numpy as np, sounddevice as sd, threading, time

PORT, BAUD, RATE = 'COM17', 115200, 8000
MAX_LAG = RATE // 10

ser = serial.Serial(PORT, BAUD, timeout=0)
buf = np.zeros(0, dtype=np.float32)
lock = threading.Lock()
running = True                      # lets us stop the thread cleanly

def reader():
    global buf
    while running:                  # exits when we set running=False
        try:
            n = ser.in_waiting
            if n:
                raw = ser.read(n)
                s = (np.frombuffer(raw, np.uint8).astype(np.float32) - 128) / 128
                with lock:
                    buf = np.concatenate([buf, s])
                    if len(buf) > MAX_LAG:
                        buf = buf[-MAX_LAG:]
            else:
                time.sleep(0.001)
        except Exception:
            break                   # port closed → stop quietly

def feed_speaker(outdata, frames, t, status):
    global buf
    with lock:
        n = min(frames, len(buf))
        outdata[:n, 0] = buf[:n]
        outdata[n:, 0] = 0
        buf = buf[n:]

t = threading.Thread(target=reader, daemon=True)
t.start()
try:
    with sd.OutputStream(channels=1, samplerate=RATE,
                         callback=feed_speaker, blocksize=128, latency='low'):
        input("Playing — press Enter to stop\n")
finally:
    running = False                 # stop the thread
    time.sleep(0.05)
    ser.close()                     # always release the port