#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Patches:
  1. include/usv_common.h:
     - DEFAULT_STA_WIFI_SSID = "Mang bao tri"
     - DEFAULT_STA_WIFI_PASS = "mangnhabiloi"
     - GSHEETS_PUSH_INTERVAL_MS = 5000UL
  2. src/ground_station/main.cpp:
     - Force loadGcsConfig() to set & save "Mang bao tri" / "mangnhabiloi"
     - Log Wi-Fi STA connection IP in loop()
     - Continuously update gcsLayerSurf/Mid/Bot on every live USV packet
     - Speed up WEB_DASHBOARD_HTML polling from 600ms to 350ms
  3. src/usv_buoy/main.cpp:
     - Connect ESP32-S3 Buoy to Wi-Fi "Mang bao tri" / "mangnhabiloi" as well
     - Increase continuous telemetry streaming rate to 450ms nonstop
"""

import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

# --- 1. Patch include/usv_common.h ---
with open("include/usv_common.h", "r", encoding="utf-8") as f:
    common = f.read()

common = common.replace(
    '#define DEFAULT_STA_WIFI_SSID     ""',
    '#define DEFAULT_STA_WIFI_SSID     "Mang bao tri"'
)
common = common.replace(
    '#define DEFAULT_STA_WIFI_PASS     ""',
    '#define DEFAULT_STA_WIFI_PASS     "mangnhabiloi"'
)
common = common.replace(
    '#define GSHEETS_PUSH_INTERVAL_MS  20000UL // Chu kỳ tự động đẩy dữ liệu lên Google Sheets (20 giây)',
    '#define GSHEETS_PUSH_INTERVAL_MS  5000UL  // Chu kỳ tự động đẩy dữ liệu liên tục lên Cloud/Web (5 giây)'
)

with open("include/usv_common.h", "w", encoding="utf-8") as f:
    f.write(common)
print("[OK] Patched include/usv_common.h with Wi-Fi 'Mang bao tri' / 'mangnhabiloi'")

# --- 2. Patch src/ground_station/main.cpp ---
with open("src/ground_station/main.cpp", "r", encoding="utf-8") as f:
    gcs = f.read()

old_load_cfg = """  if (gcsPrefs.isKey("sta_ssid")) {
    String s = gcsPrefs.getString("sta_ssid", "");
    copyStringSafe(staWifiSsid, sizeof(staWifiSsid), s.c_str());
  }
  if (gcsPrefs.isKey("sta_pass")) {
    String p = gcsPrefs.getString("sta_pass", "");
    copyStringSafe(staWifiPass, sizeof(staWifiPass), p.c_str());
  }"""

new_load_cfg = """  if (gcsPrefs.isKey("sta_ssid")) {
    String s = gcsPrefs.getString("sta_ssid", DEFAULT_STA_WIFI_SSID);
    copyStringSafe(staWifiSsid, sizeof(staWifiSsid), s.c_str());
  }
  if (gcsPrefs.isKey("sta_pass")) {
    String p = gcsPrefs.getString("sta_pass", DEFAULT_STA_WIFI_PASS);
    copyStringSafe(staWifiPass, sizeof(staWifiPass), p.c_str());
  }
  // Đảm bảo luôn kết nối tới Wi-Fi "Mang bao tri" / "mangnhabiloi" theo yêu cầu
  if (strlen(staWifiSsid) == 0 || strcmp(staWifiSsid, DEFAULT_STA_WIFI_SSID) != 0) {
    copyStringSafe(staWifiSsid, sizeof(staWifiSsid), DEFAULT_STA_WIFI_SSID);
    copyStringSafe(staWifiPass, sizeof(staWifiPass), DEFAULT_STA_WIFI_PASS);
    gcsPrefs.putString("sta_ssid", staWifiSsid);
    gcsPrefs.putString("sta_pass", staWifiPass);
  }"""

if old_load_cfg in gcs:
    gcs = gcs.replace(old_load_cfg, new_load_cfg, 1)
    print("[OK] Updated loadGcsConfig() in src/ground_station/main.cpp")

old_layer_upd = """          if (uAutoStage == AUTO_STAGE_WAIT_SURF) {
            gcsLayerSurf = {uPH, uTDS, uNTU, uTemp, uDO, true};
          } else if (uAutoStage == AUTO_STAGE_WAIT_MID) {
            gcsLayerMid = {uPH, uTDS, uNTU, uTemp, uDO, true};
          } else if (uAutoStage == AUTO_STAGE_WAIT_BOT) {
            gcsLayerBot = {uPH, uTDS, uNTU, uTemp, uDO, true};
          }"""

new_layer_upd = """          if (uAutoStage == AUTO_STAGE_WAIT_SURF || (uAutoStage == AUTO_STAGE_OFF && uDepthM < 0.65f)) {
            gcsLayerSurf = {uPH, uTDS, uNTU, uTemp, uDO, true};
          } else if (uAutoStage == AUTO_STAGE_WAIT_MID || (uDepthM >= 0.65f && uDepthM < 1.40f)) {
            gcsLayerMid = {uPH, uTDS, uNTU, uTemp, uDO, true};
          } else if (uAutoStage == AUTO_STAGE_WAIT_BOT || (uDepthM >= 1.40f)) {
            gcsLayerBot = {uPH, uTDS, uNTU, uTemp, uDO, true};
          }"""

if old_layer_upd in gcs:
    gcs = gcs.replace(old_layer_upd, new_layer_upd, 1)
    print("[OK] Enabled continuous real-time layer updates in src/ground_station/main.cpp")

old_wifi_loop = """  // Tự kết nối lại Wi-Fi STA nếu bị rớt (mỗi 15s)
  if (now - lastWifiCheckMs >= GCS_WIFI_RECONNECT_MS) {"""

new_wifi_loop = """  // Giám sát trạng thái kết nối Wi-Fi STA "Mang bao tri" thời gian thực
  static bool prevStaConnected = false;
  bool curStaConnected = (WiFi.status() == WL_CONNECTED);
  if (curStaConnected != prevStaConnected) {
    prevStaConnected = curStaConnected;
    if (curStaConnected) {
      Serial.printf("[GCS-WIFI] >>> DA KET NOI WIFI '%s' THANH CONG! IP: http://%s (3D Map: http://%s/3d) <<<\\n",
                    staWifiSsid, WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str());
    }
  }

  // Tự kết nối lại Wi-Fi STA nếu bị rớt (mỗi 15s)
  if (now - lastWifiCheckMs >= GCS_WIFI_RECONNECT_MS) {"""

if "prevStaConnected" not in gcs and old_wifi_loop in gcs:
    gcs = gcs.replace(old_wifi_loop, new_wifi_loop, 1)
    print("[OK] Added Wi-Fi STA connection IP logger in src/ground_station/main.cpp")

gcs = gcs.replace("},600);\n</script>", "},350);\n</script>")

with open("src/ground_station/main.cpp", "w", encoding="utf-8") as f:
    f.write(gcs)

# --- 3. Patch src/usv_buoy/main.cpp ---
with open("src/usv_buoy/main.cpp", "r", encoding="utf-8") as f:
    buoy = f.read()

if "#include <WiFi.h>" not in buoy:
    buoy = buoy.replace("#include <SPI.h>", "#include <SPI.h>\n#include <WiFi.h>", 1)

old_buoy_setup_end = """  lastCmdRxTime   = millis();
  lastHzStatMs    = millis();
  lastDepthCalcMs = millis();
  triggerLedFlash(0, 50, 0, 600); // Xanh Lá 600ms: Khởi động thành công!"""

new_buoy_setup_end = """  // Kết nối Wi-Fi STA "Mang bao tri" trực tiếp trên ESP32-S3
  if (strlen(DEFAULT_STA_WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(DEFAULT_STA_WIFI_SSID, DEFAULT_STA_WIFI_PASS);
    Serial.printf("[USV-WIFI] Dang ket noi Wi-Fi '%s'...\\n", DEFAULT_STA_WIFI_SSID);
  }

  lastCmdRxTime   = millis();
  lastHzStatMs    = millis();
  lastDepthCalcMs = millis();
  triggerLedFlash(0, 50, 0, 600); // Xanh Lá 600ms: Khởi động thành công!"""

if "USV-WIFI" not in buoy and old_buoy_setup_end in buoy:
    buoy = buoy.replace(old_buoy_setup_end, new_buoy_setup_end, 1)
    print("[OK] Added Wi-Fi STA connection to ESP32-S3 Buoy setup()")

# Speed up continuous LoRa + Serial telemetry transmission from 1000ms to 450ms
buoy = buoy.replace(
    "(now - lastLoraTx >= 1000)) {",
    "(now - lastLoraTx >= 450)) {"
)

with open("src/usv_buoy/main.cpp", "w", encoding="utf-8") as f:
    f.write(buoy)
print("[OK] Patched src/usv_buoy/main.cpp for continuous 450ms telemetry & Wi-Fi 'Mang bao tri'")
