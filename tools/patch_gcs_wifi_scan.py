with open('src/ground_station/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Add /api/wifi_scan endpoint
old_cfg_endpoint = """  webServer.on("/api/config", HTTP_GET, []() {"""

new_cfg_endpoint = """  webServer.on("/api/wifi_scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks(false, false);
    String json = "{\\"ok\\":true,\\"networks\\":[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\\"ssid\\":\\"";
      json += WiFi.SSID(i);
      json += "\\",\\"rssi\\":";
      json += WiFi.RSSI(i);
      json += ",\\"enc\\":";
      json += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "true" : "false";
      json += "}";
    }
    json += "]}";
    WiFi.scanDelete();
    webServer.send(200, "application/json", json);
  });

  webServer.on("/api/config", HTTP_GET, []() {"""

if old_cfg_endpoint in code:
    code = code.replace(old_cfg_endpoint, new_cfg_endpoint, 1)
    print("Added /api/wifi_scan endpoint successfully!")
else:
    print("Warning: old_cfg_endpoint not found!")

# 2. Add Wi-Fi scanner UI in Web Dashboard
old_wifi_ui = """      <div>
        <label style="font-size:0.75rem;color:var(--cyan);font-weight:700">Tên Wi-Fi Trạm Bờ kết nối (SSID)</label>
        <input id="cfg_ssid" placeholder="VD: NhaToi_2.4G">
        <label style="font-size:0.75rem;color:var(--cyan);font-weight:700;margin-top:6px;display:block">Mật khẩu Wi-Fi</label>
        <input id="cfg_pass" type="password" placeholder="Mật khẩu Wi-Fi">
        <button onclick="saveCfg()" class="btn-blue" style="width:100%;margin-top:6px">LƯU KẾT NỐI WI-FI</button>
      </div>"""

new_wifi_ui = """      <div>
        <div style="display:flex;justify-content:space-between;align-items:center">
          <label style="font-size:0.75rem;color:var(--cyan);font-weight:700">Tên Wi-Fi Trạm Bờ kết nối (SSID)</label>
          <button onclick="scanWifi()" id="btn_scan_wifi" style="padding:2px 8px;font-size:0.7rem;background:var(--blue)">🔍 QUÉT WI-FI</button>
        </div>
        <select id="wifi_select" onchange="onSelectWifi(this.value)" style="display:none;width:100%;margin-top:4px;padding:8px 10px;border-radius:6px;border:1px solid rgba(0,245,212,0.3);background:#070d1e;color:#fff;font-size:0.82rem;font-family:monospace"></select>
        <input id="cfg_ssid" placeholder="VD: NhaToi_2.4G">
        <label style="font-size:0.75rem;color:var(--cyan);font-weight:700;margin-top:6px;display:block">Mật khẩu Wi-Fi</label>
        <input id="cfg_pass" type="password" placeholder="Mật khẩu Wi-Fi">
        <button onclick="saveCfg()" class="btn-blue" style="width:100%;margin-top:6px">LƯU KẾT NỐI WI-FI</button>
      </div>"""

if old_wifi_ui in code:
    code = code.replace(old_wifi_ui, new_wifi_ui, 1)
    print("Added Wi-Fi scanner UI successfully!")
else:
    print("Warning: old_wifi_ui not found!")

# 3. Add JS functions scanWifi and onSelectWifi
old_js_hook = """function saveCfg(){"""

new_js_hook = """async function scanWifi(){
  const btn = document.getElementById('btn_scan_wifi');
  if(!btn) return;
  btn.innerText = '⏳ Đang quét...';
  try {
    const res = await fetch('/api/wifi_scan');
    const d = await res.json();
    if (d.ok && d.networks && d.networks.length > 0) {
      const sel = document.getElementById('wifi_select');
      sel.innerHTML = '<option value="">-- Chọn mạng (' + d.networks.length + ' Wi-Fi) --</option>';
      d.networks.forEach(net => {
        if (!net.ssid) return;
        const opt = document.createElement('option');
        opt.value = net.ssid;
        opt.innerText = `${net.ssid} (${net.rssi} dBm${net.enc ? ' 🔒' : ''})`;
        sel.appendChild(opt);
      });
      sel.style.display = 'block';
      btn.innerText = '✅ Xong!';
    } else {
      btn.innerText = '❌ Không thấy';
    }
  } catch(e) {
    btn.innerText = '❌ Lỗi quét';
  }
  setTimeout(() => { if(btn) btn.innerText = '🔍 QUÉT WI-FI'; }, 3000);
}
function onSelectWifi(val){
  if(val){
    document.getElementById('cfg_ssid').value = val;
    document.getElementById('cfg_pass').focus();
  }
}

function saveCfg(){"""

if old_js_hook in code:
    code = code.replace(old_js_hook, new_js_hook, 1)
    print("Added scanWifi JS functions successfully!")
else:
    print("Warning: old_js_hook not found!")

# 4. Enhance googleSheetsWifiTask with periodic Discord reports and instant alert push
old_wifi_task = """void googleSheetsWifiTask(void* pvParameters) {
  (void)pvParameters;
  for (;;) {
    if (reqPushDiscordTest && (WiFi.status() == WL_CONNECTED)) {
      sendDiscordWebhookFromGCS(false, false, "THỬ NGHIỆM KẾT NỐI DISCORD TỪ TRẠM BỜ GCS THÀNH CÔNG!");
      reqPushDiscordTest = false;
    }"""

new_wifi_task = """void googleSheetsWifiTask(void* pvParameters) {
  (void)pvParameters;
  unsigned long lastAutoDiscordMs = 0;
  uint8_t lastAlertMask = 0;
  for (;;) {
    if (reqPushDiscordTest && (WiFi.status() == WL_CONNECTED)) {
      sendDiscordWebhookFromGCS(false, false, "THỬ NGHIỆM KẾT NỐI DISCORD TỪ TRẠM BỜ GCS THÀNH CÔNG!");
      reqPushDiscordTest = false;
    }

    // Tự động đẩy Discord cảnh báo tức thời khi có sự cố tại hiện trường
    if ((WiFi.status() == WL_CONNECTED) && strlen(discordWebhookUrl) > 30) {
      unsigned long nowMs = millis();
      if (uAlertFlags != 0 && uAlertFlags != lastAlertMask) {
        lastAlertMask = uAlertFlags;
        sendDiscordWebhookFromGCS(false, true, nullptr);
      } else if (uAlertFlags == 0) {
        lastAlertMask = 0;
      }
      // Gửi định kỳ mỗi 60 giây một bản tin giám sát tổng hợp
      if (nowMs - lastAutoDiscordMs >= 60000UL) {
        lastAutoDiscordMs = nowMs;
        sendDiscordWebhookFromGCS(false, false, nullptr);
      }
    }"""

if old_wifi_task in code:
    code = code.replace(old_wifi_task, new_wifi_task, 1)
    print("Enhanced googleSheetsWifiTask successfully!")
else:
    print("Warning: old_wifi_task not found!")

with open('src/ground_station/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)
print("Finished patching src/ground_station/main.cpp!")
