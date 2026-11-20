with open('gsheets_live_bridge.py', 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

for i in range(830, min(846, len(lines))):
    print(f"{i+1}: {repr(lines[i].encode('ascii', 'replace').decode('ascii'))}")
