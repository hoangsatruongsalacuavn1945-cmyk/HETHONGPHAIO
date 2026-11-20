with open('temp_3d_script.js', 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

for i in range(398, 425):
    print(f"{i+1:3d}: {lines[i].rstrip()}")
