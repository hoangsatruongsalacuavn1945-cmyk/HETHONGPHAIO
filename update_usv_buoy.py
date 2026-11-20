import os

with open('src/usv_buoy/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Add lastGpsUartMs
s_target1 = 'unsigned long lastGpsValidMs = 0;'
s_repl1 = 'unsigned long lastGpsValidMs = 0;\nunsigned long lastGpsUartMs  = 0;'
assert s_target1 in content, 'Target 1 not found'
content = content.replace(s_target1, s_repl1, 1)

# 2. Add autoDetectAndInitCompass before updateHardwareSelfDiagnosis
compass_func = """void autoDetectAndInitCompass() {
  if (digitalRead(I2C_SDA_PIN) == LOW || digitalRead(I2C_SCL_PIN) == LOW) {
    recoverI2CBus9Clocks(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    Wire.setTimeOut(20);
  }
  // 1. Thu do QMC5883L (0x0D)
  Wire.beginTransmission(0x0D);
  if (Wire.endTransmission(true) == 0) {
    gy271Addr = 0x0D;
    Wire.beginTransmission(0x0D); Wire.write(0x0B); Wire.write(0x01); Wire.endTransmission(true);
    Wire.beginTransmission(0x0D); Wire.write(0x09); Wire.write(0x1D);
    if (Wire.endTransmission(true) == 0) {
      hasGY271 = true;
      Serial.println(F("[USV-COMPASS] Phat hien va khoi tao thanh cong QMC5883L (0x0D)!"));
      return;
    }
  }
  // 2. Thu do HMC5883L (0x1E)
  Wire.beginTransmission(0x1E);
  if (Wire.endTransmission(true) == 0) {
    gy271Addr = 0x1E;
    Wire.beginTransmission(0x1E); Wire.write(0x00); Wire.write(0x70); Wire.endTransmission(true);
    Wire.beginTransmission(0x1E); Wire.write(0x01); Wire.write(0xA0); Wire.endTransmission(true);
    Wire.beginTransmission(0x1E); Wire.write(0x02); Wire.write(0x00);
    if (Wire.endTransmission(true) == 0) {
      hasGY271 = true;
      Serial.println(F("[USV-COMPASS] Phat hien va khoi tao thanh cong HMC5883L (0x1E)!"));
      return;
    }
  }
}

"""

old_scan_compass = """  // Tu dong quet ca 2 loai chip La ban pho bien: QMC5883L (0x0D) & HMC5883L (0x1E)
  Wire.beginTransmission(0x0D);
  if (Wire.endTransmission(true) == 0) {
    gy271Addr = 0x0D;
    Wire.beginTransmission(0x0D);
    Wire.write(0x09);
    Wire.write(0x1D);
    if (Wire.endTransmission(true) == 0) {
      hasGY271 = true;
    }
  } else {
    Wire.beginTransmission(0x1E);
    if (Wire.endTransmission(true) == 0) {
      gy271Addr = 0x1E;
      Wire.beginTransmission(0x1E);
      Wire.write(0x02);
      Wire.write(0x00);
      if (Wire.endTransmission(true) == 0) {
        hasGY271 = true;
      }
    }
  }"""

assert old_scan_compass in content, 'Target 2 not found'
content = content.replace(old_scan_compass, '  autoDetectAndInitCompass();', 1)
content = content.replace('void updateHardwareSelfDiagnosis() {', compass_func + 'void updateHardwareSelfDiagnosis() {', 1)

# 3. Fix ADC read threshold in readAdcWithFloatingCheck
s_target3 = '} else if (natMv >= 3180.0f || raw_hz >= 4095) {'
s_repl3 = '} else if (natMv >= 3050.0f || raw_hz >= 4030) {'
assert s_target3 in content, 'Target 3 not found'
content = content.replace(s_target3, s_repl3, 1)

# 4. Fix pH and Battery logic in updateAllSensors
old_ph_code = """  // 2. Cảm biến pH (GPIO 7) — Kiểm tra phần cứng thực tế (bỏ phiếu 10 mẫu có trễ), nếu chưa cắm/vượt dải -> phValid = false, filtPH = 0.0
  float mvPH = 0.0f;
  phValid = updateAdcPresence(diagPH, readAdcWithFloatingCheck(PIN_PH, mvPH, &diagPH));
  float filtPH = 0.0f;
  if (phValid) {
    if (mvPH > 0.0f) {
      float vPH   = mvPH * 0.001f;
      float rawPH = constrain(7.0f + ((2.50f - vPH) / 0.18f) + phCalibOffset, 0.0f, 14.0f);
      filtPH      = kalmanPH.update(rawPH);
    } else {
      filtPH      = valPH;
    }
  } else {
    kalmanPH.init = false;
    filtPH = 0.0f;
  }"""

new_ph_code = """  // 2. Cảm biến pH (GPIO 7) — Kiểm tra phần cứng thực tế & bảo vệ vượt dải/kịch trần
  float mvPH = 0.0f;
  bool phReadOk = readAdcWithFloatingCheck(PIN_PH, mvPH, &diagPH);
  // CẢNH BÁO: Nếu chân pH bị kịch trần ADC (> 3050mV) hoặc hở mạch -> Báo SENSOR_FAULT, KHÔNG tính ra pH 3.76 gây báo động giả!
  if (diagPH.fault == ADC_SATURATED || diagPH.fault == ADC_OPEN || mvPH >= 3050.0f || mvPH < 150.0f) {
    phValid = false;
    diagPH.fault = (mvPH >= 3050.0f) ? ADC_SATURATED : ADC_OPEN;
  } else {
    phValid = updateAdcPresence(diagPH, phReadOk);
  }
  float filtPH = 0.0f;
  if (phValid && mvPH >= 150.0f && mvPH < 3050.0f) {
    float vPH   = mvPH * 0.001f;
    float rawPH = constrain(7.0f + ((2.50f - vPH) / 0.18f) + phCalibOffset, 0.0f, 14.0f);
    filtPH      = kalmanPH.update(rawPH);
  } else {
    kalmanPH.init = false;
    filtPH = 0.0f;
  }"""

assert old_ph_code in content, 'Target 4 (pH) not found'
content = content.replace(old_ph_code, new_ph_code, 1)

# 5. Fix Battery clamping in updateAllSensors
old_bat_code = """  float filtBat = 0.0f;
  if (batValid) {
    lastRawBatPinV = mvBat * 0.001f;
    float rawBat   = lastRawBatPinV * batDividerRatio;
    filtBat        = kalmanBat.update(rawBat);
  } else {"""

new_bat_code = """  float filtBat = 0.0f;
  if (batValid) {
    lastRawBatPinV = mvBat * 0.001f;
    float rawBat   = lastRawBatPinV * batDividerRatio;
    // Pin LiPo 3S kịch trần là 12.60V (3 cell x 4.20V), kẹp trần tối đa không để vọt lên 13.26V
    if (rawBat > 12.60f) {
      rawBat = 12.60f;
    }
    filtBat        = kalmanBat.update(rawBat);
  } else {"""

assert old_bat_code in content, 'Target 5 (BAT) not found'
content = content.replace(old_bat_code, new_bat_code, 1)

# 6. Fix Compass and GPS in updateAllSensors
old_compass_and_gps = """  // 9. Đọc La bàn số GY-271 thực tế (Tu dong ho tro ca QMC5883L 0x0D va HMC5883L 0x1E)
  if (!hasGY271) {
    Wire.beginTransmission(0x0D);
    if (Wire.endTransmission(true) == 0) {
      gy271Addr = 0x0D;
      Wire.beginTransmission(0x0D); Wire.write(0x09); Wire.write(0x1D); Wire.endTransmission(true);
      hasGY271 = true;
    } else {
      Wire.beginTransmission(0x1E);
      if (Wire.endTransmission(true) == 0) {
        gy271Addr = 0x1E;
        Wire.beginTransmission(0x1E); Wire.write(0x02); Wire.write(0x00); Wire.endTransmission(true);
        hasGY271 = true;
      }
    }
  }
  if (hasGY271) {
    Wire.beginTransmission(gy271Addr);
    if (gy271Addr == 0x0D) {
      Wire.write(0x00);
      if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)0x0D, (size_t)6, true) == 6) {
        int16_t mx = Wire.read() | (Wire.read() << 8);
        int16_t my = Wire.read() | (Wire.read() << 8);
        int16_t mz = Wire.read() | (Wire.read() << 8);
        (void)mz;
        float h = atan2((float)my, (float)mx) * 57.29578f;
        if (h < 0) h += 360.0f;
        valHeading = h;
      }
    } else {
      Wire.write(0x03);
      if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)0x1E, (size_t)6, true) == 6) {
        int16_t mx = (Wire.read() << 8) | Wire.read();
        int16_t mz = (Wire.read() << 8) | Wire.read();
        int16_t my = (Wire.read() << 8) | Wire.read();
        (void)mz;
        float h = atan2((float)my, (float)mx) * 57.29578f;
        if (h < 0) h += 360.0f;
        valHeading = h;
      }
    }
  }

  // 10. Đọc GPS NEO-6M tại UART1 (GPIO 17, 18) thực tế (Tuyệt đối không báo tọa độ giả!)
  int maxGpsBytes = 64; while (Serial1.available() > 0 && maxGpsBytes-- > 0) {
    if (gps.encode(Serial1.read())) {
      lastGpsValidMs = now;
      if (gps.location.isValid() && gps.location.age() < 3000) {
        portENTER_CRITICAL(&dataMux);
        gpsLat = gps.location.lat();
        gpsLng = gps.location.lng();
        gpsFixValid = true;
        lastGpsFixMs = now;
        portEXIT_CRITICAL(&dataMux);
      }
      gpsSpeedKmph = gps.speed.isValid() ? gps.speed.kmph() : 0.0f;
      gpsAltM      = gps.altitude.isValid() ? gps.altitude.meters() : 0.0f;
      gpsSats      = gps.satellites.isValid() ? (uint8_t)constrain(gps.satellites.value(), 0UL, 99UL) : 0;
    }
  }
  if (gps.charsProcessed() > lastGpsChars) {
    lastGpsChars   = gps.charsProcessed();
    lastGpsValidMs = now;
    hasGPS         = true;
  } else if (now - lastGpsValidMs > 5000) {
    hasGPS = false;
  }
  // Mất tín hiệu vệ tinh quá 5 giây -> lập tức hủy cờ GPS FIX thật sự, tránh lái theo tọa độ cũ
  if (now - lastGpsFixMs > 5000) {
    portENTER_CRITICAL(&dataMux);
    gpsFixValid = false;
    portEXIT_CRITICAL(&dataMux);
  }

  // TinyGPS++ giu co isValid() cua cau cuoi cung, nen phai tu het han FIX theo thoi gian.
  // Neu qua 5s khong co toa do moi -> ha co GPS_FIX de Autopilot/Home khong dung toa do cu.
  if (gpsFixValid && (now - lastGpsFixMs > 5000)) {
    portENTER_CRITICAL(&dataMux);
    gpsFixValid = false;
    portEXIT_CRITICAL(&dataMux);
    Serial.println("[USV-GPS] Mat FIX qua 5s -> ha co GPS_FIX (khong dung toa do cu).");
  }"""

new_compass_and_gps = """  // 9. Đọc La bàn số GY-271 thực tế (Tu dong ho tro ca QMC5883L 0x0D va HMC5883L 0x1E)
  static unsigned long lastCompassCheckMs = 0;
  if (!hasGY271 && (now - lastCompassCheckMs >= 2000)) {
    lastCompassCheckMs = now;
    autoDetectAndInitCompass();
  }
  if (hasGY271) {
    Wire.beginTransmission(gy271Addr);
    if (gy271Addr == 0x0D) {
      Wire.write(0x00);
      if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)0x0D, (size_t)6, true) == 6) {
        int16_t mx = Wire.read() | (Wire.read() << 8);
        int16_t my = Wire.read() | (Wire.read() << 8);
        int16_t mz = Wire.read() | (Wire.read() << 8);
        (void)mz;
        float h = atan2((float)my, (float)mx) * 57.29578f;
        if (h < 0) h += 360.0f;
        valHeading = h;
      }
    } else {
      Wire.write(0x03);
      if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)0x1E, (size_t)6, true) == 6) {
        int16_t mx = (Wire.read() << 8) | Wire.read();
        int16_t mz = (Wire.read() << 8) | Wire.read();
        int16_t my = (Wire.read() << 8) | Wire.read();
        (void)mz;
        float h = atan2((float)my, (float)mx) * 57.29578f;
        if (h < 0) h += 360.0f;
        valHeading = h;
      }
    }
  }

  // 10. Đọc GPS NEO-6M tại UART1 (GPIO 17, 18) thực tế (Phân biệt rõ: Giao tiếp UART OK vs Có FIX vệ tinh)
  int maxGpsBytes = 256;
  while (Serial1.available() > 0 && maxGpsBytes-- > 0) {
    char c = (char)Serial1.read();
    lastGpsUartMs = now;
    hasGPS        = true; // Nhận được byte NMEA từ module -> Phần cứng GPS kết nối bình thường!
    if (gps.encode(c)) {
      lastGpsValidMs = now;
      if (gps.location.isValid() && gps.location.age() < 4000 && gps.satellites.value() >= 4) {
        portENTER_CRITICAL(&dataMux);
        gpsLat = gps.location.lat();
        gpsLng = gps.location.lng();
        gpsFixValid = true;
        lastGpsFixMs = now;
        portEXIT_CRITICAL(&dataMux);
      }
      gpsSpeedKmph = gps.speed.isValid() ? gps.speed.kmph() : 0.0f;
      gpsAltM      = gps.altitude.isValid() ? gps.altitude.meters() : 0.0f;
      gpsSats      = gps.satellites.isValid() ? (uint8_t)constrain(gps.satellites.value(), 0UL, 99UL) : 0;
    }
  }

  // Quá 5 giây không nhận được byte nào từ UART1 -> Coi như đứt dây/hỏng module
  if (now - lastGpsUartMs > 5000) {
    hasGPS = false;
  }

  // Mất tín hiệu vệ tinh quá 5 giây (hoặc đang test trong phòng kín):
  // Hủy cờ gpsFixValid để không điều khiển Autopilot theo tọa độ cũ, nhưng VẪN GIỮ hasGPS = true!
  if (gpsFixValid && (now - lastGpsFixMs > 5000)) {
    portENTER_CRITICAL(&dataMux);
    gpsFixValid = false;
    portEXIT_CRITICAL(&dataMux);
  }"""

assert old_compass_and_gps in content, 'Target 6 (Compass & GPS) not found'
content = content.replace(old_compass_and_gps, new_compass_and_gps, 1)

# 7. Fix SIM response keywords
s_sim_rx = 'if (strstr(simRxLine, "OK") != nullptr || strstr(simRxLine, "+CSQ:") != nullptr) {'
s_sim_rx_new = 'if (strstr(simRxLine, "OK") != nullptr || strstr(simRxLine, "+CSQ:") != nullptr || strstr(simRxLine, "RDY") != nullptr || strstr(simRxLine, "+CPIN") != nullptr || strstr(simRxLine, "Ready") != nullptr || strstr(simRxLine, "+NETOPEN") != nullptr) {'
assert s_sim_rx in content, 'Target 7 (SIM RX) not found'
content = content.replace(s_sim_rx, s_sim_rx_new, 1)

# 8. Fix SIM periodic ping and retry in loop
old_sim_loop = """  if (!simNetOpen && simInitStep < 8) {
    if (now - simInitTick >= 350) {
      simInitTick = now;
      switch (simInitStep) {
        case 0: Serial2.println("ATE0"); break;
        case 1: Serial2.println("AT+CPIN?"); break;
        case 2: Serial2.println("AT+CSQ"); break;
        case 3: Serial2.println("AT+CGATT?"); break;
        case 4: Serial2.printf("AT+CGDCONT=1,\\"IP\\",\\"%s\\"\\r\\n", SIM_APN); break;
        case 5: Serial2.println("AT+NETOPEN"); break;
        case 6: Serial2.println("AT+HTTPINIT"); break;
        case 7:
          simNetOpen = true;
          Serial.printf("[SIM4G] Da cau hinh APN '%s' va mo data. San sang day Cloud.\\n", SIM_APN);
          break;
      }
      simInitStep++;
    }
  }

  if (simGsState == SIM_GS_IDLE && (now - lastSimPingMs >= 10000)) {
    lastSimPingMs = now;
    Serial2.println("AT");
  }"""

new_sim_loop = """  if (!simModemOnline) {
    if (now - simInitTick >= 3000) {
      simInitTick = now;
      Serial2.println("AT");
      if (simInitStep < 8) {
        switch (simInitStep) {
          case 0: Serial2.println("ATE0"); break;
          case 1: Serial2.println("AT+CPIN?"); break;
          case 2: Serial2.println("AT+CSQ"); break;
          case 3: Serial2.println("AT+CGATT?"); break;
          case 4: Serial2.printf("AT+CGDCONT=1,\\"IP\\",\\"%s\\"\\r\\n", SIM_APN); break;
          case 5: Serial2.println("AT+NETOPEN"); break;
          case 6: Serial2.println("AT+HTTPINIT"); break;
          case 7:
            simNetOpen = true;
            Serial.printf("[SIM4G] Da cau hinh APN '%s' va mo data. San sang day Cloud.\\n", SIM_APN);
            break;
        }
        simInitStep++;
      } else {
        simInitStep = 0;
      }
    }
  } else {
    if (simGsState == SIM_GS_IDLE && (now - lastSimPingMs >= 10000)) {
      lastSimPingMs = now;
      Serial2.println("AT+CSQ");
    }
  }"""

assert old_sim_loop in content, 'Target 8 (SIM loop) not found'
content = content.replace(old_sim_loop, new_sim_loop, 1)

# 9. In setup(): add adcAttachPin and analogSetPinAttenuation
old_adc_setup = """  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Khởi tạo RTC Pull-Down trên toàn bộ 5 chân ADC1 (GPIO 3, 4, 5, 7, 8)"""

new_adc_setup = """  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  adcAttachPin(PIN_BAT_3S);        analogSetPinAttenuation(PIN_BAT_3S, ADC_11db);
  adcAttachPin(PIN_TDS);           analogSetPinAttenuation(PIN_TDS, ADC_11db);
  adcAttachPin(PIN_TURBIDITY);     analogSetPinAttenuation(PIN_TURBIDITY, ADC_11db);
  adcAttachPin(PIN_PH);            analogSetPinAttenuation(PIN_PH, ADC_11db);
  adcAttachPin(PIN_ACS712);        analogSetPinAttenuation(PIN_ACS712, ADC_11db);

  // Khởi tạo RTC Pull-Down trên toàn bộ 5 chân ADC1 (GPIO 3, 4, 5, 7, 8)"""

assert old_adc_setup in content, 'Target 9 (ADC setup) not found'
content = content.replace(old_adc_setup, new_adc_setup, 1)

with open('src/usv_buoy/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('Update usv_buoy/main.cpp SUCCESS!')
