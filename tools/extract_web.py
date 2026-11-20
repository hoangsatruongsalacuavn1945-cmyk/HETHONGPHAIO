import re

with open('src/ground_station/main.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

marker1 = 'const char WEB_DASHBOARD_HTML[] PROGMEM = R"rawliteral('
marker2 = ')rawliteral";'

p1 = text.find(marker1)
if p1 != -1:
    p1 += len(marker1)
    p2 = text.find(marker2, p1)
    html = text[p1:p2]
    with open('temp_web.html', 'w', encoding='utf-8') as out:
        out.write(html)
    print(f"Extracted {len(html)} bytes to temp_web.html")
else:
    print("Marker not found")
