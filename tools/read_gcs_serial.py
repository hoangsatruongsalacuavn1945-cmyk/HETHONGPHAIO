import serial
import time
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

try:
    ser = serial.Serial('COM11', 115200, timeout=1)
    print("Opened COM11 successfully, capturing serial logs...")
    start = time.time()
    while time.time() - start < 4.0:
        line = ser.readline()
        if line:
            print(line.decode('utf-8', errors='replace').rstrip())
    ser.close()
except Exception as e:
    print(f"Error reading COM11: {e}")
