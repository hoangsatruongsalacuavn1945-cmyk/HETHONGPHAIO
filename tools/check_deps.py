import re
import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

for fname in ['temp_web.html', 'src/ground_station/web_3d_map.h']:
    with open(fname, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    print(f"=== File: {fname} ({len(content)} bytes) ===")
    scripts = re.findall(r'<script[^>]*src=[\'"]([^\'"]+)[\'"]', content, re.IGNORECASE)
    links = re.findall(r'<link[^>]*href=[\'"]([^\'"]+)[\'"]', content, re.IGNORECASE)
    print("External Scripts:", scripts)
    print("External CSS Links:", links)
