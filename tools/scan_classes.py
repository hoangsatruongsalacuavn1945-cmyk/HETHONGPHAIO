import re
with open('src/ground_station/web_3d_map.h', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

classes = set()
for m in re.finditer(r'class="([^"]+)"', text):
    for c in m.group(1).split():
        classes.add(c)

print(f"Total unique classes used: {len(classes)}")
print(sorted(list(classes))[:60])
