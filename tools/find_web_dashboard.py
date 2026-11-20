with open('src/ground_station/main.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

s_marker = 'const char WEB_DASHBOARD_HTML[] PROGMEM = R"rawliteral('
e_marker = ')rawliteral";'

p1 = text.find(s_marker)
p2 = text.find(e_marker, p1)
print(f"WEB_DASHBOARD_HTML: start={p1}, end={p2}, size={p2-p1} bytes")
