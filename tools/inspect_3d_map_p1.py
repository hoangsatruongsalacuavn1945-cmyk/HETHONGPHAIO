import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

with open('src/ground_station/web_3d_map.h', 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

print("Total lines:", len(lines))
for i in range(min(150, len(lines))):
    print(f"{i+1:3d}: {lines[i].rstrip()}")
