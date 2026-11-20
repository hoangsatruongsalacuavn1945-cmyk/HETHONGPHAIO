with open('gsheets_live_bridge.py', 'r', encoding='utf-8') as f:
    code = f.read()

old_open = """                ser = serial.Serial()
                ser.port = port
                ser.baudrate = self.baud
                ser.timeout = self.read_timeout
                ser.write_timeout = 1
                ser.dtr = False
                ser.rts = False
                ser.open()"""

new_open = """                ser = serial.Serial()
                ser.port = port
                ser.baudrate = self.baud
                ser.timeout = self.read_timeout
                ser.write_timeout = 1
                ser.open()
                ser.dtr = False
                ser.rts = False"""

if old_open in code:
    code = code.replace(old_open, new_open, 1)
    print("Fixed DTR/RTS order in open() successfully!")
else:
    print("Warning: old_open not found!")

with open('gsheets_live_bridge.py', 'w', encoding='utf-8') as f:
    f.write(code)
print("Updated gsheets_live_bridge.py!")
