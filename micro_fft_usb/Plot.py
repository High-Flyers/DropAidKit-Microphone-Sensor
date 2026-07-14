import serial
import matplotlib.pyplot as plt
import numpy as np

PORT = 'COM16'  # albo /dev/ttyUSB0 na Linuxie
BAUD = 115200
SAMPLES = 256
FS = 16000

ser = serial.Serial(PORT, BAUD)

plt.ion()
fig, ax = plt.subplots()

freqs = np.linspace(0, FS/2, SAMPLES//2)

while True:
    try:
        line = ser.readline().decode(errors='ignore').strip()

        if not line.startswith("FFT:"):
            continue

        line = line.replace("FFT:", "")
        values = list(map(float, line.split(',')))

        if len(values) != SAMPLES//2:
            continue

        ax.clear()
        ax.plot(freqs, values)

        ax.set_xlabel("Frequency (Hz)")
        ax.set_ylabel("Magnitude")
        ax.set_title("FFT Spectrum")

        plt.pause(0.5)

    except Exception as e:
        print("Error:", e)