#pragma once
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "mbedtls/md.h"

// =========================================================================
// CHEMSTATION AI v4.2-REAL — HỆ THỐNG PHAO QUAN TRẮC NƯỚC TỰ HÀNH (KHKT)
// Đo Pin Chính Xác + L298N Joystick Vô Cấp + Chẩn Đoán Linh Kiện Thực 100% (Không Báo Ảo)
// =========================================================================
#define FW_VERSION                "4.3-AUDIT-PRO"
#define PROTOCOL_VERSION          2

// Số trường telemetry LoRa (USV:...) — dùng chung cho Phao, Trạm bờ và Python bridge
#define TELEMETRY_FIELD_COUNT     30
#define LORA_PACKET_MAX_LEN       250  // S-01: < 255 byte FIFO phần cứng SX1278 (chống tràn bộ đệm & lỗi CRC)
#define LORA_CMD_MAX_LEN          64
#define LORA_DEFAULT_TX_INTERVAL_MS 1000UL // S-06: Chu kỳ phát mặc định 1000ms (Duty-cycle an toàn SRD 433MHz)
#define NVS_NAMESPACE_BUOY        "chemstation"
#define NVS_NAMESPACE_GCS         "chem_gcs"

// Ngưỡng bảo vệ pin đa tầng theo từng cell (tự nhận diện 1S/2S/3S) — chống chai/cháy nổ/sập nguồn LiPo
#define BAT_CELL_LOW_V            3.35f
#define BAT_CELL_TIER1_WARN_V     3.40f   // Tier 1: Cảnh báo & giới hạn công suất động cơ (3S ~ 10.20V)
#define BAT_CELL_TIER2_RTH_V      3.26f   // Tier 2: Tự hành quay về Home GPS trước khi cạn pin (3S ~ 9.78V)
#define BAT_CELL_TIER3_CUTOFF_V   3.15f   // Tier 3: Cắt khẩn cấp động cơ & tời, giữ nguồn 3.3V cho LoRa/GPS (3S ~ 9.45V)
#define BAT_CELL_CRITICAL_V       3.10f   // Tier 4: Ngưỡng Deep Sleep tự ngắt toàn bộ mạch (3S ~ 9.30V)
#define BAT_TIER2_RTH_HOLD_MS     4000UL  // Duy trì điện áp thấp 4s mới kích hoạt RTH (tránh sụt áp tức thời do đề pa)
#define BAT_TIER3_CUTOFF_HOLD_MS  2000UL  // Duy trì 2s để cắt toàn bộ tải động lực
#define BAT_DEEP_SLEEP_HOLD_MS    20000UL
#define BAT_DEEP_SLEEP_SEC        60ULL

// ---- Cấu hình Sóng LoRa Ra-02 (SX1278 433MHz) ----
#define LORA_FREQ_HZ              433E6
#define LORA_SYNC_WORD            0x12  // Chuẩn Semtech Private LoRa (tránh mất đồng bộ giải điều chế của SX1278)
#define LORA_TX_POWER_DBM         17    // 17 dBm (50mW) - Tránh bão hòa LNA cự ly gần & tiết kiệm pin
#define LORA_SF                   7
#define LORA_BW_HZ                125E3

// ---- Thời gian Fail-safe & Chuyển mạch 4G (Dual-Link Fallback) ----
#define FAILSAFE_TIMEOUT_MS              2200UL  // Quá 2.2s mất lệnh điều khiển -> Phao tự dừng động cơ tránh va chạm
#define FAILSAFE_RADIO_BLACKOUT_RTH_MS   25000UL // Quá 25s mất sóng LoRa kéo dài -> Tự động Return-To-Home GPS
#define LORA_FALLBACK_4G_MS              5000UL  // Quá 5.0s mất sóng LoRa -> Tự động bật 4G Fallback

// ---- Cấu hình mặc định Google Sheets Web App & Wi-Fi ----
#define DEFAULT_GOOGLE_SCRIPT_URL "https://script.google.com/macros/s/AKfycbzcdsFjdE1jkOlEQcafJ4BhnYB4wLumgYNlLkmcj7EeUUatMWIqFr6OJZfGpW2uuB1hZg/exec"
// B-03: Không lưu cứng Discord Webhook token thật trong mã nguồn (nạp qua NVS hoặc biến môi trường)
#define DEFAULT_DISCORD_WEBHOOK_URL ""
#define DEFAULT_STA_WIFI_SSID     "Mang bao tri"
#define DEFAULT_STA_WIFI_PASS     "mangnhabiloi"
#define DEFAULT_AP_WIFI_PASS      "ChemStation2026"
#define DEFAULT_GCS_ADMIN_TOKEN   "chemstation2026"
#define GSHEETS_PUSH_INTERVAL_MS  5000UL  // Chu kỳ tự động đẩy dữ liệu liên tục lên Cloud/Web (5 giây)
#define SIM_APN                   "v-internet"

// ---- Khoá bí mật xác thực gói tin chống nhiễu & chống giả mạo ----
#define LORA_SECRET_CRC_SALT      0xA7

// =========================================================================
// 1. MÃ ĐIỀU KHIỂN, CỜ CẢNH BÁO & BITMASK 11 LINH KIỆN PHẦN CỨNG THỰC TẾ
// =========================================================================
enum WinchCmdCode : int {
  WINCH_CMD_STOP             = 0, // Dừng tời
  WINCH_CMD_DOWN             = 1, // Hạ tời thủ công
  WINCH_CMD_HOME             = 2, // Thu tời về gốc Home (0.0m)
  WINCH_CMD_AUTO_3LVL        = 3, // Tự động đo 3 tầng nước (Mặt 0.3m - Giữa 1.0m - Đáy 1.8m)
  WINCH_CMD_HEADING_HOLD_ON  = 4, // Bật Autopilot PID khóa hướng la bàn hiện tại
  WINCH_CMD_HEADING_HOLD_OFF = 5, // Tắt Autopilot PID giữ hướng
  WINCH_CMD_SET_HOME_GPS     = 6, // Lưu vị trí Neo gốc (Home Point GPS)
  WINCH_CMD_EMERGENCY_STOP   = 8, // Phanh điện từ L298N khẩn cấp & thu lồng về Home
  WINCH_CMD_TARE_IMU         = 9, // Cân bằng góc nghiêng 0.0° (Tare MPU6050) từ xa
  WINCH_CMD_GOTO_HOME        = 10, // Tự hành quay về điểm Neo Home GPS (Autopilot + GPS)
  WINCH_CMD_STOP_ALL         = 11, // Dừng toàn bộ: động cơ, tời, autopilot, auto 3 tầng
  WINCH_CMD_SET_HEADING      = 12, // Đặt hướng mục tiêu (dùng trường M của CMD làm góc 0..359)
  WINCH_CMD_REPORT_STATUS    = 13, // Yêu cầu Phao gửi ngay gói telemetry (không đổi trạng thái)
  WINCH_CMD_INCH_DOWN        = 14, // Hạ tời 10cm (khảo sát từng tầng độ phân giải cao)
  WINCH_CMD_INCH_UP          = 15, // Thu tời 10cm
  WINCH_CMD_VIRTUAL_ANCHOR_ON= 16, // Bật Neo Ảo (Dynamic Positioning - Giữ vị trí GPS)
  WINCH_CMD_VIRTUAL_ANCHOR_OFF=17, // Tắt Neo Ảo
  WINCH_CMD_TAKE_SAMPLE      = 18, // P3-3.4: Bơm lấy mẫu nước tự động tại tầng hiện tại (tráng ống + lấy mẫu)
  WINCH_CMD_CLEAN_SENSOR     = 19, // P3-3.3: Sục rửa đầu dò tự động 30s không chặn (Non-blocking FSM)
  WINCH_CMD_CALIB_PH_2PT     = 20, // H-09: Hiệu chuẩn pH 2 điểm chuẩn 4.01 & 6.86/7.00
  WINCH_CMD_SET_DP_RADIUS    = 21, // S-07: Đặt bán kính Neo Ảo DP (trường M = bán kính * 10, vd 20 -> 2.0m)
  WINCH_CMD_SET_TX_INTERVAL  = 22  // S-06: Đặt chu kỳ phát LoRa Telemetry (trường M = ms, 500..5000ms)
};

// Cấp tốc độ điều khiển Joystick (Speed Gear Mode) cho mạch động cơ L298N
enum SpeedGearMode : uint8_t {
  GEAR_ECO    = 0, // 60% công suất L298N (Chạy êm khi lấy mẫu quan trắc)
  GEAR_CRUISE = 1, // 82% công suất L298N (Tuần tra tiêu chuẩn)
  GEAR_TURBO  = 2  // 100% công suất L298N (255 PWM - Vượt dòng chảy mạnh)
};

enum AutoProfileStage : int {
  AUTO_STAGE_OFF          = 0,
  AUTO_STAGE_HOMING_START = 1,
  AUTO_STAGE_MOVE_SURF    = 2,
  AUTO_STAGE_WAIT_SURF    = 3,
  AUTO_STAGE_MOVE_MID     = 4,
  AUTO_STAGE_WAIT_MID     = 5,
  AUTO_STAGE_MOVE_BOT     = 6,
  AUTO_STAGE_LIFT_BOT     = 7,
  AUTO_STAGE_WAIT_BOT     = 8,
  AUTO_STAGE_RETURN_HOME  = 9,
  AUTO_STAGE_COMPLETED    = 10
};

enum SystemAlertFlags : uint8_t {
  ALERT_NONE         = 0x00,
  ALERT_MOTOR_JAM    = 0x01, // Kẹt chân vịt L298N (Quá dòng > 2.8A trên ACS712 thật)
  ALERT_LOW_BATTERY  = 0x02, // Pin yếu thực tế (theo từng cell < 3.35V, tự nhận diện 1S/2S/3S)
  ALERT_HIGH_TILT    = 0x04, // Phao nghiêng lớn (> 45 độ trên MPU6050 thật)
  ALERT_WATER_QUAL   = 0x08, // Nước vượt ngưỡng cảnh báo QCVN (trên cảm biến thật)
  ALERT_WINCH_LIMIT  = 0x10, // Đã chạm đáy bùn CT1
  ALERT_SENSOR_FAULT = 0x20, // Có linh kiện cảm biến chưa cắm hoặc mất kết nối
  ALERT_4G_FALLBACK  = 0x40, // Đang hoạt động ở chế độ 4G Fallback
  ALERT_HEADING_HOLD = 0x80  // Đang bật chế độ Autopilot PID Giữ Hướng La Bàn
};

// Cờ mở rộng (gửi kèm telemetry trường flags2) — hỗ trợ 16 bit
enum SystemFlags2 : uint16_t {
  FLAG2_NONE          = 0x0000,
  FLAG2_RETURN_HOME   = 0x0001, // Autopilot đang tự hành quay về Home GPS
  FLAG2_GPS_FIX       = 0x0002, // GPS có tọa độ hợp lệ
  FLAG2_HOME_SAVED    = 0x0004, // Đã lưu Home GPS hợp lệ
  FLAG2_SIM_ONLINE    = 0x0008, // Module 4G đang phản hồi AT
  FLAG2_AUTO_RUNNING  = 0x0010, // Auto đo 3 tầng đang chạy
  FLAG2_MUD_LATCHED   = 0x0020, // Đã chạm đáy bùn CT1
  FLAG2_LOW_BAT_CRIT  = 0x0040, // Pin mức kiệt (ngưỡng Deep Sleep)
  FLAG2_CALIBRATED    = 0x0080, // Đã có hiệu chuẩn pH/Pin trong NVS
  FLAG2_VIRTUAL_ANCHOR= 0x0100, // Đang kích hoạt Neo Ảo (Dynamic Positioning - Giữ vị trí GPS)
  FLAG2_HEADING_HOLD  = 0x0200, // Đang kích hoạt Autopilot PID khóa hướng la bàn
  FLAG2_DEPTH_REAL_MS5837 = 0x0400, // Độ sâu đo bằng cảm biến thủy tĩnh MS5837 thật (I2C 0x76)
  FLAG2_DEPTH_REAL_ENCODER= 0x0800, // Độ sâu đo bằng encoder từ tính AS5600 đếm vòng thật (I2C 0x36)
  FLAG2_DEPTH_ESTIMATED   = 0x1000  // Độ sâu ước tính qua thời gian chạy tời & CT1/CT2
};

#define MS5837_I2C_ADDR       0x76 // Cảm biến áp suất nước MS5837
#define AS5600_I2C_ADDR       0x36 // Encoder góc từ tính AS5600


// Bitmask 11 linh kiện phần cứng độc lập trên Phao ESP32-S3 (Tuyệt đối không gộp ảo!)
enum HardwareHealthBits : uint16_t {
  HW_OK_DS18B20 = (1U << 0),  // 0x001: Cảm biến nhiệt độ nước DS18B20 (GPIO 6)
  HW_OK_PH      = (1U << 1),  // 0x002: Cảm biến độ pH (GPIO 7)
  HW_OK_TDS     = (1U << 2),  // 0x004: Cảm biến tổng khoáng TDS (GPIO 4)
  HW_OK_NTU     = (1U << 3),  // 0x008: Cảm biến độ đục NTU (GPIO 5)
  HW_OK_ACS712  = (1U << 4),  // 0x010: Cảm biến dòng điện ACS712 20A (GPIO 8)
  HW_OK_BAT3S   = (1U << 5),  // 0x020: Mạch đo áp Pin 3S (GPIO 3)
  HW_OK_MPU6050 = (1U << 6),  // 0x040: Cảm biến góc nghiêng 6 trục MPU6050 (I2C 0x68)
  HW_OK_GY271   = (1U << 7),  // 0x080: La bàn số GY-271 / QMC5883L (I2C 0x0D)
  HW_OK_LORA    = (1U << 8),  // 0x100: Module thu phát LoRa Ra-02 433MHz (SPI NSS=15)
  HW_OK_SIM4G   = (1U << 9),  // 0x200: Module 4G LTE SIM A7680C (UART2 GPIO 41/42)
  HW_OK_GPS     = (1U << 10)  // 0x400: Module định vị vệ tinh GPS NEO-6M (UART1 GPIO 17/18)
};
#define HW_TOTAL_COMPONENTS 11

// Cấu trúc lưu mẫu đo từng tầng nước (Mặt 0.3m - Giữa 1.0m - Đáy 1.8m)
struct WaterLayerSample {
  float ph;
  float tds;
  float ntu;
  float temp;
  float doMgL;
  bool  valid;
};

// =========================================================================
// 2. BỘ LỌC KALMAN 1 CHIỀU (1D KALMAN FILTER)
// =========================================================================
struct Kalman1D {
  float x;
  float p;
  float q;
  float r;
  bool  init;

  Kalman1D(float processNoise = 0.015f, float measureNoise = 0.12f)
    : x(0.0f), p(1.0f), q(processNoise), r(measureNoise), init(false) {}

  float update(float measurement) {
    if (isnan(measurement) || isinf(measurement)) return x;
    if (!init) {
      x    = measurement;
      p    = 1.0f;
      init = true;
      return x;
    }
    p = p + q;
    float k = p / (p + r);
    x = x + k * (measurement - x);
    p = (1.0f - k) * p;
    return x;
  }

  void reset(float val) {
    x    = val;
    p    = 1.0f;
    init = true;
  }
};

// =========================================================================
// 3. THUẬT TOÁN XỬ LÝ JOYSTICK VÔ CẤP CHO L298N (DEADZONE + EXPO)
// =========================================================================
inline float mapJoystickAxisExpo(float currentMv, float centerMv, float minMv, float maxMv,
                                 float deadzoneMv = 155.0f, float expoFactor = 0.28f) {
  float delta = currentMv - centerMv;
  if (fabsf(delta) <= deadzoneMv) return 0.0f;

  float norm = 0.0f;
  if (delta > 0.0f) {
    float span = max(250.0f, (maxMv - centerMv) - deadzoneMv);
    norm = constrain((delta - deadzoneMv) / span, 0.0f, 1.0f);
  } else {
    float span = max(250.0f, (centerMv - minMv) - deadzoneMv);
    norm = -constrain(((-delta) - deadzoneMv) / span, 0.0f, 1.0f);
  }
  return (1.0f - expoFactor) * norm + expoFactor * (norm * norm * norm);
}

inline float calcHeadingShortestError(float targetDeg, float currentDeg) {
  float err = targetDeg - currentDeg;
  while (err > 180.0f)  err -= 360.0f;
  while (err < -180.0f) err += 360.0f;
  return err;
}

inline float calcGpsDistanceMeters(double lat1, double lon1, double lat2, double lon2) {
  if ((lat1 == 0.0 && lon1 == 0.0) || (lat2 == 0.0 && lon2 == 0.0)) return 0.0f;
  const double R = 6371000.0;
  double dLat = (lat2 - lat1) * 0.017453292519943295;
  double dLon = (lon2 - lon1) * 0.017453292519943295;
  double a = sin(dLat * 0.5) * sin(dLat * 0.5) +
             cos(lat1 * 0.017453292519943295) * cos(lat2 * 0.017453292519943295) *
             sin(dLon * 0.5) * sin(dLon * 0.5);
  double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return (float)(R * c);
}

// Góc phương vị (0..360°) từ điểm 1 -> điểm 2, dùng cho Autopilot quay về Home GPS
inline float calcGpsBearingDegrees(double lat1, double lon1, double lat2, double lon2) {
  if ((lat1 == 0.0 && lon1 == 0.0) || (lat2 == 0.0 && lon2 == 0.0)) return 0.0f;
  double dLon = (lon2 - lon1) * 0.017453292519943295;
  double y = sin(dLon) * cos(lat2 * 0.017453292519943295);
  double x = cos(lat1 * 0.017453292519943295) * sin(lat2 * 0.017453292519943295) -
             sin(lat1 * 0.017453292519943295) * cos(lat2 * 0.017453292519943295) * cos(dLon);
  float bearing = (float)(atan2(y, x) * 57.29577951308232);
  if (bearing < 0.0f) bearing += 360.0f;
  return bearing;
}

// Sao chép chuỗi an toàn, luôn đảm bảo kết thúc bằng '\0'
inline void copyStringSafe(char* dst, size_t dstSize, const char* src) {
  if (!dst || dstSize == 0) return;
  if (!src) { dst[0] = '\0'; return; }
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

// Kiểm tra URL Google Apps Script đã được cấu hình thật hay vẫn là placeholder
inline bool isValidGoogleScriptUrl(const char* url) {
  if (!url || url[0] == '\0') return false;
  if (strncmp(url, "https://script.google.com", 25) != 0) return false;
  if (strstr(url, "YOUR_SCRIPT_ID") != nullptr) return false;
  return true;
}


// =========================================================================
// 4. BẢO MẬT GÓI TIN: KEYED CRC-8 (*XX) & HMAC-SHA256
// =========================================================================
inline uint8_t calcKeyedCRC8(const char* data, size_t len) {
  uint8_t crc = LORA_SECRET_CRC_SALT;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint8_t)data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0x31);
      else            crc = (uint8_t)(crc << 1);
    }
  }
  return crc;
}

static const uint8_t CHEM_HMAC_KEY[16] = {
  'C','h','e','m','S','t','a','t','i','o','n','-','2','0','2','6'
};

inline uint32_t chemHmac32(const char* data, size_t len) {
  uint8_t out[32];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(info, CHEM_HMAC_KEY, sizeof(CHEM_HMAC_KEY),
                  (const unsigned char*)data, len, out);
  return ((uint32_t)out[0] << 24) | ((uint32_t)out[1] << 16) |
         ((uint32_t)out[2] << 8)  |  (uint32_t)out[3];
}

// =========================================================================
// 5. ĐO PIN CHÍNH XÁC (1S / 2S / 3S AUTO-CELL) & TÍNH CHỈ SỐ NƯỚC THỰC TẾ
// =========================================================================
// Tự động nhận diện số cell (1S: 2.8..4.35V | 2S: 5.8..8.7V | 3S: >= 8.8V)
inline int calcBatteryCellCount(float volt) {
  if (volt <= 0.0f)       return 0;
  if (volt <= 4.45f)      return 1;
  if (volt <= 8.75f)      return 2;
  return 3;
}

inline float calcBatteryCellVolt(float volt) {
  int cells = calcBatteryCellCount(volt);
  return (cells > 0) ? (volt / (float)cells) : 0.0f;
}

// Pin yếu thực sự (đã cắm pin và điện áp mỗi cell dưới ngưỡng)
inline bool isBatteryLow(float volt) {
  if (volt < 1.0f) return false;
  return calcBatteryCellVolt(volt) < BAT_CELL_LOW_V;
}

// Pin kiệt nguy cấp -> chỉ khi đó mới cho phép Deep Sleep
inline bool isBatteryCritical(float volt) {
  if (volt < 1.0f) return false;
  return calcBatteryCellVolt(volt) < BAT_CELL_CRITICAL_V;
}

// Tính % Pin Li-ion chuẩn xác theo đường cong điện hóa thực tế (Hỗ trợ tự nhận diện 1S, 2S, 3S)
// Nếu chưa cắm Pin (volt < 1.0V) -> Trả về 0% (Tuyệt đối không báo ảo!)
inline int calcBattery3SPercent(float volt) {
  if (volt < 1.0f) return 0;

  int cells = calcBatteryCellCount(volt);
  if (cells <= 0) return 0;

  float v = volt / (float)cells;
  if (v <= 3.20f) return 0;
  if (v >= 4.18f) return 100;

  static const float cellV[] = {3.20f, 3.45f, 3.60f, 3.70f, 3.78f, 3.88f, 3.98f, 4.08f, 4.18f};
  static const float pct[]   = {0,     10,    22,    38,    52,    67,    80,    91,    100};
  for (int i = 1; i < 9; i++) {
    if (v <= cellV[i]) {
      float k = (v - cellV[i - 1]) / (cellV[i] - cellV[i - 1]);
      return constrain((int)(pct[i - 1] + k * (pct[i] - pct[i - 1]) + 0.5f), 0, 100);
    }
  }
  return 100;
}

inline float doSaturationMgL(float tempC, float tdsPpm) {
  float t = constrain(tempC, 5.0f, 45.0f);
  float doSat = 14.652f - 0.41022f * t + 0.007991f * t * t - 0.000077774f * t * t * t;
  float sPpt  = constrain(tdsPpm * 0.001f, 0.0f, 40.0f);
  doSat -= sPpt * (0.0841f - 0.00256f * t + 0.0000374f * t * t);
  return constrain(doSat, 2.0f, 14.8f);
}

inline float estimateVirtualDO(float tempC, float tdsPpm, float ph, float ntu) {
  if (tempC <= 0.1f && ph <= 0.1f && tdsPpm <= 0.1f && ntu <= 0.1f) return 0.0f;
  float t = (tempC > 1.0f) ? tempC : 28.0f;
  float p = (ph > 0.5f)    ? ph    : 7.0f;
  float doSat = doSaturationMgL(t, tdsPpm);
  float turbPenalty = constrain(ntu * 0.0022f, 0.0f, 2.8f);
  float phDev       = abs(p - 7.35f);
  float phPenalty   = (phDev > 0.6f) ? ((phDev - 0.6f) * 0.42f) : 0.0f;
  float estDO       = doSat * 0.96f - turbPenalty - phPenalty;
  return constrain(estDO, 1.50f, 14.00f);
}

inline int calcDOSaturationPercent(float doMgL, float tempC, float tdsPpm) {
  if (doMgL <= 0.05f) return 0;
  float t = (tempC > 1.0f) ? tempC : 28.0f;
  float sat = doSaturationMgL(t, tdsPpm);
  if (sat <= 0.1f) return 0;
  return constrain((int)((doMgL / sat) * 100.0f + 0.5f), 0, 150);
}

inline float calcECuScm(float tdsPpm) {
  return max(0.0f, tdsPpm * 2.0f);
}

inline float calcSalinityPpt(float tdsPpm) {
  return max(0.0f, tdsPpm * 0.001f);
}

// Chỉ đánh giá vượt ngưỡng trên các cảm biến ĐÃ CẮM (Giá trị > 0)
inline int evaluateWaterQuality(float ph, float tds, float ntu, float temp) {
  if ((ph > 0.1f && (ph < 5.5f || ph > 9.0f)) ||
      (tds > 1500.0f) ||
      (ntu > 300.0f)  ||
      (temp > 42.0f)) {
    return 2;
  }
  if ((ph > 0.1f && (ph < 6.0f || ph > 8.5f)) ||
      (tds > 1000.0f) ||
      (ntu > 120.0f)  ||
      (temp > 39.5f)) {
    return 1;
  }
  return 0;
}

// Tính VN-WQI dựa trên các cảm biến thực tế đang kết nối (Nếu chưa cắm cảm biến nước nào -> WQI = 0)
inline int calcWaterQualityIndex(float ph, float doMgL, float tds, float ntu, float temp) {
  if (ph <= 0.1f && tds <= 0.1f && ntu <= 0.1f && temp <= 0.1f) {
    return 0; // Chưa cắm cảm biến nước -> Không báo WQI ảo!
  }

  float wqiPH = 100.0f;
  if (ph > 0.1f) {
    if (ph < 5.5f || ph > 9.0f)      wqiPH = 10.0f;
    else if (ph < 6.5f)              wqiPH = 50.0f + ((ph - 5.5f) / 1.0f) * 50.0f;
    else if (ph > 8.5f)              wqiPH = 50.0f + ((9.0f - ph) / 0.5f) * 50.0f;
  }

  int doPct = calcDOSaturationPercent(doMgL, temp, tds);
  float wqiDO = 100.0f;
  if (doMgL > 0.1f) {
    if (doPct < 20)       wqiDO = 10.0f;
    else if (doPct < 50)  wqiDO = 25.0f + (doPct - 20) * (25.0f / 30.0f);
    else if (doPct < 75)  wqiDO = 50.0f + (doPct - 50) * (25.0f / 25.0f);
    else if (doPct < 88)  wqiDO = 75.0f + (doPct - 75) * (25.0f / 13.0f);
    else                  wqiDO = 100.0f;
  }

  float wqiNTU = 100.0f;
  if (ntu > 0.1f) {
    if (ntu <= 5.0f)        wqiNTU = 100.0f;
    else if (ntu <= 25.0f)  wqiNTU = 100.0f - (ntu - 5.0f) * 1.0f;
    else if (ntu <= 75.0f)  wqiNTU = 80.0f  - (ntu - 25.0f) * 0.6f;
    else if (ntu <= 200.0f) wqiNTU = 50.0f  - (ntu - 75.0f) * 0.2f;
    else                    wqiNTU = 15.0f;
  }

  float wqiTDS = 100.0f;
  if (tds > 0.1f) {
    if (tds <= 300.0f)       wqiTDS = 100.0f;
    else if (tds <= 600.0f)  wqiTDS = 100.0f - (tds - 300.0f) * (20.0f / 300.0f);
    else if (tds <= 1200.0f) wqiTDS = 80.0f  - (tds - 600.0f) * (35.0f / 600.0f);
    else                     wqiTDS = 30.0f;
  }

  float total = 0.0f;
  float weightSum = 0.0f;
  if (ph > 0.1f)   { total += wqiPH  * 0.25f; weightSum += 0.25f; }
  if (doMgL > 0.1f){ total += wqiDO  * 0.30f; weightSum += 0.30f; }
  if (ntu > 0.1f)  { total += wqiNTU * 0.25f; weightSum += 0.25f; }
  if (tds > 0.1f)  { total += wqiTDS * 0.20f; weightSum += 0.20f; }

  // Chuan hoa lai trong so theo dung cac cam bien thuc te dang co.
  // Neu khong chuan hoa, cam bien chua cam (mac dinh 100 diem) se keo WQI len sai.
  if (weightSum <= 0.0f) return 0;
  return constrain((int)((total / weightSum) + 0.5f), 10, 100);
}

inline const char* getWqiRankText(int wqi) {
  if (wqi <= 0)  return "CHUA CAM CB!";
  if (wqi >= 91) return "RAT TOT (SH)";
  if (wqi >= 76) return "TOT (AN TOAN)";
  if (wqi >= 51) return "TRUNG BINH";
  if (wqi >= 26) return "KEM (CANH BAO)";
  return "O NHIEM NANG!";
}

// Phân loại chất lượng nước theo Quy chuẩn Kỹ thuật Quốc gia QCVN 08:2023/BTNMT
inline const char* getQcvn08Classification(int wqi, float ph, float tds, float ntu) {
  if (ph <= 0.1f && tds <= 0.1f && ntu <= 0.1f) return "CHUA CAM CAM BIEN";
  if (wqi >= 91) return "MUC A: CAP NUOC SINH HOAT";
  if (wqi >= 76) return "MUC B: THUY SAN & NONG NGHIEP";
  if (wqi >= 51) return "MUC C: GIAO THONG THUY";
  if (wqi >= 26) return "MUC D: O NHIEM (CAN XL)";
  return "MUC NGUY HAI (CAM DUNG)";
}
