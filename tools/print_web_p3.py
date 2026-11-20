import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

with open('temp_web.html', 'r', encoding='utf-8') as f:
    lines = f.readlines()

for i in range(300, min(450, len(lines))):
    print(f"{i+1:3d}: {lines[i].rstrip()}")
