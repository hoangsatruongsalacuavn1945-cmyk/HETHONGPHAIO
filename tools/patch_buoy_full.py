import os

def patch_buoy_full():
    path = os.path.join("src", "usv_buoy", "main.cpp")
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # 1. Define PIN_CO2_TRIGGER and PIN_SIREN_BUZZER
    if "#define PIN_CO2_TRIGGER" not in content:
        content = content.replace(
            "#define PIN_SERVO_RUDDER  10",
            "#define PIN_SERVO_RUDDER  10  // Servo 2: Bánh lái MG90S\n#define PIN_CO2_TRIGGER   45  // Kích Rơ-le Solenoid bung Phao Cứu Sinh CO2 khi lật úp (>60° >3s)\n#define PIN_SIREN_BUZZER  46  // Còi hú cứu hộ khẩn cấp & cảnh báo cộng đồng"
        )

    # 2. PinMode in setup
    if "pinMode(PIN_CO2_TRIGGER, OUTPUT);" not in content:
        content = content.replace(
            "analogSetAttenuation(ADC_11db);",
            "analogSetAttenuation(ADC_11db);\n  pinMode(PIN_CO2_TRIGGER, OUTPUT);\n  digitalWrite(PIN_CO2_TRIGGER, LOW);\n  pinMode(PIN_SIREN_BUZZER, OUTPUT);\n  digitalWrite(PIN_SIREN_BUZZER, LOW);"
        )

    # 3. Upgrade Anti-Jam state machine to 1.5s 100% PWM pulse and siren alarm
    old_jam = """    case JAM_STOP_WAIT:
      if (now - jamTimerTick >= 200) {
        int revSpeed = (cmdMotor >= 0) ? -180 : 180;
        driveMotor(revSpeed);
        jamState = JAM_REVERSE_CLEARING;
        jamTimerTick = now;
      }
      break;

    case JAM_REVERSE_CLEARING:
      if (now - jamTimerTick >= 700) {
        driveMotor(0);
        curMotor = 0;
        jamState = JAM_LOCKED_WAIT_RELEASE;
        jamTimerTick = now;
      }
      break;"""

    new_jam = """    case JAM_STOP_WAIT:
      if (now - jamTimerTick >= 200) {
        // Đảo chiều quay chân vịt tối đa hết tốc lực (100% PWM) trong 1.5 giây
        int revSpeed = (cmdMotor >= 0) ? -255 : 255;
        driveMotor(revSpeed);
        jamState = JAM_REVERSE_CLEARING;
        jamTimerTick = now;
      }
      break;

    case JAM_REVERSE_CLEARING:
      if (now - jamTimerTick >= 1500) { // Xung lực cực đại 1.5 giây để văng rác
        driveMotor(0);
        curMotor = 0;
        if (jamConsecutiveStrikes >= 3) {
          jamState = JAM_LOCKED_WAIT_RELEASE;
          jamTimerTick = now + 5000UL;
          digitalWrite(PIN_SIREN_BUZZER, HIGH); // Bật còi hú cứu hộ
          virtualAnchorActive = true;           // Bật chế độ trôi dạt an toàn / neo ảo
          Serial.println(F("[USV-ERR] *** DA DAO CHIEU 3 LAN VAN KET RAC! BAT COI HU SOS & TROI DAT AN TOAN ***"));
        } else {
          jamState = JAM_NORMAL;
          overAmpStart = 0;
        }
      }
      break;"""

    if old_jam in content:
        content = content.replace(old_jam, new_jam)

    # 4. Add CO2 auto-inflation on roll/pitch > 60 deg > 3s
    old_mpu = """  if (hasMPU6050 && (abs(newRoll) > 45.0f || abs(newPitch) > 45.0f)) alerts |= ALERT_HIGH_TILT;"""

    new_mpu = """  if (hasMPU6050 && (abs(newRoll) > 45.0f || abs(newPitch) > 45.0f)) alerts |= ALERT_HIGH_TILT;

  // Auto-Inflation Lifebuoy (Bung phao cứu sinh CO2 chống chìm khẩn cấp khi lật > 60 độ liên tục quá 3 giây)
  static unsigned long capsizeStartMs = 0;
  static bool co2Triggered = false;
  if (hasMPU6050 && (abs(newRoll) > 60.0f || abs(newPitch) > 60.0f)) {
    if (capsizeStartMs == 0) {
      capsizeStartMs = now;
    } else if (!co2Triggered && (now - capsizeStartMs >= 3000UL)) {
      co2Triggered = true;
      digitalWrite(PIN_CO2_TRIGGER, HIGH); // Kích Rơ-le Solenoid đâm kim bình CO2 16g
      digitalWrite(PIN_SIREN_BUZZER, HIGH); // Còi hú cứu nạn khẩn cấp
      Serial.println(F("[USV-ALERT-CRITICAL] *** PHAO BI LAT UP (>60 DEG >3S)! KICH HOAT PHAO CUU SINH CO2 CHONG CHIM! ***"));
    }
  } else {
    capsizeStartMs = 0;
  }
  if (co2Triggered && (now - capsizeStartMs > 3600UL)) {
    digitalWrite(PIN_CO2_TRIGGER, LOW); // Ngắt điện cuộn Solenoid chống nóng mạch
  }"""

    if old_mpu in content:
        content = content.replace(old_mpu, new_mpu)

    # 5. Dual-Loss Return-To-Home (>10s LoRa AND >10s 4G)
    old_rth = """  // 3B. Mất sóng LoRa kéo dài (> 25s): Tự động Return-To-Home GPS về điểm Neo Home
  if (!batTier3CutoffActive && !returnHomeActive && (now - lastCmdRxTime > FAILSAFE_RADIO_BLACKOUT_RTH_MS)) {"""

    new_rth = """  // 3B. Mất tín hiệu kép (LoRa > 10s VÀ 4G Cloud > 10s): Tự động Return-To-Home GPS
  bool loraLost = (now - lastCmdRxTime > 10000UL);
  bool simLost  = (!simModemOnline || (now - lastSimOkMs > 10000UL));
  if (!batTier3CutoffActive && !returnHomeActive && loraLost && simLost) {"""

    if old_rth in content:
        content = content.replace(old_rth, new_rth)

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print("Upgraded usv_buoy/main.cpp with all critical failsafes successfully!")

if __name__ == "__main__":
    patch_buoy_full()
