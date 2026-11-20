with open('src/ground_station/web_3d_map.h', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

s_marker = 'static const char WEB_3D_MAP_HTML[] PROGMEM = R"rawliteral('
e_marker = ')rawliteral";'

p1 = text.find(s_marker)
if p1 != -1:
    p1 += len(s_marker)
    p2 = text.find(e_marker, p1)
    html = text[p1:p2]
    with open('test_3d.html', 'w', encoding='utf-8') as out:
        out.write(html)
    print(f"Saved test_3d.html ({len(html)} bytes)")
else:
    print("Marker not found")
