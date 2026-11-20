#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <Update.h>
#include <ESPmDNS.h>
#include "usv_common.h"
#include "web_3d_map.h"

// =========================================================================
// 1. ĐỊNH NGHĨA CHÂN TRẠM BỜ ESP32 (JOYSTICK L298N + 7 SWITCH + SIM 4G)
// =========================================================================
#define TFT_CS_PIN        5
#define LORA_NSS_PIN      15
#define LORA_RST_PIN      16
#define LORA_DIO0_PIN     17

// --- Kênh Joystick Analog Vô Cấp điều khiển Động cơ L298N & Bánh lái (ADC1 ESP32) ---
#define JOY1_VRY_PIN      34  // [ADC1_CH6] Trục dọc Joystick 1: Đẩy Tiến / Kéo Lùi L298N (-255..+255 PWM)
#define JOY1_VRX_PIN      35  // [ADC1_CH7] Trục ngang Joystick 1: Bẻ Bánh Lái Trái / Phải (45°..135°)
#define JOY2_VRY_PIN      36  // [ADC1_CH0 / VP] Trục dọc Joystick 2 (Tùy chọn): Hạ / Thu Tời
#define JOY1_SW_PIN       39  // [ADC1_CH3 / VN] Nút nhấn đỉnh Joystick: Đổi cấp số L298N ECO/CRUISE/TURBO

// --- 7 Công tắc / Nút nhấn vật lý (Hoạt động song song cùng Joystick) ---
#define SW1_MOTOR_FWD     32  // SW1: Tiến L298N
#define SW2_MOTOR_REV     33  // SW2: Lùi L298N
#define SW3_RUDDER_LEFT   25  // SW3: Rẽ Trái
#define SW4_RUDDER_RIGHT  26  // SW4: Rẽ Phải
#define SW5_WINCH_DOWN    27  // SW5: Hạ Tời
#define SW6_WINCH_HOME    14  // SW6: Thu Tời Về Home
#define SW7_PAGE_AUTO     13  // SW7: Nhấn nhả đổi Trang (1/2/3) | Nhấn giữ 1.2s bật Auto 3 tầng

// Cổng UART2 cho mạch SIM 4G (A7680C / SIM7600 / SIM800L) nếu lắp trực tiếp trên ESP32 Trạm Bờ
#define GCS_SIM_RX2_PIN   21
#define GCS_SIM_TX2_PIN   22

// Cấu hình Wi-Fi Access Point cho Web Dashboard Trạm Bờ (ChemStation AI)
#define GCS_WIFI_SSID     "ChemStation-AI-GCS"
#define GCS_WIFI_PASS     DEFAULT_AP_WIFI_PASS
#define GCS_MDNS_HOST     "chemstation"
#define GCS_WIFI_RECONNECT_MS 15000UL
#define GCS_PROFILE_PUSH_COOLDOWN_MS 30000UL

TFT_eSPI tft = TFT_eSPI();

// Sprites bộ nhớ RAM hỗ trợ vẽ 2 lớp siêu mượt, loại bỏ 100% nhấp nháy (Zero-Flicker TFT_eSprite)
TFT_eSprite sprHorizon = TFT_eSprite(&tft);
TFT_eSprite sprDepth   = TFT_eSprite(&tft);
TFT_eSprite sprCanvas  = TFT_eSprite(&tft); // Dùng chung cho Trang 3 (3D Bathymetry) & Trang 4 (Radar 360)
bool sprHorizonCreated = false;
bool sprDepthCreated   = false;
bool sprCanvasCreated  = false;
WebServer webServer(80);
DNSServer dnsServer;
Preferences gcsPrefs;

// =========================================================================
// 2. BẢNG MÀU ĐÃ BÙ ĐẢO KÊNH BGR CHO MÀN MẠCH ĐỎ 240x320 CỦA BẠN
// =========================================================================
#define C_BLACK     0x0000
#define C_WHITE     0xFFFF
#define C_GREY      0x7BEF
#define C_DARKGREY  0x39E7
#define C_NAVY_BG   0x5100  // Lên màn sẽ thành Xanh Navy đậm
#define C_CYAN      0xFFE0  // Bù BGR -> Lên màn ra đúng Xanh Cyan
#define C_YELLOW    0x07FF  // Bù BGR -> Lên màn ra đúng Vàng tươi
#define C_GREEN     0x07E0  // Xanh lá cây
#define C_RED       0x001F  // Bù BGR -> Lên màn ra đúng Đỏ tươi
#define C_ORANGE    0x053F  // Bù BGR -> Lên màn ra đúng Cam
#define C_MAGENTA   0xF81F
#define C_BLUE      0xF800  // Hồng tím

// Biến lưu dữ liệu nhận từ Phao ESP32-S3 (Khởi tạo = 0.0: TUYỆT ĐỐI KHÔNG BÁO ẢO!)
float uPH = 0.0f, uTDS = 0.0f, uNTU = 0.0f, uTemp = 0.0f, uDO = 0.0f;
float uAmp = 0.0f, uBat = 0.0f, uRoll = 0.0f, uPitch = 0.0f, uHeading = 0.0f;
float uMpuTemp = 0.0f, uEC = 0.0f, uDepthM = 0.00f;
int   uWQI = 0;
double uLat = 0.0, uLng = 0.0;
double homeLat = 0.0, homeLng = 0.0;
int   uCT1Mud = 0, uCT2Home = 1;
int   uAutoStage = 0;
unsigned int uAlertFlags = 0;
unsigned int uHwHealth   = 0x000; // 11-bit mask linh kiện thực tế (0 = chưa kết nối)
unsigned long uS3SeqId   = 0;
unsigned int uAdcFault   = 0x000; // Kênh ADC đã cắm nhưng VƯỢT DẢI ĐO (>3.05V, ADC kẹp trần -> số đo sai)
float uGcsChipTemp       = 0.0f;  // Nhiệt độ chip ESP32 Trạm Bờ (°C)
float uS3ChipTemp        = 0.0f;  // Nhiệt độ chip ESP32-S3 Trạm Phao (°C)

// Trường mở rộng Protocol v2 (GPS/Home/Autopilot) nhận từ Phao
unsigned int uSats      = 0;
float uGpsSpeedKmph     = 0.0f;
float uGpsAltM          = 0.0f;
float uHomeDistanceM    = 0.0f;
unsigned int uFlags2    = 0;
float uTargetHeadingDeg = 0.0f;
bool  uReturnHomeActive = false;
bool  uGpsFixValid      = false;
bool  uVirtualAnchorActive = false;
bool  uHeadingHoldActive   = false;

// Thống kê Cloud / Wi-Fi / Uptime
uint32_t gcsBootCount        = 0;
uint32_t gsheetFailCnt       = 0;
unsigned long lastGsheetPushMs = 0;
unsigned long lastGsheetAttemptMs = 0;
unsigned long lastWifiCheckMs  = 0;
unsigned long lastProfilePushMs = 0;
bool profilePushedForRun     = false;
uint32_t prfPacketCount      = 0;
unsigned long lastPrfPacketMs = 0;
char pendingSpecialCmd[48]   = "";
unsigned long lastSpecialCmdMs = 0; // Chặn gói CMD thường 1.2s sau lệnh đặc biệt (tránh cắt ngang lệnh nấc tời)

// Trạng thái hiển thị 1 linh kiện: OK / VUOTDAI (cắm nhưng quá áp ADC) / THIEU (chưa cắm)
const char* hwStateTxt(bool ok, uint16_t bit) {
  if (ok) return "OK";
  if (uAdcFault & bit) return "VUOTDAI";
  return "THIEU";
}

// Biến tính toán Công suất thực (W) & Điện năng tiêu thụ tích lũy (Wh)
float uPowerWatt       = 0.0f;
float uPeakPowerWatt   = 0.0f;
float uEnergyWh        = 0.00f;
unsigned long lastEnergyCalcMs = 0;

// =========================================================================
// BIẾN QUẢN LÝ JOYSTICK VÔ CẤP L298N (KALMAN 1D + AUTO-SCAN CHÂN ADC1)
// =========================================================================
Kalman1D kalmanJoyY(0.04f, 0.14f);
Kalman1D kalmanJoyX(0.04f, 0.14f);
Kalman1D kalmanJoyW(0.04f, 0.14f);

int   activeJoyPinY       = JOY1_VRY_PIN;
int   activeJoyPinX       = JOY1_VRX_PIN;
bool  joy1HardwarePresent = false;
bool  joy2HardwarePresent = false;
float joyCenterVyMv       = 1650.0f;
float joyCenterVxMv       = 1650.0f;
float joyCenterVwMv       = 1650.0f;
float joyCurVyMv          = 0.0f;
float joyCurVxMv          = 0.0f;
float joyNormY            = 0.0f; // -1.0 .. +1.0 (Điều khiển tốc độ L298N)
float joyNormX            = 0.0f; // -1.0 .. +1.0 (Điều khiển góc lái)

SpeedGearMode speedGear   = GEAR_CRUISE; // Mặc định: CRUISE (82% công suất L298N)
bool headingHoldRequested = false;
const char* activeCtrlSrc = "IDLE";      // "JOY" / "WEB" / "SW" / "IDLE"

bool joySwWasPressed            = false;
unsigned long lastJoySwMs       = 0;
unsigned long lastJoyHotplugMs  = 0;

// Bộ đệm lịch sử 44 điểm để vẽ đồ thị sóng thời gian thực (Sparkline) trên Trang 2 TFT
#define GRAPH_PTS 44
float histPH[GRAPH_PTS];
float histTemp[GRAPH_PTS];
bool  histInit         = false;
bool  graphNeedsRedraw = true;
unsigned long lastGraphPushMs = 0;

// Thông số sóng LoRa & Độ nhiễu RF
bool  loraHardwareOk = false;
int   uRSSI          = -110;
float uSNR           = 0.0f;
int   uBgNoiseRssi   = -108;
uint32_t crcErrorCnt = 0;

// Quản lý kết nối & giao diện
unsigned long lastRxFromBuoy = 0;
unsigned long lastTxTick     = 0;
unsigned long lastUiTick     = 0;
uint32_t rxPacketCount       = 0;
uint32_t txCmdCount          = 0;

// 3 Trang hiển thị trực quan trên TFT (0: Dashboard Tổng hợp, 1: Đồ thị & 3 Tầng nước, 2: Chẩn đoán 12 Linh kiện)
int currentPage = 0;
bool autoModeRequested = false;

// Điều khiển từ xa qua Web Dashboard (Virtual Joystick 2D & Manual Override)
int webMotorOverride  = 0;
int webRudderOverride = 90;
int webWinchOverride  = 0;
unsigned long webOverrideExpireMs = 0;

// Hiệu chuẩn điện áp Pin từ xa gửi xuống Phao
volatile bool pendingCalibBatToBuoy = false;
float targetCalibBatVolt            = 12.1f;

// Lưu 3 tầng nước (Mặt - Giữa - Đáy): Khởi tạo valid = false (Chưa đo thật = không báo ảo!)
WaterLayerSample gcsLayerSurf = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false};
WaterLayerSample gcsLayerMid  = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false};
WaterLayerSample gcsLayerBot  = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false};

// =========================================================================
// BIẾN QUẢN LÝ ĐẨY DỮ LIỆU GOOGLE SHEETS (QUA SIM 4G & WIFI DUAL-MODE)
// =========================================================================
char gsheetScriptUrl[180]  = DEFAULT_GOOGLE_SCRIPT_URL;
char discordWebhookUrl[220] = DEFAULT_DISCORD_WEBHOOK_URL;
char staWifiSsid[48]       = DEFAULT_STA_WIFI_SSID;
char staWifiPass[48]       = DEFAULT_STA_WIFI_PASS;
char apWifiPass[48]        = DEFAULT_AP_WIFI_PASS;
char gcsAdminToken[48]     = DEFAULT_GCS_ADMIN_TOKEN;
unsigned long lastWifiScanMs = 0;
String cachedWifiScanJson  = "";
bool otaUploadAuthorized   = false;
bool otaHeaderChecked      = false;

volatile bool reqPushGSheetsTele    = false;
volatile bool reqPushGSheetsProfile = false;
volatile bool reqPushDiscordTest    = false;
volatile bool pendingSyncUrlToBuoy  = false;
uint32_t gsheetPushCount            = 0;
int      gsheetLastStatus           = 0;
unsigned long lastGsheetTriggerMs   = 0;

uint32_t discordPushCount           = 0;
int      discordLastStatus          = 0;
unsigned long lastDiscordPushMs     = 0;

enum GcsSimState : uint8_t {
  GSIM_IDLE = 0,
  GSIM_TERM_PREV,
  GSIM_INIT,
  GSIM_SSL_CFG,
  GSIM_HTTPSSL,
  GSIM_PARA_REDIR,
  GSIM_PARA_URL,
  GSIM_ACTION,
  GSIM_WAIT,
  GSIM_READ,
  GSIM_WAIT_READ,
  GSIM_TERM
};
GcsSimState   gcsSimState      = GSIM_IDLE;
int           gcsSimLastDataLen = 0;
unsigned long gcsSimTick       = 0;
unsigned long gcsSimRetryAfterMs = 0;
uint8_t       gcsSimInitStep   = 0;
unsigned long gcsSimInitTick   = 0;
bool          gcsSimNetReady   = false;
bool          gcsSimIsProfile  = false;
bool          gcsSimModemAlive = false;
char          gcsSimRxBuf[160];
size_t        gcsSimRxIdx      = 0;
char          gcsUsbBuf[180];
size_t        gcsUsbIdx        = 0;

bool sw7WasPressed  = false;
bool sw7LongHandled = false;
unsigned long sw7PressStart = 0;
unsigned long sw7LastRelease = 0;
int  sw7ClickStreak = 0;

int joyDisconnectCounter    = 0;
float filteredMotorPWM      = 0.0f;
bool joySafetyNeutralPassed = false;

int prevMotorPWM  = 0;
int prevRudderAng = 90;
int prevWinchCmd  = 0;

// =========================================================================
// 3. HÀM KIỂM TRA & ĐỌC JOYSTICK PHẦN CỨNG (LỌC TRUNG VỊ 5 MẪU + CHỐNG MẤT KẾT NỐI ẢO)
// =========================================================================
float readJoystickFastMv(int pin) {
  // Lọc trung vị 5 mẫu siêu tốc (loại bỏ 100% gai điện áp ADC ESP32 và nhiễu tiếp xúc)
  uint32_t buf[5];
  for (int i = 0; i < 5; i++) {
    buf[i] = analogReadMilliVolts(pin);
    delayMicroseconds(60);
  }
  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 5; j++) {
      if (buf[i] > buf[j]) {
        uint32_t temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
  return (float)buf[2];
}

void calibrateJoystickCenterNow() {
  // Tự động quét 2 cặp chân ADC1: Cặp 1 chuẩn (GPIO 34 & 35), Cặp 2 (GPIO 36 & 39)
  const int joyPairs[2][2] = {
    {JOY1_VRY_PIN, JOY1_VRX_PIN},
    {36, 39}
  };

  joy1HardwarePresent = false;
  for (int p = 0; p < 2; p++) {
    int py = joyPairs[p][0];
    int px = joyPairs[p][1];

    float sumY = 0, sumX = 0;
    for (int k = 0; k < 8; k++) {
      sumY += readJoystickFastMv(py);
      sumX += readJoystickFastMv(px);
      delayMicroseconds(300);
    }
    float vy = sumY / 8.0f;
    float vx = sumX / 8.0f;

    // Chân không cắm (hở mạch) trên ESP32 ADC1 có áp rò đặc trưng 105..195mV
    // Chân cắm biến trở (3.3V hoặc 5V) có điện áp từ 350mV đến 3150mV
    bool isFloatingY = (vy >= 95.0f && vy <= 205.0f);
    bool isFloatingX = (vx >= 95.0f && vx <= 205.0f);
    bool validRangeY = (vy >= 350.0f && vy <= 3150.0f);
    bool validRangeX = (vx >= 350.0f && vx <= 3150.0f);

    if (!isFloatingY && !isFloatingX && validRangeY && validRangeX) {
      joy1HardwarePresent     = true;
      activeJoyPinY           = py;
      activeJoyPinX           = px;
      joyCenterVyMv           = vy;
      joyCenterVxMv           = vx;
      kalmanJoyY.reset(vy);
      kalmanJoyX.reset(vx);
      joyDisconnectCounter    = 0;
      joySafetyNeutralPassed  = false; // Yêu cầu trả cần về tâm trước khi tăng ga
      Serial.printf("[GCS-JOY] DA KET NOI Joystick L298N tren GPIO%d(VRy=%.0fmV) & GPIO%d(VRx=%.0fmV)!\n",
                    activeJoyPinY, joyCenterVyMv, activeJoyPinX, joyCenterVxMv);
      break;
    }
  }

  if (!joy1HardwarePresent) {
    Serial.println("[GCS-JOY] CHUA CAM Joystick phan cung tren GPIO34/35 (hoac 36/39) -> Su dung Web Joystick / Switch.");
  }

  joy2HardwarePresent = false;
}

float getSpeedGearScale() {
  if (speedGear == GEAR_ECO)   return 0.60f;
  if (speedGear == GEAR_TURBO) return 1.00f;
  return 0.82f; // GEAR_CRUISE
}

const char* getSpeedGearText() {
  if (speedGear == GEAR_ECO)   return "ECO";
  if (speedGear == GEAR_TURBO) return "TURBO";
  return "CRUISE";
}

bool readHardwareJoystick(int &outMotorPWM, int &outRudderAng, int &outWinchCmd) {
  unsigned long now = millis();
  outWinchCmd = WINCH_CMD_STOP;
  if (!joy1HardwarePresent) {
    if (now - lastJoyHotplugMs >= 1500) {
      lastJoyHotplugMs = now;
      calibrateJoystickCenterNow();
    }
    joyNormY = 0.0f;
    joyNormX = 0.0f;
    filteredMotorPWM = 0.0f;
    return false;
  }

  bool anyActive = false;

  if (joy1HardwarePresent) {
    float rawVy = readJoystickFastMv(activeJoyPinY);
    float rawVx = readJoystickFastMv(activeJoyPinX);

    // Phát hiện rút dây: Chỉ ngắt kết nối nếu CẢ 2 TRỤC đồng thời rớt vào vùng hở mạch liên tục > 25 lần (~500ms)
    bool isFloatingBoth = (rawVy >= 95.0f && rawVy <= 205.0f) && (rawVx >= 95.0f && rawVx <= 205.0f);
    bool isZeroBoth     = (rawVy < 30.0f && rawVx < 30.0f);

    if (isFloatingBoth || isZeroBoth) {
      joyDisconnectCounter++;
      if (joyDisconnectCounter >= 25) {
        joy1HardwarePresent     = false;
        joyNormY                = 0.0f;
        joyNormX                = 0.0f;
        filteredMotorPWM        = 0.0f;
        joySafetyNeutralPassed  = false;
        Serial.printf("[GCS-JOY] CANH BAO: Da rut day Joystick (Y=%.0fmV X=%.0fmV float=%d zero=%d) -> Dung an toan!\n",
                      rawVy, rawVx, (int)isFloatingBoth, (int)isZeroBoth);
        return false;
      }
    } else {
      joyDisconnectCounter = 0;
    }

    joyCurVyMv = kalmanJoyY.update(rawVy);
    joyCurVxMv = kalmanJoyX.update(rawVx);

    // Deadzone trung tâm 120mV chống rung trôi cần, expoFactor 0.28 lái êm ở dải đầu
    joyNormY = mapJoystickAxisExpo(joyCurVyMv, joyCenterVyMv, 80.0f, 3200.0f, 120.0f, 0.28f);
    joyNormX = mapJoystickAxisExpo(joyCurVxMv, joyCenterVxMv, 80.0f, 3200.0f, 120.0f, 0.28f);

    // Khóa an toàn khởi động (Neutral Safety): Bắt buộc nhả cần về tâm trước khi cho chạy
    if (!joySafetyNeutralPassed) {
      if (fabsf(joyNormY) < 0.06f && fabsf(joyNormX) < 0.06f) {
        joySafetyNeutralPassed = true;
      } else {
        outMotorPWM  = 0;
        outRudderAng = 90;
        return false;
      }
    }

    float scale = getSpeedGearScale();
    int targetMotorPWM = 0;
    int targetRudderAng = 90;

    if (fabsf(joyNormY) > 0.03f || fabsf(joyNormX) > 0.03f) {
      anyActive = true;
      targetMotorPWM  = constrain((int)(joyNormY * 255.0f * scale), -255, 255);
      targetRudderAng = constrain((int)(90.0f + joyNormX * 45.0f), 45, 135);
    }

    // Chế độ Smooth Throttle: Tăng/giảm tốc mượt mà chống sốc dòng L298N và chống sụt áp
    filteredMotorPWM += 0.35f * ((float)targetMotorPWM - filteredMotorPWM);
    if (fabsf(filteredMotorPWM) < 4.0f && targetMotorPWM == 0) filteredMotorPWM = 0.0f;

    outMotorPWM  = (int)filteredMotorPWM;
    outRudderAng = targetRudderAng;
  }

  return anyActive;
}

// =========================================================================
// 4. HÀM BỔ TRỢ QUẢN LÝ SPI, ĐỒ THỊ SPARKLINE & TÍNH % LINH KIỆN THỰC TẾ
// =========================================================================
inline void selectTFT() {
  digitalWrite(LORA_NSS_PIN, HIGH);
}

inline void selectLoRa() {
  digitalWrite(TFT_CS_PIN, HIGH);
}

void pushGraphSample(float phVal, float tempVal) {
  if (!histInit) {
    for (int i = 0; i < GRAPH_PTS; i++) {
      histPH[i]   = phVal;
      histTemp[i] = tempVal;
    }
    histInit         = true;
    graphNeedsRedraw = true;
    return;
  }
  for (int i = 0; i < GRAPH_PTS - 1; i++) {
    histPH[i]   = histPH[i + 1];
    histTemp[i] = histTemp[i + 1];
  }
  histPH[GRAPH_PTS - 1]   = phVal;
  histTemp[GRAPH_PTS - 1] = tempVal;
  graphNeedsRedraw        = true;
}

int countConnectedHwBits(unsigned int mask) {
  int bits = 0;
  for (int i = 0; i < HW_TOTAL_COMPONENTS; i++) {
    if (mask & (1U << i)) bits++;
  }
  return bits;
}

int calcHwHealthPercent(unsigned int mask) {
  return (countConnectedHwBits(mask) * 100) / HW_TOTAL_COMPONENTS;
}

// Tạo chuỗi liệt kê các linh kiện đang CHƯA CẮM / MẤT KẾT NỐI
void buildMissingHwString(unsigned int mask, char* outBuf, size_t maxLen) {
  outBuf[0] = '\0';
  struct Item { uint16_t bit; const char* name; };
  static const Item items[] = {
    {HW_OK_DS18B20, "DS18B20"},
    {HW_OK_PH,      "pH"},
    {HW_OK_TDS,     "TDS"},
    {HW_OK_NTU,     "NTU"},
    {HW_OK_ACS712,  "ACS712"},
    {HW_OK_BAT3S,   "Pin3S"},
    {HW_OK_MPU6050, "MPU6050"},
    {HW_OK_GY271,   "GY271"},
    {HW_OK_LORA,    "LoRa"},
    {HW_OK_SIM4G,   "SIM4G"},
    {HW_OK_GPS,     "GPS"}
  };
  size_t len = 0;
  int missingCnt = 0;
  for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
    if ((mask & items[i].bit) == 0) {
      int n = snprintf(outBuf + len, maxLen - len, "%s%s", (missingCnt > 0) ? "," : "", items[i].name);
      if (n > 0 && len + (size_t)n < maxLen) len += (size_t)n;
      missingCnt++;
    }
  }
  if (missingCnt == 0) {
    snprintf(outBuf, maxLen, "DU 11/11 LINH KIEN OK!");
  }
}

bool isGoogleUrlConfigured() {
  return isValidGoogleScriptUrl(gsheetScriptUrl);
}

// Thoát ký tự đặc biệt để chuỗi luôn hợp lệ khi nhúng vào JSON.
// SSID/Wi-Fi hoặc URL có dấu " hoặc \ sẽ làm hỏng toàn bộ JSON nếu không thoát.
void jsonEscapeInto(char* out, size_t outLen, const char* in) {
  if (!out || outLen == 0) return;
  size_t o = 0;
  for (size_t i = 0; in && in[i] != '\0' && (o + 3) < outLen; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c == '"' || c == '\\') {
      out[o++] = '\\';
      out[o++] = (char)c;
    } else if (c == '\n') {
      out[o++] = '\\'; out[o++] = 'n';
    } else if (c == '\r') {
      out[o++] = '\\'; out[o++] = 'r';
    } else if (c == '\t') {
      out[o++] = '\\'; out[o++] = 't';
    } else if (c < 0x20) {
      // Bỏ ký tự điều khiển không hợp lệ trong JSON
    } else {
      out[o++] = (char)c;
    }
  }
  out[o] = '\0';
}

void buildGoogleSheetsFullUrl(char* outBuf, size_t maxLen, bool isProfile, const char* srcName) {
  if (isProfile) {
    snprintf(
      outBuf, maxLen,
      "%s?mode=profile&ph1=%.2f&tds1=%.0f&ntu1=%.0f&t1=%.1f&do1=%.2f"
      "&ph2=%.2f&tds2=%.0f&ntu2=%.0f&t2=%.1f&do2=%.2f"
      "&ph3=%.2f&tds3=%.0f&ntu3=%.0f&t3=%.1f&do3=%.2f&src=%s&fw=%s",
      gsheetScriptUrl,
      gcsLayerSurf.ph, gcsLayerSurf.tds, gcsLayerSurf.ntu, gcsLayerSurf.temp, gcsLayerSurf.doMgL,
      gcsLayerMid.ph,  gcsLayerMid.tds,  gcsLayerMid.ntu,  gcsLayerMid.temp,  gcsLayerMid.doMgL,
      gcsLayerBot.ph,  gcsLayerBot.tds,  gcsLayerBot.ntu,  gcsLayerBot.temp,  gcsLayerBot.doMgL,
      srcName, FW_VERSION
    );
  } else {
    bool alarmNow = (uAlertFlags & (ALERT_MOTOR_JAM | ALERT_HIGH_TILT | ALERT_WATER_QUAL | ALERT_LOW_BATTERY)) != 0;
    snprintf(
      outBuf, maxLen,
      "%s?mode=tele&seq=%lu&ph=%.2f&do=%.2f&tds=%.0f&ntu=%.0f"
      "&temp=%.1f&watt=%.1f&amp=%.2f&bat=%.2f&roll=%.1f&pitch=%.1f&hdg=%.0f"
      "&lat=%.5f&lng=%.5f&rssi=%d&snr=%.1f&hw=%d&hwm=%u&alr=%u&wqi=%d&dep=%.2f&ec=%.0f"
      "&sats=%u&spd=%.2f&alt=%.1f&homed=%.1f&flags2=%u&tgthdg=%.0f&alarm=%d&src=%s&fw=%s",
      gsheetScriptUrl, (unsigned long)uS3SeqId,
      uPH, uDO, uTDS, uNTU, uTemp, uPowerWatt, uAmp, uBat,
      uRoll, uPitch, uHeading, uLat, uLng, uRSSI, uSNR,
      calcHwHealthPercent(uHwHealth), uHwHealth, uAlertFlags, uWQI, uDepthM, uEC,
      uSats, uGpsSpeedKmph, uGpsAltM, uHomeDistanceM, uFlags2, uTargetHeadingDeg,
      alarmNow ? 1 : 0, srcName, FW_VERSION
    );
  }
}

const char* getCompassDir(float heading) {
  if (heading >= 337.5f || heading < 22.5f)  return "B";
  if (heading < 67.5f)                       return "DB";
  if (heading < 112.5f)                      return "D";
  if (heading < 157.5f)                      return "DN";
  if (heading < 202.5f)                      return "N";
  if (heading < 247.5f)                      return "TN";
  if (heading < 292.5f)                      return "T";
  return "TB";
}

const char* getNoiseLevelText(float snr, int rssi, uint16_t &outColor) {
  if (snr >= 5.0f && rssi >= -90) {
    outColor = C_GREEN;
    return "SACH";
  } else if (snr >= -2.0f && rssi >= -102) {
    outColor = C_YELLOW;
    return "NHIEU NHE";
  } else {
    outColor = C_RED;
    return "NHIEU CAO";
  }
}

const char* getAutoStageShortText(int stage) {
  switch (stage) {
    case AUTO_STAGE_HOMING_START: return "VE HOME 0m";
    case AUTO_STAGE_MOVE_SURF:    return "HA MAT 0.3m";
    case AUTO_STAGE_WAIT_SURF:    return "DO MAT 0.3m";
    case AUTO_STAGE_MOVE_MID:     return "HA GIUA 1m";
    case AUTO_STAGE_WAIT_MID:     return "DO GIUA 1m";
    case AUTO_STAGE_MOVE_BOT:     return "HA DAY 1.8m";
    case AUTO_STAGE_LIFT_BOT:     return "NHAC BUN 5cm";
    case AUTO_STAGE_WAIT_BOT:     return "DO DAY 1.8m";
    case AUTO_STAGE_RETURN_HOME:  return "THU VE HOME";
    case AUTO_STAGE_COMPLETED:    return "HOAN TAT 3T";
    default:                      return "L298N + JOY";
  }
}

void drawProgressBar(int x, int y, int w, int h, int percent, uint16_t fillCol) {
  percent = constrain(percent, 0, 100);
  int fillW = ((w - 2) * percent) / 100;
  tft.drawRect(x, y, w, h, C_DARKGREY);
  if (fillW > 0) {
    tft.fillRect(x + 1, y + 1, fillW, h - 2, fillCol);
  }
  if (fillW < w - 2) {
    tft.fillRect(x + 1 + fillW, y + 1, (w - 2) - fillW, h - 2, C_BLACK);
  }
}

// =========================================================================
// 5. VẼ KHUNG TĨNH 3 TRANG CHUẨN ĐỨNG 240x320 (SẮC NÉT, TRỰC QUAN KHKT)
// =========================================================================
void drawStaticScreenPage0() {
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

    // 4. Thuyen Catamaran USV 3D Chan Thuc (Bien doi 3D Euler: Roll quanh X, Pitch quanh Z, Heading quanh Y)
    float cR = cosf(uRoll * 0.0174532925f),  sR = sinf(uRoll * 0.0174532925f);
    float cP = cosf(uPitch * 0.0174532925f), sP = sinf(uPitch * 0.0174532925f);
    float cH = cosf(uHeading * 0.0174532925f), sH = sinf(uHeading * 0.0174532925f);

    auto transformBoat = [&](float bx, float by, float bz, float &ox, float &oy, float &oz) {
      float x1 = bx * cR - by * sR;
      float y1 = bx * sR + by * cR;
      float y2 = y1 * cP - bz * sP;
      float z2 = y1 * sP + bz * cP;
      ox = x1 * cH - z2 * sH;
      oy = waterY + y2;
      oz = x1 * sH + z2 * cH;
    };

    auto drawBoatLineSpr = [&](float x1, float y1, float z1, float x2, float y2, float z2, uint16_t col) {
      float ox1, oy1, oz1, ox2, oy2, oz2;
      transformBoat(x1, y1, z1, ox1, oy1, oz1);
      transformBoat(x2, y2, z2, ox2, oy2, oz2);
      drawLineSpr(ox1, oy1, oz1, ox2, oy2, oz2, col);
    };

    // Phao Trai (Catamaran Port Hull with pointed bow)
    drawBoatLineSpr(-6.5f, 1.2f, -8.0f, -6.5f, 1.2f, 5.5f, C_YELLOW);
    drawBoatLineSpr(-6.5f, 1.2f, 5.5f,  -5.0f, 1.2f, 9.5f, C_YELLOW);
    drawBoatLineSpr(-5.0f, 1.2f, 9.5f,  -3.5f, 1.2f, 5.5f, C_YELLOW);
    drawBoatLineSpr(-3.5f, 1.2f, 5.5f,  -3.5f, 1.2f, -8.0f, C_YELLOW);
    drawBoatLineSpr(-3.5f, 1.2f, -8.0f, -6.5f, 1.2f, -8.0f, C_YELLOW);

    // Phao Phai (Catamaran Starboard Hull with pointed bow)
    drawBoatLineSpr(3.5f, 1.2f, -8.0f, 3.5f, 1.2f, 5.5f, C_YELLOW);
    drawBoatLineSpr(3.5f, 1.2f, 5.5f,  5.0f, 1.2f, 9.5f, C_YELLOW);
    drawBoatLineSpr(5.0f, 1.2f, 9.5f,  6.5f, 1.2f, 5.5f, C_YELLOW);
    drawBoatLineSpr(6.5f, 1.2f, 5.5f,  6.5f, 1.2f, -8.0f, C_YELLOW);
    drawBoatLineSpr(6.5f, 1.2f, -8.0f, 3.5f, 1.2f, -8.0f, C_YELLOW);

    // San Trung Tam & Tam Pin Nguon PV Solar
    drawBoatLineSpr(-3.5f, 1.6f, -3.0f, 3.5f, 1.6f, -3.0f, C_WHITE);
    drawBoatLineSpr(-3.5f, 1.6f,  3.0f, 3.5f, 1.6f,  3.0f, C_WHITE);
    drawBoatLineSpr(-2.5f, 2.0f, -2.0f, 2.5f, 2.0f,  2.0f, C_CYAN);
    drawBoatLineSpr(-2.5f, 2.0f,  2.0f, 2.5f, 2.0f, -2.0f, C_CYAN);

    // Cot anten LoRa & Den Bao Hieu
    float antBaseX, antBaseY, antBaseZ, antTopX, antTopY, antTopZ;
    transformBoat(0.0f, 1.8f, -1.0f, antBaseX, antBaseY, antBaseZ);
    transformBoat(0.0f, 6.5f, -1.0f, antTopX, antTopY, antTopZ);
    drawLineSpr(antBaseX, antBaseY, antBaseZ, antTopX, antTopY, antTopZ, C_CYAN);
    Point2D pAnt = project3D(antTopX, antTopY, antTopZ, tft3dPitch, tft3dYaw, 108, 68, 1.45f, 110.0f);
    if (pAnt.ok && pAnt.x >= 2 && pAnt.x < 214 && pAnt.y >= 2 && pAnt.y < 142) {
      sprCanvas.fillCircle(pAnt.x, pAnt.y, 2, C_GREEN);
    }

    // 5. Chum Song Sieu Am Sonar 3D Cone & Cap Toi Do Sau
    float probeY = waterY - constrain(uDepthM * 11.5f, 0.0f, 27.0f);
    const float coneR = 8.5f;
    float cpx1, cpz1, cpx2, cpz2, cpx3, cpz3, cpx4, cpz4, dummyY;
    transformBoat(-coneR, 0.0f, 0.0f, cpx1, dummyY, cpz1);
    transformBoat( coneR, 0.0f, 0.0f, cpx2, dummyY, cpz2);
    transformBoat(0.0f, 0.0f, -coneR, cpx3, dummyY, cpz3);
    transformBoat(0.0f, 0.0f,  coneR, cpx4, dummyY, cpz4);
    drawLineSpr(antBaseX, waterY, antBaseZ, cpx1, probeY, cpz1, 0x0210);
    drawLineSpr(antBaseX, waterY, antBaseZ, cpx2, probeY, cpz2, 0x0210);
    drawLineSpr(antBaseX, waterY, antBaseZ, cpx3, probeY, cpz3, 0x0210);
    drawLineSpr(antBaseX, waterY, antBaseZ, cpx4, probeY, cpz4, 0x0210);

    // Cap toi va qua roi long do cam bien
    drawLineSpr(antBaseX, antBaseY, antBaseZ, antBaseX, probeY, antBaseZ, C_WHITE);
    Point2D pProbe = project3D(antBaseX, probeY, antBaseZ, tft3dPitch, tft3dYaw, 108, 68, 1.45f, 110.0f);
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
  snprintf(buf, sizeof(buf), "Mui: %.0fd (%s) | Pin: %.2fV %s",
           uHeading, getCompassDir(uHeading), uBat, uVirtualAnchorActive ? "[NEO AO]" : (uReturnHomeActive ? "[VE HOME]" : (okBAT ? "[PIN OK]" : "")));
  tft.drawString(buf, 8, 209, 1);

  tft.setTextColor(C_GREY, C_BLACK);
  snprintf(buf, sizeof(buf), "Goc 3D: Yaw:%.0fd Pitch:%.0fd | Joy: Xoay", tft3dYaw, tft3dPitch);
  tft.drawString(buf, 8, 222, 1);
  tft.setTextPadding(0);
}

void drawRadarAutopilotTFT() {
  selectTFT();
  bool gpsOkTft = (uSats > 0) && (uLat != 0.0 || uLng != 0.0);

  if (sprCanvasCreated) {
    sprCanvas.fillSprite(C_BLACK);
    const int cx = 108, cy = 70;

    // Cac vong cu ly radar quang học (20m, 40m, 60m)
    sprCanvas.drawCircle(cx, cy, 60, C_DARKGREY);
    sprCanvas.drawCircle(cx, cy, 40, C_DARKGREY);
    sprCanvas.drawCircle(cx, cy, 20, C_DARKGREY);
    sprCanvas.drawFastHLine(cx - 62, cy, 124, C_DARKGREY);
    sprCanvas.drawFastVLine(cx, cy - 62, 124, C_DARKGREY);

    // Tia quet Radar 360 do song dong (Animated Tactical Sweep Beam)
    float sweepAng = millis() * 0.0028f;
    int sx  = cx + (int)(60.0f * cosf(sweepAng));
    int sy  = cy + (int)(60.0f * sinf(sweepAng));
    int sx2 = cx + (int)(58.0f * cosf(sweepAng - 0.12f));
    int sy2 = cy + (int)(58.0f * sinf(sweepAng - 0.12f));
    sprCanvas.drawLine(cx, cy, sx2, sy2, 0x028A);
    sprCanvas.drawLine(cx, cy, sx,  sy,  0x05BF);

    // Cac ve tinh GPS quanh quy dao radar khi co FIX thuc te
    if (uSats > 0) {
      int maxSat = (uSats > 8) ? 8 : (int)uSats;
      for (int s = 0; s < maxSat; s++) {
        float sAng = (s * 45.0f + 15.0f) * 0.0174532925f;
        float sDist = 32.0f + ((s % 3) * 11.0f);
        int satX = cx + (int)(sDist * cosf(sAng));
        int satY = cy + (int)(sDist * sinf(sAng));
        sprCanvas.fillCircle(satX, satY, 2, C_GREEN);
      }
    }

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

    // Diem moc HOME & vector phuong vi (chi ve khi GPS da FIX that)
    if (gpsOkTft && (uHomeDistanceM > 0.5f || (homeLat != 0.0 && homeLng != 0.0))) {
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
  if (gpsOkTft) snprintf(buf, sizeof(buf), "GPS: %.5f, %.5f (%u VT)", uLat, uLng, uSats);
  else          snprintf(buf, sizeof(buf), "GPS: CHUA FIX (0 VT)");
  tft.drawString(buf, 8, 196, 1);

  tft.setTextColor(C_YELLOW, C_BLACK);
  if (gpsOkTft) snprintf(buf, sizeof(buf), "Cach Home: %.1fm | Toc do: %.1f km/h", uHomeDistanceM, uGpsSpeedKmph);
  else          snprintf(buf, sizeof(buf), "Cach Home: -- | Toc do: --");
  tft.drawString(buf, 8, 209, 1);

  tft.setTextColor(C_GREEN, C_BLACK);
  const char* autoTxt = uReturnHomeActive ? "TU VE HOME GPS" : ((uAlertFlags & ALERT_HEADING_HOLD) ? "GIU HUONG PID" : "DIEU KHIEN TAY");
  if (gpsOkTft) snprintf(buf, sizeof(buf), "Che do: %s | Cao do: %.1fm", autoTxt, uGpsAltM);
  else          snprintf(buf, sizeof(buf), "Che do: %s | Cao do: --", autoTxt);
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
  int bx = 158, by = 4, bw = 24, bh = 11;
  tft.drawRect(bx, by, bw, bh, C_WHITE);
  tft.fillRect(bx + bw, by + 3, 2, 5, C_WHITE); // Cuc duong pin
  int fillW = constrain((batPct * (bw - 4)) / 100, 0, bw - 4);
  uint16_t batCol = (batPct >= 50) ? C_GREEN : ((batPct >= 20) ? C_YELLOW : C_RED);
  if (fillW > 0) tft.fillRect(bx + 2, by + 2, fillW, bh - 4, batCol);
  if (fillW < bw - 4) tft.fillRect(bx + 2 + fillW, by + 2, (bw - 4) - fillW, bh - 4, C_BLACK);

  // So von va % ben canh thoi pin (bo tri thoang sach dep)
  tft.setTextPadding(36);
  tft.setTextColor(batCol, C_NAVY_BG);
  snprintf(buf, sizeof(buf), "%.1fV", uBat);
  tft.drawString(buf, 188, 4, 1);
  snprintf(buf, sizeof(buf), "%d%%", batPct);
  tft.drawString(buf, 188, 14, 1);

  // Dai trang thai duoi header: So linh kien & So trang
  tft.setTextPadding(95);
  tft.setTextColor(C_CYAN, C_NAVY_BG);
  snprintf(buf, sizeof(buf), "LK:%d/11 %s", okCount, (okCount == HW_TOTAL_COMPONENTS) ? "FULL" : "THIEU");
  tft.drawString(buf, 5, 20, 1);

  tft.setTextPadding(34);
  tft.setTextColor(C_YELLOW, C_NAVY_BG);
  snprintf(buf, sizeof(buf), "P%d/5", currentPage + 1);
  tft.drawString(buf, 208, 20, 1);

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

  const int gw = 216, gh = 62;
  if (sprCanvasCreated) {
    sprCanvas.fillSprite(C_BLACK);
    sprCanvas.drawRect(0, 0, gw, gh, C_DARKGREY);
    for (int x = 4; x < gw - 2; x += 10) {
      sprCanvas.drawPixel(x, gh / 2, C_DARKGREY);
    }
    if (histInit) {
      for (int i = 0; i < GRAPH_PTS - 1; i++) {
        int x1 = 2 + (i * (gw - 4)) / (GRAPH_PTS - 1);
        int x2 = 2 + ((i + 1) * (gw - 4)) / (GRAPH_PTS - 1);

        int yPh1 = gh - 3 - (int)(constrain((histPH[i] - 5.0f) / 4.5f, 0.0f, 1.0f) * (gh - 6));
        int yPh2 = gh - 3 - (int)(constrain((histPH[i + 1] - 5.0f) / 4.5f, 0.0f, 1.0f) * (gh - 6));
        sprCanvas.drawLine(x1, yPh1, x2, yPh2, C_GREEN);

        int yT1 = gh - 3 - (int)(constrain((histTemp[i] - 15.0f) / 25.0f, 0.0f, 1.0f) * (gh - 6));
        int yT2 = gh - 3 - (int)(constrain((histTemp[i + 1] - 15.0f) / 25.0f, 0.0f, 1.0f) * (gh - 6));
        sprCanvas.drawLine(x1, yT1, x2, yT2, C_CYAN);
      }
    }
    sprCanvas.pushSprite(12, 54);
  } else {
    const int gx = 10, gy = 54;
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
    const char* dSrc = (uFlags2 & FLAG2_DEPTH_REAL_MS5837) ? "MS5837 Thuy Tinh" :
                       ((uFlags2 & FLAG2_DEPTH_REAL_ENCODER) ? "Encoder AS5600" : "Uoc Tinh Toi");
    snprintf(buf, sizeof(buf), "%s | %s", getAutoStageShortText(uAutoStage), dSrc);
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
    // 4 O CHI SO NUOC SAC NET KEM THANH DO DONG VA KIM DINH VI (Y = 136..234)
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

    // Thanh do pH da dai & con tro kim dinh vi gia tri thuc te
    tft.fillRect(7, 174, 106, 5, C_BLACK);
    if (okPH) {
      tft.fillRect(7, 175, 30, 3, C_RED);    // Axit < 6.0
      tft.fillRect(37, 175, 42, 3, C_GREEN); // Chuan QCVN 6.0 - 8.5
      tft.fillRect(79, 175, 34, 3, C_CYAN);  // Kiem > 8.5
      int nX = constrain(7 + (int)((uPH / 14.0f) * 104.0f), 7, 111);
      tft.fillRect(nX - 1, 173, 3, 7, C_WHITE);
    } else {
      tft.fillRect(7, 176, 106, 2, C_DARKGREY);
    }

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

    // Thanh do TDS da dai & con tro kim dinh vi gia tri thuc te
    tft.fillRect(126, 174, 106, 5, C_BLACK);
    if (okTDS) {
      tft.fillRect(126, 175, 24, 3, C_CYAN);   // Tinh khiet < 150ppm
      tft.fillRect(150, 175, 44, 3, C_GREEN);  // Tieu chuan 150 - 500ppm
      tft.fillRect(194, 175, 24, 3, C_YELLOW); // Trung binh 500 - 800ppm
      tft.fillRect(218, 175, 14, 3, C_RED);    // O nhiem > 800ppm
      int nX = constrain(126 + (int)((uTDS / 800.0f) * 104.0f), 126, 230);
      tft.fillRect(nX - 1, 173, 3, 7, C_WHITE);
    } else {
      tft.fillRect(126, 176, 106, 2, C_DARKGREY);
    }

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

    // Thanh do NTU da dai & con tro kim dinh vi gia tri thuc te
    tft.fillRect(7, 225, 106, 5, C_BLACK);
    if (okNTU) {
      tft.fillRect(7, 226, 26, 3, C_CYAN);   // Rat trong < 20 NTU
      tft.fillRect(33, 226, 38, 3, C_GREEN);  // An toan 20 - 50 NTU
      tft.fillRect(71, 226, 26, 3, C_YELLOW); // Duc nhe 50 - 100 NTU
      tft.fillRect(97, 226, 16, 3, C_RED);    // Duc bun > 100 NTU
      int nX = constrain(7 + (int)((uNTU / 150.0f) * 104.0f), 7, 111);
      tft.fillRect(nX - 1, 224, 3, 7, C_WHITE);
    } else {
      tft.fillRect(7, 227, 106, 2, C_DARKGREY);
    }

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

    // Thanh do Nhiet do da dai & con tro kim dinh vi gia tri thuc te
    tft.fillRect(126, 225, 106, 5, C_BLACK);
    if (okDS) {
      tft.fillRect(126, 226, 26, 3, C_BLUE);   // Mat < 20°C
      tft.fillRect(152, 226, 52, 3, C_GREEN);  // Toi uu 20 - 30°C
      tft.fillRect(204, 226, 28, 3, C_RED);    // Nong > 30°C
      int nX = constrain(126 + (int)(((uTemp - 15.0f) / 25.0f) * 104.0f), 126, 230);
      tft.fillRect(nX - 1, 224, 3, 7, C_WHITE);
    } else {
      tft.fillRect(126, 227, 106, 2, C_DARKGREY);
    }

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

bool sendDiscordWebhookFromGCS(bool isProfile, bool isAlert, const char* customMsg) {
  if (WiFi.status() != WL_CONNECTED) {
    discordLastStatus = -2; // Mat ket noi WiFi STA
    return false;
  }
  if (strlen(discordWebhookUrl) < 30) {
    discordLastStatus = -3; // Chua cau hinh Discord Webhook URL
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(8);

  HTTPClient https;
  https.setTimeout(9000);
  https.setConnectTimeout(6000);

  if (!https.begin(client, discordWebhookUrl)) {
    discordLastStatus = -1;
    return false;
  }
  https.addHeader("Content-Type", "application/json");

  const char* title = customMsg ? customMsg : (
    isAlert ? "🚨 [CẢNH BÁO KHẨN CẤP] USV PHÁT HIỆN SỰ CỐ TẠI HIỆN TRƯỜNG!" :
    (isProfile ? "🔬 [HỒ SƠ ĐO SÂU 3 TẦNG] KẾT QUẢ QUAN TRẮC ĐỘ SÂU (MẶT - GIỮA - ĐÁY)" :
                 "🌊 [USV AQUA-MONITOR] BÁO CÁO GIÁM SÁT MÔI TRƯỜNG NƯỚC & THIẾT BỊ")
  );
  int color = (isAlert || (uAlertFlags != 0)) ? 15158332 : (isProfile ? 3447003 : 3066993);

  char* jsonBuf = (char*)malloc(2600);
  if (!jsonBuf) {
    https.end();
    discordLastStatus = -4;
    return false;
  }

  // Chỉ báo số liệu khi Phao đang online và cảm biến tương ứng ĐÃ CẮM.
  // Không báo số 0 giả khi mất sóng hoặc thiếu linh kiện.
  bool hasTele = (lastRxFromBuoy > 0) && ((millis() - lastRxFromBuoy) < 10000UL);
  bool okPH    = hasTele && (uHwHealth & HW_OK_PH)      && uPH   > 0.0f;
  bool okTDS   = hasTele && (uHwHealth & HW_OK_TDS)     && uTDS  > 0.0f;
  bool okNTU   = hasTele && (uHwHealth & HW_OK_NTU)     && uNTU  > 0.0f;
  bool okTemp  = hasTele && (uHwHealth & HW_OK_DS18B20) && uTemp > 0.0f;
  bool okDO    = hasTele && uDO > 0.0f;
  bool okEC    = hasTele && uEC > 0.0f;
  bool okWQI   = hasTele && uWQI > 0;
  bool okBat   = hasTele && (uHwHealth & HW_OK_BAT3S)   && uBat > 0.0f;
  bool okAmp   = hasTele && (uHwHealth & HW_OK_ACS712) != 0;
  bool okImu   = hasTele && (uHwHealth & HW_OK_MPU6050) != 0;
  bool okMag   = hasTele && (uHwHealth & HW_OK_GY271)   != 0;
  bool gpsOk   = hasTele && (uHwHealth & HW_OK_GPS) && uSats > 0 && (uLat != 0.0f || uLng != 0.0f);

  char sPH[24], sDO[28], sTDS[24], sEC[28], sNTU[24], sTemp[24], sWQI[16];
  if (okPH)   snprintf(sPH,   sizeof(sPH),   "%.2f", uPH);        else copyStringSafe(sPH,   sizeof(sPH),   "--");
  if (okDO)   snprintf(sDO,   sizeof(sDO),   "%.2f mg/L", uDO);   else copyStringSafe(sDO,   sizeof(sDO),   "--");
  if (okTDS)  snprintf(sTDS,  sizeof(sTDS),  "%.0f ppm", uTDS);   else copyStringSafe(sTDS,  sizeof(sTDS),  "--");
  if (okEC)   snprintf(sEC,   sizeof(sEC),   "%.0f µS/cm", uEC);  else copyStringSafe(sEC,   sizeof(sEC),   "--");
  if (okNTU)  snprintf(sNTU,  sizeof(sNTU),  "%.0f NTU", uNTU);   else copyStringSafe(sNTU,  sizeof(sNTU),  "--");
  if (okTemp) snprintf(sTemp, sizeof(sTemp), "%.1f °C", uTemp);   else copyStringSafe(sTemp, sizeof(sTemp), "--");
  if (okWQI)  snprintf(sWQI,  sizeof(sWQI),  "%d/100", uWQI);     else copyStringSafe(sWQI,  sizeof(sWQI),  "--");

  char waterField[440];
  snprintf(waterField, sizeof(waterField),
    "• **pH**: `%s` | **DO**: `%s`\\n"
    "• **TDS**: `%s` | **EC**: `%s`\\n"
    "• **Độ đục**: `%s` | **Nhiệt độ**: `%s`\\n"
    "• **Chỉ số VN-WQI**: **%s**",
    sPH, sDO, sTDS, sEC, sNTU, sTemp, sWQI
  );

  char sBat[32], sAmp[24], sWatt[24], sDepth[24];
  if (okBat) snprintf(sBat, sizeof(sBat), "%.2f V (%d%%)", uBat, calcBattery3SPercent(uBat));
  else       copyStringSafe(sBat, sizeof(sBat), "--");
  if (okAmp) snprintf(sAmp, sizeof(sAmp), "%.2f A", uAmp);
  else       copyStringSafe(sAmp, sizeof(sAmp), "--");
  if (okBat && okAmp) snprintf(sWatt, sizeof(sWatt), "%.2f W", uPowerWatt);
  else                copyStringSafe(sWatt, sizeof(sWatt), "--");
  if (hasTele) snprintf(sDepth, sizeof(sDepth), "%.2f m", uDepthM);
  else         copyStringSafe(sDepth, sizeof(sDepth), "--");

  char pwrField[340];
  snprintf(pwrField, sizeof(pwrField),
    "• **Điện áp Pin**: `%s`\\n"
    "• **Dòng điện**: `%s`\\n"
    "• **Công suất**: `%s`\\n"
    "• **Độ sâu tời**: `%s`",
    sBat, sAmp, sWatt, sDepth
  );

  char sGps[64], sSats[80], sHome[24], sHdg[48];
  if (gpsOk) snprintf(sGps,  sizeof(sGps),  "%.5f, %.5f", uLat, uLng);
  else       copyStringSafe(sGps, sizeof(sGps), "Chưa có GPS FIX");
  if (gpsOk) snprintf(sSats, sizeof(sSats), "%u SV | Tốc độ: %.1f km/h", (unsigned)uSats, uGpsSpeedKmph);
  else       copyStringSafe(sSats, sizeof(sSats), "--");
  if (gpsOk) snprintf(sHome, sizeof(sHome), "%.1f m", uHomeDistanceM);
  else       copyStringSafe(sHome, sizeof(sHome), "--");
  if (okMag) snprintf(sHdg,  sizeof(sHdg),  "%.0f° (Mục tiêu: %.0f°)", uHeading, uTargetHeadingDeg);
  else       copyStringSafe(sHdg, sizeof(sHdg), "--");

  char gpsField[400];
  snprintf(gpsField, sizeof(gpsField),
    "• **Tọa độ**: `%s`\\n"
    "• **Vệ tinh**: `%s`\\n"
    "• **Cách Home**: `%s`\\n"
    "• **La bàn**: `%s`",
    sGps, sSats, sHome, sHdg
  );

  char sRf[64], sImu[48];
  if (hasTele) snprintf(sRf, sizeof(sRf), "%d dBm | SNR: %.1f dB", uRSSI, uSNR);
  else         copyStringSafe(sRf, sizeof(sRf), "--");
  if (okImu) snprintf(sImu, sizeof(sImu), "%.1f° / %.1f°", uRoll, uPitch);
  else       copyStringSafe(sImu, sizeof(sImu), "--");

  char rfField[340];
  snprintf(rfField, sizeof(rfField),
    "• **LoRa**: `%s`\\n"
    "• **Gói tin #Seq**: `#%lu` | **Uptime GCS**: `%lu s`\\n"
    "• **Góc Roll/Pitch**: `%s`",
    sRf, (unsigned long)uS3SeqId, millis() / 1000UL, sImu
  );

  char hwField[450];
  snprintf(hwField, sizeof(hwField),
    "%s GPS • %s Compass • %s IMU • %s LoRa\\n"
    "%s pH • %s TDS • %s Nhiệt độ • %s Độ đục\\n"
    "%s ACS712 Dòng • %s Đo Pin 3S • %s 4G SIM",
    (uHwHealth & HW_OK_GPS)     ? "`[OK]`" : "`[LỖI]`",
    (uHwHealth & HW_OK_GY271)   ? "`[OK]`" : "`[LỖI]`",
    (uHwHealth & HW_OK_MPU6050) ? "`[OK]`" : "`[LỖI]`",
    (uHwHealth & HW_OK_LORA)    ? "`[OK]`" : "`[LỖI]`",
    (uHwHealth & HW_OK_PH)      ? "`[OK]`" : "`[LỖI]`",
    (uHwHealth & HW_OK_TDS)     ? "`[OK]`" : "`[LỖI]`",
    (uHwHealth & HW_OK_DS18B20) ? "`[OK]`" : "`[LỖI]`",
    (uHwHealth & HW_OK_NTU)     ? "`[OK]`" : "`[LỖI]`",
    (uHwHealth & HW_OK_ACS712)  ? "`[OK]`" : "`[LỖI]`",
    (uHwHealth & HW_OK_BAT3S)   ? "`[OK]`" : "`[LỖI]`",
    (uHwHealth & HW_OK_SIM4G)   ? "`[OK]`" : "`[LỖI]`"
  );

  // Tuyệt đối không tạo link Google Maps khi chưa có GPS FIX (tránh link toạ độ 0,0 giả).
  char linksField[460];
  if (gpsOk) {
    snprintf(linksField, sizeof(linksField),
      "🌐 [**Xem Bản Đồ 3D WebGL Hồ Nước Trực Quan**](http://192.168.4.1/3d)\\n"
      "📍 [**Xem Vị Trí Vệ Tinh Google Maps**](https://www.google.com/maps?q=%.5f,%.5f)\\n"
      "🕹️ [**Trạm Điều Khiển GCS Web Dashboard**](http://192.168.4.1)",
      uLat, uLng
    );
  } else {
    snprintf(linksField, sizeof(linksField), "%s",
      "🌐 [**Xem Bản Đồ 3D WebGL Hồ Nước Trực Quan**](http://192.168.4.1/3d)\\n"
      "📍 **Vị trí**: Chưa có GPS FIX (không tạo liên kết bản đồ giả)\\n"
      "🕹️ [**Trạm Điều Khiển GCS Web Dashboard**](http://192.168.4.1)"
    );
  }

  snprintf(jsonBuf, 2600,
    "{\"username\":\"ChemStation AI - USV Buoy Monitor\","
    "\"avatar_url\":\"https://img.icons8.com/fluency/96/boat.png\","
    "\"embeds\":[{\"title\":\"%s\",\"color\":%d,"
    "\"fields\":["
    "{\"name\":\"🧪 CHỈ SỐ NƯỚC & CHẤT LƯỢNG\",\"value\":\"%s\",\"inline\":false},"
    "{\"name\":\"🔋 NĂNG LƯỢNG & NGUỒN PIN\",\"value\":\"%s\",\"inline\":true},"
    "{\"name\":\"🧭 GPS & ĐIỀU HƯỚNG\",\"value\":\"%s\",\"inline\":true},"
    "{\"name\":\"📡 VIỄN THÔNG LORA & TRẠM BỜ\",\"value\":\"%s\",\"inline\":false},"
    "{\"name\":\"🛠️ SỨC KHỎE 11 LINH KIỆN PHẦN CỨNG\",\"value\":\"%s\",\"inline\":false},"
    "{\"name\":\"🗺️ BẢN ĐỒ 3D TRỰC QUAN & LIÊN KẾT ĐIỀU HÀNH\",\"value\":\"%s\",\"inline\":false}"
    "],"
    "\"footer\":{\"text\":\"ChemStation AI USV v%s • GCS WiFi Direct\",\"icon_url\":\"https://img.icons8.com/fluency/48/water.png\"}}]}",
    title, color, waterField, pwrField, gpsField, rfField, hwField, linksField, FW_VERSION
  );

  int httpCode = https.POST(jsonBuf);
  free(jsonBuf);
  discordLastStatus = httpCode;
  bool ok = (httpCode >= 200 && httpCode < 300);
  https.end();

  if (ok) {
    discordPushCount++;
    lastDiscordPushMs = millis();
    Serial.printf("[GCS-DISCORD] Webhook POST OK (HTTP %d, Tong: %lu)\n", httpCode, (unsigned long)discordPushCount);
  } else {
    Serial.printf("[GCS-DISCORD] Webhook POST FAIL (HTTP %d)\n", httpCode);
  }
  return ok;
}

// =========================================================================
// 7. ĐẨY DỮ LIỆU LÊN GOOGLE SHEETS QUA CẢ WIFI (CORE 0 TASK) & MẠCH SIM 4G
// =========================================================================
void googleSheetsWifiTask(void* pvParameters) {
  (void)pvParameters;
  esp_task_wdt_add(NULL); // S-08: Đăng ký task Core 0 vào Task Watchdog
  unsigned long lastAutoDiscordMs = 0;
  uint8_t lastAlertMask = 0;
  for (;;) {
    esp_task_wdt_reset();
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
      // Chỉ gửi khi Phao còn online, tránh gửi số liệu cũ/0 gây hiểu nhầm.
      if (nowMs - lastAutoDiscordMs >= 60000UL &&
          lastRxFromBuoy > 0 && (nowMs - lastRxFromBuoy) < 10000UL) {
        lastAutoDiscordMs = nowMs;
        sendDiscordWebhookFromGCS(false, false, nullptr);
      }
    }
    if ((reqPushGSheetsTele || reqPushGSheetsProfile) && isGoogleUrlConfigured() &&
        WiFi.status() == WL_CONNECTED) {
      unsigned long nowMs = millis();
      // Giãn cách 5s giữa các lần thử để tránh spam khi mạng yếu / DNS lỗi
      if (nowMs - lastGsheetAttemptMs >= 5000UL) {
        lastGsheetAttemptMs = nowMs;

        bool isPrf = reqPushGSheetsProfile && !reqPushGSheetsTele; // Ưu tiên telemetry (dữ liệu thời gian thực)
        if (isPrf) reqPushGSheetsProfile = false;
        else       reqPushGSheetsTele    = false;

        char url[850];
        buildGoogleSheetsFullUrl(url, sizeof(url), isPrf, "ESP32_WIFI");

        WiFiClientSecure client;
        client.setInsecure();
        client.setHandshakeTimeout(8);
        HTTPClient https;
        https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        https.setTimeout(9000);
        https.setConnectTimeout(6000);

        bool ok = false;
        if (https.begin(client, url)) {
          int httpCode = https.GET();
          gsheetLastStatus = httpCode;
          if (httpCode == 200) {
            String body = https.getString();
            ok = (body.indexOf("OK_") >= 0 || body.indexOf("OK") >= 0);
          } else {
            ok = (httpCode == 302);
          }
          https.end();
        } else {
          gsheetLastStatus = -1;
        }

        if (ok) {
          gsheetPushCount++;
          lastGsheetPushMs = millis();
          Serial.printf("[GCS-GSHEET] WiFi push OK (HTTP %d, %s, Tong: %lu)\n",
                        gsheetLastStatus, isPrf ? "PROFILE" : "TELE",
                        (unsigned long)gsheetPushCount);
          if (isPrf && strlen(discordWebhookUrl) > 30) {
            sendDiscordWebhookFromGCS(true, false, nullptr);
          }
        } else {
          gsheetFailCnt++;
          // Giữ lại yêu cầu để lần sau thử lại (hoặc chuyển sang SIM 4G khi mất WiFi)
          if (isPrf) reqPushGSheetsProfile = true;
          else       reqPushGSheetsTele    = true;
          Serial.printf("[GCS-GSHEET] WiFi push FAIL (HTTP %d, %s, Loi: %lu)\n",
                        gsheetLastStatus, isPrf ? "PROFILE" : "TELE",
                        (unsigned long)gsheetFailCnt);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

// Khởi động lượt đẩy kế tiếp qua SIM 4G nếu đang rảnh và Wi-Fi không khả dụng
void startNextSimPushIfIdle() {
  if (gcsSimState != GSIM_IDLE) return;
  if (!gcsSimNetReady) return; // Cho module 4G khoi tao APN/NETOPEN xong
  if (millis() < gcsSimRetryAfterMs) return; // Backoff sau lan that bai
  if (WiFi.status() == WL_CONNECTED) return; // Uu tien Wi-Fi (task Core 0 xu ly)
  if (!reqPushGSheetsTele && !reqPushGSheetsProfile) return;
  if (!isGoogleUrlConfigured()) return;

  gcsSimIsProfile = reqPushGSheetsProfile && !reqPushGSheetsTele;
  // Da nhan yeu cau -> xoa co, tranh gui lap vo han khi mang 4G loi
  if (gcsSimIsProfile) reqPushGSheetsProfile = false;
  else                 reqPushGSheetsTele    = false;

  gcsSimState = GSIM_TERM_PREV;
  gcsSimTick  = millis();
  Serial.printf("[GCS-SIM] Bat dau day %s qua 4G...\n",
                gcsSimIsProfile ? "PROFILE 3 TANG" : "TELEMETRY");
}

void updateGcsSim4GUploader() {
  unsigned long now = millis();

  // Khoi tao module 4G theo tung buoc: tat echo, kiem tra SIM, APN, mo data, HTTP.
  // Chi chay 1 lan sau boot; neu khong co module thi tu bo qua sau 8 buoc.
  if (!gcsSimNetReady && gcsSimInitStep < 8) {
    if (now - gcsSimInitTick >= 400) {
      gcsSimInitTick = now;
      switch (gcsSimInitStep) {
        case 0: Serial2.println("ATE0"); break;
        case 1: Serial2.println("AT+CPIN?"); break;
        case 2: Serial2.println("AT+CSQ"); break;
        case 3: Serial2.println("AT+CGATT?"); break;
        case 4: Serial2.printf("AT+CGDCONT=1,\"IP\",\"%s\"\r\n", SIM_APN); break;
        case 5: Serial2.println("AT+NETOPEN"); break;
        case 6: Serial2.println("AT+HTTPINIT"); break;
        case 7:
          gcsSimNetReady = true;
          Serial.printf("[GCS-SIM] Da cau hinh APN '%s' va mo data 4G.\n", SIM_APN);
          break;
      }
      gcsSimInitStep++;
    }
  }

  while (Serial2.available() > 0) {
    char c = (char)Serial2.read();
    if (c == '\n' || c == '\r') {
      if (gcsSimRxIdx > 0) {
        gcsSimRxBuf[gcsSimRxIdx] = '\0';
        if (strstr(gcsSimRxBuf, "OK") != nullptr) gcsSimModemAlive = true;
        // S-04: Chi chap nhan +HTTPACTION khi dang thuc su cho phan hoi, va doc body AT+HTTPREAD xac nhan "OK_"
        if (gcsSimState == GSIM_WAIT && strncmp(gcsSimRxBuf, "+HTTPACTION:", 12) == 0) {
          int m = 0, st = 0, dlen = 0;
          if (sscanf(gcsSimRxBuf + 12, "%d,%d,%d", &m, &st, &dlen) >= 2) {
            gsheetLastStatus = st;
            gcsSimLastDataLen = dlen;
            if (st == 200) {
              if (dlen > 0) {
                gcsSimState = GSIM_READ;
                gcsSimTick  = now;
              } else {
                gsheetPushCount++;
                lastGsheetPushMs   = now;
                gcsSimRetryAfterMs = 0;
                gcsSimState = GSIM_TERM;
                gcsSimTick  = now;
              }
            } else {
              gsheetFailCnt++;
              // Khong mat goi: dua yeu cau tro lai hang doi va backoff 10s
              if (gcsSimIsProfile) reqPushGSheetsProfile = true;
              else                 reqPushGSheetsTele    = true;
              gcsSimRetryAfterMs = now + 10000UL;
              Serial.printf("[GCS-SIM] HTTP %d -> se thu lai sau 10s.\n", st);
              gcsSimState = GSIM_TERM;
              gcsSimTick  = now;
            }
          }
        } else if (gcsSimState == GSIM_WAIT_READ) {
          if (strstr(gcsSimRxBuf, "OK_") != nullptr) {
            gsheetPushCount++;
            lastGsheetPushMs   = now;
            gcsSimRetryAfterMs = 0;
            gcsSimState = GSIM_TERM;
            gcsSimTick  = now;
          }
        }
        gcsSimRxIdx = 0;
      }
    } else if (gcsSimRxIdx < sizeof(gcsSimRxBuf) - 1) {
      gcsSimRxBuf[gcsSimRxIdx++] = c;
    }
  }

  switch (gcsSimState) {
    case GSIM_IDLE:
      break;
    case GSIM_TERM_PREV:
      if (now - gcsSimTick >= 100) {
        Serial2.println("AT+HTTPTERM");
        gcsSimState = GSIM_INIT;
        gcsSimTick  = now;
      }
      break;
    case GSIM_INIT:
      if (now - gcsSimTick >= 250) {
        Serial2.println("AT+HTTPINIT");
        gcsSimState = GSIM_SSL_CFG;
        gcsSimTick  = now;
      }
      break;
    case GSIM_SSL_CFG:
      // S-04: Cau hinh TLS 1.2 & bo qua CA cho SIM A7680C
      if (now - gcsSimTick >= 150) {
        Serial2.println("AT+CSSLCFG=\"sslversion\",0,3");
        Serial2.println("AT+CSSLCFG=\"authmode\",0,0");
        gcsSimState = GSIM_HTTPSSL;
        gcsSimTick  = now;
      }
      break;
    case GSIM_HTTPSSL:
      if (now - gcsSimTick >= 150) {
        Serial2.println("AT+HTTPSSL=1");
        gcsSimState = GSIM_PARA_REDIR;
        gcsSimTick  = now;
      }
      break;
    case GSIM_PARA_REDIR:
      if (now - gcsSimTick >= 150) {
        Serial2.println("AT+HTTPPARA=\"REDIR\",1");
        gcsSimState = GSIM_PARA_URL;
        gcsSimTick  = now;
      }
      break;
    case GSIM_PARA_URL:
      if (now - gcsSimTick >= 300) {
        char url[700];
        buildGoogleSheetsFullUrl(url, sizeof(url), gcsSimIsProfile, "ESP32_SIM4G");
        Serial2.printf("AT+HTTPPARA=\"URL\",\"%s\"\r\n", url);
        gcsSimState = GSIM_ACTION;
        gcsSimTick  = now;
      }
      break;
    case GSIM_ACTION:
      if (now - gcsSimTick >= 350) {
        Serial2.println("AT+HTTPACTION=0");
        gcsSimState = GSIM_WAIT;
        gcsSimTick  = now;
      }
      break;
    case GSIM_WAIT:
      if (now - gcsSimTick >= 9000) {
        // Het thoi gian cho phan hoi HTTPACTION -> coi nhu that bai
        gcsSimState      = GSIM_TERM;
        gcsSimTick       = now;
        gsheetFailCnt++;
        gsheetLastStatus = -2;
        if (gcsSimIsProfile) reqPushGSheetsProfile = true;
        else                 reqPushGSheetsTele    = true;
        gcsSimRetryAfterMs = now + 10000UL;
        Serial.println("[GCS-SIM] Timeout cho +HTTPACTION -> that bai, se thu lai sau 10s.");
      }
      break;
    case GSIM_READ:
      if (now - gcsSimTick >= 150) {
        Serial2.printf("AT+HTTPREAD=0,%d\r\n", constrain(gcsSimLastDataLen, 1, 128));
        gcsSimState = GSIM_WAIT_READ;
        gcsSimTick  = now;
      }
      break;
    case GSIM_WAIT_READ:
      if (now - gcsSimTick >= 2000) {
        gsheetPushCount++;
        lastGsheetPushMs   = now;
        gcsSimRetryAfterMs = 0;
        gcsSimState = GSIM_TERM;
        gcsSimTick  = now;
      }
      break;
    case GSIM_TERM:
      if (now - gcsSimTick >= 200) {
        Serial2.println("AT+HTTPTERM");
        gcsSimState = GSIM_IDLE;
        gcsSimTick  = now;
        startNextSimPushIfIdle();
      }
      break;
  }
}

void triggerGoogleSheetsPush(bool isProfile) {
  if (!isGoogleUrlConfigured()) return;
  // Không đẩy bảng 3 tầng khi chưa có mẫu đo thật (tránh ghi toàn số 0)
  if (isProfile && !gcsLayerSurf.valid && !gcsLayerMid.valid && !gcsLayerBot.valid) {
    Serial.println("[GCS-GSHEET] Chua co du lieu 3 tang that -> khong day PROFILE.");
    return;
  }
  if (isProfile) reqPushGSheetsProfile = true;
  else           reqPushGSheetsTele    = true;
  startNextSimPushIfIdle();
}

// =========================================================================
// 8B. LƯU / NẠP CẤU HÌNH TRẠM BỜ (Google URL + Wi-Fi STA) VÀO NVS
// =========================================================================
void loadGcsConfig() {
  gcsPrefs.begin(NVS_NAMESPACE_GCS, false);
  gcsBootCount = gcsPrefs.getUInt("boot_cnt", 0) + 1;
  gcsPrefs.putUInt("boot_cnt", gcsBootCount);

  if (gcsPrefs.isKey("gs_url")) {
    String u = gcsPrefs.getString("gs_url", DEFAULT_GOOGLE_SCRIPT_URL);
    copyStringSafe(gsheetScriptUrl, sizeof(gsheetScriptUrl), u.c_str());
  }
  if (gcsPrefs.isKey("dc_url")) {
    String d = gcsPrefs.getString("dc_url", DEFAULT_DISCORD_WEBHOOK_URL);
    copyStringSafe(discordWebhookUrl, sizeof(discordWebhookUrl), d.c_str());
  }
  if (gcsPrefs.isKey("sta_ssid")) {
    String s = gcsPrefs.getString("sta_ssid", DEFAULT_STA_WIFI_SSID);
    copyStringSafe(staWifiSsid, sizeof(staWifiSsid), s.c_str());
  }
  if (gcsPrefs.isKey("sta_pass")) {
    String p = gcsPrefs.getString("sta_pass", DEFAULT_STA_WIFI_PASS);
    copyStringSafe(staWifiPass, sizeof(staWifiPass), p.c_str());
  }
  if (gcsPrefs.isKey("ap_pass")) {
    String ap = gcsPrefs.getString("ap_pass", DEFAULT_AP_WIFI_PASS);
    if (ap.length() >= 8) copyStringSafe(apWifiPass, sizeof(apWifiPass), ap.c_str());
  }
  if (gcsPrefs.isKey("adm_tok")) {
    String tk = gcsPrefs.getString("adm_tok", DEFAULT_GCS_ADMIN_TOKEN);
    if (tk.length() >= 4) copyStringSafe(gcsAdminToken, sizeof(gcsAdminToken), tk.c_str());
  }
  // Chỉ nạp mặc định lần đầu tiên nếu người dùng chưa từng cấu hình sta_ssid trong NVS
  if (!gcsPrefs.isKey("sta_ssid")) {
    copyStringSafe(staWifiSsid, sizeof(staWifiSsid), DEFAULT_STA_WIFI_SSID);
    copyStringSafe(staWifiPass, sizeof(staWifiPass), DEFAULT_STA_WIFI_PASS);
    gcsPrefs.putString("sta_ssid", staWifiSsid);
    gcsPrefs.putString("sta_pass", staWifiPass);
    gcsPrefs.putString("ap_pass", apWifiPass);
    gcsPrefs.putString("adm_tok", gcsAdminToken);
  }
  Serial.printf("[GCS-CFG] Boot #%lu | Google URL: %s | Discord: %s | STA Wi-Fi: %s\n",
                (unsigned long)gcsBootCount,
                isGoogleUrlConfigured() ? "DA CAU HINH" : "CHUA CAU HINH",
                (strlen(discordWebhookUrl) > 30) ? "DA CAU HINH" : "CHUA CAU HINH",
                strlen(staWifiSsid) ? staWifiSsid : "(chi phat AP)");
}

void saveGcsConfig() {
  gcsPrefs.putString("gs_url", gsheetScriptUrl);
  gcsPrefs.putString("dc_url", discordWebhookUrl);
  gcsPrefs.putString("sta_ssid", staWifiSsid);
  gcsPrefs.putString("sta_pass", staWifiPass);
  gcsPrefs.putString("ap_pass", apWifiPass);
  gcsPrefs.putString("adm_tok", gcsAdminToken);
  Serial.println("[GCS-CFG] Da luu cau hinh vao NVS.");
}

void queueSpecialCmd(const char* cmd) {
  copyStringSafe(pendingSpecialCmd, sizeof(pendingSpecialCmd), cmd);
  Serial.printf("[GCS-CMD] Xep hang lenh dac biet: %s\n", pendingSpecialCmd);
}
const char WEB_DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no,viewport-fit=cover">
<title>USV AQUA-TACTICAL | Trạm Điều Khiển Trung Tâm</title>
<style>
:root{
  --bg:#030712;--bg-card:rgba(15,23,42,0.88);--border:rgba(0,245,212,0.22);
  --cyan:#00f5d4;--blue:#00bbf9;--green:#06d6a0;--amber:#ffd166;--red:#ff4757;
  --text:#f8fafc;--sub:#94a3b8;--dark:#0b1329;
}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
body{background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,sans-serif;padding:10px;min-height:100vh;background-image:radial-gradient(ellipse at top,rgba(0,187,249,0.08) 0%,transparent 70%)}
.hdr{background:linear-gradient(135deg,#0a152e,#112347);border:1px solid var(--border);border-radius:12px;padding:10px 14px;margin-bottom:10px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;box-shadow:0 8px 24px rgba(0,0,0,0.5)}
.hdr-left{display:flex;align-items:center;gap:10px}
.pulse{width:10px;height:10px;border-radius:50%;background:var(--green);box-shadow:0 0 10px var(--green);animation:pulse 1.8s infinite}
@keyframes pulse{0%{opacity:0.4;transform:scale(0.85)}50%{opacity:1;transform:scale(1.2)}100%{opacity:0.4;transform:scale(0.85)}}
.title{font-size:1.05rem;font-weight:900;letter-spacing:1px;color:var(--cyan);text-shadow:0 0 12px rgba(0,245,212,0.3)}
.badges{display:flex;gap:6px;flex-wrap:wrap;align-items:center}
.badge{padding:4px 9px;border-radius:6px;font-size:0.75rem;font-weight:700;font-family:monospace;background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.12);display:inline-flex;align-items:center;gap:4px}
.badge.green{background:rgba(6,214,160,0.15);color:var(--green);border-color:var(--green)}
.badge.cyan{background:rgba(0,245,212,0.15);color:var(--cyan);border-color:var(--cyan)}
.badge.amber{background:rgba(255,209,102,0.15);color:var(--amber);border-color:var(--amber)}
.badge.red{background:rgba(255,71,87,0.15);color:var(--red);border-color:var(--red)}

.tabs{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-bottom:10px}
@media(max-width:768px){.tabs{grid-template-columns:repeat(2,1fr)}}
.tbl{width:100%;border-collapse:collapse;font-size:0.78rem;font-family:monospace;margin-top:6px}
.tbl th{background:rgba(0,245,212,0.12);color:var(--cyan);padding:8px 6px;text-align:left;border:1px solid rgba(0,245,212,0.25)}
.tbl td{padding:7px 6px;border:1px solid rgba(255,255,255,0.08)}
.tbl tr:hover{background:rgba(255,255,255,0.04)}
.strat-box{margin-top:10px;padding:10px;border-radius:8px;background:rgba(0,187,249,0.08);border:1px solid rgba(0,187,249,0.3);font-size:0.78rem;line-height:1.5}
.tab-btn{padding:10px 6px;border-radius:8px;font-size:0.82rem;font-weight:800;background:rgba(255,255,255,0.04);border:1px solid rgba(255,255,255,0.1);color:var(--sub);cursor:pointer;transition:all 0.2s;text-align:center;user-select:none}
.tab-btn.active{background:linear-gradient(90deg,rgba(0,245,212,0.22),rgba(0,187,249,0.22));border-color:var(--cyan);color:var(--cyan);box-shadow:0 0 16px rgba(0,245,212,0.28)}

.card{background:var(--bg-card);border:1px solid var(--border);border-radius:12px;padding:12px;margin-bottom:10px;box-shadow:0 8px 24px rgba(0,0,0,0.45);backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px)}
.card-title{font-size:0.82rem;font-weight:800;color:var(--cyan);text-transform:uppercase;letter-spacing:0.8px;margin-bottom:8px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:4px}
.grid-2{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:10px}
.grid-4{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:8px}

.m-pill{background:rgba(255,255,255,0.03);border:1px solid rgba(255,255,255,0.07);border-radius:8px;padding:8px 10px;transition:border-color 0.2s}
.m-pill:hover{border-color:rgba(0,245,212,0.3)}
.m-lbl{font-size:0.7rem;color:var(--sub);text-transform:uppercase;letter-spacing:0.5px;font-weight:600}
.m-val{font-size:1.45rem;font-weight:900;color:#fff;margin:2px 0;font-family:monospace;letter-spacing:-0.5px}
.m-sub{font-size:0.72rem;color:var(--sub)}

.btns{display:grid;gap:6px;margin-top:6px}
button{padding:9px 12px;border-radius:8px;border:1px solid rgba(255,255,255,0.12);background:rgba(255,255,255,0.06);color:#fff;font-weight:800;font-size:0.78rem;cursor:pointer;transition:all 0.15s;user-select:none}
button:hover{filter:brightness(1.18);transform:translateY(-1px)}
button:active{transform:translateY(0)}
.btn-cyan{background:var(--cyan)!important;color:#040714!important;border-color:var(--cyan)!important}
.btn-green{background:var(--green)!important;color:#040714!important;border-color:var(--green)!important}
.btn-blue{background:var(--blue)!important;color:#040714!important;border-color:var(--blue)!important}
.btn-amber{background:var(--amber)!important;color:#040714!important;border-color:var(--amber)!important}
.btn-red{background:var(--red)!important;color:#fff!important;border-color:var(--red)!important}

input{width:100%;box-sizing:border-box;margin-top:4px;padding:8px 10px;border-radius:6px;border:1px solid rgba(0,245,212,0.3);background:#070d1e;color:#fff;font-size:0.82rem;font-family:monospace}
canvas{display:block;border-radius:8px}
.tab-content{display:none}
.tab-content.active{display:block}

.hw-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:6px;margin-top:8px}
.hw-node{padding:7px 9px;border-radius:6px;font-size:0.75rem;font-family:monospace;display:flex;justify-content:space-between;align-items:center;background:rgba(255,255,255,0.03);border:1px solid rgba(255,255,255,0.08)}
.hw-node.ok{border-color:var(--green);color:var(--green);background:rgba(6,214,160,0.06)}
.hw-node.err{border-color:var(--red);color:var(--red);background:rgba(255,71,87,0.06)}
.gear-btn.act{background:var(--cyan);color:#040714;border-color:var(--cyan)}
</style>
</head>
<body>

<!-- Header Bar -->
<div class="hdr">
  <div class="hdr-left">
    <div class="pulse"></div>
    <div>
      <div class="title">USV AQUA-TACTICAL COMMAND</div>
      <div style="font-size:0.72rem;color:var(--sub)" id="header_status">Quan trắc nước đa tầng & Tự hành hải trình 3D</div>
    </div>
  </div>
  <div class="badges">
    <span class="badge green" id="badge_lora">📡 LORA: ON</span>
    <span class="badge cyan" id="badge_wifi">📶 STA: OFF</span>
    <span class="badge amber" id="badge_bat">🔋 PIN: --</span>
    <span class="badge" id="badge_gps">🛰️ GPS: 0 SV</span>
    <button id="btnAudio" onclick="toggleAudio()" style="padding:4px 9px;font-size:0.72rem">🔊 BẬT ÂM</button>
  </div>
</div>

<!-- Navigation Tabs -->
<div class="tabs">
  <div class="tab-btn active" onclick="switchTab('cockpit')" id="tab_cockpit">🕹️ BUỒNG LÁI HUD</div>
  <div class="tab-btn" onclick="switchTab('map3d')" id="tab_map3d">🌐 BẢN ĐỒ 3D WEBGL</div>
  <div class="tab-btn" onclick="switchTab('data')" id="tab_data">📊 NHẬT KÝ & 3 TẦNG</div>
  <div class="tab-btn" onclick="switchTab('config')" id="tab_config">⚙️ ĐỒNG BỘ & CẤU HÌNH</div>
</div>

<!-- TAB 1: COCKPIT HUD -->
<div id="view_cockpit" class="tab-content active">
  <div class="grid-2">
    <!-- Cột trái: Horizon Gyro & Chỉ số nước -->
    <div>
      <!-- Thẻ Chân trời nhân tạo & Góc nghiêng thân tàu -->
      <div class="card">
        <div class="card-title">
          <span>GÓC NGHIÊNG THÂN TÀU & CHÂN TRỜI (ATTITUDE HUD)</span>
          <span id="txt_gyro" style="font-family:monospace;font-size:0.75rem">Roll: 0.0° | Pitch: 0.0°</span>
        </div>
        <canvas id="horizonCanvas" style="width:100%;height:150px;background:#030712"></canvas>
        <div style="display:flex;justify-content:space-between;margin-top:6px;font-size:0.75rem;color:var(--sub);font-family:monospace">
          <span>La Bàn: <b id="txt_hdg" style="color:var(--cyan)">--°</b></span>
          <span>Vận Tốc: <b id="txt_spd" style="color:#fff">0.0 km/h</b></span>
          <span>Về Home: <b id="txt_homed" style="color:var(--green)">-- m</b></span>
        </div>
      </div>

      <!-- Thẻ 4 Chỉ số chất lượng nước tinh gọn -->
      <div class="card">
        <div class="card-title">
          <span>CHỈ SỐ CHẤT LƯỢNG NƯỚC (WATER MATRIX)</span>
          <span id="qcvn_badge" class="badge green">VN-WQI: ĐẠT CHUẨN</span>
        </div>
        <div class="grid-4">
          <div class="m-pill">
            <div class="m-lbl">Độ pH</div>
            <div class="m-val" id="val_ph" style="color:var(--green)">--</div>
            <div class="m-sub" id="sub_ph">DO: -- mg/L</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Khoáng TDS</div>
            <div class="m-val" id="val_tds" style="color:var(--cyan)">--</div>
            <div class="m-sub" id="sub_ec">EC: -- µS/cm</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Độ đục NTU</div>
            <div class="m-val" id="val_ntu" style="color:var(--amber)">--</div>
            <div class="m-sub" id="sub_ntu">Nước: --</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Nhiệt độ</div>
            <div class="m-val" id="val_temp" style="color:#fff">--°C</div>
            <div class="m-sub" id="sub_temp">S3: --° | GCS: --°</div>
          </div>
        </div>
        <div id="qcvn_banner" style="margin-top:8px;padding:6px 10px;border-radius:6px;background:rgba(6,214,160,0.12);border:1px solid rgba(6,214,160,0.3);font-size:0.75rem;color:var(--green)">
          Đánh giá quy chuẩn QCVN 08:2023/BTNMT Cột A - An toàn
        </div>
      </div>

      <!-- Thẻ Biểu đồ sóng thời gian thực -->
      <div class="card">
        <div class="card-title">
          <span>BIỂU ĐỒ SÓNG TELEMETRY THỜI GIAN THỰC</span>
          <button onclick="dlCsv()" style="padding:3px 8px;font-size:0.72rem;background:var(--green);color:#000">TẢI CSV</button>
        </div>
        <canvas id="chartCanvas" style="width:100%;height:100px;background:#030712"></canvas>
      </div>
    </div>

    <!-- Cột phải: Khảo sát 3 tầng nước, Nguồn & Joystick -->
    <div>
      <!-- Thẻ Cột nước 3 tầng & Tời đo sâu -->
      <div class="card">
        <div class="card-title">
          <span>KHẢO SÁT CHIỀU SÂU 3 TẦNG & TỜI LỒNG ĐO</span>
          <span id="txt_stage" style="color:var(--amber);font-family:monospace;font-size:0.75rem">Tời: 0.00m</span>
        </div>
        <canvas id="depthCanvas" style="width:100%;height:90px;background:#030712"></canvas>
        <div class="btns" style="grid-template-columns:repeat(4,1fr);margin-top:8px">
          <button onclick="sp('inch_dn')" class="btn-blue">HẠ 10CM</button>
          <button onclick="sp('inch_up')" class="btn-blue">THU 10CM</button>
          <button onclick="cmd(0,90,3)" class="btn-green">AUTO 3 TẦNG</button>
          <button onclick="cmd(0,90,2)" class="btn-amber">THU TỜI 0m</button>
        </div>
      </div>

      <!-- Thẻ Nguồn Pin 3S & Solar Station -->
      <div class="card">
        <div class="card-title">
          <span>NGUỒN NĂNG LƯỢNG PIN 3S & QUANG ĐIỆN SOLAR</span>
          <span id="txt_power_mode" style="font-size:0.75rem">Sẵn Sàng</span>
        </div>
        <div class="grid-4">
          <div class="m-pill">
            <div class="m-lbl">Điện áp Pin</div>
            <div class="m-val" id="val_bat" style="color:var(--green)">-- V</div>
            <div class="m-sub" id="sub_bat">Dung lượng: --%</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Dòng tải</div>
            <div class="m-val" id="val_amp" style="color:var(--amber)">-- A</div>
            <div class="m-sub">ACS712 Motor</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Công suất</div>
            <div class="m-val" id="val_watt" style="color:#fff">-- W</div>
            <div class="m-sub" id="sub_wh">Năng lượng: -- Wh</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Solar PV</div>
            <div class="m-val" id="val_sol" style="color:var(--cyan)">-- W</div>
            <div class="m-sub" id="sub_sol">Cân bằng: --</div>
          </div>
        </div>
      </div>

      <!-- Thẻ Bàn Điều Khiển Joystick & Lái -->
      <div class="card">
        <div class="card-title">
          <span>BUỒNG LÁI ĐIỀU HƯỚNG TỰ HÀNH & JOYSTICK</span>
          <span id="joy_src" style="font-size:0.75rem">Nguồn: Chờ lệnh</span>
        </div>
        <div style="display:flex;justify-content:center;margin:6px 0">
          <canvas id="joyCanvas" width="180" height="180" style="touch-action:none;cursor:crosshair;background:#030712;border:1px solid rgba(0,245,212,0.3)"></canvas>
        </div>
        <div style="text-align:center;font-size:0.75rem;color:var(--sub);margin-bottom:6px;font-family:monospace" id="joy_text">
          Chạm hoặc kéo chuột để lái thuyền USV
        </div>
        <div class="btns" style="grid-template-columns:repeat(3,1fr)">
          <button onclick="setGear(0)" id="g0" class="gear-btn">ECO 60%</button>
          <button onclick="setGear(1)" id="g1" class="gear-btn act">CRUISE 82%</button>
          <button onclick="setGear(2)" id="g2" class="gear-btn">TURBO 100%</button>
        </div>
        <div class="btns" style="grid-template-columns:repeat(4,1fr);margin-top:6px">
          <button onclick="sp('anchor_on')" class="btn-cyan">NEO ẢO</button>
          <button onclick="sp('home')" class="btn-blue">LƯU HOME</button>
          <button onclick="sp('goto')" class="btn-green">VỀ HOME</button>
          <button onclick="sp('stopall')" class="btn-red">DỪNG HẾT</button>
        </div>
      </div>
    </div>
  </div>

  <!-- Thẻ Ma trận 11 Linh kiện phần cứng thực tế -->
  <div class="card">
    <div class="card-title">
      <span>CHẨN ĐOÁN SỨC KHỎE 11 LINH KIỆN PHẦN CỨNG THỰC TẾ (KHÔNG ẢO)</span>
      <span id="hw_summary" style="color:var(--green)">11/11 OK</span>
    </div>
    <div class="hw-grid" id="hw_nodes"></div>
  </div>
</div>

<!-- TAB 2: BẢN ĐỒ 3D WEBGL -->
<div id="view_map3d" class="tab-content">
  <div class="card" style="padding:6px">
    <div class="card-title" style="padding:6px 8px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:6px">
      <span>BẢN ĐỒ 3D WEBGL KHÔNG GIAN HỒ NƯỚC & PHAO USV THỜI GIAN THỰC</span>
      <div style="display:flex;gap:6px">
        <button onclick="document.getElementById('frame3D').src='/3d'" class="btn-blue" style="padding:4px 10px;font-size:0.75rem">🔄 TẢI LẠI 3D</button>
        <button onclick="window.open('/3d','_blank')" class="btn-cyan" style="padding:4px 10px;font-size:0.75rem">⛶ MỞ TAB TOÀN MÀN HÌNH</button>
      </div>
    </div>
    <iframe id="frame3D" src="/3d" style="width:100%;height:calc(100vh - 165px);min-height:580px;border:none;border-radius:8px;background:#030712"></iframe>
  </div>
</div>

<!-- TAB 4: NHẬT KÝ TELEMETRY & PHÂN TÍCH 3 TẦNG NƯỚC -->
<div id="view_data" class="tab-content">
  <!-- Card 1: HỒ SƠ 3 TẦNG NƯỚC -->
  <div class="card">
    <div class="card-title">
      <span>KHẢO SÁT CHẤT LƯỢNG NƯỚC 3 TẦNG ĐỘ SÂU (HYDROGRAPHIC PROFILE)</span>
      <div style="display:flex;gap:6px">
        <button onclick="cmd(0,90,3)" class="btn-green" style="padding:4px 9px;font-size:0.73rem">⚡ TỰ ĐỘNG ĐO 3 TẦNG</button>
        <button onclick="spPush('profile')" class="btn-blue" style="padding:4px 9px;font-size:0.73rem">📤 ĐẨY 3 TẦNG LÊN SHEETS</button>
      </div>
    </div>
    <div style="overflow-x:auto">
      <table class="tbl">
        <thead>
          <tr>
            <th>TẦNG ĐO</th>
            <th>ĐỘ SÂU</th>
            <th>TRẠNG THÁI</th>
            <th>ĐỘ pH</th>
            <th>TDS (ppm)</th>
            <th>ĐỘ ĐỤC</th>
            <th>NHIỆT ĐỘ</th>
            <th>DO (mg/L)</th>
            <th>ĐÁNH GIÁ TẦNG</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td style="font-weight:700;color:var(--cyan)">🌊 TẦNG MẶT</td>
            <td>0.30 m</td>
            <td id="prf_s_st">--</td>
            <td id="prf_s_ph" style="font-weight:700">--</td>
            <td id="prf_s_tds">--</td>
            <td id="prf_s_ntu">--</td>
            <td id="prf_s_temp">--</td>
            <td id="prf_s_do">--</td>
            <td id="prf_s_eval">--</td>
          </tr>
          <tr>
            <td style="font-weight:700;color:var(--green)">💧 TẦNG GIỮA</td>
            <td>1.00 m</td>
            <td id="prf_m_st">--</td>
            <td id="prf_m_ph" style="font-weight:700">--</td>
            <td id="prf_m_tds">--</td>
            <td id="prf_m_ntu">--</td>
            <td id="prf_m_temp">--</td>
            <td id="prf_m_do">--</td>
            <td id="prf_m_eval">--</td>
          </tr>
          <tr>
            <td style="font-weight:700;color:var(--amber)">⚓ TẦNG ĐÁY</td>
            <td>1.80 m</td>
            <td id="prf_b_st">--</td>
            <td id="prf_b_ph" style="font-weight:700">--</td>
            <td id="prf_b_tds">--</td>
            <td id="prf_b_ntu">--</td>
            <td id="prf_b_temp">--</td>
            <td id="prf_b_do">--</td>
            <td id="prf_b_eval">--</td>
          </tr>
        </tbody>
      </table>
    </div>

    <div class="strat-box" id="strat_analysis">
      <div style="font-weight:800;color:var(--cyan);margin-bottom:4px">🔬 ĐÁNH GIÁ PHÂN TẦNG SINH THÁI HỒ NƯỚC:</div>
      <div id="strat_detail">Đang đợi hoàn tất chu kỳ đo 3 tầng...</div>
    </div>
  </div>

  <!-- Card 2: NHẬT KÝ TELEMETRY THỜI GIAN THỰC -->
  <div class="card">
    <div class="card-title">
      <div style="display:flex;align-items:center;gap:8px">
        <span>NHẬT KÝ TELEMETRY TRỰC TIẾP (LIVE STREAMING LOG)</span>
        <span class="pulse" style="width:7px;height:7px"></span>
      </div>
      <div style="display:flex;gap:6px;flex-wrap:wrap">
        <button onclick="dlCsv()" class="btn-green" style="padding:4px 9px;font-size:0.73rem">📥 TẢI TOÀN BỘ CSV</button>
        <button onclick="window.open('https://docs.google.com/spreadsheets/d/1IVcoOZhhrgMh_IOLvDm1-ssyUtHGmMhFSyZD3fYICDY','_blank')" class="btn-cyan" style="padding:4px 9px;font-size:0.73rem">📊 MỞ GOOGLE SHEETS</button>
        <button onclick="clearLogTable()" style="padding:4px 9px;font-size:0.73rem">🗑️ XÓA NHẬT KÝ</button>
      </div>
    </div>
    <div style="max-height:360px;overflow-y:auto;overflow-x:auto">
      <table class="tbl">
        <thead>
          <tr>
            <th>THỜI GIAN</th>
            <th>GÓI #</th>
            <th>ĐỘ pH</th>
            <th>TDS (ppm)</th>
            <th>ĐỘ ĐỤC</th>
            <th>NHIỆT ĐỘ</th>
            <th>PIN 3S</th>
            <th>TỜI SÂU</th>
            <th>GPS SV</th>
            <th>QCVN 08</th>
          </tr>
        </thead>
        <tbody id="live_log_body">
        </tbody>
      </table>
    </div>
  </div>
</div>

<!-- TAB 3: ĐỒNG BỘ & CẤU HÌNH -->
<div id="view_config" class="tab-content">
  <div class="grid-2">
    <!-- Cài đặt Cloud Google Sheets & Discord -->
    <div class="card">
      <div class="card-title">ĐỒNG BỘ ĐÁM MÂY GOOGLE SHEETS & DISCORD</div>
      <div style="font-size:0.75rem;color:var(--sub);margin-bottom:6px;font-family:monospace" id="cloud_info">Trạng thái: Đang kết nối...</div>

      <div style="margin-top:8px">
        <label style="font-size:0.75rem;color:var(--cyan);font-weight:700">Google Apps Script Web App URL</label>
        <input id="cfg_url" placeholder="https://script.google.com/macros/s/.../exec">
        <div class="btns" style="grid-template-columns:repeat(3,1fr);margin-top:6px">
          <button onclick="spPush('tele')" class="btn-green">PUSH TELE</button>
          <button onclick="spPush('profile')" class="btn-blue">PUSH 3 TẦNG</button>
          <button onclick="sp('sync')" class="btn-amber">SYNC XUỐNG PHAO</button>
        </div>
      </div>

      <div style="margin-top:12px">
        <label style="font-size:0.75rem;color:var(--cyan);font-weight:700">Discord Webhook URL (Báo cáo ảnh 3D & Cảnh báo)</label>
        <input id="cfg_discord" placeholder="https://discord.com/api/webhooks/...">
        <div class="btns" style="grid-template-columns:repeat(2,1fr);margin-top:6px">
          <button onclick="testDiscord()" class="btn-cyan">GỬI TEST DISCORD</button>
          <button onclick="saveCfg()" class="btn-green">LƯU CẤU HÌNH CLOUD</button>
        </div>
      </div>
    </div>

    <!-- Cài đặt Wi-Fi & Hiệu chuẩn -->
    <div class="card">
      <div class="card-title">CẤU HÌNH WI-FI & HIỆU CHUẨN PHẦN CỨNG</div>
      <div>
        <div style="display:flex;justify-content:space-between;align-items:center">
          <label style="font-size:0.75rem;color:var(--cyan);font-weight:700">Tên Wi-Fi Trạm Bờ kết nối (SSID)</label>
          <button onclick="scanWifi()" id="btn_scan_wifi" style="padding:2px 8px;font-size:0.7rem;background:var(--blue)">🔍 QUÉT WI-FI</button>
        </div>
        <select id="wifi_select" onchange="onSelectWifi(this.value)" style="display:none;width:100%;margin-top:4px;padding:8px 10px;border-radius:6px;border:1px solid rgba(0,245,212,0.3);background:#070d1e;color:#fff;font-size:0.82rem;font-family:monospace"></select>
        <input id="cfg_ssid" placeholder="VD: NhaToi_2.4G">
        <label style="font-size:0.75rem;color:var(--cyan);font-weight:700;margin-top:6px;display:block">Mật khẩu Wi-Fi</label>
        <input id="cfg_pass" type="password" placeholder="Mật khẩu Wi-Fi">
        <button onclick="saveCfg()" class="btn-blue" style="width:100%;margin-top:6px">LƯU KẾT NỐI WI-FI</button>
      </div>

      <div style="margin-top:12px;padding-top:10px;border-top:1px solid rgba(255,255,255,0.1)">
        <div style="font-size:0.75rem;color:var(--cyan);font-weight:700;margin-bottom:6px">Hiệu chuẩn Cảm biến & Tay cầm</div>
        <div style="display:flex;gap:6px">
          <input id="vbat_cal" placeholder="V Von thuc te (VD: 12.60)" style="margin-top:0">
          <button onclick="calBat()" class="btn-amber" style="white-space:nowrap">LƯU PIN</button>
        </div>
        <div class="btns" style="grid-template-columns:repeat(2,1fr);margin-top:6px">
          <button onclick="calJoy()">LƯU TÂM JOYSTICK</button>
          <button onclick="sp('cal_imu')">RESET LA BÀN IMU</button>
        </div>
      </div>

      <div class="btns" style="grid-template-columns:repeat(2,1fr);margin-top:12px">
        <button onclick="if(confirm('Khởi động lại Trạm Bờ?'))location.href='/api/reboot?token='+encodeURIComponent(getGcsToken())">REBOOT GCS</button>
        <button onclick="if(confirm('Xóa sạch cấu hình về mặc định?'))location.href='/api/factory_reset?token='+encodeURIComponent(getGcsToken())" class="btn-red">RESET GỐC</button>
      </div>
    </div>
  </div>
</div>

<script>
// --- Tab Switching ---
function switchTab(t){
  ['cockpit','map3d','data','config'].forEach(k=>{
    const v=document.getElementById('view_'+k);
    const b=document.getElementById('tab_'+k);
    if(v) v.className='tab-content'+(k===t?' active':'');
    if(b) b.className='tab-btn'+(k===t?' active':'');
  });
  if(t==='map3d'){
    const f=document.getElementById('frame3D');
    if(f && (!f.src || f.src==='about:blank' || !f.getAttribute('src'))){
      f.src='/3d';
    }
  }
}

const liveLogs=[];
function clearLogTable(){
  liveLogs.length=0;
  const tb=document.getElementById('live_log_body');
  if(tb) tb.innerHTML='';
}

// --- Audio Alerts ---
let audioCtx=null,soundEnabled=false,lastBeepMs=0;
function toggleAudio(){
  soundEnabled=!soundEnabled;
  const btn=document.getElementById('btnAudio');
  if(soundEnabled){
    if(!audioCtx) audioCtx=new (window.AudioContext||window.webkitAudioContext)();
    btn.style.background='var(--green)';btn.innerText='🔊 ÂM: BẬT';
    playTone(880,0.12);
  }else{
    btn.style.background='rgba(255,255,255,0.06)';btn.innerText='🔇 ÂM: TẮT';
  }
}
function playTone(freq,dur){
  if(!soundEnabled||!audioCtx)return;
  try{
    const o=audioCtx.createOscillator(),g=audioCtx.createGain();
    o.type='sine';o.frequency.value=freq;
    g.gain.setValueAtTime(0.08,audioCtx.currentTime);
    g.gain.exponentialRampToValueAtTime(0.001,audioCtx.currentTime+dur);
    o.connect(g);g.connect(audioCtx.destination);
    o.start();o.stop(audioCtx.currentTime+dur);
  }catch(e){}
}

// --- Auth Helper & Commands (P0-1) ---
function getGcsToken(){return localStorage.getItem('gcs_token')||'chemstation2026';}
function setGcsToken(t){if(t)localStorage.setItem('gcs_token',t.trim());}
function authHdr(){return {'X-GCS-Token':getGcsToken()};}
let gearScale=0.82;
function cmd(m,r,w){fetch(`/api/cmd?m=${m}&r=${r}&w=${w}`,{headers:authHdr()}).catch(()=>{});}
function sp(a){fetch(`/api/cmd?${a}=1`,{headers:authHdr()}).catch(()=>{});}
function spPush(p){fetch(`/api/cmd?push=${p}`,{headers:authHdr()}).catch(()=>{});}
function setGear(g){
  fetch(`/api/cmd?gear=${g}`,{headers:authHdr()}).catch(()=>{});
  gearScale=(g===0)?0.60:((g===2)?1.0:0.82);
  [0,1,2].forEach(i=>document.getElementById('g'+i).className='gear-btn'+(i===g?' act':''));
}
function calJoy(){fetch('/api/cmd?caljoy=1',{headers:authHdr()});alert('Đã lưu tâm Joystick!');}
function calBat(){
  const v=parseFloat(document.getElementById('vbat_cal').value);
  if(v>=3.0&&v<=18.0){fetch(`/api/cmd?cbat=${v.toFixed(2)}`,{headers:authHdr()});alert('Đã gửi điện áp hiệu chuẩn: '+v.toFixed(2)+'V');}
}

// --- Configuration ---
async function loadCfg(){
  try{
    const r=await fetch('/api/config',{headers:authHdr()});const d=await r.json();
    document.getElementById('cfg_url').value=d.url||'';
    document.getElementById('cfg_discord').value=d.discord||'';
    document.getElementById('cfg_ssid').value=d.ssid||'';
    document.getElementById('cfg_pass').value='';
  }catch(e){}
}
async function scanWifi(){
  const btn = document.getElementById('btn_scan_wifi');
  if(!btn) return;
  btn.innerText = '⏳ Đang quét...';
  try {
    const res = await fetch('/api/wifi_scan',{headers:authHdr()});
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

async function saveCfg(){
  const body=new URLSearchParams();
  body.set('url',document.getElementById('cfg_url').value.trim());
  body.set('discord',document.getElementById('cfg_discord').value.trim());
  body.set('ssid',document.getElementById('cfg_ssid').value.trim());
  const p=document.getElementById('cfg_pass').value;
  if(p)body.set('pass',p);
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-GCS-Token':getGcsToken()},body});
    const d=await r.json();
    alert(d.ok?'Đã lưu cấu hình Google Sheets & Discord!':'Lỗi: '+(d.err||r.status));
    loadCfg();
  }catch(e){alert('Không thể kết nối đến Trạm Bờ!');}
}
async function testDiscord(){
  try{
    const r=await fetch('/api/discord_test',{headers:authHdr()});const d=await r.json();
    alert(d.ok?'Đã gửi thông báo Discord thành công! Vui lòng kiểm tra kênh chat.':'Lỗi gửi Discord: '+(d.err||'Kiểm tra kết nối Wi-Fi'));
  }catch(e){alert('Không thể gọi API Trạm Bờ!');}
}
loadCfg();

// --- DPR-Aware Dynamic Canvas Helper ---
function getDprCtx(canvas){
  const dpr=window.devicePixelRatio||1;
  const rect=canvas.getBoundingClientRect();
  const reqW=Math.round(rect.width*dpr), reqH=Math.round(rect.height*dpr);
  if(canvas.width!==reqW||canvas.height!==reqH){
    canvas.width=reqW;
    canvas.height=reqH;
  }
  const ctx=canvas.getContext('2d');
  ctx.save();
  ctx.scale(dpr,dpr);
  return {ctx, w:rect.width, h:rect.height};
}
function constrain(v,min,max){return Math.max(min,Math.min(max,v));}

// --- Canvas 1: Horizon & Attitude Gyro ---
const hzC=document.getElementById('horizonCanvas');
let curRoll=0, curPitch=0, curHdg=0;
function drawHorizon(roll,pitch,hdg){
  curRoll=roll; curPitch=pitch; curHdg=hdg;
  const {ctx, w, h}=getDprCtx(hzC);
  const cx=w/2, cy=h/2;
  ctx.clearRect(0,0,w,h);
  ctx.save();
  ctx.translate(cx,cy);
  ctx.rotate(-roll*Math.PI/180);
  const pY=constrain(pitch*1.2,-h*0.4,h*0.4);

  // Bầu trời & Biển
  ctx.fillStyle='#0f2b48';
  ctx.fillRect(-w,-h-pY,w*2,h);
  ctx.fillStyle='#041b2f';
  ctx.fillRect(-w,-pY,w*2,h*2);

  // Đường chân trời neon cyan
  ctx.strokeStyle='#00f5d4';ctx.lineWidth=2;
  ctx.beginPath();ctx.moveTo(-w,-pY);ctx.lineTo(w,-pY);ctx.stroke();

  // Thang đo Pitch (-20, -10, 10, 20)
  ctx.strokeStyle='rgba(255,255,255,0.4)';ctx.lineWidth=1;
  [-20,-10,10,20].forEach(d=>{
    const y=-pY-(d*1.2);
    ctx.beginPath();ctx.moveTo(-18,y);ctx.lineTo(18,y);ctx.stroke();
  });
  ctx.restore();

  // Silhouette Thuyền Catamaran ở tâm
  ctx.strokeStyle='#ffd166';ctx.lineWidth=2.5;
  ctx.strokeRect(cx-26,cy-4,14,8); // Phao trái
  ctx.strokeRect(cx+12,cy-4,14,8); // Phao phải
  ctx.strokeStyle='#fff';ctx.lineWidth=2;
  ctx.beginPath();ctx.moveTo(cx-12,cy);ctx.lineTo(cx+12,cy);ctx.stroke();
  ctx.fillStyle='#00f5d4';ctx.beginPath();ctx.arc(cx,cy,3,0,Math.PI*2);ctx.fill();
  ctx.restore();
}

// --- Canvas 2: Water Depth Column & Winch Probe ---
const dpC=document.getElementById('depthCanvas');
let curDep=0, curStage=0;
function drawDepth(depthM,stage){
  curDep=depthM; curStage=stage;
  const {ctx, w, h}=getDprCtx(dpC);
  ctx.clearRect(0,0,w,h);

  // Gradient 3 tầng nước nằm ngang
  const g=ctx.createLinearGradient(10,0,w-20,0);
  g.addColorStop(0,'#00f5d4');   // Mặt nước
  g.addColorStop(0.45,'#00bbf9'); // Giữa hồ
  g.addColorStop(1,'#0c1b33');   // Đáy hồ
  ctx.fillStyle=g;
  ctx.fillRect(10,22,w-20,26);
  ctx.strokeStyle='rgba(255,255,255,0.2)';ctx.lineWidth=1;
  ctx.strokeRect(10,22,w-20,26);

  // Thang mốc: 0.0m, 0.3m, 1.0m, 1.8m, 2.0m
  ctx.fillStyle='rgba(255,255,255,0.7)';ctx.font='10px monospace';
  const getX=d=>10+constrain(d/2.0,0,1)*(w-20);
  [{d:0.0,l:'0m'},{d:0.3,l:'MẶT 0.3m'},{d:1.0,l:'GIỮA 1.0m'},{d:1.8,l:'ĐÁY 1.8m'},{d:2.0,l:'2m'}].forEach(m=>{
    const x=getX(m.d);
    ctx.fillRect(x,18,1.5,34);
    ctx.fillText(m.l,x-12,14);
  });

  // Vị trí quả rọi lồng cảm biến thực tế
  const probeX=getX(depthM);
  ctx.fillStyle='#ff4757';
  ctx.beginPath();ctx.arc(probeX,35,8,0,Math.PI*2);ctx.fill();
  ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.stroke();
  ctx.fillStyle='#fff';ctx.font='bold 11px monospace';
  ctx.fillText(`Lồng: ${depthM.toFixed(2)}m`,Math.max(12,probeX-30),72);
  ctx.restore();
}

// --- Canvas 3: Virtual Touch Joystick ---
const jp=document.getElementById('joyCanvas'),jctx=jp.getContext('2d');
let jDrag=false,jx=0,jy=0,lastJoySend=0;
function drawJoyPad(nx,ny){
  const cx=jp.width/2,cy=jp.height/2,rMax=jp.width/2-18;
  jctx.clearRect(0,0,jp.width,jp.height);
  jctx.strokeStyle='rgba(0,245,212,0.25)';jctx.lineWidth=1.5;
  jctx.beginPath();jctx.arc(cx,cy,rMax*0.5,0,Math.PI*2);jctx.stroke();
  jctx.beginPath();jctx.arc(cx,cy,rMax,0,Math.PI*2);jctx.stroke();
  jctx.beginPath();jctx.moveTo(cx-rMax,cy);jctx.lineTo(cx+rMax,cy);
  jctx.moveTo(cx,cy-rMax);jctx.lineTo(cx,cy+rMax);jctx.stroke();
  const kx=cx+nx*rMax,ky=cy-ny*rMax;
  jctx.strokeStyle='rgba(0,245,212,0.7)';jctx.lineWidth=2.5;
  jctx.beginPath();jctx.moveTo(cx,cy);jctx.lineTo(kx,ky);jctx.stroke();
  jctx.fillStyle='var(--green)';jctx.beginPath();jctx.arc(kx,ky,18,0,Math.PI*2);jctx.fill();
  jctx.strokeStyle='#fff';jctx.lineWidth=2;jctx.stroke();
}
drawJoyPad(0,0);

function sendJoyVec(nx,ny,force){
  const now=Date.now();
  if(!force && now-lastJoySend<90)return;
  lastJoySend=now;
  const m=Math.round(ny*255*gearScale),r=Math.round(90+nx*45);
  document.getElementById('joy_text').innerText=`L298N PWM: ${m} | Bánh lái: ${r}° | Cấp số: ${Math.round(gearScale*100)}%`;
  fetch(`/api/cmd?m=${m}&r=${r}&w=0`,{headers:authHdr()}).catch(()=>{});
}
function handleJoyMove(clientX,clientY){
  const rect=jp.getBoundingClientRect(),cx=rect.width/2,cy=rect.height/2,rMax=rect.width/2-18;
  let dx=clientX-rect.left-cx,dy=cy-(clientY-rect.top);
  const dist=Math.hypot(dx,dy);
  if(dist>rMax){dx=(dx/dist)*rMax;dy=(dy/dist)*rMax;}
  jx=dx/rMax;jy=dy/rMax;
  drawJoyPad(jx,jy);sendJoyVec(jx,jy,false);
}
function resetJoy(){if(!jDrag)return;jDrag=false;jx=0;jy=0;drawJoyPad(0,0);sendJoyVec(0,0,true);}
jp.addEventListener('pointerdown',e=>{jDrag=true;try{jp.setPointerCapture(e.pointerId);}catch(err){}handleJoyMove(e.clientX,e.clientY);});
jp.addEventListener('pointermove',e=>{if(jDrag)handleJoyMove(e.clientX,e.clientY);});
jp.addEventListener('pointerup',resetJoy);
jp.addEventListener('pointercancel',resetJoy);

// --- Canvas 4: Real-time Telemetry Sparkline ---
const hist=[];
const chC=document.getElementById('chartCanvas');
function drawChart(){
  const {ctx, w, h}=getDprCtx(chC);
  ctx.clearRect(0,0,w,h);
  if(hist.length<2)return;
  const plot=(arr,min,max,col)=>{
    ctx.beginPath();ctx.strokeStyle=col;ctx.lineWidth=2;
    arr.forEach((v,i)=>{
      const x=(i/(arr.length-1))*(w-16)+8;
      const y=h-8-constrain((v-min)/(max-min),0,1)*(h-16);
      if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
    });
    ctx.stroke();
  };
  plot(hist.map(x=>x.ph),0,14,'#06d6a0');
  plot(hist.map(x=>x.temp),0,45,'#00bbf9');
  plot(hist.map(x=>x.bat),9,14,'#ffd166');
  ctx.restore();
}
function dlCsv(){
  let csv='ThoiGian,Seq,pH,DO_mgL,TDS_ppm,EC_uScm,NTU,NhietNuoc_C,Pin_V,Dong_A,CongSuat_W,Roll,Pitch\n';
  hist.forEach(d=>{csv+=`${d.ts},${d.seq},${d.ph},${d.do},${d.tds},${d.ec},${d.ntu},${d.temp},${d.bat},${d.amp},${d.watt},${d.roll},${d.pitch}\n`;});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv;charset=utf-8;'}));
  a.download='USV_Aqua_Telemetry.csv';a.click();
}

// Window resize handler
window.addEventListener('resize',()=>{
  drawHorizon(curRoll,curPitch,curHdg);
  drawDepth(curDep,curStage);
  drawChart();
});

// Hardware component definitions
const hwDefs=[
  {b:1,n:'Nhiệt DS18B20'},{b:2,n:'Cảm biến pH'},{b:4,n:'Cảm biến TDS'},
  {b:8,n:'Độ đục NTU'},{b:16,n:'Dòng ACS712'},{b:32,n:'Đo Pin 3S'},
  {b:64,n:'IMU MPU6050'},{b:128,n:'La bàn GY271'},{b:256,n:'LoRa SX1278'},
  {b:512,n:'SIM 4G A7680'},{b:1024,n:'GPS NEO-8M'}
];

// Telemetry Polling Loop (P2-1: Có khóa chống chồng request pollInFlight)
let pollInFlight=false;
async function pollTelemetryLoop(){
  if(pollInFlight)return;
  pollInFlight=true;
  try{
    const r=await fetch('/api/telemetry');const d=await r.json();
    d.ts=new Date().toLocaleTimeString();hist.push(d);if(hist.length>50)hist.shift();
    const m=d.hwm||0;

    // Badges & Header
    document.getElementById('badge_lora').innerText=`📡 LORA: ${d.rssi}dBm`;
    document.getElementById('badge_wifi').innerText=d.sta?`📶 STA: ${d.ip}`:'📶 AP 192.168.4.1';
    document.getElementById('badge_bat').innerText=`🔋 ${d.bat.toFixed(1)}V (${d.batpct}%)`;
    document.getElementById('badge_gps').innerText=`🛰️ GPS: ${d.sats||0} SV`;
    document.getElementById('header_status').innerText=d.online?`LIVE LoRa #${d.seq} (${d.src}) | Uptime: ${Math.round(d.uptime/60)}ph`:'MẤT KẾT NỐI PHAO!';

    // Attitude Gyro
    drawHorizon(d.roll||0,d.pitch||0,d.hdg||0);
    document.getElementById('txt_gyro').innerText=`Roll: ${(d.roll||0).toFixed(1)}° | Pitch: ${(d.pitch||0).toFixed(1)}°`;
    document.getElementById('txt_hdg').innerText=`${(d.hdg||0).toFixed(0)}°`;
    document.getElementById('txt_spd').innerText=`${(d.spd||0).toFixed(1)} km/h`;
    document.getElementById('txt_homed').innerText=`${(d.homed||0).toFixed(1)} m`;

    // Water Column
    drawDepth(d.dep||0,d.stage||0);
    document.getElementById('txt_stage').innerText=`Độ sâu: ${(d.dep||0).toFixed(2)}m`;

    // Water Matrix
    document.getElementById('val_ph').innerText=(m&2)?d.ph.toFixed(2):'CHƯA CẮM';
    document.getElementById('sub_ph').innerText=(m&2)?`DO: ${d.do.toFixed(2)} mg/L`:'GPIO 7';
    document.getElementById('val_tds').innerText=(m&4)?d.tds.toFixed(0)+' ppm':'CHƯA CẮM';
    document.getElementById('sub_ec').innerText=(m&4)?`EC: ${d.ec.toFixed(0)} µS/cm`:'GPIO 4';
    document.getElementById('val_ntu').innerText=(m&8)?d.ntu.toFixed(0)+' NTU':'CHƯA CẮM';
    document.getElementById('sub_ntu').innerText=(m&8)?(d.ntu<=50?'Nước trong':'Nước đục'):'GPIO 5';
    document.getElementById('val_temp').innerText=(m&1)?d.temp.toFixed(1)+'°C':'CHƯA CẮM';
    document.getElementById('sub_temp').innerText=`S3: ${(d.s3t||0).toFixed(0)}°C | GCS: ${(d.gcst||0).toFixed(0)}°C`;

    // QCVN 08 Classification
    let qtxt='QCVN 08:2023/BTNMT Cột A - Rất Tốt (Nước sinh hoạt)';
    let qcol='var(--green)';
    if(d.wqi>=75){qtxt='MỨC A: RẤT TỐT (WQI: '+d.wqi+') - Đạt quy chuẩn QCVN 08';}
    else if(d.wqi>=50){qtxt='MỨC B: TRUNG BÌNH (WQI: '+d.wqi+') - Bảo tồn thủy sinh';qcol='var(--amber)';}
    else if(d.wqi>0){qtxt='MỨC NGUY HẠI (WQI: '+d.wqi+') - Ô nhiễm vượt chuẩn!';qcol='var(--red)';}
    document.getElementById('qcvn_badge').innerText=`VN-WQI: ${d.wqi}/100`;
    document.getElementById('qcvn_banner').innerText=qtxt;
    document.getElementById('qcvn_banner').style.color=qcol;

    // Power & Battery
    document.getElementById('val_bat').innerText=(m&32)?d.bat.toFixed(2)+' V':'--';
    document.getElementById('sub_bat').innerText=(m&32)?`Dung lượng: ${d.batpct}%`:'GPIO 3';
    document.getElementById('val_amp').innerText=(m&16)?d.amp.toFixed(2)+' A':'--';
    document.getElementById('val_watt').innerText=(m&16&&m&32)?d.watt.toFixed(1)+' W':'--';
    const solW=(m&32&&d.bat>11.5)?Math.min(25.0,(d.bat-11.2)*2.8):0.0;
    document.getElementById('val_sol').innerText=solW.toFixed(1)+' W';
    document.getElementById('sub_sol').innerText=`Cân bằng: ${(solW-d.watt).toFixed(1)}W`;

    // Hardware Grid
    let okCnt=0,hwHtml='';
    hwDefs.forEach(h=>{
      const ok=(m&h.b)!==0;if(ok)okCnt++;
      hwHtml+=`<div class="hw-node ${ok?'ok':'err'}"><span>${h.n}</span><span>${ok?'OK':'LỖI'}</span></div>`;
    });
    document.getElementById('hw_nodes').innerHTML=hwHtml;
    document.getElementById('hw_summary').innerText=`${okCnt}/11 LINH KIỆN HOẠT ĐỘNG`;

    // Cloud Info
    document.getElementById('cloud_info').innerText=
      `Google Sheets: HTTP ${d.gsstat} | Đã gửi: ${d.gspush} | Lỗi: ${d.gsfail} | Heap: ${Math.round(d.heap/1024)}KB`;

    // Ho So 3 Tang Nuoc
    if(d.prf){
      const updateRow=(prefix, layer)=>{
        const stEl=document.getElementById(prefix+'_st');
        const phEl=document.getElementById(prefix+'_ph');
        const tdsEl=document.getElementById(prefix+'_tds');
        const ntuEl=document.getElementById(prefix+'_ntu');
        const tEl=document.getElementById(prefix+'_temp');
        const doEl=document.getElementById(prefix+'_do');
        const evEl=document.getElementById(prefix+'_eval');
        if(!layer || !layer.v){
          if(stEl) stEl.innerHTML='<span style="color:var(--sub)">Chờ đo</span>';
          if(phEl) phEl.innerText='--';
          if(tdsEl) tdsEl.innerText='--';
          if(ntuEl) ntuEl.innerText='--';
          if(tEl) tEl.innerText='--';
          if(doEl) doEl.innerText='--';
          if(evEl) evEl.innerText='Chưa có mẫu';
        } else {
          if(stEl) stEl.innerHTML='<span style="color:var(--green)">✅ Đã đo</span>';
          if(phEl){
            phEl.innerText=layer.ph.toFixed(2);
            phEl.style.color=(layer.ph<6.5||layer.ph>8.5)?'var(--amber)':'var(--green)';
          }
          if(tdsEl) tdsEl.innerText=layer.tds.toFixed(0)+' ppm';
          if(ntuEl) ntuEl.innerText=layer.ntu.toFixed(0)+' NTU';
          if(tEl) tEl.innerText=layer.t.toFixed(1)+'°C';
          if(doEl) doEl.innerText=layer.do.toFixed(2)+' mg/L';
          if(evEl){
            let eStr='Bình thường';
            if(layer.ntu>100) eStr='Đục bùn đáy';
            else if(layer.ph<6.0) eStr='Axit nhẹ';
            else if(layer.do<3.0) eStr='Thiếu oxy';
            evEl.innerText=eStr;
          }
        }
      };
      updateRow('prf_s', d.prf.s);
      updateRow('prf_m', d.prf.m);
      updateRow('prf_b', d.prf.b);

      const stratEl=document.getElementById('strat_detail');
      if(stratEl && d.prf.s && d.prf.s.v && d.prf.b && d.prf.b.v){
        const dt = (d.prf.s.t - d.prf.b.t).toFixed(1);
        const dtds = (d.prf.b.tds - d.prf.s.tds).toFixed(0);
        const ddo = (d.prf.s.do - d.prf.b.do).toFixed(2);
        stratEl.innerHTML =
          `• Chênh lệch nhiệt độ mặt - đáy: <b>ΔT = ${dt}°C</b> (${Math.abs(dt)>3.0?'Cảnh báo sốc nhiệt thủy sinh!':'Phân tầng bình thường'}).<br>`+
          `• Chênh lệch khoáng chất đáy - mặt: <b>ΔTDS = ${dtds} ppm</b>.<br>`+
          `• Dưỡng khí hòa tan đáy hồ: <b>DO = ${d.prf.b.do.toFixed(2)} mg/L</b> (${d.prf.b.do<2.0?'🚨 Nguy cơ yếm khí đáy hồ!':'Đủ dưỡng khí thủy sinh'}).`;
      }
    }

    // Bang nhat ky telemetry truc tiep
    const tbody = document.getElementById('live_log_body');
    if(tbody && d.seq){
      if(liveLogs.length===0 || liveLogs[0].seq !== d.seq){
        liveLogs.unshift(d);
        if(liveLogs.length>30) liveLogs.pop();
        let qCol = (d.wqi>=75)?'var(--green)':((d.wqi>=50)?'var(--amber)':'var(--red)');
        let rowHtml = `<tr>
          <td>${d.ts}</td>
          <td style="color:var(--cyan)">#${d.seq}</td>
          <td style="font-weight:700;color:${(d.ph<6.5||d.ph>8.5)?'var(--amber)':'var(--green)'}">${d.ph.toFixed(2)}</td>
          <td>${d.tds.toFixed(0)}</td>
          <td>${d.ntu.toFixed(0)}</td>
          <td>${d.temp.toFixed(1)}°C</td>
          <td>${d.bat.toFixed(2)}V</td>
          <td>${(d.dep||0).toFixed(2)}m</td>
          <td>${d.sats||0}</td>
          <td style="color:${qCol}">WQI ${d.wqi}</td>
        </tr>`;
        tbody.insertAdjacentHTML('afterbegin', rowHtml);
        while(tbody.rows.length>30){ tbody.deleteRow(tbody.rows.length-1); }
      }
    }

    drawChart();
  }catch(e){}finally{
    pollInFlight=false;
    setTimeout(pollTelemetryLoop,600);
  }
}
pollTelemetryLoop();
</script>
</body>
</html>
)rawliteral";

const char OTA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ChemStation AI - OTA Update</title>
<style>body{background:#070d1f;color:#e0fbfc;font-family:system-ui,sans-serif;padding:18px} .card{background:#141e38;border:1px solid #2b3d63;border-radius:10px;padding:16px;max-width:620px;margin:auto} h1{color:#00f5d4;font-size:1.2rem} input,button{width:100%;box-sizing:border-box;margin-top:10px;padding:10px;border-radius:6px;border:1px solid #5bc0be;background:#070d1f;color:#fff} button{background:#06d6a0;color:#000;font-weight:800;cursor:pointer} a{color:#8ecae6}</style></head><body>
<div class="card"><h1>NẠP FIRMWARE QUA WI-FI (OTA)</h1>
<p>Chọn file <b>firmware.bin</b> được tạo bởi PlatformIO. Không tắt nguồn trong quá trình nạp.</p>
<form method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="firmware" accept=".bin" required>
<button type="submit">BẮT ĐẦU NẠP</button></form>
<p><a href="/">← Quay lại Dashboard</a></p></div></body></html>
)rawliteral";


// P0-1: Kiểm tra xác thực Quản trị cho toàn bộ HTTP route nhạy cảm (/api/cmd, /api/config, /update, v.v.)
bool isWebAuthenticated() {
  if (strlen(gcsAdminToken) == 0) return true;
  if (webServer.hasHeader("X-GCS-Token")) {
    String h = webServer.header("X-GCS-Token");
    h.trim();
    if (h == gcsAdminToken) return true;
  }
  if (webServer.hasArg("token")) {
    String t = webServer.arg("token");
    t.trim();
    if (t == gcsAdminToken) return true;
  }
  if (webServer.hasHeader("Cookie")) {
    String ck = webServer.header("Cookie");
    String needle = String("gcs_token=") + gcsAdminToken;
    if (ck.indexOf(needle) >= 0) return true;
  }
  if (webServer.authenticate("admin", gcsAdminToken)) return true;
  return false;
}

bool requireWebAuth() {
  if (isWebAuthenticated()) return true;
  webServer.send(401, "application/json", "{\"ok\":false,\"err\":\"Unauthorized: Yeu cau ma xac thuc Quan tri (X-GCS-Token)\"}");
  return false;
}

void setupWebServer() {
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  IPAddress apIP(192, 168, 4, 1);
  IPAddress apNetmask(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, apNetmask);
  if (strlen(staWifiSsid) > 0) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(GCS_WIFI_SSID, apWifiPass);
    WiFi.begin(staWifiSsid, staWifiPass);
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(GCS_WIFI_SSID, apWifiPass);
  }
  dnsServer.start(53, "*", apIP);
  if (MDNS.begin(GCS_MDNS_HOST)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[GCS-MDNS] http://%s.local\n", GCS_MDNS_HOST);
  }

  const char* headerKeys[] = {"X-GCS-Token", "Cookie"};
  webServer.collectHeaders(headerKeys, 2);

  webServer.on("/api/login", HTTP_ANY, []() {
    String tk = webServer.arg("token");
    if (tk.length() == 0) tk = webServer.arg("pass");
    tk.trim();
    if (tk == gcsAdminToken) {
      String cookie = String("gcs_token=") + gcsAdminToken + "; Path=/; HttpOnly; SameSite=Strict";
      webServer.sendHeader("Set-Cookie", cookie);
      webServer.send(200, "application/json", "{\"ok\":true}");
    } else {
      webServer.send(401, "application/json", "{\"ok\":false,\"err\":\"Sai ma xac thuc Quan tri!\"}");
    }
  });

  webServer.on("/", HTTP_GET, []() {
    webServer.send_P(200, "text/html", WEB_DASHBOARD_HTML);
  });

  webServer.on("/3d", HTTP_GET, []() {
#ifdef HAVE_WEB_3D_MAP_GZ
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send_P(200, "text/html; charset=utf-8", (const char*)WEB_3D_MAP_HTML_GZ, sizeof(WEB_3D_MAP_HTML_GZ));
#else
    webServer.send_P(200, "text/html; charset=utf-8", WEB_3D_MAP_HTML);
#endif
  });

  webServer.on("/api/telemetry", HTTP_GET, []() {
    static char json[3584];
    bool s3Online = (rxPacketCount > 0) && (millis() - lastRxFromBuoy < 2800);
    bool okBAT = (uHwHealth & HW_OK_BAT3S) != 0;
    int batPct = okBAT ? calcBattery3SPercent(uBat) : 0;
    char ipBuf[24];
    copyStringSafe(ipBuf, sizeof(ipBuf), WiFi.localIP().toString().c_str());
    bool staUp = (WiFi.status() == WL_CONNECTED);
    unsigned long ageMs = (lastRxFromBuoy > 0) ? (millis() - lastRxFromBuoy) : 0;
    snprintf(
      json, sizeof(json),
      "{\"online\":%s,\"seq\":%lu,\"ph\":%.2f,\"do\":%.2f,\"tds\":%.0f,\"ec\":%.0f,\"ntu\":%.0f,"
      "\"temp\":%.1f,\"mtmp\":%.1f,\"s3t\":%.1f,\"gcst\":%.1f,\"wqi\":%d,\"dep\":%.2f,"
      "\"bat\":%.2f,\"batpct\":%d,\"amp\":%.2f,\"watt\":%.2f,"
      "\"rssi\":%d,\"snr\":%.1f,\"roll\":%.1f,\"pitch\":%.1f,\"hdg\":%.0f,"
      "\"lat\":%.5f,\"lng\":%.5f,\"src\":\"%s\",\"jx\":%.2f,\"jy\":%.2f,"
      "\"hwm\":%u,\"adf\":%u,\"joy\":%s,\"alr\":%u,"
      "\"sats\":%u,\"spd\":%.2f,\"alt\":%.1f,\"homed\":%.1f,\"flags2\":%u,\"tgthdg\":%.0f,"
      "\"stage\":%d,\"age\":%lu,\"uptime\":%lu,\"heap\":%u,"
      "\"gsstat\":%d,\"gspush\":%lu,\"gsfail\":%lu,\"gslast\":%lu,"
      "\"prf\":{\"s\":{\"v\":%s,\"ph\":%.2f,\"tds\":%.0f,\"ntu\":%.0f,\"t\":%.1f,\"do\":%.2f},"
      "\"m\":{\"v\":%s,\"ph\":%.2f,\"tds\":%.0f,\"ntu\":%.0f,\"t\":%.1f,\"do\":%.2f},"
      "\"b\":{\"v\":%s,\"ph\":%.2f,\"tds\":%.0f,\"ntu\":%.0f,\"t\":%.1f,\"do\":%.2f}},"
      "\"sta\":%s,\"wifi\":%d,\"ip\":\"%s\",\"fw\":\"%s\",\"proto\":%d,\"boot\":%lu}",
      s3Online ? "true" : "false", (unsigned long)uS3SeqId,
      uPH, uDO, uTDS, uEC, uNTU, uTemp, uMpuTemp, uS3ChipTemp, uGcsChipTemp, uWQI, uDepthM,
      uBat, batPct, uAmp, uPowerWatt,
      uRSSI, uSNR, uRoll, uPitch, uHeading,
      uLat, uLng, activeCtrlSrc, joyNormX, joyNormY,
      uHwHealth, uAdcFault, joy1HardwarePresent ? "true" : "false", uAlertFlags,
      uSats, uGpsSpeedKmph, uGpsAltM, uHomeDistanceM, uFlags2, uTargetHeadingDeg,
      (int)uAutoStage, ageMs, millis() / 1000UL, (unsigned int)ESP.getFreeHeap(),
      gsheetLastStatus, (unsigned long)gsheetPushCount, (unsigned long)gsheetFailCnt,
      lastGsheetPushMs ? (millis() - lastGsheetPushMs) : 0UL,
      gcsLayerSurf.valid ? "true" : "false", gcsLayerSurf.ph, gcsLayerSurf.tds, gcsLayerSurf.ntu, gcsLayerSurf.temp, gcsLayerSurf.doMgL,
      gcsLayerMid.valid  ? "true" : "false", gcsLayerMid.ph,  gcsLayerMid.tds,  gcsLayerMid.ntu,  gcsLayerMid.temp,  gcsLayerMid.doMgL,
      gcsLayerBot.valid  ? "true" : "false", gcsLayerBot.ph,  gcsLayerBot.tds,  gcsLayerBot.ntu,  gcsLayerBot.temp,  gcsLayerBot.doMgL,
      staUp ? "true" : "false", staUp ? WiFi.RSSI() : 0, ipBuf,
      FW_VERSION, PROTOCOL_VERSION, (unsigned long)gcsBootCount
    );
    webServer.send(200, "application/json", json);
  });

  // 1. API TRẢ VỀ DỮ LIỆU LỊCH SỬ CHO THANH TRƯỢT TUA 3D (TIME SLIDER)
  webServer.on("/api/history", HTTP_GET, []() {
    String json = "{\"ok\":true,\"rows\":[";
    int n = min(30, (int)GRAPH_PTS);
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      int idx = GRAPH_PTS - 1 - i;
      json += "{\"seq\":" + String(uS3SeqId > i ? (uS3SeqId - i) : 1) + 
              ",\"ph\":" + String(histPH[idx] > 0.1f ? String(histPH[idx], 2) : "0.00") + 
              ",\"temp\":" + String(histTemp[idx] > 0.1f ? String(histTemp[idx], 1) : "0.0") + 
              ",\"tds\":" + String((int)uTDS) + 
              ",\"ntu\":" + String((int)uNTU) + 
              ",\"wqi\":" + String(uWQI) + 
              ",\"depth\":" + String(uDepthM, 2) + 
              ",\"lat\":" + String(uLat, 6) + 
              ",\"lng\":" + String(uLng, 6) + "}";
    }
    json += "]}";
    webServer.send(200, "application/json", json);
  });

  // 2. API TƯ VẤN AI DỰA TRÊN QUY CHUẨN QCVN 08:2023 THỰC TẾ
  webServer.on("/api/ai_consult", HTTP_GET, []() {
    const char* risk = (uWQI >= 75) ? "AN TOÀN (A1)" : ((uWQI >= 50) ? "CẢNH BÁO (B1)" : "Ô NHIỄM (B2)");
    const char* badge = (uWQI >= 75) ? "emerald" : ((uWQI >= 50) ? "amber" : "red");
    
    String diag = "Đánh giá chuẩn QCVN 08:2023: ";
    if (uPH > 0.1f && (uPH < 6.5f || uPH > 8.5f)) diag += "Độ pH vượt ngưỡng an toàn sinh thái. ";
    if (uNTU > 100.0f) diag += "Độ đục cao do xáo trộn bùn đáy hồ. ";
    if (uTDS > 1000.0f) diag += "Tổng khoáng cao, có nguy cơ nhiễm mặn. ";
    if (diag.length() < 50) diag += "Mọi thông số quan trắc đang nằm trong giới hạn an toàn.";

    String json = "{\"risk_level\":\"" + String(risk) + "\",\"badge_color\":\"" + String(badge) + "\",\"diagnostics\":[\"" + diag + "\"]}";
    webServer.send(200, "application/json", json);
  });

  webServer.on("/api/cmd", HTTP_ANY, []() {
    if (!requireWebAuth()) return;
    if (webServer.hasArg("page")) {
      int p = constrain(webServer.arg("page").toInt(), 0, 4);
      if (p != currentPage) {
        currentPage = p;
        drawCurrentStaticPage();
      }
      webServer.send(200, "text/plain", "OK");
      return;
    }
    if (webServer.hasArg("gear")) {
      int g = constrain(webServer.arg("gear").toInt(), 0, 2);
      speedGear = (SpeedGearMode)g;
      webServer.send(200, "text/plain", "OK");
      return;
    }
    if (webServer.hasArg("caljoy")) {
      calibrateJoystickCenterNow();
      webServer.send(200, "text/plain", "OK");
      return;
    }
    if (webServer.hasArg("cbat")) {
      float v = webServer.arg("cbat").toFloat();
      if (v >= 3.0f && v <= 18.0f) {
        targetCalibBatVolt    = v;
        pendingCalibBatToBuoy = true;
      }
      webServer.send(200, "text/plain", "OK");
      return;
    }
    if (webServer.hasArg("tare"))    { queueSpecialCmd("CMD:0,90,9");  webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("home") || webServer.hasArg("sethome")) { queueSpecialCmd("CMD:0,90,6"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("goto"))    { queueSpecialCmd("CMD:0,90,10"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("winch_home")){ queueSpecialCmd("CMD:0,90,2"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("stopall")) { queueSpecialCmd("CMD:0,90,11"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("hdg_on"))  { queueSpecialCmd("CMD:0,90,4");  webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("hdg_off")) { queueSpecialCmd("CMD:0,90,5");  webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("inch_dn")) { queueSpecialCmd("CMD:0,90,14"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("inch_up")) { queueSpecialCmd("CMD:0,90,15"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("anchor_on")){ queueSpecialCmd("CMD:0,90,16"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("anchor_off")){ queueSpecialCmd("CMD:0,90,17"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("report"))  { queueSpecialCmd("CMD:0,90,13"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("emerg"))   { queueSpecialCmd("CMD:0,90,8");  webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("sample"))  { queueSpecialCmd("CMD:0,90,18"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("clean"))   { queueSpecialCmd("CMD:0,90,19"); webServer.send(200, "text/plain", "OK"); return; }
    if (webServer.hasArg("cal_ph")) {
      int pt = webServer.arg("cal_ph").toInt();
      char c[32];
      snprintf(c, sizeof(c), "CMD:%d,90,20", (pt == 4) ? 4 : 7);
      queueSpecialCmd(c);
      webServer.send(200, "text/plain", "OK");
      return;
    }
    if (webServer.hasArg("dp_rad")) {
      int dm = constrain(webServer.arg("dp_rad").toInt(), 5, 100);
      char c[32];
      snprintf(c, sizeof(c), "CMD:%d,90,21", dm);
      queueSpecialCmd(c);
      webServer.send(200, "text/plain", "OK");
      return;
    }
    if (webServer.hasArg("tx_int")) {
      int ms = constrain(webServer.arg("tx_int").toInt(), 500, 5000);
      char c[32];
      snprintf(c, sizeof(c), "CMD:%d,90,22", ms);
      queueSpecialCmd(c);
      webServer.send(200, "text/plain", "OK");
      return;
    }
    if (webServer.hasArg("hdg")) {
      int deg = constrain(webServer.arg("hdg").toInt(), 0, 359);
      char c[32];
      snprintf(c, sizeof(c), "CMD:%d,90,12", deg);
      queueSpecialCmd(c);
      webServer.send(200, "text/plain", "OK");
      return;
    }
    if (webServer.hasArg("sync")) {
      pendingSyncUrlToBuoy = true;
      webServer.send(200, "text/plain", "OK");
      return;
    }
    if (webServer.hasArg("push")) {
      bool isPrf = (webServer.arg("push") == "profile");
      triggerGoogleSheetsPush(isPrf);
      webServer.send(200, "text/plain", "OK");
      return;
    }
    if (webServer.hasArg("m")) webMotorOverride  = constrain(webServer.arg("m").toInt(), -255, 255);
    if (webServer.hasArg("r")) webRudderOverride = constrain(webServer.arg("r").toInt(), 45, 135);
    if (webServer.hasArg("w")) webWinchOverride  = constrain(webServer.arg("w").toInt(), 0, 9);

    if (webWinchOverride == WINCH_CMD_AUTO_3LVL) {
      autoModeRequested = true;
    } else if (webWinchOverride == WINCH_CMD_EMERGENCY_STOP) {
      autoModeRequested = false;
      webMotorOverride  = 0;
      webRudderOverride = 90;
    } else {
      // Lệnh tời thủ công (DỪNG / HẠ / THU HOME) phải hủy chế độ Auto 3 tầng,
      // giống hành vi công tắc vật lý SW5/SW6 — tránh Auto bật lại sau 2.5s.
      autoModeRequested = false;
    }
    webOverrideExpireMs = millis() + 2500UL;
    webServer.send(200, "text/plain", "OK");
  });

  webServer.on("/api/wifi_scan", HTTP_GET, []() {
    // P1-1: Bảo vệ xác thực + Giới hạn tần suất quét (Rate-limit 10 giây)
    if (!requireWebAuth()) return;
    unsigned long nowMs = millis();
    if (cachedWifiScanJson.length() > 0 && (nowMs - lastWifiScanMs < 10000UL)) {
      webServer.send(200, "application/json", cachedWifiScanJson);
      return;
    }
    int n = WiFi.scanNetworks(false, false);
    String json = "{\"ok\":true,\"networks\":[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      char ssidEsc[110];
      jsonEscapeInto(ssidEsc, sizeof(ssidEsc), WiFi.SSID(i).c_str());
      json += "{\"ssid\":\"";
      json += ssidEsc;
      json += "\",\"rssi\":";
      json += WiFi.RSSI(i);
      json += ",\"enc\":";
      json += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "true" : "false";
      json += "}";
    }
    json += "]}";
    WiFi.scanDelete();
    lastWifiScanMs = nowMs;
    cachedWifiScanJson = json;
    webServer.send(200, "application/json", json);
  });

  webServer.on("/api/config", HTTP_GET, []() {
    // Thoát JSON trước khi ghép chuỗi (SSID/URL có thể chứa " hoặc \)
    static char urlEsc[400], discEsc[460], ssidEsc[110], ipEsc[40], apEsc[40], mdnsEsc[40];
    jsonEscapeInto(urlEsc,   sizeof(urlEsc),   gsheetScriptUrl);
    const char* discSrc = isWebAuthenticated() ? discordWebhookUrl : (strlen(discordWebhookUrl) > 20 ? "https://discord.com/api/webhooks/***" : "");
    jsonEscapeInto(discEsc,  sizeof(discEsc),  discSrc);
    jsonEscapeInto(ssidEsc,  sizeof(ssidEsc),  staWifiSsid);
    jsonEscapeInto(ipEsc,    sizeof(ipEsc),    WiFi.localIP().toString().c_str());
    jsonEscapeInto(apEsc,    sizeof(apEsc),    GCS_WIFI_SSID);
    jsonEscapeInto(mdnsEsc,  sizeof(mdnsEsc),  GCS_MDNS_HOST);

    static char json[1500];
    snprintf(
      json, sizeof(json),
      "{\"url\":\"%s\",\"discord\":\"%s\",\"ssid\":\"%s\",\"has_pass\":%s,\"ap\":\"%s\",\"ip\":\"%s\","
      "\"sta\":%s,\"fw\":\"%s\",\"boot\":%lu,\"uptime\":%lu,\"heap\":%u,\"mdns\":\"http://%s.local\"}",
      urlEsc, discEsc, ssidEsc, strlen(staWifiPass) ? "true" : "false",
      apEsc, ipEsc,
      (WiFi.status() == WL_CONNECTED) ? "true" : "false",
      FW_VERSION, (unsigned long)gcsBootCount, millis() / 1000UL,
      (unsigned int)ESP.getFreeHeap(), mdnsEsc
    );
    webServer.send(200, "application/json", json);
  });

  webServer.on("/api/config", HTTP_POST, []() {
    if (!requireWebAuth()) return;
    bool changedWifi = false;
    if (webServer.hasArg("ap_pass")) {
      String ap = webServer.arg("ap_pass"); ap.trim();
      if (ap.length() >= 8) copyStringSafe(apWifiPass, sizeof(apWifiPass), ap.c_str());
    }
    if (webServer.hasArg("adm_tok")) {
      String tk = webServer.arg("adm_tok"); tk.trim();
      if (tk.length() >= 4) copyStringSafe(gcsAdminToken, sizeof(gcsAdminToken), tk.c_str());
    }
    if (webServer.hasArg("url")) {
      String u = webServer.arg("url");
      u.trim();
      if (u.length() == 0 || isValidGoogleScriptUrl(u.c_str())) {
        copyStringSafe(gsheetScriptUrl, sizeof(gsheetScriptUrl), u.c_str());
        pendingSyncUrlToBuoy = true;
      } else {
        webServer.send(400, "application/json", "{\"ok\":false,\"err\":\"URL phai bat dau bang https://script.google.com va khong chua YOUR_SCRIPT_ID\"}");
        return;
      }
    }
    if (webServer.hasArg("discord")) {
      String d = webServer.arg("discord");
      d.trim();
      copyStringSafe(discordWebhookUrl, sizeof(discordWebhookUrl), d.c_str());
    }
    if (webServer.hasArg("ssid")) {
      String s = webServer.arg("ssid"); s.trim();
      // Dashboard KHONG gui lai mat khau (o trong) khi nguoi dung chi sua URL/SSID.
      // Vi vay: khong co arg "pass" => giu nguyen mat khau cu, tranh xoa nham.
      bool passProvided = webServer.hasArg("pass");
      String p = passProvided ? webServer.arg("pass") : String(staWifiPass);
      if (strcmp(s.c_str(), staWifiSsid) != 0 || (passProvided && strcmp(p.c_str(), staWifiPass) != 0)) {
        copyStringSafe(staWifiSsid, sizeof(staWifiSsid), s.c_str());
        copyStringSafe(staWifiPass, sizeof(staWifiPass), p.c_str());
        changedWifi = true;
      }
    }
    saveGcsConfig();
    webServer.send(200, "application/json", "{\"ok\":true}");
    if (changedWifi) {
      Serial.printf("[GCS-WIFI] Dang ket noi lai STA: %s\n", strlen(staWifiSsid) ? staWifiSsid : "(off)");
      WiFi.disconnect(true);
      delay(100);
      if (strlen(staWifiSsid) > 0) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(staWifiSsid, staWifiPass);
      } else {
        WiFi.mode(WIFI_AP);
      }
    }
  });

  webServer.on("/api/reboot", HTTP_GET, []() {
    if (!requireWebAuth()) return;
    webServer.send(200, "text/plain", "REBOOTING");
    delay(300);
    ESP.restart();
  });

  webServer.on("/api/factory_reset", HTTP_GET, []() {
    if (!requireWebAuth()) return;
    webServer.send(200, "text/plain", "FACTORY_RESET");
    delay(300);
    gcsPrefs.clear();
    gcsPrefs.end();
    ESP.restart();
  });

  webServer.on("/api/discord_test", HTTP_GET, []() {
    if (!requireWebAuth()) return;
    if (WiFi.status() != WL_CONNECTED) {
      webServer.send(400, "application/json", "{\"ok\":false,\"err\":\"Trạm Bờ chưa kết nối Wi-Fi Internet! Vui lòng nhập SSID/Pass Wi-Fi và bấm Lưu Cấu Hình.\"}");
      return;
    }
    if (strlen(discordWebhookUrl) < 30) {
      webServer.send(400, "application/json", "{\"ok\":false,\"err\":\"Chưa nhập Discord Webhook URL! Vui lòng dán link Webhook vào ô Discord và bấm Lưu Cấu Hình.\"}");
      return;
    }
    reqPushDiscordTest = true;
    unsigned long waitStart = millis();
    while (reqPushDiscordTest && (millis() - waitStart < 3500)) {
      esp_task_wdt_reset();
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (!reqPushDiscordTest && discordLastStatus >= 200 && discordLastStatus < 300) {
      webServer.send(200, "application/json", "{\"ok\":true}");
    } else {
      char errJson[128];
      snprintf(errJson, sizeof(errJson), "{\"ok\":false,\"err\":\"HTTP Error %d khi gửi tới Discord\"}", discordLastStatus);
      webServer.send(500, "application/json", errJson);
    }
  });

  webServer.on("/update", HTTP_GET, []() {
    if (!requireWebAuth()) return;
    webServer.send_P(200, "text/html", OTA_HTML);
  });

  webServer.on("/update", HTTP_POST,
    []() {
      if (!otaUploadAuthorized) {
        webServer.send(401, "application/json", "{\"ok\":false,\"err\":\"OTA Unauthorized\"}");
        return;
      }
      bool ok = !Update.hasError() && otaHeaderChecked;
      webServer.sendHeader("Connection", "close");
      webServer.send(ok ? 200 : 400, "text/plain", ok ? "OK_REBOOTING" : "FAIL_INVALID_IMAGE");
      if (ok) { delay(500); ESP.restart(); }
    },
    []() {
      HTTPUpload& upload = webServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        esp_task_wdt_reset();
        otaUploadAuthorized = isWebAuthenticated();
        otaHeaderChecked = false;
        if (!otaUploadAuthorized) {
          Serial.println("[GCS-OTA] Tu choi OTA: Chua xac thuc Quan tri!");
          return;
        }
        Serial.printf("[GCS-OTA] Bat dau nap: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        } else if (webServer.hasArg("md5") && webServer.arg("md5").length() == 32) {
          Update.setMD5(webServer.arg("md5").c_str());
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!otaUploadAuthorized) return;
        esp_task_wdt_reset();
        if (!otaHeaderChecked && upload.currentSize > 0) {
          // P0-2: Kiểm tra byte định danh ESP32 Firmware Image (0xE9)
          if (upload.buf[0] != 0xE9) {
            Serial.printf("[GCS-OTA] File khong hop le (magic=0x%02X != 0xE9) -> Huy OTA!\n", upload.buf[0]);
            Update.abort();
            return;
          }
          otaHeaderChecked = true;
        }
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (!otaUploadAuthorized) return;
        esp_task_wdt_reset();
        if (otaHeaderChecked && Update.end(true)) {
          Serial.printf("[GCS-OTA] NAP THANH CONG %u bytes. Dang khoi dong lai...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
          if (Update.canRollBack()) {
            Update.rollBack();
            Serial.println("[GCS-OTA] Da kich hoat Rollback phien ban an toan truoc do.");
          }
        }
      }
    }
  );

  webServer.on("/generate_204", HTTP_GET, []() {
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

  webServer.begin();
  Serial.printf("[GCS-WEB] Wi-Fi AP '%s' san sang tai http://192.168.4.1 (hoac http://%s.local)\n",
                GCS_WIFI_SSID, GCS_MDNS_HOST);
}

// =========================================================================
// 9. SETUP & VÒNG LẶP CHÍNH
// =========================================================================
void setup() {
  Serial.setRxBufferSize(512);
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, GCS_SIM_RX2_PIN, GCS_SIM_TX2_PIN);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  loadGcsConfig();
  Serial.printf("\n[GCS-BOOT] FW %s | Boot #%lu | Free heap: %u bytes\n",
                FW_VERSION, (unsigned long)gcsBootCount, (unsigned int)ESP.getFreeHeap());

  pinMode(TFT_CS_PIN, OUTPUT);   digitalWrite(TFT_CS_PIN, HIGH);
  pinMode(LORA_NSS_PIN, OUTPUT); digitalWrite(LORA_NSS_PIN, HIGH);

  pinMode(SW1_MOTOR_FWD,    INPUT_PULLUP);
  pinMode(SW2_MOTOR_REV,    INPUT_PULLUP);
  pinMode(SW3_RUDDER_LEFT,  INPUT_PULLUP);
  pinMode(SW4_RUDDER_RIGHT, INPUT_PULLUP);
  pinMode(SW5_WINCH_DOWN,   INPUT_PULLUP);
  pinMode(SW6_WINCH_HOME,   INPUT_PULLUP);
  pinMode(SW7_PAGE_AUTO,    INPUT_PULLUP);

  pinMode(JOY1_VRY_PIN, INPUT);
  pinMode(JOY1_VRX_PIN, INPUT);
  pinMode(JOY2_VRY_PIN, INPUT);
  pinMode(JOY1_SW_PIN,  INPUT);

  calibrateJoystickCenterNow();

  tft.init();
  tft.setRotation(1);
  Serial.printf("[GCS-TFT] Resolution: %dx%d (Rotation 1)\n", tft.width(), tft.height());

  // Khởi tạo các sprite đệm bộ nhớ RAM (Off-Screen Double Buffering)
  sprHorizonCreated = (sprHorizon.createSprite(74, 54) != nullptr);
  sprDepthCreated   = (sprDepth.createSprite(14, 76) != nullptr);
  sprCanvasCreated  = (sprCanvas.createSprite(216, 146) != nullptr);
  Serial.printf("[GCS-SPRITES] Horizon:%d Depth:%d Canvas:%d | Heap:%u\n",
                sprHorizonCreated, sprDepthCreated, sprCanvasCreated, (unsigned int)ESP.getFreeHeap());
  drawCurrentStaticPage();

  selectLoRa();
  LoRa.setSPI(tft.getSPIinstance());
  LoRa.setPins(LORA_NSS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  if (LoRa.begin(LORA_FREQ_HZ)) {
    LoRa.setSignalBandwidth(LORA_BW_HZ);
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setCodingRate4(5);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setPreambleLength(8);
    LoRa.setTxPower(LORA_TX_POWER_DBM);
    LoRa.enableCrc();
    LoRa.parsePacket();
    loraHardwareOk = true;
    Serial.println("[GCS] Tram Bo LoRa 433MHz (Shared VSPI) San Sang! (CRC HW: ON)");
  } else {
    loraHardwareOk = false;
    Serial.println("[GCS-ERR] Khong tim thay module LoRa tren Tram Bo!");
  }

  setupWebServer();
  // S-08: Khởi tạo Task Watchdog (30s) trước khi tạo task Core 0 để cả 2 nhân đều được giám sát chống treo
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
  lastWifiCheckMs = millis();
  xTaskCreatePinnedToCore(googleSheetsWifiTask, "gsWifiTask", 24576, nullptr, 1, nullptr, 0);

  lastEnergyCalcMs = millis();
  refreshScreenValues(0, 90, 0);
}

void loop() {
  esp_task_wdt_reset();
  dnsServer.processNextRequest();
  webServer.handleClient();
  updateGcsSim4GUploader();
  unsigned long now = millis();

  // Giám sát trạng thái kết nối Wi-Fi STA "Mang bao tri" thời gian thực
  static bool prevStaConnected = false;
  bool curStaConnected = (WiFi.status() == WL_CONNECTED);
  if (curStaConnected != prevStaConnected) {
    prevStaConnected = curStaConnected;
    if (curStaConnected) {
      Serial.printf("[GCS-WIFI] >>> DA KET NOI WIFI '%s' THANH CONG! IP: http://%s (3D Map: http://%s/3d) <<<\n",
                    staWifiSsid, WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str());
    }
  }

  // Tự động kiểm tra lại nếu mạng Wi-Fi STA bị rớt (nhường hoàn toàn cho WiFi.setAutoReconnect(true) xử lý ở nền tránh treo TCP/IP stack)
  if (now - lastWifiCheckMs >= 60000UL) {
    lastWifiCheckMs = now;
  }

  // Nếu Wi-Fi rớt mà còn dữ liệu chờ -> chuyển sang đẩy qua SIM 4G
  static unsigned long lastSimSchedMs = 0;
  if (now - lastSimSchedMs >= 1000UL) {
    lastSimSchedMs = now;
    startNextSimPushIfIdle();
  }

  // 0. Đọc cổng USB Serial từng ký tự Non-blocking 100%
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (gcsUsbIdx > 0) {
        gcsUsbBuf[gcsUsbIdx] = '\0';
        if (strncmp(gcsUsbBuf, "PAGE:", 5) == 0) {
          currentPage = constrain(atoi(gcsUsbBuf + 5), 0, 4);
          drawCurrentStaticPage();
        } else if (strncmp(gcsUsbBuf, "CAL_JOY", 7) == 0) {
          calibrateJoystickCenterNow();
        } else if (strncmp(gcsUsbBuf, "SCAN_ADC", 8) == 0) {
          Serial.printf("[SCAN-ADC] G34=%dmV G35=%dmV G36=%dmV G39=%dmV G32=%dmV G33=%dmV G13=%d\n",
                        analogReadMilliVolts(34), analogReadMilliVolts(35),
                        analogReadMilliVolts(36), analogReadMilliVolts(39),
                        analogReadMilliVolts(32), analogReadMilliVolts(33),
                        digitalRead(13));
        } else if (strncmp(gcsUsbBuf, "CAL_BAT:", 8) == 0) {
          targetCalibBatVolt    = strtof(gcsUsbBuf + 8, nullptr);
          pendingCalibBatToBuoy = true;
        } else if (strncmp(gcsUsbBuf, "STATUS", 6) == 0) {
          char ipStr[24];
          copyStringSafe(ipStr, sizeof(ipStr), WiFi.localIP().toString().c_str());
          Serial.printf("\n[GCS-STATUS] FW:%s | Boot:#%lu | Uptime:%lu s | Heap:%u B\n",
                        FW_VERSION, (unsigned long)gcsBootCount, millis() / 1000UL,
                        (unsigned int)ESP.getFreeHeap());
          Serial.printf("  LoRa:%s RSSI:%d SNR:%.1f | RX:%lu goi | TX:%lu lenh | CRC loi:%lu\n",
                        loraHardwareOk ? "OK" : "LOI", uRSSI, uSNR,
                        (unsigned long)rxPacketCount, (unsigned long)txCmdCount,
                        (unsigned long)crcErrorCnt);
          Serial.printf("  WiFi STA:%s IP:%s | Google Sheets HTTP:%d OK:%lu Loi:%lu\n",
                        (WiFi.status() == WL_CONNECTED) ? "ON" : "OFF", ipStr,
                        gsheetLastStatus, (unsigned long)gsheetPushCount,
                        (unsigned long)gsheetFailCnt);
          Serial.printf("  Phao: %s | Seq:%lu | GPS:%u sat | Cach Home:%.1fm | Pin:%.2fV\n",
                        (rxPacketCount > 0 && millis() - lastRxFromBuoy < 2800) ? "ONLINE" : "MAT SONG",
                        (unsigned long)uS3SeqId, uSats, uHomeDistanceM, uBat);
        } else if (strncmp(gcsUsbBuf, "PUSHPRF", 7) == 0) {
          triggerGoogleSheetsPush(true);
        } else if (strncmp(gcsUsbBuf, "PUSH", 4) == 0) {
          triggerGoogleSheetsPush(false);
        } else if (strncmp(gcsUsbBuf, "SYNC_URL", 8) == 0) {
          pendingSyncUrlToBuoy = true;
        } else if (strncmp(gcsUsbBuf, "CMD:", 4) == 0) {
          int cm = 0, cr = 90, cw = 0;
          if (sscanf(gcsUsbBuf + 4, "%d,%d,%d", &cm, &cr, &cw) == 3) {
            if (cw == WINCH_CMD_AUTO_3LVL) {
              autoModeRequested = true;
              webWinchOverride  = WINCH_CMD_AUTO_3LVL;
              webOverrideExpireMs = now + 2500UL;
            } else if (cw == WINCH_CMD_HOME || cw == WINCH_CMD_EMERGENCY_STOP || cw == 11) {
              autoModeRequested = false;
              webMotorOverride  = 0;
              webRudderOverride = 90;
              webWinchOverride  = cw;
              webOverrideExpireMs = now + 2500UL;
            } else if (cw == 0) {
              webMotorOverride  = constrain(cm, -255, 255);
              webRudderOverride = constrain(cr, 45, 135);
              webOverrideExpireMs = now + 2500UL;
            }
          }
          queueSpecialCmd(gcsUsbBuf);
        }
        gcsUsbIdx = 0;
      }
    } else if (gcsUsbIdx < sizeof(gcsUsbBuf) - 1) {
      gcsUsbBuf[gcsUsbIdx++] = c;
    }
  }

  // 0. Đọc cảm biến nhiệt độ chip nội ESP32 Trạm Bờ (On-die Temperature Sensor)
  float curGcsT = temperatureRead();
  if (curGcsT > -30.0f && curGcsT < 115.0f) {
    uGcsChipTemp = (uGcsChipTemp > 1.0f) ? (uGcsChipTemp * 0.92f + curGcsT * 0.08f) : curGcsT;
  }



  bool justReceivedLoRa = false;
  if (loraHardwareOk) {
    selectLoRa();
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0) {
      justReceivedLoRa = true;
      char recv[300];
      size_t idx = 0;
      while (LoRa.available() && idx < sizeof(recv) - 1) {
        recv[idx++] = (char)LoRa.read();
      }
      recv[idx] = '\0';
      while (LoRa.available()) LoRa.read();

      uRSSI = LoRa.packetRssi();
      uSNR  = LoRa.packetSnr();

      Serial.printf("[GCS-RAW-RX len=%d] RSSI:%d SNR:%.1f | %s\n", packetSize, uRSSI, uSNR, recv);

      // S-02: Bắt buộc mọi gói tin LoRa phải có hậu tố *<CRC8> hợp lệ (khởi tạo crcOk = false)
      bool crcOk = false;
      char* starPtr = strchr(recv, '*');
      if (starPtr != nullptr && isxdigit((unsigned char)starPtr[1]) && isxdigit((unsigned char)starPtr[2])) {
        size_t payloadLen = (size_t)(starPtr - recv);
        uint8_t calcCrc = calcKeyedCRC8(recv, payloadLen);
        uint8_t pktCrc  = (uint8_t)strtoul(starPtr + 1, nullptr, 16);
        if (calcCrc != pktCrc) {
          crcErrorCnt++;
          Serial.printf("[GCS-CRC-FAIL] calc:0x%02X != pkt:0x%02X\n", calcCrc, pktCrc);
        } else {
          crcOk = true;
          *starPtr = '\0';
        }
      } else {
        crcErrorCnt++;
        Serial.println("[GCS-CRC-MISSING] Goi tin LoRa thieu hau to *CRC8 -> Loai bo!");
      }

      if (crcOk && strncmp(recv, "USV:", 4) == 0) {
        // S-03: Parse nguyên tử (All-or-Nothing) vào biến tạm, kiểm tra NaN/Inf và ngưỡng vật lý trước khi cập nhật biến toàn cục
        float tPH = 0.0f, tTDS = 0.0f, tNTU = 0.0f, tTemp = 0.0f, tAmp = 0.0f, tBat = 0.0f;
        float tRoll = 0.0f, tPitch = 0.0f, tHeading = 0.0f, tDO = 0.0f, tMpuTemp = 0.0f, tEC = 0.0f, tDepthM = 0.0f, rxS3ChipT = 0.0f;
        int tCT1Mud = 0, tCT2Home = 0, tAutoStage = 0, tWQI = 0;
        double tLat = 0.0, tLng = 0.0;
        unsigned int tAlertFlags = 0, tHwHealth = 0, tAdcFault = 0, tSats = 0, tFlags2 = 0;
        unsigned long tS3SeqId = 0;
        float tGpsSpeedKmph = 0.0f, tGpsAltM = 0.0f, tHomeDistanceM = 0.0f, tTargetHeadingDeg = 0.0f;

        int parsed = sscanf(
          recv + 4,
          "%f,%f,%f,%f,%f,%f,%f,%d,%d,%f,%f,%lf,%lf,%d,%u,%lu,%f,%u,%d,%f,%f,%f,%u,%f,%u,%f,%f,%f,%u,%f",
          &tPH, &tTDS, &tNTU, &tTemp, &tAmp, &tBat, &tRoll, &tCT1Mud, &tCT2Home,
          &tPitch, &tHeading, &tLat, &tLng, &tAutoStage, &tAlertFlags, &tS3SeqId,
          &tDO, &tHwHealth, &tWQI, &tMpuTemp, &tEC, &tDepthM, &tAdcFault, &rxS3ChipT,
          &tSats, &tGpsSpeedKmph, &tGpsAltM, &tHomeDistanceM, &tFlags2, &tTargetHeadingDeg
        );

        bool validRanges = (parsed >= 24) &&
                           !isnan(tPH) && !isinf(tPH) && (tPH >= 0.0f && tPH <= 14.5f) &&
                           !isnan(tTDS) && !isinf(tTDS) && (tTDS >= 0.0f && tTDS <= 5000.0f) &&
                           !isnan(tNTU) && !isinf(tNTU) && (tNTU >= 0.0f && tNTU <= 4000.0f) &&
                           !isnan(tTemp) && !isinf(tTemp) && (tTemp >= -10.0f && tTemp <= 85.0f) &&
                           !isnan(tBat) && !isinf(tBat) && (tBat >= 0.0f && tBat <= 30.0f) &&
                           (tWQI >= 0 && tWQI <= 100) &&
                           !isnan(tLat) && !isinf(tLat) && (tLat >= -90.0 && tLat <= 90.0) &&
                           !isnan(tLng) && !isinf(tLng) && (tLng >= -180.0 && tLng <= 180.0);

        if (validRanges) {
          uPH = tPH; uTDS = tTDS; uNTU = tNTU; uTemp = tTemp; uAmp = tAmp; uBat = tBat;
          uRoll = tRoll; uCT1Mud = tCT1Mud; uCT2Home = tCT2Home;
          uPitch = tPitch; uHeading = tHeading; uLat = tLat; uLng = tLng;
          uAutoStage = tAutoStage; uAlertFlags = tAlertFlags; uS3SeqId = tS3SeqId;
          uDO = tDO; uHwHealth = tHwHealth; uWQI = tWQI; uMpuTemp = tMpuTemp;
          uEC = tEC; uDepthM = tDepthM; uAdcFault = tAdcFault;
          uS3ChipTemp = (rxS3ChipT > 1.0f) ? rxS3ChipT : ((tMpuTemp > 1.0f) ? tMpuTemp : uS3ChipTemp);
          // P1-6: Chủ động reset các trường mở rộng về 0/false nếu gói ngắn hơn 30 trường (chống đóng băng số cũ)
          uSats             = (parsed >= 25) ? tSats : 0;
          uGpsSpeedKmph     = (parsed >= 26) ? tGpsSpeedKmph : 0.0f;
          uGpsAltM          = (parsed >= 27) ? tGpsAltM : 0.0f;
          uHomeDistanceM    = (parsed >= 28) ? tHomeDistanceM : 0.0f;
          uFlags2           = (parsed >= 29) ? tFlags2 : 0;
          uTargetHeadingDeg = (parsed >= 30) ? tTargetHeadingDeg : 0.0f;

          if (parsed >= 29) {
            uReturnHomeActive    = (uFlags2 & FLAG2_RETURN_HOME) != 0;
            uGpsFixValid         = (uFlags2 & FLAG2_GPS_FIX) != 0;
            uVirtualAnchorActive = (uFlags2 & FLAG2_VIRTUAL_ANCHOR) != 0;
            uHeadingHoldActive   = (uFlags2 & FLAG2_HEADING_HOLD) != 0;
          } else {
            uReturnHomeActive    = false;
            uGpsFixValid         = (uSats >= 4) && (fabs(uLat) > 0.001 || fabs(uLng) > 0.001);
            uVirtualAnchorActive = false;
            uHeadingHoldActive   = false;
          }

          lastRxFromBuoy = now;
          rxPacketCount++;

          if (uAutoStage == AUTO_STAGE_WAIT_SURF || (uAutoStage == AUTO_STAGE_OFF && uDepthM < 0.65f)) {
            gcsLayerSurf = {uPH, uTDS, uNTU, uTemp, uDO, true};
          } else if (uAutoStage == AUTO_STAGE_WAIT_MID || (uDepthM >= 0.65f && uDepthM < 1.40f)) {
            gcsLayerMid = {uPH, uTDS, uNTU, uTemp, uDO, true};
          } else if (uAutoStage == AUTO_STAGE_WAIT_BOT || (uDepthM >= 1.40f)) {
            gcsLayerBot = {uPH, uTDS, uNTU, uTemp, uDO, true};
          }
          if (uAutoStage == AUTO_STAGE_COMPLETED) {
            autoModeRequested = false;
            // Đẩy bảng 3 tầng lên Google Sheets đúng 1 lần cho mỗi chu trình Auto
            if (!profilePushedForRun &&
                (now - lastProfilePushMs >= GCS_PROFILE_PUSH_COOLDOWN_MS)) {
              profilePushedForRun = true;
              lastProfilePushMs   = now;
              triggerGoogleSheetsPush(true);
              Serial.println("[GCS-AUTO] Hoan tat 3 tang -> Day bang PROFILE len Google Sheets.");
            }
          } else if (uAutoStage != AUTO_STAGE_RETURN_HOME) {
            profilePushedForRun = false;
          }

          if (now - lastGsheetTriggerMs >= GSHEETS_PUSH_INTERVAL_MS) {
            lastGsheetTriggerMs = now;
            triggerGoogleSheetsPush(false);
          }

          char missStr[60];
          buildMissingHwString(uHwHealth, missStr, sizeof(missStr));
          Serial.printf("[GCS-RX #%lu | S3#%lu | LK:%d/11(%s)%s] pH:%.2f TDS:%.0f NTU:%.0f Nuoc:%.1fC | S3-Chip:%.1fC | GCS-Chip:%.1fC | Pin:%.2fV Dong:%.2fA R:%.1f P:%.1f | GPS:%u sat %.1fkm/h\n",
                        (unsigned long)rxPacketCount, (unsigned long)uS3SeqId,
                        countConnectedHwBits(uHwHealth), missStr,
                        uAdcFault ? " VUOT_DAI_ADC!" : "",
                        uPH, uTDS, uNTU, uTemp, uS3ChipTemp, uGcsChipTemp, uBat, uAmp, uRoll, uPitch,
                        uSats, uGpsSpeedKmph);
        }
      } else if (crcOk && strncmp(recv, "PRF:", 4) == 0) {
        // Bảng 3 tầng do Phao gửi ngay sau khi đo xong: pH,TDS,NTU,Temp x 3 tầng
        float p1, d1, n1, t1, p2, d2, n2, t2, p3, d3, n3, t3;
        int n = sscanf(recv + 4, "%f,%f,%f,%f;%f,%f,%f,%f;%f,%f,%f,%f",
                       &p1, &d1, &n1, &t1, &p2, &d2, &n2, &t2, &p3, &d3, &n3, &t3);
        if (n >= 12) {
          gcsLayerSurf.ph = p1; gcsLayerSurf.tds = d1; gcsLayerSurf.ntu = n1; gcsLayerSurf.temp = t1;
          gcsLayerMid.ph  = p2; gcsLayerMid.tds  = d2; gcsLayerMid.ntu  = n2; gcsLayerMid.temp  = t2;
          gcsLayerBot.ph  = p3; gcsLayerBot.tds  = d3; gcsLayerBot.ntu  = n3; gcsLayerBot.temp  = t3;
          gcsLayerSurf.valid = gcsLayerMid.valid = gcsLayerBot.valid = true;
          prfPacketCount++;
          lastPrfPacketMs = now;
          Serial.printf("[GCS-PRF #%lu] Mat:pH%.2f/TDS%.0f/NTU%.0f/%.1fC | Giua:pH%.2f/TDS%.0f | Day:pH%.2f/TDS%.0f\n",
                        (unsigned long)prfPacketCount, p1, d1, n1, t1, p2, d2, p3, d3);
          if (!profilePushedForRun) {
            profilePushedForRun = true;
            lastProfilePushMs   = now;
            triggerGoogleSheetsPush(true);
          }
        }
      }
    }
  }

  // 2. Xử lý nút nhấn đa năng SW trên đỉnh Joystick & công tắc SW7 (Cắm vào GPIO 13 có INPUT_PULLUP)
  // - Nhấn 1 lần (Single Click): Đổi trang màn hình TFT (0 -> 1 -> 2 -> 0)
  // - Nhấn đúp (Double Click): Đổi cấp số L298N (ECO 60% -> CRUISE 82% -> TURBO 100%)
  // - Nhấn giữ 1.2s (Long Press): Bật / Tắt chế độ Auto đo 3 tầng nước
  bool joyBtnDown = (digitalRead(SW7_PAGE_AUTO) == LOW);
  if (joyBtnDown && !sw7WasPressed) {
    sw7WasPressed  = true;
    sw7LongHandled = false;
    sw7PressStart  = now;
  } else if (joyBtnDown && sw7WasPressed) {
    if (!sw7LongHandled && (now - sw7PressStart >= 1200)) {
      sw7LongHandled    = true;
      autoModeRequested = !autoModeRequested;
      Serial.printf("[GCS-JOY] Nhan giu SW (>1.2s): Che do Auto do 3 tang: %s\n", autoModeRequested ? "BAT" : "TAT");
    }
  } else if (!joyBtnDown && sw7WasPressed) {
    sw7WasPressed = false;
    if (!sw7LongHandled) {
      if (now - sw7LastRelease < 380) {
        // Double Click -> Đổi cấp tốc độ L298N
        speedGear = (speedGear == GEAR_ECO) ? GEAR_CRUISE : ((speedGear == GEAR_CRUISE) ? GEAR_TURBO : GEAR_ECO);
        Serial.printf("[GCS-JOY] Nhan dup SW: Doi cap toc do L298N -> %s (%d%%)\n",
                      getSpeedGearText(), (int)(getSpeedGearScale() * 100));
        sw7LastRelease = 0;
      } else {
        // Single Click -> Đổi trang màn hình TFT
        currentPage = (currentPage + 1) % 5;
        drawCurrentStaticPage();
        Serial.printf("[GCS-JOY] Nhan 1 lan SW: Chuyen sang Trang %d/5\n", currentPage + 1);
        sw7LastRelease = now;
      }
    }
  }

  // 3. Hợp nhất điều khiển mạch L298N: Joystick Analog (GPIO34/35) + Công tắc (SW1..SW6) + Web Joystick 2D
  int motorPWM  = 0;
  int rudderAng = 90;
  int winchCmd  = WINCH_CMD_STOP;
  activeCtrlSrc = "IDLE";

  int joyM = 0, joyR = 90, joyW = WINCH_CMD_STOP;
  bool joyActive = readHardwareJoystick(joyM, joyR, joyW);
  if (joyActive) {
    motorPWM      = joyM;
    rudderAng     = joyR;
    winchCmd      = joyW;
    activeCtrlSrc = "JOY";
  }

  bool hwSwitchActive = false;
  float gearScale = getSpeedGearScale();
  if (digitalRead(SW1_MOTOR_FWD) == LOW)      { motorPWM = (int)(255 * gearScale);  hwSwitchActive = true; }
  else if (digitalRead(SW2_MOTOR_REV) == LOW) { motorPWM = -(int)(230 * gearScale); hwSwitchActive = true; }

  if (digitalRead(SW3_RUDDER_LEFT) == LOW)       { rudderAng = 50;  hwSwitchActive = true; }
  else if (digitalRead(SW4_RUDDER_RIGHT) == LOW) { rudderAng = 130; hwSwitchActive = true; }

  if (digitalRead(SW5_WINCH_DOWN) == LOW) {
    winchCmd = WINCH_CMD_DOWN;
    autoModeRequested = false;
    hwSwitchActive = true;
  } else if (digitalRead(SW6_WINCH_HOME) == LOW) {
    winchCmd = WINCH_CMD_HOME;
    autoModeRequested = false;
    hwSwitchActive = true;
  } else if (autoModeRequested) {
    winchCmd = WINCH_CMD_AUTO_3LVL;
  }

  if (hwSwitchActive) {
    activeCtrlSrc = "SW";
  } else if (!joyActive && now < webOverrideExpireMs) {
    motorPWM      = webMotorOverride;
    rudderAng     = webRudderOverride;
    activeCtrlSrc = "WEB";
    if (webWinchOverride != 0) winchCmd = webWinchOverride;
    if (webWinchOverride >= 4) {
      webWinchOverride = 0;
    }
  }

  // 4. Phát lệnh điều khiển L298N & Bánh lái xuống Phao (TDMA đồng bộ sau RX để tránh xung đột FIFO SX1278):
  bool controlChanged = (motorPWM != prevMotorPWM) ||
                        (rudderAng != prevRudderAng) ||
                        (winchCmd != prevWinchCmd);
  bool hasUrgentTx = controlChanged || (pendingSpecialCmd[0] != '\0') || pendingCalibBatToBuoy || pendingSyncUrlToBuoy;

  if ((hasUrgentTx && (now - lastTxTick >= 65)) || (justReceivedLoRa && (now - lastTxTick >= 250)) || (now - lastTxTick >= 1450)) {
    if (loraHardwareOk) {
      selectLoRa();
      // Kiểm tra FIFO lần cuối trước khi TX để tuyệt đối không ghi đè gói RX vừa tới
      if (justReceivedLoRa || LoRa.parsePacket() == 0) {
        lastTxTick    = now;
        prevMotorPWM  = motorPWM;
        prevRudderAng = rudderAng;
        prevWinchCmd  = winchCmd;
      uBgNoiseRssi = LoRa.rssi();
      if (pendingSpecialCmd[0] != '\0') {
        // Lệnh đặc biệt (GOTO_HOME, STOP_ALL, SET_HEADING...) được ưu tiên gửi ngay
        char spCmd[LORA_CMD_MAX_LEN];
        copyStringSafe(spCmd, sizeof(spCmd), pendingSpecialCmd);
        pendingSpecialCmd[0] = '\0';

        char spPkt[LORA_CMD_MAX_LEN + 5];
        int spLen = snprintf(spPkt, sizeof(spPkt) - 5, "%s", spCmd);
        uint8_t spCrc = calcKeyedCRC8(spPkt, spLen);
        snprintf(spPkt + spLen, 5, "*%02X", spCrc);

        LoRa.beginPacket();
        LoRa.print(spPkt);
        LoRa.endPacket();
        txCmdCount++;
        lastSpecialCmdMs = now;
        Serial.printf("[GCS-TX-SPECIAL] %s\n", spPkt);
      } else if (pendingCalibBatToBuoy) {
        pendingCalibBatToBuoy = false;
        LoRa.beginPacket();
        LoRa.printf("CBAT:%.2f", targetCalibBatVolt);
        LoRa.endPacket();
      } else if (pendingSyncUrlToBuoy) {
        pendingSyncUrlToBuoy = false;
        LoRa.beginPacket();
        LoRa.print("GURL:");
        LoRa.print(gsheetScriptUrl);
        LoRa.endPacket();
      } else if (now - lastSpecialCmdMs >= 1200UL) {
        // Không gửi CMD thường ngay sau lệnh đặc biệt: lệnh nấc tời 10cm cần đủ 900ms
        char cmdPkt[48];
        int len = snprintf(cmdPkt, sizeof(cmdPkt) - 5, "CMD:%d,%d,%d", motorPWM, rudderAng, winchCmd);
        uint8_t crc = calcKeyedCRC8(cmdPkt, len);
        snprintf(cmdPkt + len, 5, "*%02X", crc);

        LoRa.beginPacket();
        LoRa.print(cmdPkt);
        LoRa.endPacket();
        txCmdCount++;
      }
      }
    }
  }

  // 5. Cập nhật giao diện màn hình TFT mỗi 200ms
  if (now - lastUiTick >= 200) {
    lastUiTick = now;
    refreshScreenValues(motorPWM, rudderAng, winchCmd);
  }
}
