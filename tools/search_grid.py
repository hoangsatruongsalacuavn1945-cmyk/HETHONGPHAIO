with open('temp_3d_script.js', 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if 'gridIndices' in line or 'gridVbo' in line:
        print(f"{i+1:3d}: {line.strip()[:85]}")
