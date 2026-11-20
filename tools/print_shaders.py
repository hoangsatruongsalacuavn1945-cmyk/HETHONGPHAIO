import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

with open('test_3d.html', 'r', encoding='utf-8') as f:
    text = f.read()

# find where gl is initialized
pos = text.find("const canvas = document.getElementById('glCanvas');")
lines = text[pos:].split('\n')
for i in range(min(100, len(lines))):
    print(f"{i+1:3d}: {lines[i]}")
