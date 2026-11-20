import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

new_screens_code = r'''void drawStaticScreenPage0() {
  selectTFT();
  tft.fillScreen(C_BLACK);
  tft.setTextPadding(0);

  // Header background & border (Y = 0..34)
  tft.fillRect(0, 0, 240, 34, C_NAVY_BG);
  tft.drawFastHLine(0, 34, 240, C_CYAN);

  // Upper Deck: Goc nghieng & Thuoc do sau (Y = 36..132, H = 96)
  tft.drawRoundRect(3, 36, 115, 96, 4, C_CYAN);
  tft.setTextColor(C_CYAN, C_BLACK);
  tft.drawString("GOC NGHIENG TAU", 7, 38, 1);

  tft.drawRoundRect(122, 36, 115, 96, 4, C_CYAN);
  tft.setTextColor(C_GREEN, C_BLACK);
  tft.drawString("DO SAU TOI & WQI", 126, 38, 1);

  // Middle Water Matrix Grid: 4 O chi so nuoc (Y = 136..234, H = 98)
  tft.drawRoundRect(3, 136, 115, 47, 4, C_DARKGREY);
  tft.setTextColor(C_GREEN, C_BLACK);
  tft.drawString("1. DO pH NUOC", 7, 138, 1);

  tft.drawRoundRect(122, 136, 115, 47, 4, C_DARKGREY);
  tft.setTextColor(C_CYAN, C_BLACK);
  tft.drawString("2. KHOANG TDS/EC", 126, 138, 1);

  tft.drawRoundRect(3, 187, 115, 47, 4, C_DARKGREY);
  tft.setTextColor(C_ORANGE, C_BLACK);
  tft.drawString("3. DO DUC NTU", 7, 189, 1);

  tft.drawRoundRect(122, 187, 115, 47, 4, C_DARKGREY);
  tft.setTextColor(C_YELLOW, C_BLACK);
  tft.drawString("4. NHIET DS18B20", 126, 189, 1);

  // Footer Frame: Buong lai L298N & Nguon Pin (Y = 238..318, H = 80)
  tft.drawRoundRect(3, 238, 234, 80, 4, C_CYAN);
  tft.drawFastHLine(6, 264, 228, C_DARKGREY);
  tft.drawFastHLine(6, 288, 228, C_DARKGREY);
}

void drawStaticScreenPage1() {
  selectTFT();
  tft.fillScreen(C_BLACK);
  tft.setTextPadding(0);

  // Header background & border (Y = 0..34)
  tft.fillRect(0, 0, 240, 34, C_NAVY_BG);
  tft.drawFastHLine(0, 34, 240, C_CYAN);

  // Khoi 1: Do thi song pH & Nhiet do (Y = 36..132, H = 96)
  tft.drawRoundRect(3, 36, 234, 96, 4, C_CYAN);
  tft.setTextColor(C_CYAN, C_BLACK);
  tft.drawString("1. DO THI: pH(XANH LA) & NHIET(CYAN)", 8, 38, 1);

  // Khoi 2: Ho so 3 tang nuoc thuc te (Y = 136..234, H = 98)
  tft.drawRoundRect(3, 136, 234, 98, 4, C_CYAN);
  tft.setTextColor(C_GREEN, C_BLACK);
  tft.drawString("2. HO SO 3 TANG (0.3m - 1.0m - 1.8m)", 8, 138, 1);
  tft.setTextColor(C_YELLOW, C_BLACK);
  tft.drawString("TANG      pH    TDS   NTU  NHIET   DO", 8, 150, 1);
  tft.drawFastHLine(6, 160, 228, C_DARKGREY);

  // Footer (Y = 238..318)
  tft.drawRoundRect(3, 238, 234, 80, 4, C_CYAN);
  tft.drawFastHLine(6, 264, 228, C_DARKGREY);
  tft.drawFastHLine(6, 288, 228, C_DARKGREY);
  graphNeedsRedraw = true;
}

void drawStaticScreenPage2() {
  selectTFT();
  tft.fillScreen(C_BLACK);
  tft.setTextPadding(0);

  tft.fillRect(0, 0, 240, 34, C_NAVY_BG);
  tft.drawFastHLine(0, 34, 240, C_CYAN);

  // Khoi 1: 11 Linh kien phan cung (Y = 36..146, H = 110)
  tft.drawRoundRect(3, 36, 234, 110, 4, C_CYAN);
  tft.setTextColor(C_GREEN, C_BLACK);
  tft.drawString("1. CHAN DOAN 11 LINH KIEN (KHONG AO)", 8, 38, 1);

  // Khoi 2: Linh kien thieu & Joystick ADC (Y = 150..234, H = 84)
  tft.drawRoundRect(3, 150, 234, 84, 4, C_CYAN);
  tft.setTextColor(C_CYAN, C_BLACK);
  tft.drawString("2. LINH KIEN THIEU & JOYSTICK L298N", 8, 152, 1);

  // Footer (Y = 238..318)
  tft.drawRoundRect(3, 238, 234, 80, 4, C_CYAN);
  tft.drawFastHLine(6, 264, 228, C_DARKGREY);
  tft.drawFastHLine(6, 288, 228, C_DARKGREY);
}

void drawStaticScreenPage3() {
  selectTFT();
  tft.fillScreen(C_BLACK);
  tft.setTextPadding(0);

  tft.fillRect(0, 0, 240, 34, C_NAVY_BG);
  tft.drawFastHLine(0, 34, 240, C_CYAN);

  // Khung 3D Viewport (Y = 36..234, H = 198)
  tft.drawRoundRect(3, 36, 234, 198, 4, C_CYAN);
  tft.setTextColor(C_CYAN, C_BLACK);
  tft.drawString("3. BAN DO 3D LONG HO & USV PHIEU CANH", 8, 38, 1);

  // Footer (Y = 238..318)
  tft.drawRoundRect(3, 238, 234, 80, 4, C_CYAN);
  tft.drawFastHLine(6, 264, 228, C_DARKGREY);
  tft.drawFastHLine(6, 288, 228, C_DARKGREY);
}

void drawStaticScreenPage4() {
  selectTFT();
  tft.fillScreen(C_BLACK);
  tft.setTextPadding(0);

  tft.fillRect(0, 0, 240, 34, C_NAVY_BG);
  tft.drawFastHLine(0, 34, 240, C_CYAN);

  // Khung Radar Viewport (Y = 36..234, H = 198)
  tft.drawRoundRect(3, 36, 234, 198, 4, C_CYAN);
  tft.setTextColor(C_CYAN, C_BLACK);
  tft.drawString("4. RADAR LA BAN 360d & AUTOPILOT GPS", 8, 38, 1);

  // Footer (Y = 238..318)
  tft.drawRoundRect(3, 238, 234, 80, 4, C_CYAN);
  tft.drawFastHLine(6, 264, 228, C_DARKGREY);
  tft.drawFastHLine(6, 288, 228, C_DARKGREY);
}

void drawCurrentStaticPage() {
  if (currentPage == 0)      drawStaticScreenPage0();
  else if (currentPage == 1) drawStaticScreenPage1();
  else if (currentPage == 2) drawStaticScreenPage2();
  else if (currentPage == 3) drawStaticScreenPage3();
  else                       drawStaticScreenPage4();
}

// =========================================================================
// 5B. DONG CO DO HOA 3D VECTOR CHO MAN HINH TFT (PERSPECTIVE 3D ENGINE)
// =========================================================================
static float tft3dYaw   = 36.0f;
static float tft3dPitch = 32.0f;
static unsigned long lastJoy3dInputMs = 0;

struct Point3D { float x, y, z; };
struct Point2D { int x, y; bool ok; };

inline Point2D project3D(float x, float y, float z, float pitchDeg, float yawDeg,
                         int cx = 108, int cy = 70, float scale = 1.45f, float camDist = 110.0f) {
  float ry = yawDeg * 0.0174532925f;
  float cyaw = cosf(ry), syaw = sinf(ry);
  float x1 = x * cyaw - z * syaw;
  float z1 = x * syaw + z * cyaw;

  float rp = pitchDeg * 0.0174532925f;
  float cpitch = cosf(rp), spitch = sinf(rp);
  float y2 = y * cpitch - z1 * spitch;
  float z2 = y * spitch + z1 * cpitch;

  float dist = z2 + camDist;
  if (dist < 15.0f) return {cx, cy, false};

  float factor = (camDist * scale) / dist;
  int sx = (int)(cx + x1 * factor);
  int sy = (int)(cy - y2 * factor);

  return {sx, sy, true};
}

void draw3DWaterMapTFT() {
  selectTFT();

  // Dieu khien xoay 3D bang Joystick hoac tu dong xoay nhe khi ranh tay
  if (joy1HardwarePresent && (fabsf(joyNormX) > 0.12f || fabsf(joyNormY) > 0.12f)) {
    tft3dYaw   += joyNormX * 5.0f;
    tft3dPitch  = constrain(tft3dPitch - joyNormY * 4.0f, 15.0f, 75.0f);
    if (tft3dYaw >= 360.0f) tft3dYaw -= 360.0f;
    if (tft3dYaw < 0.0f)    tft3dYaw += 360.0f;
    lastJoy3dInputMs = millis();
  } else if (millis() - lastJoy3dInputMs >= 1500) {
    tft3dYaw += 1.2f;
    if (tft3dYaw >= 360.0f) tft3dYaw -= 360.0f;
  }

  if (sprCanvasCreated) {
    sprCanvas.fillSprite(C_BLACK);

    auto drawLineSpr = [&](float x1, float y1, float z1, float x2, float y2, float z2, uint16_t color) {
      Point2D p1 = project3D(x1, y1, z1, tft3dPitch, tft3dYaw, 108, 68, 1.45f, 110.0f);
      Point2D p2 = project3D(x2, y2, z2, tft3dPitch, tft3dYaw, 108, 68, 1.45f, 110.0f);
      if (p1.ok && p2.ok && p1.x >= 0 && p1.x < 216 && p1.y >= 0 && p1.y < 144 &&
          p2.x >= 0 && p2.x < 216 && p2.y >= 0 && p2.y < 144) {
        sprCanvas.drawLine(p1.x, p1.y, p2.x, p2.y, color);
      }
    };

    // 1. Luoi dia hinh long ho (Bathymetry Grid 5x5)
    const float gridSteps[5] = {-34.0f, -17.0f, 0.0f, 17.0f, 34.0f};
    auto calcBedY = [](float gx, float gz) -> float {
      return -24.0f + 7.0f * (cosf(gx * 0.045f) * cosf(gz * 0.045f));
    };
    for (int i = 0; i < 5; i++) {
      float gz = gridSteps[i];
      for (int j = 0; j < 4; j++) {
        float gx1 = gridSteps[j], gx2 = gridSteps[j + 1];
        drawLineSpr(gx1, calcBedY(gx1, gz), gz, gx2, calcBedY(gx2, gz), gz, C_DARKGREY);
      }
    }
    for (int j = 0; j < 5; j++) {
      float gx = gridSteps[j];
      for (int i = 0; i < 4; i++) {
        float gz1 = gridSteps[i], gz2 = gridSteps[i + 1];
        drawLineSpr(gx, calcBedY(gx, gz1), gz1, gx, calcBedY(gx, gz2), gz2, C_DARKGREY);
      }
    }

    // 2. Mat phang nuoc song dong
    const float waterY = 10.0f;
    drawLineSpr(-35.0f, waterY, -35.0f,  35.0f, waterY, -35.0f, C_CYAN);
    drawLineSpr( 35.0f, waterY, -35.0f,  35.0f, waterY,  35.0f, C_CYAN);
    drawLineSpr( 35.0f, waterY,  35.0f, -35.0f, waterY,  35.0f, C_CYAN);
    drawLineSpr(-35.0f, waterY,  35.0f, -35.0f, waterY, -35.0f, C_CYAN);
    float wave = sinf(millis() * 0.003f) * 1.2f;
    drawLineSpr(-22.0f, waterY + wave, -14.0f, 22.0f, waterY - wave, -14.0f, 0x1A7A);
    drawLineSpr(-18.0f, waterY - wave,  14.0f, 18.0f, waterY + wave,  14.0f, 0x1A7A);

    // 3. 3 vong tron da tang nuoc (Mat, Giua, Day)
    auto drawRingSpr = [&](float ly, bool valid, float ph, uint16_t defCol) {
      uint16_t col = valid ? ((ph >= 6.5f && ph <= 8.5f) ? C_GREEN : ((ph >= 5.5f && ph <= 9.0f) ? C_YELLOW : C_RED)) : defCol;
      const float r = 12.0f;
      for (int deg = 0; deg < 360; deg += 45) {
        float a1 = deg * 0.0174532925f, a2 = (deg + 45) * 0.0174532925f;
        drawLineSpr(r * cosf(a1), ly, r * sinf(a1), r * cosf(a2), ly, r * sinf(a2), col);
      }
    };
    drawRingSpr(waterY - 4.0f,  gcsLayerSurf.valid, gcsLayerSurf.ph, C_CYAN);
    drawRingSpr(waterY - 13.0f, gcsLayerMid.valid,  gcsLayerMid.ph,  C_GREEN);
    drawRingSpr(waterY - 22.0f, gcsLayerBot.valid,  gcsLayerBot.ph,  C_ORANGE);

    // 4. Thuyen Catamaran USV xoay theo Heading
    float hRad = (uHeading) * 0.0174532925f;
    float cH = cosf(hRad), sH = sinf(hRad);
    auto rotH = [&](float bx, float bz, float &ox, float &oz) {
      ox = bx * cH - bz * sH;
      oz = bx * sH + bz * cH;
    };
    float bx1, bz1, bx2, bz2;
    // Phao trai & phai
    rotH(-5.5f, -10.0f, bx1, bz1); rotH(-5.5f, 10.0f, bx2, bz2);
    drawLineSpr(bx1, waterY + 1.2f, bz1, bx2, waterY + 1.2f, bz2, C_YELLOW);
    rotH( 5.5f, -10.0f, bx1, bz1); rotH( 5.5f, 10.0f, bx2, bz2);
    drawLineSpr(bx1, waterY + 1.2f, bz1, bx2, waterY + 1.2f, bz2, C_YELLOW);
    // Cau ngang
    rotH(-5.5f, 0.0f, bx1, bz1); rotH( 5.5f, 0.0f, bx2, bz2);
    drawLineSpr(bx1, waterY + 1.8f, bz1, bx2, waterY + 1.8f, bz2, C_WHITE);
    // Cot anten
    rotH(0.0f, 0.0f, bx1, bz1);
    drawLineSpr(bx1, waterY + 1.8f, bz1, bx1, waterY + 6.5f, bz1, C_CYAN);

    // 5. Cap toi va qua roi long do cam bien
    float probeY = waterY - constrain(uDepthM * 11.5f, 0.0f, 27.0f);
    drawLineSpr(bx1, waterY + 1.8f, bz1, bx1, probeY, bz1, C_WHITE);
    Point2D pProbe = project3D(bx1, probeY, bz1, tft3dPitch, tft3dYaw, 108, 68, 1.45f, 110.0f);
    if (pProbe.ok && pProbe.x >= 2 && pProbe.x < 214 && pProbe.y >= 2 && pProbe.y < 142) {
      uint16_t probeCol = (uCT1Mud == 1) ? C_RED : ((uCT2Home == 1) ? C_CYAN : C_GREEN);
      sprCanvas.fillCircle(pProbe.x, pProbe.y, 3, probeCol);
      sprCanvas.drawCircle(pProbe.x, pProbe.y, 4, C_WHITE);
    }

    // Day sprite RAM ra man hinh ST7789 trong 1ms (100% ZERO FLICKER!)
    sprCanvas.pushSprite(12, 48);
  }

  // 6. HUD Telemetry Text Overlay ben duoi Sprite (Y = 196..234)
  char buf[64];
  tft.setTextPadding(220);
  tft.setTextColor(C_CYAN, C_BLACK);
  snprintf(buf, sizeof(buf), "Sau: %.2fm (%s) | WQI:%d (%s)",
           uDepthM, getAutoStageShortText(uAutoStage), uWQI, getQcvn08Classification(uWQI, uPH, uTDS, uNTU));
  tft.drawString(buf, 8, 196, 1);

  tft.setTextColor(C_YELLOW, C_BLACK);
  bool okBAT = (uHwHealth & HW_OK_BAT3S) != 0;
  snprintf(buf, sizeof(buf), "Mui: %.0fd (%s) | Pin: %.2fV (%d%%)",
           uHeading, getCompassDir(uHeading), uBat, okBAT ? calcBattery3SPercent(uBat) : 0);
  tft.drawString(buf, 8, 209, 1);

  tft.setTextColor(C_GREY, C_BLACK);
  snprintf(buf, sizeof(buf), "Goc 3D: Yaw:%.0fd Pitch:%.0fd | Joy: Xoay", tft3dYaw, tft3dPitch);
  tft.drawString(buf, 8, 222, 1);
  tft.setTextPadding(0);
}

void drawRadarAutopilotTFT() {
  selectTFT();

  if (sprCanvasCreated) {
    sprCanvas.fillSprite(C_BLACK);
    const int cx = 108, cy = 70;

    // Cac vong cu ly radar
    sprCanvas.drawCircle(cx, cy, 60, C_DARKGREY);
    sprCanvas.drawCircle(cx, cy, 40, C_DARKGREY);
    sprCanvas.drawCircle(cx, cy, 20, C_DARKGREY);
    sprCanvas.drawFastHLine(cx - 62, cy, 124, C_DARKGREY);
    sprCanvas.drawFastVLine(cx, cy - 62, 124, C_DARKGREY);

    // Ky hieu 4 huong la ban
    sprCanvas.setTextColor(C_RED, C_BLACK);   sprCanvas.drawString("B", cx - 3, cy - 68, 1);
    sprCanvas.setTextColor(C_CYAN, C_BLACK);  sprCanvas.drawString("D", cx + 64, cy - 3, 1);
    sprCanvas.setTextColor(C_WHITE, C_BLACK); sprCanvas.drawString("N", cx - 3, cy + 60, 1);
    sprCanvas.setTextColor(C_CYAN, C_BLACK);  sprCanvas.drawString("T", cx - 72, cy - 3, 1);

    // Mui ten chi huong mui thuyen uHeading
    float hRad = (uHeading - 90.0f) * 0.0174532925f;
    int tipX = cx + (int)(54.0f * cosf(hRad));
    int tipY = cy + (int)(54.0f * sinf(hRad));
    int w1X  = cx + (int)(40.0f * cosf(hRad + 2.5f));
    int w1Y  = cy + (int)(40.0f * sinf(hRad + 2.5f));
    int w2X  = cx + (int)(40.0f * cosf(hRad - 2.5f));
    int w2Y  = cy + (int)(40.0f * sinf(hRad - 2.5f));
    sprCanvas.fillTriangle(tipX, tipY, w1X, w1Y, w2X, w2Y, C_YELLOW);
    sprCanvas.fillCircle(cx, cy, 3, C_WHITE);

    // Diem moc HOME & vector phuong vi
    if (uHomeDistanceM > 0.5f || (homeLat != 0.0 && homeLng != 0.0)) {
      float bearing = calcGpsBearingDegrees(uLat, uLng, homeLat, homeLng);
      float bRad = (bearing - 90.0f) * 0.0174532925f;
      int bx = cx + (int)(56.0f * cosf(bRad));
      int by = cy + (int)(56.0f * sinf(bRad));
      sprCanvas.drawLine(cx, cy, bx, by, C_GREEN);
      sprCanvas.fillCircle(bx, by, 3, C_GREEN);
      sprCanvas.setTextColor(C_GREEN, C_BLACK);
      sprCanvas.drawString("HOME", bx + 3, by - 3, 1);
    }

    sprCanvas.pushSprite(12, 48);
  }

  // Text thong so dinh vi GPS Autopilot (Y = 196..234)
  char buf[64];
  tft.setTextPadding(220);
  tft.setTextColor(C_CYAN, C_BLACK);
  snprintf(buf, sizeof(buf), "GPS: %.5f, %.5f (%u VT)", uLat, uLng, uSats);
  tft.drawString(buf, 8, 196, 1);

  tft.setTextColor(C_YELLOW, C_BLACK);
  snprintf(buf, sizeof(buf), "Cach Home: %.1fm | Toc do: %.1f km/h", uHomeDistanceM, uGpsSpeedKmph);
  tft.drawString(buf, 8, 209, 1);

  tft.setTextColor(C_GREEN, C_BLACK);
  const char* autoTxt = uReturnHomeActive ? "TU VE HOME GPS" : ((uAlertFlags & ALERT_HEADING_HOLD) ? "GIU HUONG PID" : "DIEU KHIEN TAY");
  snprintf(buf, sizeof(buf), "Che do: %s | Cao do: %.1fm", autoTxt, uGpsAltM);
  tft.drawString(buf, 8, 222, 1);
  tft.setTextPadding(0);
}

// =========================================================================
// 6. CAP NHAT GIAO DIEN DONG TRUC QUAN (KHONG NHAP NHAY - ZERO FLICKER)
// =========================================================================
void refreshHeaderAndFooter(int mPWM, int rAng, int wCmd) {
  char buf[64];
  unsigned long ageMs = millis() - lastRxFromBuoy;
  bool s3Online = (rxPacketCount > 0) && (ageMs < 2800);
  int okCount   = countConnectedHwBits(uHwHealth);

  // 1. Trai: Logo + Ten USV + So thu tu goi tin
  tft.setTextPadding(100);
  if (!loraHardwareOk) {
    tft.setTextColor(C_RED, C_NAVY_BG);
    tft.drawString("LORA: LOI!", 5, 3, 2);
  } else if (s3Online) {
    tft.setTextColor(C_GREEN, C_NAVY_BG);
    snprintf(buf, sizeof(buf), "AQUA-USV #%lu", (unsigned long)uS3SeqId);
    tft.drawString(buf, 5, 3, 2);
  } else {
    tft.setTextColor(C_RED, C_NAVY_BG);
    snprintf(buf, sizeof(buf), "MAT SONG(%lus)", ageMs / 1000UL);
    tft.drawString(buf, 5, 3, 2);
  }

  // 2. Giua: 4 Vach song LoRa do hoa (Signal Bars)
  int barCount = (uRSSI >= -75) ? 4 : ((uRSSI >= -90) ? 3 : ((uRSSI >= -105) ? 2 : (s3Online ? 1 : 0)));
  uint16_t sigCol = (barCount >= 3) ? C_GREEN : ((barCount == 2) ? C_YELLOW : C_RED);
  for (int b = 0; b < 4; b++) {
    int bx = 112 + b * 5;
    int bh = 3 + b * 3; // 3, 6, 9, 12 px
    if (b < barCount) {
      tft.fillRect(bx, 15 - bh, 3, bh, sigCol);
    } else {
      tft.fillRect(bx, 15 - bh, 3, bh, C_BLACK);
      tft.drawPixel(bx + 1, 14, C_DARKGREY);
    }
  }

  // Text RSSI nho duoi vach song
  tft.setTextPadding(40);
  tft.setTextColor(sigCol, C_NAVY_BG);
  snprintf(buf, sizeof(buf), "%ddB", uRSSI);
  tft.drawString(buf, 108, 18, 1);

  // 3. Phai: Thoi pin do hoa chan thuc (Battery Pack with positive nub)
  bool okBAT = (uHwHealth & HW_OK_BAT3S) != 0;
  int batPct = okBAT ? calcBattery3SPercent(uBat) : 0;
  int bx = 168, by = 4, bw = 24, bh = 11;
  tft.drawRect(bx, by, bw, bh, C_WHITE);
  tft.fillRect(bx + bw, by + 3, 2, 5, C_WHITE); // Cuc duong pin
  int fillW = constrain((batPct * (bw - 4)) / 100, 0, bw - 4);
  uint16_t batCol = (batPct >= 50) ? C_GREEN : ((batPct >= 20) ? C_YELLOW : C_RED);
  if (fillW > 0) tft.fillRect(bx + 2, by + 2, fillW, bh - 4, batCol);
  if (fillW < bw - 4) tft.fillRect(bx + 2 + fillW, by + 2, (bw - 4) - fillW, bh - 4, C_BLACK);

  // So von va % ben canh thoi pin
  tft.setTextPadding(44);
  tft.setTextColor(batCol, C_NAVY_BG);
  snprintf(buf, sizeof(buf), "%.1fV", uBat);
  tft.drawString(buf, 198, 4, 1);
  snprintf(buf, sizeof(buf), "%d%%", batPct);
  tft.drawString(buf, 198, 14, 1);

  // Dai trang thai duoi header: So linh kien & So trang
  tft.setTextPadding(100);
  tft.setTextColor(C_CYAN, C_NAVY_BG);
  snprintf(buf, sizeof(buf), "LK:%d/11 %s", okCount, (okCount == HW_TOTAL_COMPONENTS) ? "FULL" : "THIEU");
  tft.drawString(buf, 5, 20, 1);

  tft.setTextPadding(50);
  tft.setTextColor(C_YELLOW, C_NAVY_BG);
  snprintf(buf, sizeof(buf), "P%d/5", currentPage + 1);
  tft.drawString(buf, 210, 20, 1);

  // =========================================================================
  // FOOTER CHUAN DONG NHAT TREN CA 5 TRANG (Y = 238..318)
  // =========================================================================
  // Hang 1 (Y=241): Thanh cong suat L298N & Banh lai
  tft.setTextPadding(220);
  const char* mDir = (mPWM > 0) ? "TIEN" : ((mPWM < 0) ? "LUI" : "DUNG");
  int motorPct = (abs(mPWM) * 100) / 255;
  tft.setTextColor(C_WHITE, C_BLACK);
  snprintf(buf, sizeof(buf), "L298N:%s %d%% | Lai:%dd (%s)", mDir, motorPct, rAng, getCompassDir(uHeading));
  tft.drawString(buf, 8, 241, 2);

  // Thanh do cong suat dong co 2 chieu (Zero Center Power Bar)
  int mx = 8, my = 257, mw = 224, mh = 4;
  tft.fillRect(mx, my, mw, mh, C_DARKGREY);
  int cx = mx + mw / 2;
  int pW = (mPWM * (mw / 2)) / 255;
  if (pW > 0) {
    tft.fillRect(cx, my, pW, mh, C_GREEN);
  } else if (pW < 0) {
    tft.fillRect(cx + pW, my, -pW, mh, C_ORANGE);
  }
  tft.drawFastVLine(cx, my - 1, mh + 2, C_WHITE); // Vach moc giua 0%

  // Hang 2 (Y=266): Dien ap Pin, Dong ACS712, Cong suat
  tft.setTextPadding(220);
  tft.setTextColor(C_YELLOW, C_BLACK);
  snprintf(buf, sizeof(buf), "Pin:%.2fV | Dong:%.2fA | CS:%.1fW", uBat, uAmp, uPowerWatt);
  tft.drawString(buf, 8, 268, 2);

  // Hang 3 (Y=288): Toi do sau, Cong tac hanh trinh CT1/CT2
  tft.setTextPadding(220);
  const char* wStr = (wCmd == 1) ? "HA" : ((wCmd == 2) ? "THU" : ((wCmd == 3) ? "AUTO" : "DUNG"));
  tft.setTextColor(C_CYAN, C_BLACK);
  snprintf(buf, sizeof(buf), "Toi:%.2fm (%s) | CT1:%s CT2:%s",
           uDepthM, wStr, uCT1Mud ? "BUN!" : "OK", uCT2Home ? "HOME" : "THA");
  tft.drawString(buf, 8, 289, 1);

  // Hang 4 (Y=303): Trang thai Joystick & Cap so
  tft.setTextColor(joy1HardwarePresent ? C_GREEN : C_ORANGE, C_BLACK);
  snprintf(buf, sizeof(buf), "Joy:%s | Cap:%s | SW:Doi Trang",
           joy1HardwarePresent ? "DA CAM(OK)" : "WEB/CHUA CAM", getSpeedGearText());
  tft.drawString(buf, 8, 303, 1);

  tft.setTextPadding(0);
}

void drawRealtimeSparklineGraph() {
  if (!graphNeedsRedraw) return;
  graphNeedsRedraw = false;

  const int gx = 10, gy = 54, gw = 220, gh = 62;
  tft.fillRect(gx, gy, gw, gh, C_BLACK);
  tft.drawRect(gx, gy, gw, gh, C_DARKGREY);
  for (int x = gx + 4; x < gx + gw - 2; x += 8) {
    tft.drawPixel(x, gy + gh / 2, C_DARKGREY);
  }

  if (!histInit) return;

  for (int i = 0; i < GRAPH_PTS - 1; i++) {
    int x1 = gx + 2 + (i * (gw - 4)) / (GRAPH_PTS - 1);
    int x2 = gx + 2 + ((i + 1) * (gw - 4)) / (GRAPH_PTS - 1);

    int yPh1 = gy + gh - 3 - (int)(constrain((histPH[i] - 5.0f) / 4.5f, 0.0f, 1.0f) * (gh - 6));
    int yPh2 = gy + gh - 3 - (int)(constrain((histPH[i + 1] - 5.0f) / 4.5f, 0.0f, 1.0f) * (gh - 6));
    tft.drawLine(x1, yPh1, x2, yPh2, C_GREEN);

    int yT1 = gy + gh - 3 - (int)(constrain((histTemp[i] - 20.0f) / 20.0f, 0.0f, 1.0f) * (gh - 6));
    int yT2 = gy + gh - 3 - (int)(constrain((histTemp[i + 1] - 20.0f) / 20.0f, 0.0f, 1.0f) * (gh - 6));
    tft.drawLine(x1, yT1, x2, yT2, C_CYAN);
  }
}

void refreshScreenValues(int mPWM, int rAng, int wCmd) {
  selectTFT();
  char buf[64];

  bool okDS   = (uHwHealth & HW_OK_DS18B20) != 0;
  bool okPH   = (uHwHealth & HW_OK_PH)      != 0;
  bool okTDS  = (uHwHealth & HW_OK_TDS)     != 0;
  bool okNTU  = (uHwHealth & HW_OK_NTU)     != 0;
  bool okACS  = (uHwHealth & HW_OK_ACS712)  != 0;
  bool okBAT  = (uHwHealth & HW_OK_BAT3S)   != 0;
  bool okMPU  = (uHwHealth & HW_OK_MPU6050) != 0;
  bool okGY   = (uHwHealth & HW_OK_GY271)   != 0;
  bool okLORA = (uHwHealth & HW_OK_LORA)    != 0;
  bool okSIM  = (uHwHealth & HW_OK_SIM4G)   != 0;
  bool okGPS  = (uHwHealth & HW_OK_GPS)     != 0;

  uPowerWatt = (okBAT && okACS) ? (uBat * uAmp) : 0.0f;
  if (uPowerWatt > uPeakPowerWatt) uPeakPowerWatt = uPowerWatt;

  unsigned long now = millis();
  if (lastEnergyCalcMs > 0 && now > lastEnergyCalcMs) {
    float dtHours = (now - lastEnergyCalcMs) / 3600000.0f;
    uEnergyWh += uPowerWatt * dtHours;
  }
  lastEnergyCalcMs = now;

  if (now - lastGraphPushMs >= 600) {
    lastGraphPushMs = now;
    pushGraphSample(okPH ? uPH : 0.0f, okDS ? uTemp : 0.0f);
  }

  int doSatPct = calcDOSaturationPercent(uDO, uTemp, uTDS);

  refreshHeaderAndFooter(mPWM, rAng, wCmd);

  if (currentPage == 0) {
    // =====================================================================
    // TRANG 1/5: TACTICAL COCKPIT & ARTIFICIAL HORIZON (KHONG NHAP NHAY)
    // =====================================================================
    // 1. VE CHAN TROI NHAN TAO TRONG SPRITE RAM (X=23, Y=48, W=74, H=54)
    if (sprHorizonCreated) {
      sprHorizon.fillSprite(C_BLACK);
      const int w = 74, h = 54, cx = 37, cy = 27;

      // Xoay goc Roll & truot Pitch
      float rollRad = (-uRoll) * 0.0174532925f;
      float cosR = cosf(rollRad), sinR = sinf(rollRad);
      int pitchOff = constrain((int)(uPitch * 0.6f), -16, 16);

      // Duong phan chia chan troi
      int x1 = cx - (int)(32 * cosR) + (int)(pitchOff * sinR);
      int y1 = cy - (int)(32 * sinR) - (int)(pitchOff * cosR);
      int x2 = cx + (int)(32 * cosR) + (int)(pitchOff * sinR);
      int y2 = cy + (int)(32 * sinR) - (int)(pitchOff * cosR);
      sprHorizon.drawLine(x1, y1, x2, y2, C_WHITE);
      sprHorizon.drawLine(x1, y1 + 1, x2, y2 + 1, C_CYAN);

      // Thang do Pitch (-10, +10)
      for (int p = -10; p <= 10; p += 20) {
        int py = cy - (int)(p * 0.6f) - pitchOff;
        if (py >= 4 && py <= h - 4) {
          sprHorizon.drawFastHLine(cx - 8, py, 16, C_DARKGREY);
        }
      }

      // Vong tron bao chan troi
      sprHorizon.drawCircle(cx, cy, 25, C_CYAN);

      // Silhouette Thuyen Catamaran o tam
      sprHorizon.fillRect(cx - 16, cy - 3, 6, 4, C_YELLOW);
      sprHorizon.fillRect(cx + 10, cy - 3, 6, 4, C_YELLOW);
      sprHorizon.drawFastHLine(cx - 10, cy - 1, 20, C_WHITE);
      sprHorizon.fillCircle(cx, cy, 2, C_RED);

      sprHorizon.pushSprite(23, 48); // Day ra man hinh trong 1ms (ZERO FLICKER!)
    }

    // So do Roll/Pitch & Mui ben duoi chan troi
    tft.setTextPadding(100);
    tft.setTextColor(C_WHITE, C_BLACK);
    snprintf(buf, sizeof(buf), "R:%+4.1fd  P:%+4.1fd", uRoll, uPitch);
    tft.drawString(buf, 8, 107, 1);

    tft.setTextColor(C_YELLOW, C_BLACK);
    snprintf(buf, sizeof(buf), "MUI:%03.0fd (%s)", uHeading, getCompassDir(uHeading));
    tft.drawString(buf, 8, 119, 1);

    // 2. THUOC DO SAU TOI THANG DUNG TRONG SPRITE RAM (X=126, Y=48, W=14, H=76)
    if (sprDepthCreated) {
      sprDepth.fillSprite(C_BLACK);
      const int rw = 14, rh = 76;
      sprDepth.drawRect(0, 0, rw, rh, C_DARKGREY);

      // 3 tang nuoc: Nong Cyan (0..24), Giua Blue (25..48), Day Navy (49..74)
      sprDepth.fillRect(1, 1, rw - 2, 24, 0x05BF);
      sprDepth.fillRect(1, 25, rw - 2, 24, 0x0356);
      sprDepth.fillRect(1, 49, rw - 2, rh - 50, 0x018C);

      // Vach chia do sau 0.5m
      for (int d = 0; d <= 4; d++) {
        int my = (d * (rh - 2)) / 4;
        sprDepth.drawFastHLine(rw - 4, my, 4, C_WHITE);
      }

      // Qua roi long do cam bien di chuyen theo uDepthM
      int probeY = constrain((int)((uDepthM / 2.0f) * (rh - 8)), 0, rh - 8);
      uint16_t probeCol = uCT1Mud ? C_RED : (uCT2Home ? C_CYAN : C_GREEN);
      sprDepth.fillRect(1, probeY, rw - 2, 6, probeCol);
      sprDepth.drawRect(1, probeY, rw - 2, 6, C_WHITE);

      sprDepth.pushSprite(126, 48); // Day ra man hinh trong 1ms (ZERO FLICKER!)
    }

    // Cac thong so toi & nuoc ben phai thuoc do
    tft.setTextPadding(86);
    tft.setTextColor(C_CYAN, C_BLACK);
    snprintf(buf, sizeof(buf), "%.2fm", uDepthM);
    tft.drawString(buf, 145, 50, 4);

    tft.setTextColor(C_YELLOW, C_BLACK);
    snprintf(buf, sizeof(buf), "%s", getAutoStageShortText(uAutoStage));
    tft.drawString(buf, 145, 78, 1);

    tft.setTextColor(C_GREEN, C_BLACK);
    snprintf(buf, sizeof(buf), "WQI:%d (%s)", uWQI, (uWQI >= 75) ? "TOT" : ((uWQI >= 50) ? "TB" : "KEM"));
    tft.drawString(buf, 145, 92, 1);

    tft.setTextColor(0x841F, C_BLACK);
    snprintf(buf, sizeof(buf), "DO:%.1fmg/L", uDO);
    tft.drawString(buf, 145, 106, 1);

    tft.setTextColor(C_GREY, C_BLACK);
    snprintf(buf, sizeof(buf), "BH:%d%%", doSatPct);
    tft.drawString(buf, 145, 118, 1);

    // =====================================================================
    // 4 O CHI SO NUOC SAC NET (Y = 136..234)
    // =====================================================================
    // O 1: pH (X=3, Y=136, W=115, H=47)
    tft.setTextPadding(62);
    if (okPH) {
      tft.setTextColor((uPH < 6.0f || uPH > 8.5f) ? C_RED : C_WHITE, C_BLACK);
      snprintf(buf, sizeof(buf), "%.2f", uPH);
      tft.drawString(buf, 7, 149, 4);
    } else {
      tft.setTextColor(C_RED, C_BLACK);
      tft.drawString("CHUA CAM", 8, 154, 2);
    }
    tft.setTextPadding(44);
    tft.setTextColor(okPH ? C_CYAN : C_GREY, C_BLACK);
    snprintf(buf, sizeof(buf), "DO:%.1f", uDO);
    tft.drawString(buf, 72, 149, 1);
    tft.setTextColor(okPH ? C_GREEN : C_RED, C_BLACK);
    snprintf(buf, sizeof(buf), "BH:%d%%", doSatPct);
    tft.drawString(buf, 72, 162, 1);
    tft.fillRect(7, 176, 24, 2, C_RED);
    tft.fillRect(31, 176, 40, 2, C_GREEN);
    tft.fillRect(71, 176, 40, 2, C_CYAN);

    // O 2: Khoang TDS & EC (X=122, Y=136, W=115, H=47)
    tft.setTextPadding(62);
    if (okTDS) {
      tft.setTextColor(C_WHITE, C_BLACK);
      snprintf(buf, sizeof(buf), "%.0f", uTDS);
      tft.drawString(buf, 126, 149, 4);
    } else {
      tft.setTextColor(C_RED, C_BLACK);
      tft.drawString("CHUA CAM", 127, 154, 2);
    }
    tft.setTextPadding(46);
    tft.setTextColor(okTDS ? C_GREY : C_RED, C_BLACK);
    tft.drawString(okTDS ? "ppm" : "THIEU", 188, 149, 1);
    tft.setTextColor(okTDS ? C_CYAN : C_RED, C_BLACK);
    snprintf(buf, sizeof(buf), "EC:%.0f", uEC);
    tft.drawString(buf, 188, 162, 1);

    // O 3: Do duc NTU (X=3, Y=187, W=115, H=47)
    tft.setTextPadding(62);
    if (okNTU) {
      tft.setTextColor((uNTU > 100.0f) ? C_ORANGE : C_WHITE, C_BLACK);
      snprintf(buf, sizeof(buf), "%.0f", uNTU);
      tft.drawString(buf, 7, 199, 4);
    } else {
      tft.setTextColor(C_RED, C_BLACK);
      tft.drawString("CHUA CAM", 8, 204, 2);
    }
    tft.setTextPadding(44);
    tft.setTextColor(okNTU ? C_GREY : C_RED, C_BLACK);
    tft.drawString("NTU", 72, 199, 1);
    tft.setTextColor(okNTU ? ((uNTU <= 50.0f) ? C_GREEN : C_ORANGE) : C_RED, C_BLACK);
    tft.drawString(okNTU ? ((uNTU <= 50.0f) ? "TRONG" : "DUC") : "THIEU", 72, 212, 1);

    // O 4: Nhiet do DS18B20 (X=122, Y=187, W=115, H=47)
    tft.setTextPadding(62);
    if (okDS) {
      tft.setTextColor(C_WHITE, C_BLACK);
      snprintf(buf, sizeof(buf), "%.1f", uTemp);
      tft.drawString(buf, 126, 199, 4);
    } else {
      tft.setTextColor(C_RED, C_BLACK);
      tft.drawString("CHUA CAM", 127, 204, 2);
    }
    tft.setTextPadding(48);
    tft.setTextColor(okDS ? C_YELLOW : C_RED, C_BLACK);
    tft.drawString(okDS ? "oC" : "THIEU", 188, 199, 1);
    tft.setTextColor(C_CYAN, C_BLACK);
    snprintf(buf, sizeof(buf), "S:%.0f G:%.0f", uS3ChipTemp, uGcsChipTemp);
    tft.drawString(buf, 172, 212, 1);

    tft.setTextPadding(0);
  } else if (currentPage == 1) {
    // =====================================================================
    // TRANG 2/5: DO THI THOI GIAN THUC & HO SO DO 3 TANG NUOC
    // =====================================================================
    drawRealtimeSparklineGraph();

    tft.setTextPadding(220);
    tft.setTextColor(C_YELLOW, C_BLACK);
    snprintf(buf, sizeof(buf), "pH:%s | Nhiet:%s | Pin:%s",
             okPH ? "OK" : "THIEU", okDS ? "OK" : "THIEU", okBAT ? "OK" : "THIEU");
    tft.drawString(buf, 9, 118, 1);

    tft.setTextPadding(220);
    tft.setTextColor(gcsLayerSurf.valid ? C_CYAN : C_GREY, C_BLACK);
    if (gcsLayerSurf.valid) {
      snprintf(buf, sizeof(buf), "1.Mat : %.2f %3.0f  %3.0f %.1fC %.1f",
               gcsLayerSurf.ph, gcsLayerSurf.tds, gcsLayerSurf.ntu, gcsLayerSurf.temp, gcsLayerSurf.doMgL);
    } else {
      snprintf(buf, sizeof(buf), "1.Mat (0.3m): CHUA DO THUC TE");
    }
    tft.drawString(buf, 8, 163, 2);

    tft.setTextColor(gcsLayerMid.valid ? C_GREEN : C_GREY, C_BLACK);
    if (gcsLayerMid.valid) {
      snprintf(buf, sizeof(buf), "2.Giua: %.2f %3.0f  %3.0f %.1fC %.1f",
               gcsLayerMid.ph, gcsLayerMid.tds, gcsLayerMid.ntu, gcsLayerMid.temp, gcsLayerMid.doMgL);
    } else {
      snprintf(buf, sizeof(buf), "2.Giua(1.0m): CHUA DO THUC TE");
    }
    tft.drawString(buf, 8, 183, 2);

    tft.setTextColor(gcsLayerBot.valid ? C_ORANGE : C_GREY, C_BLACK);
    if (gcsLayerBot.valid) {
      snprintf(buf, sizeof(buf), "3.Day : %.2f %3.0f  %3.0f %.1fC %.1f",
               gcsLayerBot.ph, gcsLayerBot.tds, gcsLayerBot.ntu, gcsLayerBot.temp, gcsLayerBot.doMgL);
    } else {
      snprintf(buf, sizeof(buf), "3.Day (1.8m): CHUA DO THUC TE");
    }
    tft.drawString(buf, 8, 203, 2);

    tft.setTextColor(C_YELLOW, C_BLACK);
    snprintf(buf, sizeof(buf), "TT:%s | Sau:%.2fm", getAutoStageShortText(uAutoStage), uDepthM);
    tft.drawString(buf, 8, 222, 2);

    tft.setTextPadding(0);
  } else if (currentPage == 2) {
    // =====================================================================
    // TRANG 3/5: BANG KIEM TRA 11 LINH KIEN PHAN CUNG (KHONG BAO AO!)
    // =====================================================================
    tft.setTextPadding(220);

    tft.setTextColor(C_WHITE, C_BLACK);
    snprintf(buf, sizeof(buf), "1.DS18B20:%s | 2.pH :%s",
             okDS ? "OK" : "THIEU", hwStateTxt(okPH, HW_OK_PH));
    tft.drawString(buf, 9, 52, 2);

    snprintf(buf, sizeof(buf), "3.TDS    :%s | 4.NTU:%s",
             hwStateTxt(okTDS, HW_OK_TDS), hwStateTxt(okNTU, HW_OK_NTU));
    tft.drawString(buf, 9, 70, 2);

    snprintf(buf, sizeof(buf), "5.ACS712 :%s | 6.Pin:%s",
             hwStateTxt(okACS, HW_OK_ACS712), hwStateTxt(okBAT, HW_OK_BAT3S));
    tft.drawString(buf, 9, 88, 2);

    snprintf(buf, sizeof(buf), "7.MPU6050:%s | 8.LaBan:%s",
             okMPU ? "OK" : "THIEU", okGY ? "OK" : "THIEU");
    tft.drawString(buf, 9, 106, 2);

    snprintf(buf, sizeof(buf), "9.LoRa:%s|10.4G:%s|11.GPS:%s",
             okLORA ? "OK" : "LOI", okSIM ? "OK" : "THIEU", okGPS ? "OK" : "THIEU");
    tft.drawString(buf, 9, 124, 2);

    tft.setTextColor(joy1HardwarePresent ? C_GREEN : C_ORANGE, C_BLACK);
    float liveVy = readJoystickFastMv(JOY1_VRY_PIN);
    float liveVx = readJoystickFastMv(JOY1_VRX_PIN);
    if (joy1HardwarePresent) {
      snprintf(buf, sizeof(buf), "12.Joy: OK (Y:%.0fmV X:%.0fmV | %s)",
               joyCurVyMv, joyCurVxMv, getSpeedGearText());
    } else {
      snprintf(buf, sizeof(buf), "12.Joy: CHUA CAM (Y:%.0f X:%.0fmV - %s)",
               liveVy, liveVx, (liveVy >= 95 && liveVy <= 205 && liveVx >= 95 && liveVx <= 205) ? "LONG DAY/HO MACH" : "KIEM TRA DAY");
    }
    tft.drawString(buf, 9, 144, 1);

    // Khung 2: Liet ke linh kien thieu & trang thai L298N
    char missBuf[64];
    buildMissingHwString(uHwHealth, missBuf, sizeof(missBuf));
    tft.setTextColor((countConnectedHwBits(uHwHealth) == HW_TOTAL_COMPONENTS) ? C_GREEN : C_RED, C_BLACK);
    snprintf(buf, sizeof(buf), "Thieu: %-.21s", missBuf);
    tft.drawString(buf, 9, 168, 2);

    tft.setTextColor(C_CYAN, C_BLACK);
    snprintf(buf, sizeof(buf), "L298N PWM:%d | Lai:%dd(%s)",
             mPWM, rAng, getSpeedGearText());
    tft.drawString(buf, 9, 188, 2);

    tft.setTextColor(C_YELLOW, C_BLACK);
    snprintf(buf, sizeof(buf), "Chip S3:%.1fC | GCS:%.1fC",
             uS3ChipTemp, uGcsChipTemp);
    tft.drawString(buf, 9, 208, 2);

    tft.setTextPadding(0);
  } else if (currentPage == 3) {
    // =====================================================================
    // TRANG 4/5: BAN DO 3D LONG HO & THUYEN USV (100% ZERO FLICKER)
    // =====================================================================
    draw3DWaterMapTFT();
  } else {
    // =====================================================================
    // TRANG 5/5: RADAR LA BAN 360d & AUTOPILOT GPS HOME (100% ZERO FLICKER)
    // =====================================================================
    drawRadarAutopilotTFT();
  }
}

'''

with open('src/ground_station/main.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

p1 = text.find('void drawStaticScreenPage0() {')
p2 = text.find('bool sendDiscordWebhookFromGCS(bool isProfile')

if p1 != -1 and p2 != -1:
    text = text[:p1] + new_screens_code + text[p2:]
    with open('src/ground_station/main.cpp', 'w', encoding='utf-8') as out:
        out.write(text)
    print("Replaced screen drawing section successfully!")
else:
    print("Markers not found! p1:", p1, "p2:", p2)
