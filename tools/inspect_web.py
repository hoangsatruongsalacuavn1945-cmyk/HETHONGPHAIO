import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

with open('temp_web.html', 'r', encoding='utf-8') as f:
    lines = f.readlines()

print('Total lines in web dashboard:', len(lines))
for i, line in enumerate(lines):
    l_str = line.strip()
    if any(tag in l_str for tag in ['<title>', '<style>', '<header', '<nav', 'class="tab', '<canvas', '<script', 'function update', 'function fetch', 'function draw']):
        print(f'{i+1}: {l_str[:85]}')
