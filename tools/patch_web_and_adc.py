import os
import sys

def patch_gcs():
    path = os.path.join("src", "ground_station", "main.cpp")
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # 1. Include DNSServer
    if "#include <DNSServer.h>" not in content:
        content = content.replace(
            "#include <WebServer.h>",
            "#include <WebServer.h>\n#include <DNSServer.h>"
        )

    # 2. Declare dnsServer
    if "DNSServer dnsServer;" not in content:
        content = content.replace(
            "WebServer webServer(80);",
            "WebServer webServer(80);\nDNSServer dnsServer;"
        )

    # 3. SoftAPConfig & dnsServer.start
    old_setup = """  if (strlen(staWifiSsid) > 0) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(GCS_WIFI_SSID, GCS_WIFI_PASS);
    WiFi.begin(staWifiSsid, staWifiPass);
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(GCS_WIFI_SSID, GCS_WIFI_PASS);
  }"""
    
    new_setup = """  IPAddress apIP(192, 168, 4, 1);
  IPAddress apNetmask(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, apNetmask);
  if (strlen(staWifiSsid) > 0) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(GCS_WIFI_SSID, GCS_WIFI_PASS);
    WiFi.begin(staWifiSsid, staWifiPass);
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(GCS_WIFI_SSID, GCS_WIFI_PASS);
  }
  dnsServer.start(53, "*", apIP);"""

    if "dnsServer.start" not in content and old_setup in content:
        content = content.replace(old_setup, new_setup)

    # 4. Captive Portal handlers before webServer.begin()
    captive_routes = """  webServer.on("/generate_204", HTTP_GET, []() {
    webServer.sendHeader("Location", "http://192.168.4.1/", true);
    webServer.send(302, "text/plain", "");
  });
  webServer.on("/hotspot-detect.html", HTTP_GET, []() {
    webServer.sendHeader("Location", "http://192.168.4.1/", true);
    webServer.send(302, "text/plain", "");
  });
  webServer.on("/canonical.html", HTTP_GET, []() {
    webServer.sendHeader("Location", "http://192.168.4.1/", true);
    webServer.send(302, "text/plain", "");
  });
  webServer.on("/ncsi.txt", HTTP_GET, []() {
    webServer.send(200, "text/plain", "Microsoft NCSI");
  });
  webServer.onNotFound([]() {
    String host = webServer.hostHeader();
    if (!host.startsWith("192.168.4.1") && !host.startsWith("chemstation.local")) {
      webServer.sendHeader("Location", "http://192.168.4.1/", true);
      webServer.send(302, "text/plain", "");
      return;
    }
    webServer.send(404, "text/plain", "Not Found");
  });

  webServer.begin();"""

    if "/generate_204" not in content and "webServer.begin();" in content:
        content = content.replace("  webServer.begin();", captive_routes)

    # 5. loop dnsServer.processNextRequest
    old_loop = """void loop() {
  esp_task_wdt_reset();
  webServer.handleClient();"""

    new_loop = """void loop() {
  esp_task_wdt_reset();
  dnsServer.processNextRequest();
  webServer.handleClient();"""

    if "dnsServer.processNextRequest();" not in content and old_loop in content:
        content = content.replace(old_loop, new_loop)

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print("Patched ground_station/main.cpp successfully!")

def patch_buoy():
    path = os.path.join("src", "usv_buoy", "main.cpp")
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # 1. Fix pulldown/pullup in checkAdcChannel
    old_check = """  if (natMv < 90.0f) {
    // Điện áp sát 0V: Có thể là cảm biến TDS/NTU để khô HOẶC là chân hở mạch trôi về 0V
    // Thử kéo nhẹ bằng Pull-Up RTC 45kOhm trong 60us
    rtc_gpio_pullup_en(gpin);
    delayMicroseconds(60);
    uint32_t v_pu = analogReadMilliVolts(pin);
    rtc_gpio_pullup_dis(gpin); // Ngắt ngay lập tức

    probeSwing = (v_pu > natMv) ? (uint16_t)(v_pu - natMv) : 0;
    // Chân hở mạch sẽ vọt lên >1800mV do không có tải kéo xuống.
    // Cảm biến thật (Op-amp TDS/NTU) có trở kháng ra thấp sẽ ghìm điện áp xuống < 450mV.
    if (v_pu >= 1800) {
      isConnected = false;
      fault = ADC_OPEN;
    }
  } else if (natMv >= 3050.0f || raw_hz >= 4030) {
    // Điện áp kẹp trần: Có thể là cảm biến pH ngoài không khí / Pin đầy HOẶC chân hở mạch tích điện 3.3V
    // Thử xả nhẹ bằng Pull-Down RTC 45kOhm trong 60us
    rtc_gpio_pulldown_en(gpin);
    delayMicroseconds(60);
    uint32_t v_pd = analogReadMilliVolts(pin);
    rtc_gpio_pulldown_dis(gpin); // Ngắt ngay lập tức

    probeSwing = (natMv > v_pd) ? (uint16_t)(natMv - v_pd) : 0;
    // Chân hở mạch sẽ bị xả sạch về < 300mV.
    // Cảm biến thật (pH Op-amp / cầu phân áp Pin) giữ điện áp > 1800mV.
    if (v_pd < 300) {
      isConnected = false;
      fault = ADC_OPEN;
    } else {
      fault = ADC_SATURATED;
    }
  }"""

    new_check = """  if (natMv < 90.0f) {
    // Điện áp sát 0V: Có thể là cảm biến TDS/NTU để khô HOẶC là chân hở mạch trôi về 0V
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(100);
    uint32_t v_pu = analogReadMilliVolts(pin);
    pinMode(pin, INPUT);
    adcAttachPin(pin);

    probeSwing = (v_pu > natMv) ? (uint16_t)(v_pu - natMv) : 0;
    if (v_pu >= 1800) {
      isConnected = false;
      fault = ADC_OPEN;
    }
  } else if (natMv >= 3050.0f || raw_hz >= 4030) {
    // Điện áp kẹp trần: Có thể là chân hở mạch tích điện 3.3V hoặc cảm biến bão hòa
    pinMode(pin, INPUT_PULLDOWN);
    delayMicroseconds(100);
    uint32_t v_pd = analogReadMilliVolts(pin);
    pinMode(pin, INPUT);
    adcAttachPin(pin);

    probeSwing = (natMv > v_pd) ? (uint16_t)(natMv - v_pd) : 0;
    if (v_pd < 600) {
      isConnected = false;
      fault = ADC_OPEN;
    } else {
      fault = ADC_SATURATED;
    }
  }"""

    if old_check in content:
        content = content.replace(old_check, new_check)

    # 2. Fix pH fault classification
    old_ph = """  if (diagPH.fault == ADC_SATURATED || diagPH.fault == ADC_OPEN || mvPH >= 3050.0f || mvPH < 150.0f) {
    phValid = false;
    diagPH.fault = (mvPH >= 3050.0f) ? ADC_SATURATED : ADC_OPEN;
  } else {
    phValid = updateAdcPresence(diagPH, phReadOk);
  }"""

    new_ph = """  if (diagPH.fault == ADC_SATURATED || diagPH.fault == ADC_OPEN || mvPH >= 3050.0f || mvPH < 150.0f) {
    phValid = false;
    diagPH.fault = (mvPH >= 3050.0f && phReadOk) ? ADC_SATURATED : ADC_OPEN;
  } else {
    phValid = updateAdcPresence(diagPH, phReadOk);
  }"""

    if old_ph in content:
        content = content.replace(old_ph, new_ph)

    # 3. Only set adcFaultMask if the sensor is actually connected
    old_af = """  // Mặt nạ kênh ADC lỗi bão hòa kẹp trần (raw >= 4050 hoặc >3.05V)
  uint16_t afMask = 0;
  if (diagPH.fault == ADC_SATURATED)  afMask |= HW_OK_PH;
  if (diagTDS.fault == ADC_SATURATED) afMask |= HW_OK_TDS;
  if (diagNTU.fault == ADC_SATURATED) afMask |= HW_OK_NTU;
  if (diagACS.fault == ADC_SATURATED) afMask |= HW_OK_ACS712;
  if (diagBAT.fault == ADC_SATURATED) afMask |= HW_OK_BAT3S;
  adcFaultMask = afMask;"""

    new_af = """  // Mặt nạ kênh ADC lỗi bão hòa kẹp trần (CHỈ KHI CẢM BIẾN ĐANG CẮM mà bị bão hòa)
  uint16_t afMask = 0;
  if (phValid && diagPH.fault == ADC_SATURATED)  afMask |= HW_OK_PH;
  if (tdsValid && diagTDS.fault == ADC_SATURATED) afMask |= HW_OK_TDS;
  if (ntuValid && diagNTU.fault == ADC_SATURATED) afMask |= HW_OK_NTU;
  if (acs712Valid && diagACS.fault == ADC_SATURATED) afMask |= HW_OK_ACS712;
  if (batValid && diagBAT.fault == ADC_SATURATED) afMask |= HW_OK_BAT3S;
  adcFaultMask = afMask;"""

    if old_af in content:
        content = content.replace(old_af, new_af)

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print("Patched usv_buoy/main.cpp successfully!")

if __name__ == "__main__":
    patch_gcs()
    patch_buoy()
