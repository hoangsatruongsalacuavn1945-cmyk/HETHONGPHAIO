import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

with open('temp_3d_script.js', 'r', encoding='utf-8') as f:
    lines = f.readlines()

for i in range(500, min(560, len(lines))):
    print(f"{i+1:3d}: {lines[i].rstrip()}")
