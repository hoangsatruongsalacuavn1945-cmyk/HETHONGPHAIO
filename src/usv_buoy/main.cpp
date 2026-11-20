#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <LoRa.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <TinyGPSPlus.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <driver/adc.h>
#include <esp32-hal-rgb-led.h>
#include "usv_common.h"

// =========================================================================
// CHỌN LOẠI MẠCH ĐỘNG CƠ (Mặc định dùng L298N = 0, nếu dùng Motor Shield = 1)
// =========================================================================
#define USE_ARDUINO_MOTOR_SHIELD  0
#define USE_BTS7960_DRIVER        0
// H-01: Giới hạn PWM an toàn cho L298N (165/255 ~ 65% duty) chống quá nhiệt khi kéo motor 775;
// Khi nâng cấp lên BTS7960 (USE_BTS7960_DRIVER == 1), cho phép mở 100% công suất (255 PWM).
#if USE_BTS7960_DRIVER
  #define MOTOR_MAX_SAFE_PWM      255
#else
  #define MOTOR_MAX_SAFE_PWM      165
#endif

// =========================================================================
// 1. ĐỊNH NGHĨA CHÂN ESP32-S3-WROOM-1 (GIỮ NGUYÊN 100% THEO SƠ ĐỒ PHẦN CỨNG)
// =========================================================================
// --- Trái: 5 Kênh ADC1 + Nhiệt độ DS18B20 + I2C + 2 Công tắc hành trình ---
#ifdef USE_SAFE_ALT_PINOUT
  #define PIN_BAT_3S        2   // [Alt Safe Pinout] Dời khỏi chân Strapping GPIO 3
#else
  #define PIN_BAT_3S        3   // [ADC1] Cầu trở 33k/10k đo áp Pin 3S từ P+ BMS
#endif
#define PIN_TDS           4   // [ADC1] Cảm biến TDS
#define PIN_TURBIDITY     5   // [ADC1] Cảm biến Độ đục (Chế độ Analog AO)
#define PIN_DS18B20       6   // [1-Wire] Cảm biến nhiệt độ nước DS18B20
#define PIN_PH            7   // [ADC1] Cảm biến đo pH (PH-4502C / E-201-C chân Po)
#define PIN_ACS712        8   // [ADC1] Cảm biến dòng ACS712 20A

#define I2C_SDA_PIN       1   // [I2C SDA Chung] MPU6050 (0x68) + La bàn GY-271 (0x0D)
#define I2C_SCL_PIN       2   // [I2C SCL Chung] MPU6050 (0x68) + La bàn GY-271 (0x0D)
#define MPU6050_ADDR      0x68
#define GY271_ADDR        0x0D

#define PIN_CT1_BOTTOM    38  // [CT Hành trình 1] Chống chùng bùn (COM->GND, NO->38)
#define PIN_CT2_TOP_HOME  39  // [CT Hành trình 2] Điểm gốc Home (COM->GND, NO->39)

// --- Phải: LoRa Ra-02 (NSS=15) + 2 Servo + L298N/Shield + SIM A7680C + GPS ---
#define LORA_NSS_PIN      15  // Cố định theo yêu cầu
#define LORA_SCK_PIN      11  // An toàn tuyệt đối cho cả bản N16R8
#define LORA_MISO_PIN     13
#define LORA_MOSI_PIN     14
#define LORA_RST_PIN      21
#define LORA_DIO0_PIN     47

#define PIN_SERVO_WINCH   9   // Servo 1: Tời MG996R 360°
#define PIN_SERVO_RUDDER  10  // Servo 2: Bánh lái MG90S
#ifdef USE_SAFE_ALT_PINOUT
  #define PIN_CO2_TRIGGER   19  // [Alt Safe Pinout] Dời khỏi chân Strapping VDD_SPI GPIO 45
  #define PIN_SIREN_BUZZER  20  // [Alt Safe Pinout] Dời khỏi chân Strapping ROM_LOG GPIO 46
#else
  #define PIN_CO2_TRIGGER   45  // Kích Rơ-le Solenoid bung Phao Cứu Sinh CO2 khi lật úp (>60° >3s)
  #define PIN_SIREN_BUZZER  46  // Còi hú cứu hộ khẩn cấp & cảnh báo cộng đồng  // Servo 2: Bánh lái MG90S
#endif

#define PIN_MOTOR_PWM     16  // L298N: ENA  | Motor Shield: PWMA (D3)
#define PIN_MOTOR_IN1     40  // L298N: IN1  | Motor Shield: DIRA (D12)
#define PIN_MOTOR_IN2     12  // L298N: IN2  | Motor Shield: BRAKEA (D9)

#define SIM_TX2_PIN       41  // Nối vào chân RXD của SIM A7680C
#define SIM_RX2_PIN       42  // Nối vào chân TXD của SIM A7680C

#define GPS_TX1_PIN       17  // [Chờ lắp sau] Nối vào chân RX của GPS NEO-6M
#define GPS_RX1_PIN       18  // [Chờ lắp sau] Nối vào chân TX của GPS NEO-6M

// Kênh PWM LEDC riêng cho Motor (Tránh xung đột Timer 0,1 của thư viện ESP32Servo)
#define MOTOR_LEDC_CH     4
#define MOTOR_LEDC_FREQ   5000
#define MOTOR_LEDC_RES    8

// Ngưỡng bảo vệ an toàn & Watchdog
#define CURRENT_JAM_LIMIT_A     2.80f   // Dòng kẹt chân vịt (A)
#define WINCH_MAX_RUN_TIME_MS   25000UL // Thời gian chạy tời tối đa chống đứt cáp (25s)
#define MUD_RELIFT_TIME_MS      350UL   // Thời gian nhấc nhẹ khỏi bùn đáy (350ms ~ 5cm)
#define WDT_TIMEOUT_SEC         8       // Hardware Task Watchdog 8 giây

// Tự hành quay về Home GPS & cảnh báo Cloud
#define RETURN_HOME_RADIUS_M    2.5f    // Bán kính coi như đã về tới Home (m)
#define RETURN_HOME_MOTOR_PWM   150     // Công suất động cơ khi tự hành về Home
#define GSHEETS_ALARM_COOLDOWN_MS 60000UL // Giới hạn tần suất đẩy cảnh báo khẩn lên Cloud
#define SIM_HTTP_TIMEOUT_MS     12000UL // Chờ tối đa phản hồi HTTP của SIM

// =========================================================================
// 2. ĐỐI TƯỢNG, BỘ LỌC KALMAN 1D & BIẾN ĐA NHIỆM DUAL-CORE (FREERTOS)
// =========================================================================
OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);
Servo winchServo;
Servo rudderServo;
TinyGPSPlus gps;
Preferences nvsPrefs;

// Mutex đồng bộ dữ liệu giữa Core 0 (Sensor/Cloud) và Core 1 (RF/Control/NVS)
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;

// Bộ lọc Kalman 1 chiều cho 5 kênh cảm biến
Kalman1D kalmanPH(0.005f, 0.12f);
Kalman1D kalmanTDS(0.05f, 10.00f);
Kalman1D kalmanNTU(0.04f, 12.00f);
Kalman1D kalmanAmp(0.006f, 0.12f);
Kalman1D kalmanBat(0.005f, 0.08f);

// Biến ngắt công tắc hành trình (Active-LOW: COM->GND, NO->GPIO, có lọc chống dội 45ms)
volatile int  winchMode = 0; // 0: Dừng, 1: Đang hạ tời, 2: Đang thu về Home
volatile bool trigBottomMud = false;
volatile bool trigTopHome   = false;
volatile unsigned long lastIsrMudTick  = 0;
volatile unsigned long lastIsrHomeTick = 0;

// Biến đo lường cảm biến THỰC TẾ 100% (Tuyệt đối không khởi tạo số ảo: chưa cắm linh kiện = 0.0)
float valPH = 0.0f, valTDS = 0.0f, valNTU = 0.0f, valTemp = 0.0f, valDO = 0.0f;
float valAmp = 0.0f, valVolt3S = 0.0f, valRoll = 0.0f, valPitch = 0.0f, valHeading = 0.0f;
float rawRollUncomp = 0.0f, rawPitchUncomp = 0.0f;
float rollTareOffset = 0.0f, pitchTareOffset = 0.0f;
bool  imuBootTareDone = false;
int   imuWarmupSamples = 0;
bool  autoCmdLatched   = false;
float valMpuTemp = 0.0f;    // Nhiệt độ chip MPU6050 thực tế (°C) - chưa cắm = 0.0
float valS3ChipTemp = 0.0f; // Nhiệt độ chip ESP32-S3 nội (°C) từ cảm biến on-die
float valEC      = 0.0f;    // Độ dẫn điện EC thực tế (µS/cm)
int   valWQI     = 0;       // Chỉ số chất lượng nước VN-WQI (0..100, chưa cắm = 0)
// Cảm biến độ sâu thủy tĩnh MS5837-02BA / MS5837-30BA (I2C 0x76)
bool     hasMS5837            = false;
uint16_t ms5837_C[8]          = {0};
float    ms5837PressureMbar   = 1013.25f;
float    ms5837TempC          = 25.0f;
float    ms5837SurfaceMbar    = 1013.25f;
bool     ms5837Tared          = false;
float    ms5837DepthM         = 0.00f;
unsigned long ms5837LastReadMs= 0;
uint8_t  ms5837Step           = 0; // 0: Idle/Start D1, 1: Read D1 & Start D2, 2: Read D2 & Calc
unsigned long ms5837StepTick  = 0;
uint32_t ms5837D1             = 0;
uint32_t ms5837D2             = 0;

// Encoder từ tính AS5600 đếm vòng rulo tời (I2C 0x36)
bool     hasAS5600            = false;
uint16_t as5600LastRaw        = 0;
int32_t  as5600TotalTicks     = 0;
float    as5600DepthM         = 0.00f;
#define  WINCH_SPOOL_CIRCUM_M 0.125f // D = 40mm -> Chu vi ~ 0.125m/vòng

float winchDepthM = 0.00f;  // Độ sâu lồng cảm biến hiện tại (m)
unsigned long lastDepthCalcMs = 0;

double gpsLat = 0.0, gpsLng = 0.0;
bool sensorsInitialized = false;
bool ds18b20Valid       = false;
bool phValid            = false;
bool tdsValid           = false;
bool ntuValid           = false;
bool acs712Valid        = false;
bool batValid           = false;

// Chẩn đoán chi tiết từng kênh ADC (lý do lỗi thật: HỞ MẠCH / BÃO HÒA ADC / NHIỄU)
enum AdcFault : uint8_t { ADC_OK = 0, ADC_OPEN = 1, ADC_SATURATED = 2, ADC_NOISY = 3 };
struct AdcDiag {
  uint16_t mv;       // Điện áp trung vị (mV)
  uint16_t swing;    // Độ lệch thử kéo Pull-Up/Pull-Down (mV)
  uint16_t span;     // Độ tản mạn trung vị (mV)
  uint16_t raw;      // Giá trị ADC thô 12-bit
  uint8_t  fault;    // Lý do lỗi mẫu gần nhất
  uint16_t hist;     // Cửa sổ trượt 10 mẫu OK
  uint16_t satHist;  // Cửa sổ trượt 10 mẫu BÃO HÒA
  bool     valid;    // Trạng thái ĐÃ CẮM (có trễ chống nhấp nháy)
  bool     saturated;// Trạng thái VƯỢT DẢI ĐO (có trễ)
};
AdcDiag diagPH  = {}, diagTDS = {}, diagNTU = {}, diagACS = {}, diagBAT = {};
uint16_t adcFaultMask = 0; // Bit theo HW_OK_* : kênh đang VƯỢT DẢI ĐO ADC (>3.05V)
bool hasGPS             = false;
uint32_t lastGpsChars   = 0;
unsigned long lastGpsValidMs = 0;
unsigned long lastGpsUartMs  = 0;

// Hiệu chuẩn điểm 0 cảm biến dòng ACS712, Offset pH & Hệ số chia áp Pin (33k/10k = 4.30) lưu trong NVS
float acsZeroVolt     = 1.65f;
float phCalibOffset   = 0.00f;
float phSlopeVPerUnit = 0.18f;  // H-09: Độ dốc điện cực pH 2 điểm (V/pH, mặc định 0.180V/pH)
float phRefVolt7      = 2.50f;  // Điện áp chuẩn tại pH 7.00 / 6.86 (V)
float lastRawPhVolt   = 2.50f;  // Điện áp thực tế gần nhất tại chân pH (V)
float batDividerRatio = 4.30f;  // P0-5: Tỉ số chuẩn mạch cầu phân áp 33kΩ / 10kΩ = 4.30 (hỗ trợ hiệu chuẩn VOM lưu NVS)
float tdsCalibFactor  = 1.00f;  // Hệ số hiệu chuẩn TDS (nhân vào ppm thô)
float ntuCalibOffset  = 0.00f;  // Offset hiệu chuẩn độ đục NTU
float lastRawBatPinV  = 0.00f;
bool  nvsCalibrated   = false;
uint32_t bootCount    = 0;

// S-09: Hiệu chuẩn Hard-Iron (Offset) & Soft-Iron (Scale) cho La bàn từ trường GY-271
float magOffsetX = 0.0f, magOffsetY = 0.0f, magOffsetZ = 0.0f;
float magScaleX  = 1.0f, magScaleY  = 1.0f, magScaleZ  = 1.0f;

// S-06 & S-07: Cấu hình chu kỳ phát LoRa & Bán kính Neo Ảo DP
unsigned long loraTxIntervalMs = LORA_DEFAULT_TX_INTERVAL_MS; // Mặc định 1000ms (500..5000ms)
float dpSafeRadiusM     = 1.5f; // Bán kính vùng chết Neo Ảo DP (m)
float dpActivateRadiusM = 2.5f; // Bán kính kích hoạt đẩy DP (m)

// Bộ lọc vị trí GPS (Median-3 + EMA alpha=0.25) & HDOP cho DP/RTH
double filtGpsLat = 0.0, filtGpsLng = 0.0;
float  gpsHdop    = 99.9f;

// Máy trạng thái Return-To-Home (RTH FSM - Mục 3.2)
enum RthFsmState : uint8_t {
  RTH_IDLE = 0,
  RTH_ARMED,
  RTH_TURN_HOME,
  RTH_CRUISE,
  RTH_APPROACH,
  RTH_ARRIVED,
  RTH_ABORT
};
RthFsmState rthState = RTH_IDLE;
unsigned long rthStartMs = 0;
unsigned long rthGpsLossStartMs = 0;

// P3-3.3 & 3.4: Máy trạng thái Sục rửa đầu dò tự động (30s) & Lấy mẫu nước tự động (20s) Non-blocking
enum CleanSensorState : uint8_t {
  CLEAN_IDLE = 0,
  CLEAN_WASH_PUMP,  // 10s bơm tia nước sạch
  CLEAN_AIR_FLUSH,  // 10s sục khí
  CLEAN_SETTLE      // 10s chờ ổn định điện cực trước khi đọc lại
};
CleanSensorState cleanState = CLEAN_IDLE;
unsigned long cleanStepStartMs = 0;

enum SampleWaterState : uint8_t {
  SAMPLE_IDLE = 0,
  SAMPLE_FLUSH_PIPE,  // 5s tráng đường ống
  SAMPLE_FILL_BOTTLE, // 15s bơm đầy chai mẫu
  SAMPLE_DONE
};
SampleWaterState sampleState = SAMPLE_IDLE;
unsigned long sampleStepStartMs = 0;
uint8_t sampleBottleIdx = 0;

// Cấu hình cảnh báo Cloud/SMS (có thể đổi qua Serial: SMS:<số điện thoại>)
char smsAlertNumber[24] = "";
unsigned long lastAlarmPushMs = 0;
uint32_t simGsFailCnt = 0;
uint32_t loraTxOkCnt  = 0;
uint32_t loraTxFailCnt = 0;

// Trạng thái tự hành quay về Home GPS
bool  returnHomeActive = false;
float homeDistanceM    = 0.0f;
float gpsSpeedKmph     = 0.0f;
float gpsAltM          = 0.0f;
uint8_t gpsSats        = 0;
bool  gpsFixValid      = false;
unsigned long lastGpsFixMs = 0; // Lan cuoi co toa do GPS hop le (timeout 5s -> mat FIX that su)
uint16_t flags2        = 0;

// Khởi tạo module SIM 4G (APN + NETOPEN) theo từng bước Non-blocking
uint8_t simInitStep       = 0;
unsigned long simInitTick = 0;
bool simApnReady          = false;
bool simNetOpen           = false;
TaskHandle_t core0TaskHandle = nullptr;

// Cờ nhận diện từng linh kiện phần cứng thực tế 100% (Tuyệt đối không báo ảo!)
bool hasMPU6050      = false;
uint8_t mpuAddr      = MPU6050_ADDR;
int curSdaPin        = I2C_SDA_PIN;
int curSclPin        = I2C_SCL_PIN;
unsigned long lastI2cCheckTick = 0;
unsigned long lastMpuReadUs    = 0;
bool hasGY271        = false;
uint8_t gy271Addr    = 0x0D; // 0x0D (QMC5883L) hoac 0x1E (HMC5883L)
unsigned long winchInchUntilMs = 0;
bool virtualAnchorActive = false;
double virtualAnchorLat  = 0.0;
double virtualAnchorLng  = 0.0;
bool loraReady       = false;
bool simModemOnline  = false;
unsigned long lastSimOkMs = 0;
bool is4GFallback    = false;
uint16_t hwHealthMask = 0;
unsigned long lastDiagPrintMs = 0;

// Tần số thực thi đa nhiệm trên 2 nhân CPU (Hz)
volatile uint32_t core0LoopCnt = 0;
volatile uint32_t core1LoopCnt = 0;
uint16_t core0Hz = 20;
uint16_t core1Hz = 250;
unsigned long lastHzStatMs = 0;

// Hàng đợi lệnh an toàn luồng từ Core 0 (4G/USB) chuyển sang Core 1 thực thi
char pendingCore0Cmd[180];
volatile bool hasPendingCore0Cmd = false;
volatile bool reqSaveNvsUrl      = false;
volatile bool reqSaveNvsCalib    = false;
volatile bool reqEnterDeepSleep  = false;
volatile bool reqReboot          = false;
volatile bool reqFactoryReset    = false;
volatile bool reqStatusPrint     = false;

// Biến điều khiển nhận từ Trạm bờ (LoRa hoặc 4G Fallback)
int cmdMotor  = 0;      // -255..255 (Lệnh mục tiêu từ Trạm bờ)
int curMotor  = 0;      // -255..255 (Tốc độ thực tế sau Soft-Start Ramp)
int cmdRudder = 90;     // 45..135 độ
int cmdWinch  = 0;      // 0: Dừng, 1: Hạ, 2: Thu, 3: Auto đo 3 tầng

// Bộ điều khiển Tự hành Giữ hướng La bàn (Autopilot PID Heading Hold) & Neo GPS
bool  headingHoldActive  = false;
float targetHeadingDeg   = 0.0f;
float pidHeadingIntegral = 0.0f;
float lastGyroYawRate    = 0.0f;
unsigned long lastPidTickMs = 0;
double homeGpsLat = 0.0, homeGpsLng = 0.0;

// Quản lý thời gian & trạng thái bảo vệ Non-blocking
unsigned long lastCmdRxTime     = 0;
unsigned long lastLoraTx        = 0;
unsigned long lastLoraHealthMs  = 0;
unsigned long lastSimTx         = 0;
unsigned long lastSimPingMs     = 0;
unsigned long lastSoftStartTick = 0;
unsigned long lastTempReqTick   = 0;
unsigned long winchStartTick    = 0;
unsigned long lowBatDangerStart = 0;
unsigned long lowBatTier2Start  = 0;
unsigned long lowBatTier3Start  = 0;
bool batTier3CutoffActive       = false;
bool batTier2RthTriggered       = false;
unsigned long brownoutCooldownUntil = 0;

// Bộ đệm tĩnh nhận UART2 (SIM A7680C) và USB Serial0 (Zero-Heap, Non-blocking 100%)
char simRxLine[160];
size_t simRxIdx = 0;
char usbRxLine[180];
size_t usbRxIdx = 0;
bool emergencyAlertSent = false;

// Cấu hình & Máy trạng thái đẩy dữ liệu Google Sheets qua SIM 4G A7680C
char googleScriptUrl[180] = DEFAULT_GOOGLE_SCRIPT_URL;
enum SimGSheetsState : uint8_t {
  SIM_GS_IDLE = 0,
  SIM_GS_TERM_PREV,
  SIM_GS_HTTPINIT,
  SIM_GS_SSL_CFG,
  SIM_GS_HTTPSSL,
  SIM_GS_HTTPPARA_REDIR,
  SIM_GS_HTTPPARA_URL,
  SIM_GS_HTTPACTION,
  SIM_GS_WAIT_RESPONSE,
  SIM_GS_HTTPREAD,
  SIM_GS_WAIT_READ,
  SIM_GS_HTTPTERM
};
SimGSheetsState simGsState       = SIM_GS_IDLE;
unsigned long   simGsStepTick    = 0;
volatile bool   pendingGsProfile = false;
bool            currentGsIsPrf   = false;
uint32_t        simGsSuccessCnt  = 0;
int             simLastHttpCode  = 0;
int             simLastDataLen   = 0;

// Nhấc bùn đáy Non-blocking & Khóa chống đập bùn lặp lại
bool isReliftingFromMud   = false;
unsigned long reliftStart = 0;
bool mudBottomLatched     = false;

// Chống kẹt chân vịt (Anti-Jam State Machine Non-blocking)
enum JamState : uint8_t {
  JAM_NORMAL = 0,
  JAM_STOP_WAIT,
  JAM_REVERSE_CLEARING,
  JAM_LOCKED_WAIT_RELEASE
};
JamState jamState          = JAM_NORMAL;
unsigned long jamTimerTick = 0;
unsigned long overAmpStart = 0;
uint8_t jamConsecutiveStrikes = 0;
unsigned long lastNormalRunTick = 0;

// Máy trạng thái Auto đo 3 tầng nước (Mặt - Giữa - Đáy)
AutoProfileStage autoStage     = AUTO_STAGE_OFF;
unsigned long    autoStageTick = 0;
WaterLayerSample layerSurface  = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false};
WaterLayerSample layerMiddle   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false};
WaterLayerSample layerBottom   = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false};

// Phản hồi Telemetry đồng bộ ngay sau khi nhận lệnh từ Trạm bờ
bool pendingTelemetryReply      = false;
unsigned long cmdReceivedAtTick = 0;
uint8_t systemAlertFlags        = ALERT_NONE;
uint32_t usvTxSeq               = 0;

// =========================================================================
// 2B. HỆ THỐNG ĐÈN LED RGB WS2812 THÔNG MINH TRÊN ESP32-S3 (GPIO 48)
// Cho phép quan sát toàn bộ trạng thái hoạt động của phao bằng mắt thường:
//   1. XANH LÁ (Nhịp tim êm dịu 1.2s): Hệ thống hoạt động tốt, nước đạt chuẩn (WQI >= 50), Pin đầy
//   2. XANH DƯƠNG (Chớp 70ms): Vừa phát gói tin LoRa Telemetry về Trạm Bờ
//   3. XANH CYAN (Chớp 100ms): Vừa nhận lệnh điều khiển (Ga/Lái/Tời) từ Trạm Bờ
//   4. XANH CYAN (Sáng liên tục): Động cơ đẩy L298N đang chạy (curMotor != 0)
//   5. VÀNG (Chớp 400ms): Tời đang cuộn/hạ (Cảnh báo cơ khí) HOẶC nước ô nhiễm nhẹ (WQI 35-50)
//   6. ĐỎ (Chớp nhanh 180ms): BÁO ĐỘNG NGUY HIỂM (Nước ô nhiễm nặng / Pin < 10.6V / Lật phao > 45° / Kẹt chân vịt)
//   7. TÍM MAGENTA (Chớp 600ms): CẢNH BÁO THIẾU LINH KIỆN chính (DS18B20, pH, hoặc Pin chưa cắm)
//   8. TRẮNG (Sáng liên tục): Đang khởi động & Tự kiểm tra phần cứng
// =========================================================================
uint8_t  ledFlashR = 0, ledFlashG = 0, ledFlashB = 0;
unsigned long ledFlashUntilMs = 0;
unsigned long lastLedUpdateMs = 0;

void setBuoyRgbLed(uint8_t r, uint8_t g, uint8_t b) {
  #ifdef RGB_BUILTIN
    neopixelWrite(RGB_BUILTIN, r, g, b);
  #else
    neopixelWrite(48, r, g, b);
  #endif
}

void triggerLedFlash(uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs) {
  ledFlashR = r;
  ledFlashG = g;
  ledFlashB = b;
  ledFlashUntilMs = millis() + durationMs;
  setBuoyRgbLed(r, g, b);
}

void updateBuoyStatusLed() {
  unsigned long now = millis();
  if (now - lastLedUpdateMs < 30) return;
  lastLedUpdateMs = now;

  // Ưu tiên 0: Flash xung truyền thông tức thời (LoRa TX = Xanh dương, LoRa RX = Xanh Cyan)
  if (now < ledFlashUntilMs) {
    setBuoyRgbLed(ledFlashR, ledFlashG, ledFlashB);
    return;
  }

  // Ưu tiên 1: BÁO ĐỘNG NGUY HIỂM (Chớp ĐỎ nhịp nhanh 180ms)
  // - Góc nghiêng lật > 45°
  // - Kẹt chân vịt (Jam Overcurrent)
  // - Pin yếu (ngưỡng mỗi cell < 3.35V, tự nhận diện 1S/2S/3S)
  // - Nước ô nhiễm nghiêm trọng (pH < 5.5 hoặc > 9.5, hoặc DO < 4.0mg/L)
  bool isSeriousAlarm = false;
  if (abs(valRoll) > 45.0f || abs(valPitch) > 45.0f) isSeriousAlarm = true;
  if (jamState != JAM_NORMAL) isSeriousAlarm = true;
  if (batValid && isBatteryLow(valVolt3S)) isSeriousAlarm = true;
  if (valPH > 0.0f && (valPH < 5.5f || valPH > 9.5f)) isSeriousAlarm = true;

  if (isSeriousAlarm) {
    bool onPhase = (now % 360) < 180;
    if (onPhase) setBuoyRgbLed(45, 0, 0); // Đỏ cảnh báo nguy hiểm
    else         setBuoyRgbLed(0, 0, 0);
    return;
  }

  // Ưu tiên 2: CẢNH BÁO THIẾU LINH KIỆN QUAN TRỌNG (Chớp TÍM MAGENTA 600ms)
  // Khi các cảm biến cơ bản hoặc pin chưa được cắm (chân hở mạch)
  bool isMissingMajorHw = (!ds18b20Valid || !phValid || !batValid);
  if (isMissingMajorHw) {
    bool onPhase = (now % 600) < 300;
    if (onPhase) setBuoyRgbLed(35, 0, 35); // Tím Magenta
    else         setBuoyRgbLed(0, 0, 0);
    return;
  }

  // Ưu tiên 3: TỜI ĐANG HOẠT ĐỘNG (Chớp VÀNG 400ms cảnh báo cơ khí)
  if (winchMode != 0) {
    bool onPhase = (now % 400) < 200;
    if (onPhase) setBuoyRgbLed(40, 30, 0); // Vàng cơ khí
    else         setBuoyRgbLed(0, 0, 0);
    return;
  }

  // Ưu tiên 4: ĐỘNG CƠ ĐẨY L298N ĐANG CHẠY (Sáng XANH CYAN liên tục)
  if (curMotor != 0) {
    setBuoyRgbLed(0, 35, 35); // Cyan
    return;
  }

  // Ưu tiên 5: CHẤT LƯỢNG NƯỚC CẢNH BÁO NHẸ (Chớp CAM 1s)
  if (valWQI > 0 && valWQI < 50) {
    bool onPhase = (now % 1000) < 400;
    if (onPhase) setBuoyRgbLed(40, 18, 0); // Cam
    else         setBuoyRgbLed(0, 0, 0);
    return;
  }

  // Ưu tiên 6: HOẠT ĐỘNG BÌNH THƯỜNG - NHỊP TIM XANH LÁ (Green Heartbeat 1.2s)
  bool beatPhase = (now % 1200) < 140;
  if (beatPhase) {
    setBuoyRgbLed(0, 35, 0); // Xanh lá dịu
  } else {
    setBuoyRgbLed(0, 0, 0);
  }
}

// =========================================================================
// 3. NGẮT PHẦN CỨNG 2 CÔNG TẮC HÀNH TRÌNH T125 (GPIO 38 & GPIO 39 - ACTIVE LOW)
// =========================================================================
void IRAM_ATTR isrBottomMud() {
  unsigned long now = millis();
  if (now - lastIsrMudTick > 45) {
    lastIsrMudTick = now;
    if (winchMode == 1) {
      trigBottomMud = true;
    }
  }
}

void IRAM_ATTR isrTopHome() {
  unsigned long now = millis();
  if (now - lastIsrHomeTick > 45) {
    lastIsrHomeTick = now;
    if (winchMode == 2) {
      trigTopHome = true;
    }
  }
}

// =========================================================================
// 4. HÀM ĐIỀU KHIỂN ĐỘNG CƠ & TỜI (CÓ BÙ ÁP, ĐO ĐỘ SÂU & P-CONTROL)
// =========================================================================
void writeMotorPWM(int pwmVal) {
  ledcWrite(MOTOR_LEDC_CH, constrain(pwmVal, 0, 255));
}

// P0-4: Khóa cắt phần cứng trực tiếp chân cầu H trên cả 2 nhân (Core 0 & Core 1) khi kẹt/sập áp Tier 3
inline void enforceHardwareMotorSafetyGuard() {
  if (jamState == JAM_LOCKED_WAIT_RELEASE || batTier3CutoffActive) {
    ledcWrite(MOTOR_LEDC_CH, 0);
    digitalWrite(PIN_MOTOR_IN1, LOW);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    curMotor = 0;
    cmdMotor = 0;
  }
}

void driveMotor(int speedPWM) {
  speedPWM = constrain(speedPWM, -255, 255);

  // Khóa động cơ 6 giây sau Brownout Reset để điện áp nguồn LiPo & tụ lọc ổn định hoàn toàn
  if (millis() < brownoutCooldownUntil) {
    digitalWrite(PIN_MOTOR_IN1, LOW);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    writeMotorPWM(0);
    return;
  }

  // BẢO VỆ ĐA TẦNG NGUỒN PIN 3S LIPO:
  // Tier 3: Cắt nguồn động cơ khẩn cấp (< 3.15V/cell hoặc < 9.45V 3S) -> Giữ điện áp cho 3.3V bus, LoRa & GPS SOS!
  if (batValid && valVolt3S > 1.0f && (calcBatteryCellVolt(valVolt3S) < BAT_CELL_TIER3_CUTOFF_V || batTier3CutoffActive)) {
    digitalWrite(PIN_MOTOR_IN1, LOW);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    writeMotorPWM(0);
    return;
  }

  // Tier 1: Cảnh báo & giới hạn công suất (< 3.40V/cell hoặc < 10.20V 3S)
  if (batValid && valVolt3S > 1.0f && calcBatteryCellVolt(valVolt3S) < BAT_CELL_TIER1_WARN_V) {
    speedPWM = constrain(speedPWM, -140, 140);
  }
  // Khoá liên động an toàn cơ khí (Mechanical Safety Interlock):
  if (winchDepthM > 0.45f || winchMode != 0) {
    speedPWM = constrain(speedPWM, -160, 160);
  }

  // H-01: Giới hạn PWM an toàn theo loại mạch cầu H (L298N = 165 PWM ~ 65% duty để chống cháy; BTS7960 = 255 PWM)
  if (jamState == JAM_NORMAL) {
    speedPWM = constrain(speedPWM, -MOTOR_MAX_SAFE_PWM, MOTOR_MAX_SAFE_PWM);
  }

  // Chống chập cầu H L298N (Shoot-Through Protection & 800ms Direction-Reversal Guard):
  // Khi đảo chiều trực tiếp (Tiến <-> Lùi), giữ PWM = 0 tối thiểu 800ms (ở chế độ thường) để triệt tiêu xung dòng khởi động ngược
  static int lastMotorDir = 0; // 1: Tiến, -1: Lùi, 0: Dừng
  static unsigned long reversalHoldUntilMs = 0;
  int newMotorDir = (speedPWM >= 8) ? 1 : ((speedPWM <= -8) ? -1 : 0);
  if (lastMotorDir != 0 && newMotorDir != 0 && lastMotorDir != newMotorDir) {
    writeMotorPWM(0);
    digitalWrite(PIN_MOTOR_IN1, LOW);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    delayMicroseconds(50);
    if (jamState == JAM_NORMAL) {
      reversalHoldUntilMs = millis() + 800UL;
    }
  }
  if (newMotorDir != 0) lastMotorDir = newMotorDir;
  if (jamState == JAM_NORMAL && millis() < reversalHoldUntilMs) {
    writeMotorPWM(0);
    digitalWrite(PIN_MOTOR_IN1, LOW);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    return;
  }

  // Bù vùng chết áp rơi trên Transistor cầu H L298N (~1.6V -> PWM tối thiểu 62 để động cơ bắt đầu quay mượt từ khi nhích Joystick)
  int absOut = 0;
  if (abs(speedPWM) >= 8) {
    absOut = map(abs(speedPWM), 8, 255, 62, 255);
  }

#if USE_ARDUINO_MOTOR_SHIELD == 0
  // ĐIỀU KHIỂN MẠCH CẦU H L298N (ENA = GPIO 16, IN1 = GPIO 40, IN2 = GPIO 12)
  if (speedPWM >= 8) {
    digitalWrite(PIN_MOTOR_IN1, HIGH);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    writeMotorPWM(absOut);
  } else if (speedPWM <= -8) {
    digitalWrite(PIN_MOTOR_IN1, LOW);
    digitalWrite(PIN_MOTOR_IN2, HIGH);
    writeMotorPWM(absOut);
  } else {
    digitalWrite(PIN_MOTOR_IN1, LOW);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    writeMotorPWM(0);
  }
#else
  if (speedPWM >= 8) {
    digitalWrite(PIN_MOTOR_IN2, LOW);
    digitalWrite(PIN_MOTOR_IN1, HIGH);
    writeMotorPWM(absOut);
  } else if (speedPWM <= -8) {
    digitalWrite(PIN_MOTOR_IN2, LOW);
    digitalWrite(PIN_MOTOR_IN1, LOW);
    writeMotorPWM(absOut);
  } else {
    digitalWrite(PIN_MOTOR_IN2, HIGH);
    writeMotorPWM(0);
  }
#endif
}

void updateMotorSoftStart() {
  if (jamState != JAM_NORMAL) return;

  // Ngắt động cơ an toàn nếu phao lật (> 60 độ sau khi đã trừ góc Tare)
  if (abs(valRoll) > 60.0f || abs(valPitch) > 60.0f) {
    curMotor = 0;
    driveMotor(0);
    return;
  }

  if (millis() - lastSoftStartTick >= 20) {
    lastSoftStartTick = millis();
    // BẢO VỆ CHỐNG QUÁ NHIỆT CẦU H L298N (Dynamic Current Derating):
    // Nếu dòng điện duy trì > 2.1A (75% ngưỡng kẹt) liên tục quá 1.5s, tự động giảm 20% PWM để giải nhiệt
    static unsigned long highCurrentStartMs = 0;
    int targetMotor = cmdMotor;
    if (acs712Valid && valAmp > (CURRENT_JAM_LIMIT_A * 0.75f)) {
      if (highCurrentStartMs == 0) highCurrentStartMs = millis();
      else if (millis() - highCurrentStartMs >= 1500UL) {
        targetMotor = (int)(cmdMotor * 0.80f);
      }
    } else {
      highCurrentStartMs = 0;
    }

    if (targetMotor == 0) {
      // Giảm tốc mượt mà về 0 (200ms) để triệt tiêu dòng phản kháng (Back-EMF) từ cuộn cảm động cơ
      if (curMotor > 0) curMotor = max(0, curMotor - 25);
      else if (curMotor < 0) curMotor = min(0, curMotor + 25);
    } else if (curMotor < targetMotor) {
      curMotor = min(targetMotor, curMotor + 15);
    } else if (curMotor > targetMotor) {
      curMotor = max(targetMotor, curMotor - 15);
    }
    driveMotor(curMotor);
  }
}

void driveWinchVariable(int action, int speedDelta = 45) {
  // 1. Tier 3: Cắt tải tời khẩn cấp khi pin kiệt
  if (batValid && valVolt3S > 1.0f && (calcBatteryCellVolt(valVolt3S) < BAT_CELL_TIER3_CUTOFF_V || batTier3CutoffActive)) {
    winchServo.write(90);
    winchMode = 0;
    return;
  }
  speedDelta = constrain(speedDelta, 12, 60);
  if (action == 1) {
    // 2. An toàn góc nghiêng: Không hạ tời khi thuyền nghiêng lớn (>40°) tránh quấn cáp vào chân vịt
    if (abs(valRoll) > 40.0f || abs(valPitch) > 40.0f) {
      winchServo.write(90);
      winchMode = 0;
      Serial.println(F("[USV-WINCH] Tu choi ha toi: Thuyen dang nghieng > 40 do!"));
      return;
    }
    // 3. Giới hạn độ sâu an toàn 2.40m: Chống tời hết dây làm đứt/đảo chiều cuộn cáp
    if (winchDepthM >= 2.40f) {
      winchServo.write(90);
      winchMode = 0;
      Serial.println(F("[USV-WINCH] Da dat do sau toi da 2.40m! Tu dong dung ha toi."));
      return;
    }
    if (mudBottomLatched) {
      winchServo.write(90);
      winchMode = 0;
      return;
    }
    if (winchMode != 1) winchStartTick = millis();
    winchMode = 1;
    winchServo.write(90 + speedDelta); // Nhả cáp hạ lồng
  } else if (action == 2) {
    mudBottomLatched = false;
    unsigned long now = millis();
    // 4. Giới hạn thu tời kép (Dual-Redundancy Home Stop): Dừng khi CT2 chạm hoặc khi độ sâu về 0.0m (chống đứt dây CT2)
    if (digitalRead(PIN_CT2_TOP_HOME) == LOW || (winchDepthM <= 0.02f && (now - winchStartTick >= 1500UL))) {
      winchServo.write(90);
      winchMode   = 0;
      winchDepthM = 0.00f;
      return;
    }
    if (winchMode != 2) winchStartTick = now;
    winchMode = 2;
    winchServo.write(90 - speedDelta); // Cuốn cáp thu lồng
  } else {
    winchMode = 0;
    winchServo.write(90);
  }
}

void driveWinch(int action) {
  driveWinchVariable(action, 45);
}


// =========================================================================
// 4b. DRIVER CẢM BIẾN ĐỘ SÂU THỦY TĨNH MS5837 (0x76) & ENCODER AS5600 (0x36)
// =========================================================================
void autoDetectDepthSensors() {
  // 1. Kiểm tra cảm biến áp suất thủy tĩnh MS5837-02BA / MS5837-30BA
  Wire.beginTransmission(MS5837_I2C_ADDR);
  if (Wire.endTransmission(true) == 0) {
    Wire.beginTransmission(MS5837_I2C_ADDR);
    Wire.write(0x1E); // Reset MS5837
    Wire.endTransmission(true);
    delay(10);
    bool promOk = true;
    for (uint8_t i = 0; i < 7; i++) {
      Wire.beginTransmission(MS5837_I2C_ADDR);
      Wire.write(0xA0 + (i * 2));
      if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)MS5837_I2C_ADDR, (size_t)2, true) == 2) {
        ms5837_C[i] = (Wire.read() << 8) | Wire.read();
      } else {
        promOk = false;
        break;
      }
    }
    if (promOk && ms5837_C[1] != 0 && ms5837_C[2] != 0) {
      hasMS5837 = true;
      Serial.println(F("[USV-DEPTH] Phat hien cam bien ap suat thuy tinh MS5837 (0x76)! Chuyen sang DO DO SAU THAT 100%."));
    }
  }

  // 2. Kiểm tra Encoder từ tính AS5600 đếm vòng rulo tời
  Wire.beginTransmission(AS5600_I2C_ADDR);
  if (Wire.endTransmission(true) == 0) {
    Wire.beginTransmission(AS5600_I2C_ADDR);
    Wire.write(0x0E);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_I2C_ADDR, (size_t)2, true) == 2) {
      as5600LastRaw = ((Wire.read() & 0x0F) << 8) | Wire.read();
      hasAS5600 = true;
      as5600TotalTicks = 0;
      Serial.println(F("[USV-DEPTH] Phat hien Encoder tu tinh AS5600 (0x36) tren rulo toi! Chuyen sang DEM VONG THAT."));
    }
  }
}

void updateMS5837() {
  unsigned long now = millis();
  if (ms5837Step == 0 && (now - ms5837LastReadMs >= 50)) {
    Wire.beginTransmission(MS5837_I2C_ADDR);
    Wire.write(0x48); // D1 conversion (OSR 4096)
    if (Wire.endTransmission(true) == 0) {
      ms5837Step = 1;
      ms5837StepTick = now;
    }
  } else if (ms5837Step == 1 && (now - ms5837StepTick >= 10)) {
    Wire.beginTransmission(MS5837_I2C_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)MS5837_I2C_ADDR, (size_t)3, true) == 3) {
      ms5837D1 = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
      Wire.beginTransmission(MS5837_I2C_ADDR);
      Wire.write(0x58); // D2 conversion (OSR 4096)
      if (Wire.endTransmission(true) == 0) {
        ms5837Step = 2;
        ms5837StepTick = now;
      } else {
        ms5837Step = 0;
      }
    } else {
      ms5837Step = 0;
    }
  } else if (ms5837Step == 2 && (now - ms5837StepTick >= 10)) {
    Wire.beginTransmission(MS5837_I2C_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)MS5837_I2C_ADDR, (size_t)3, true) == 3) {
      ms5837D2 = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
      int32_t dT = (int32_t)ms5837D2 - ((int32_t)ms5837_C[5] * 256);
      int32_t temp = 2000 + (int64_t)dT * ms5837_C[6] / 8388608;

      int64_t off = (int64_t)ms5837_C[2] * 65536 + ((int64_t)ms5837_C[4] * dT) / 128;
      int64_t sens = (int64_t)ms5837_C[1] * 32768 + ((int64_t)ms5837_C[3] * dT) / 256;

      if (temp < 2000) {
        int64_t tDiff = temp - 2000;
        int64_t ti = (3 * (int64_t)dT * dT) / 8589934592LL;
        int64_t offi = (3 * tDiff * tDiff) / 2;
        int64_t sensi = (5 * tDiff * tDiff) / 8;
        if (temp < -1500) {
          int64_t tDiff2 = temp + 1500;
          offi += 7 * tDiff2 * tDiff2;
          sensi += 4 * tDiff2 * tDiff2;
        }
        temp -= ti;
        off -= offi;
        sens -= sensi;
      }

      int32_t press = (((int64_t)ms5837D1 * sens) / 2097152 - off) / 32768;
      ms5837PressureMbar = press / 100.0f;
      ms5837TempC = temp / 100.0f;
      if (!ms5837Tared && ms5837PressureMbar > 500.0f) {
        ms5837SurfaceMbar = ms5837PressureMbar;
        ms5837Tared = true;
      }
    }
    ms5837Step = 0;
    ms5837LastReadMs = now;
  }
}

void updateAS5600() {
  static unsigned long lastAs5600ReadMs = 0;
  unsigned long now = millis();
  if (now - lastAs5600ReadMs < 20) return;
  lastAs5600ReadMs = now;

  Wire.beginTransmission(AS5600_I2C_ADDR);
  Wire.write(0x0E);
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_I2C_ADDR, (size_t)2, true) == 2) {
    uint16_t curAngle = ((Wire.read() & 0x0F) << 8) | Wire.read();
    int16_t diff = (int16_t)curAngle - (int16_t)as5600LastRaw;
    if (diff > 2048) diff -= 4096;
    else if (diff < -2048) diff += 4096;
    if (winchMode == 1 && diff < 0) diff = -diff;
    if (winchMode == 2 && diff > 0) diff = -diff;
    as5600TotalTicks += diff;
    if (as5600TotalTicks < 0) as5600TotalTicks = 0;
    as5600LastRaw = curAngle;
  }
}

void updateWinchDepthEstimate() {
  unsigned long now = millis();
  if (lastDepthCalcMs == 0) {
    lastDepthCalcMs = now;
    return;
  }
  float dtSec = (now - lastDepthCalcMs) * 0.001f;
  lastDepthCalcMs = now;

  bool atHomeSwitch = (digitalRead(PIN_CT2_TOP_HOME) == LOW);

  // 1. Tự động Tare điểm 0 khi chạm công tắc Home trên mặt nước (CT2)
  if (atHomeSwitch) {
    if (hasMS5837 && ms5837PressureMbar > 500.0f) {
      ms5837SurfaceMbar = ms5837PressureMbar;
      ms5837Tared = true;
    }
    if (hasAS5600) {
      as5600TotalTicks = 0;
    }
    if (winchMode == 2 || winchMode == 0) {
      winchDepthM = 0.00f;
    }
  }

  // 2. ƯU TIÊN 1: CẢM BIẾN ÁP SUẤT THỦY TĨNH MS5837 (ĐỘ SÂU THẬT 100%)
  if (hasMS5837 && ms5837Tared) {
    float deltaP = ms5837PressureMbar - ms5837SurfaceMbar;
    ms5837DepthM = (deltaP > 0.0f) ? constrain(deltaP * 0.01022f, 0.00f, 3.50f) : 0.00f;
    winchDepthM = ms5837DepthM;
    flags2 |= FLAG2_DEPTH_REAL_MS5837;
    flags2 &= ~(FLAG2_DEPTH_REAL_ENCODER | FLAG2_DEPTH_ESTIMATED);
    return;
  }

  // 3. ƯU TIÊN 2: ENCODER TỪ TÍNH AS5600 ĐẾM VÒNG RULO TỜI (ĐỘ SÂU THẬT 100%)
  if (hasAS5600) {
    float turns = (float)as5600TotalTicks / 4096.0f;
    as5600DepthM = constrain(turns * WINCH_SPOOL_CIRCUM_M, 0.00f, 3.50f);
    winchDepthM = as5600DepthM;
    flags2 |= FLAG2_DEPTH_REAL_ENCODER;
    flags2 &= ~(FLAG2_DEPTH_REAL_MS5837 | FLAG2_DEPTH_ESTIMATED);
    return;
  }

  // 4. ƯU TIÊN 3: DỰ PHÒNG ƯỚC LƯỢNG THỜI GIAN (BÙ ĐIỆN ÁP PIN 3S)
  flags2 |= FLAG2_DEPTH_ESTIMATED;
  flags2 &= ~(FLAG2_DEPTH_REAL_MS5837 | FLAG2_DEPTH_REAL_ENCODER);

  if (atHomeSwitch && winchMode == 2) {
    winchDepthM = 0.00f;
    return;
  }
  float vFactor = (valVolt3S >= 8.0f) ? constrain(valVolt3S / 11.8f, 0.80f, 1.15f) : 1.0f;
  if (winchMode == 1) {
    winchDepthM = constrain(winchDepthM + 0.22f * vFactor * dtSec, 0.00f, 2.50f);
  } else if (winchMode == 2) {
    winchDepthM = constrain(winchDepthM - 0.24f * vFactor * dtSec, 0.00f, 2.50f);
  }
}

unsigned long getVoltageCompensatedDuration(unsigned long nominalMs) {
  if (valVolt3S < 8.0f) return nominalMs;
  float factor = constrain(11.8f / valVolt3S, 0.88f, 1.22f);
  return (unsigned long)(nominalMs * factor);
}

// =========================================================================
// 5. GIẢI CỨU BUS I2C, KIỂM TRA TRỞ KHÁNG CHÂN ADC & LỌC KALMAN 1D
// =========================================================================
void recoverI2CBus9Clocks(int sda, int scl) {
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, OUTPUT);
  for (int i = 0; i < 9; i++) {
    if (digitalRead(sda) == HIGH) break;
    digitalWrite(scl, LOW);
    delayMicroseconds(10);
    digitalWrite(scl, HIGH);
    delayMicroseconds(10);
  }
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);
  delayMicroseconds(10);
  digitalWrite(scl, HIGH);
  delayMicroseconds(10);
  digitalWrite(sda, HIGH);
  delayMicroseconds(10);
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
}

// Đọc điện áp mV đã hiệu chuẩn eFuse nhà máy kèm lọc trung vị cắt biên (Trimmed Mean)
float readMilliVoltsFiltered(int pin, int samples = 9) {
  uint32_t buf[15];
  if (samples > 15) samples = 15;
  for (int i = 0; i < samples; i++) {
    buf[i] = analogReadMilliVolts(pin);
    delayMicroseconds(50);
  }
  for (int i = 0; i < samples - 1; i++) {
    for (int j = i + 1; j < samples; j++) {
      if (buf[i] > buf[j]) {
        uint32_t tmp = buf[i];
        buf[i] = buf[j];
        buf[j] = tmp;
      }
    }
  }
  uint32_t sum = 0;
  int count = 0;
  int trim = (samples >= 7) ? 2 : 0;
  for (int i = trim; i < samples - trim; i++) {
    sum += buf[i];
    count++;
  }
  return (float)sum / (float)max(1, count);
}

// Kiểm tra trở kháng và phân biệt chính xác 100% giữa CHÂN HỞ MẠCH (CHƯA CẮM) và CẢM BIẾN THẬT:
// - Cảm biến TDS khi để khô ngoài không khí hoặc trong nước siêu tinh khiết có điện áp cực thấp (0..15mV).
//   Nếu kiểm tra bằng trở xả thông thường sẽ bị nhầm là hở mạch -> Báo "CHƯA CẮM" giả tạo!
// - Bằng phương pháp xung trở kéo tích cực (Active Impedance Test):
//   + Khi chân hở mạch (không cắm dây): Điện dung ký sinh chỉ ~10pF, khi bật trở kéo Pull-up 45k áp vọt ngay lên >2000mV.
//   + Khi cắm cảm biến thật (TDS, pH, ACS, Pin, NTU): Trở kháng ngõ ra của Op-amp hoặc cầu phân áp rất thấp,
//     giữ điện áp ổn định (độ lệch < 350mV), lập tức nhận diện là ĐÃ CẮM!
//   + Sau khi kiểm tra, tắt toàn bộ trở kéo (High-Z) để đảm bảo độ chính xác đo lường tuyệt đối 100%.
bool readAdcWithFloatingCheck(int pin, float &outMv, AdcDiag *dg = nullptr) {
  gpio_num_t gpin = (gpio_num_t)pin;
  int8_t chIdx = digitalPinToAnalogChannel(pin);
  adc1_channel_t ch = (adc1_channel_t)((chIdx >= 0) ? chIdx : 0);

  // 1. Luôn ngắt mọi trở kéo nội ở trạng thái bình thường để đo High-Z trung thực 100%
  adcAttachPin(pin);
  rtc_gpio_pullup_dis(gpin);
  rtc_gpio_pulldown_dis(gpin);
  gpio_pullup_dis(gpin);
  gpio_pulldown_dis(gpin);
  delayMicroseconds(150);

  // Đọc bỏ 2 mẫu mồi (Dummy Reads) để xả sạch điện tích dư trên tụ S&H của SAR ADC1 từ kênh trước (chống nhảy số chéo kênh!)
  (void)analogReadMilliVolts(pin);
  delayMicroseconds(80);
  (void)analogReadMilliVolts(pin);
  delayMicroseconds(80);

  // Đo 10 mẫu High-Z tự nhiên kèm loại bỏ nhiễu biên (Trimmed Mean)
  uint32_t buf[10];
  for (int i = 0; i < 10; i++) {
    buf[i] = analogReadMilliVolts(pin);
    delayMicroseconds(150);
  }
  uint16_t raw_hz = (chIdx >= 0) ? (uint16_t)adc1_get_raw(ch) : (uint16_t)analogRead(pin);

  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buf[i] > buf[j]) {
        uint32_t tmp = buf[i]; buf[i] = buf[j]; buf[j] = tmp;
      }
    }
  }
  uint32_t sum = 0;
  for (int i = 2; i < 8; i++) sum += buf[i];
  float natMv = (float)sum / 6.0f;
  uint32_t trimmedSpan = buf[7] - buf[2];

  // 2. Kiểm tra trở kháng xung siêu ngắn (25us) trực tiếp trên thanh ghi RTC/GPIO + adc1_get_raw(ch):
  // - Tuyệt đối KHÔNG gọi pinMode(INPUT_PULLUP/INPUT_PULLDOWN) vì sẽ làm xả tụ lọc RC của cảm biến (NTU, TDS, ACS, BAT)!
  // - Đọc trực tiếp adc1_get_raw(ch) khi đang bật trở kéo (vì analogReadMilliVolts gọi __adcAttachPin sẽ tự tắt trở kéo trước khi đọc!).
  uint16_t raw_pu = raw_hz;
  uint16_t raw_pd = raw_hz;
  if (chIdx >= 0) {
    rtc_gpio_pullup_en(gpin);
    gpio_pullup_en(gpin);
    delayMicroseconds(45);
    raw_pu = (uint16_t)adc1_get_raw(ch);
    rtc_gpio_pullup_dis(gpin);
    gpio_pullup_dis(gpin);

    rtc_gpio_pulldown_en(gpin);
    gpio_pulldown_en(gpin);
    delayMicroseconds(45);
    raw_pd = (uint16_t)adc1_get_raw(ch);
    rtc_gpio_pulldown_dis(gpin);
    gpio_pulldown_dis(gpin);
    delayMicroseconds(80);
  }

  bool isConnected = true;
  uint16_t rawSwing = (raw_pu > raw_pd) ? (raw_pu - raw_pd) : 0;
  uint16_t probeSwing = (uint16_t)((rawSwing * 3100UL) / 4095UL);
  uint8_t fault = ADC_OK;

  if (natMv < 90.0f && raw_pu >= 2200) {
    isConnected = false;
    fault = ADC_OPEN;
  } else if (natMv >= 3050.0f && raw_pd < 800) {
    isConnected = false;
    fault = ADC_OPEN;
  } else if (raw_pu >= 3400 && raw_pd <= 600 && probeSwing >= 1800) {
    isConnected = false;
    fault = ADC_OPEN;
  } else if (natMv >= 3060.0f || raw_hz >= 4040) {
    fault = ADC_SATURATED;
  } else if (trimmedSpan > 280) {
    fault = ADC_NOISY;
  }

  if (dg) {
    dg->mv    = (uint16_t)natMv;
    dg->swing = probeSwing;
    dg->span  = (uint16_t)trimmedSpan;
    dg->raw   = raw_hz;
    dg->fault = fault;
  }

  if (!isConnected) {
    outMv = 0.0f;
    return false;
  }

  outMv = natMv;
  return true;
}

// Bộ lọc cửa sổ trượt 8 mẫu có trễ chống nhấp nháy ĐÃ CẮM <-> CHƯA CẮM:
// Chuyển sang ĐÃ CẮM khi >= 5/8 mẫu OK, chuyển về CHƯA CẮM khi <= 2/8 mẫu OK.
bool updateAdcPresence(AdcDiag &d, bool sampleOk) {
  d.hist = ((d.hist << 1) | (sampleOk ? 1 : 0)) & 0xFF;
  int okCnt = __builtin_popcount(d.hist);
  if (!d.valid && okCnt >= 5)      d.valid = true;
  else if (d.valid && okCnt <= 2)  d.valid = false;
  return d.valid;
}

const char* adcStateText(const AdcDiag &d) {
  if (d.valid) return "OK";
  if (d.fault == ADC_NOISY) return "NHIEU";
  if (d.fault == ADC_SATURATED) return "VUOT_DAI_ADC";
  return "CHUA_CAM";
}

void saveWaterLayerSample(WaterLayerSample &slot, const char* nvsKeyPrefix = nullptr) {
  portENTER_CRITICAL(&dataMux);
  slot.ph    = valPH;
  slot.tds   = valTDS;
  slot.ntu   = valNTU;
  slot.temp  = valTemp;
  slot.doMgL = valDO;
  slot.valid = true;
  portEXIT_CRITICAL(&dataMux);

  if (nvsKeyPrefix) {
    char k[16];
    snprintf(k, sizeof(k), "%s_ph", nvsKeyPrefix);
    nvsPrefs.putFloat(k, slot.ph);
    snprintf(k, sizeof(k), "%s_tds", nvsKeyPrefix);
    nvsPrefs.putFloat(k, slot.tds);
    snprintf(k, sizeof(k), "%s_ntu", nvsKeyPrefix);
    nvsPrefs.putFloat(k, slot.ntu);
    snprintf(k, sizeof(k), "%s_tmp", nvsKeyPrefix);
    nvsPrefs.putFloat(k, slot.temp);
    snprintf(k, sizeof(k), "%s_do", nvsKeyPrefix);
    nvsPrefs.putFloat(k, slot.doMgL);
    snprintf(k, sizeof(k), "%s_ok", nvsKeyPrefix);
    nvsPrefs.putBool(k, true);
  }
}

// Nạp lại 1 tầng nước đã lưu trong NVS. Chỉ coi là hợp lệ khi có đủ khóa "_ok" và "_ph".
// Tuyệt đối không tạo số liệu ảo nếu người dùng chưa từng đo.
bool loadWaterLayerSample(WaterLayerSample &slot, const char* nvsKeyPrefix) {
  char k[16];
  snprintf(k, sizeof(k), "%s_ok", nvsKeyPrefix);
  if (!nvsPrefs.getBool(k, false)) {
    slot = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false};
    return false;
  }
  snprintf(k, sizeof(k), "%s_ph", nvsKeyPrefix);  slot.ph    = nvsPrefs.getFloat(k, 0.0f);
  snprintf(k, sizeof(k), "%s_tds", nvsKeyPrefix); slot.tds   = nvsPrefs.getFloat(k, 0.0f);
  snprintf(k, sizeof(k), "%s_ntu", nvsKeyPrefix); slot.ntu   = nvsPrefs.getFloat(k, 0.0f);
  snprintf(k, sizeof(k), "%s_tmp", nvsKeyPrefix); slot.temp  = nvsPrefs.getFloat(k, 0.0f);
  snprintf(k, sizeof(k), "%s_do", nvsKeyPrefix);  slot.doMgL = nvsPrefs.getFloat(k, 0.0f);
  slot.valid = (slot.ph > 0.0f || slot.tds > 0.0f || slot.ntu > 0.0f || slot.temp > 0.0f);
  return slot.valid;
}


bool wakeMPU6050At(uint8_t addr, uint8_t* outWhoAmI = nullptr) {
  Wire.beginTransmission(addr);
  if (Wire.endTransmission(true) != 0) return false;

  Wire.beginTransmission(addr);
  Wire.write(0x75);
  if (Wire.endTransmission(true) != 0) return false;
  if (Wire.requestFrom(addr, (size_t)1, true) != 1) return false;
  uint8_t who = Wire.read();
  if (who == 0x00 || who == 0xFF) return false;
  if (outWhoAmI) *outWhoAmI = who;

  Wire.beginTransmission(addr);
  Wire.write(0x6B);
  Wire.write(0x00);
  if (Wire.endTransmission(true) != 0) return false;

  Wire.beginTransmission(addr);
  Wire.write(0x1A);
  Wire.write(0x03);
  Wire.endTransmission(true);

  Wire.beginTransmission(addr);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  Wire.beginTransmission(addr);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission(true);
  return true;
}

void autoDetectAndInitCompass();

void scanAndRecoverI2C(bool fullBootScan = false) {
  (void)fullBootScan;
  // CHỈ quét và phục hồi chân I2C chuyên dụng (GPIO 1 SDA, GPIO 2 SCL).
  // Tuyệt đối không chạm vào GPIO 17, 18 (GPS UART1) hay GPIO 41, 42 (SIM 4G UART2)!
  const int bootPairs[][2] = {
    {I2C_SDA_PIN, I2C_SCL_PIN},
    {I2C_SCL_PIN, I2C_SDA_PIN}
  };
  const int numPairs = sizeof(bootPairs) / sizeof(bootPairs[0]);
  uint8_t testAddrs[2] = {0x68, 0x69};

  for (int p = 0; p < numPairs; p++) {
    int sda = bootPairs[p][0];
    int scl = bootPairs[p][1];
    Wire.end();
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(200);
    if (digitalRead(sda) == LOW && digitalRead(scl) == HIGH) {
      recoverI2CBus9Clocks(sda, scl);
    }
    if (digitalRead(sda) == LOW || digitalRead(scl) == LOW) continue;

    Wire.begin(sda, scl, 100000);
    Wire.setTimeOut(15);

    for (int i = 0; i < 2; i++) {
      uint8_t who = 0;
      if (wakeMPU6050At(testAddrs[i], &who)) {
        hasMPU6050    = true;
        mpuAddr       = testAddrs[i];
        curSdaPin     = sda;
        curSclPin     = scl;
        lastMpuReadUs = micros();
        Serial.printf("[USV-I2C] TIM THAY MPU6050 (WHO_AM_I=0x%02X) tai 0x%02X tren SDA=%d, SCL=%d!\n",
                      who, mpuAddr, curSdaPin, curSclPin);
        break;
      }
    }
    if (hasMPU6050) break;
  }

  if (!hasMPU6050) {
    curSdaPin = I2C_SDA_PIN;
    curSclPin = I2C_SCL_PIN;
    Wire.end();
    pinMode(curSdaPin, INPUT_PULLUP);
    pinMode(curSclPin, INPUT_PULLUP);
    Wire.begin(curSdaPin, curSclPin, 100000);
    Wire.setTimeOut(15);
  }

  autoDetectAndInitCompass();
}

// Chẩn đoán từng linh kiện phần cứng thực tế 100% (11 bit độc lập - Tuyệt đối không gộp ảo!)
void autoDetectAndInitCompass() {
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

void updateHardwareSelfDiagnosis() {
  uint16_t mask = 0;
  if (ds18b20Valid)   mask |= HW_OK_DS18B20; // Bit 0: DS18B20 (GPIO 6)
  if (phValid)        mask |= HW_OK_PH;      // Bit 1: Cảm biến pH (GPIO 7)
  if (tdsValid)       mask |= HW_OK_TDS;     // Bit 2: Cảm biến TDS (GPIO 4)
  if (ntuValid)       mask |= HW_OK_NTU;     // Bit 3: Cảm biến Độ đục NTU (GPIO 5)
  if (acs712Valid)    mask |= HW_OK_ACS712;  // Bit 4: Cảm biến dòng ACS712 (GPIO 8)
  if (batValid)       mask |= HW_OK_BAT3S;   // Bit 5: Đo áp Pin 3S (GPIO 3)
  if (hasMPU6050)     mask |= HW_OK_MPU6050; // Bit 6: MPU6050 (I2C 0x68)
  if (hasGY271)       mask |= HW_OK_GY271;   // Bit 7: La bàn số GY-271 (I2C 0x0D)
  if (loraReady)      mask |= HW_OK_LORA;    // Bit 8: LoRa Ra-02 (SPI NSS=15)
  if (simModemOnline) mask |= HW_OK_SIM4G;   // Bit 9: SIM 4G A7680C (UART2)
  if (hasGPS)         mask |= HW_OK_GPS;     // Bit 10: GPS NEO-6M (UART1)
  hwHealthMask = mask;

  // Mặt nạ kênh ADC lỗi bão hòa kẹp trần (CHỈ KHI CẢM BIẾN ĐANG CẮM mà bị bão hòa vượt ngưỡng thực)
  // Lưu ý: Mạch PH-4502C cấp nguồn 5V có điểm trung tính pH 7.00 tự nhiên ở ~3084mV (raw ~4070),
  // chỉ đánh dấu lỗi bão hòa pH khi kẹp cứng sát trần tuyệt đối (raw >= 4093 hoặc pH lệch ngoài dải).
  uint16_t afMask = 0;
  if (phValid && diagPH.fault == ADC_SATURATED && (diagPH.raw >= 4093 || valPH < 3.5f || valPH > 11.5f)) afMask |= HW_OK_PH;
  if (tdsValid && diagTDS.fault == ADC_SATURATED) afMask |= HW_OK_TDS;
  if (ntuValid && diagNTU.fault == ADC_SATURATED) afMask |= HW_OK_NTU;
  if (acs712Valid && diagACS.fault == ADC_SATURATED) afMask |= HW_OK_ACS712;
  if (batValid && valVolt3S > 13.0f && diagBAT.fault == ADC_SATURATED) afMask |= HW_OK_BAT3S;
  adcFaultMask = afMask;

  // Định kỳ 5 giây in báo cáo chi tiết từng linh kiện ĐÃ CẮM và CHƯA CẮM ra Serial
  if (millis() - lastDiagPrintMs >= 5000) {
    lastDiagPrintMs = millis();
    Serial.printf("[USV-HW-CHECK] Mask=0x%03X | DS18B20:%s | pH:%s | TDS:%s | NTU:%s | ACS712:%s | PIN3S:%s(%.2fV) | MPU6050:%s | GY271:%s | LoRa:%s | SIM4G:%s | GPS:%s\n",
                  (unsigned int)mask,
                  ds18b20Valid   ? "OK" : "CHUA_CAM",
                  phValid        ? "OK" : "CHUA_CAM",
                  tdsValid       ? "OK" : "CHUA_CAM",
                  ntuValid       ? "OK" : "CHUA_CAM",
                  acs712Valid    ? "OK" : "CHUA_CAM",
                  batValid       ? "OK" : "CHUA_CAM", valVolt3S,
                  hasMPU6050     ? "OK" : "CHUA_CAM",
                  hasGY271       ? "OK" : "CHUA_CAM",
                  loraReady      ? "OK" : "LOI",
                  simModemOnline ? "OK" : "CHUA_CAM",
                  hasGPS         ? "OK" : "CHUA_CAM");
    // Số liệu thô từng chân ADC để kiểm chứng (mV trung vị / dp=PU_PD / độ lệch thử kéo sw / raw 12-bit / số mẫu OK trong 8)
    Serial.printf("[USV-ADC-DIAG] pH(G7):%umV dp%02u sw%u raw%u ok%d/8 | TDS(G4):%umV dp%02u sw%u raw%u ok%d/8 | NTU(G5):%umV dp%02u sw%u raw%u ok%d/8 | ACS(G8):%umV dp%02u sw%u raw%u ok%d/8 | BAT(G3):%umV dp%02u sw%u raw%u ok%d/8 x%.3f\n",
                  diagPH.mv,  diagPH.span,  diagPH.swing,  diagPH.raw,  __builtin_popcount(diagPH.hist),
                  diagTDS.mv, diagTDS.span, diagTDS.swing, diagTDS.raw, __builtin_popcount(diagTDS.hist),
                  diagNTU.mv, diagNTU.span, diagNTU.swing, diagNTU.raw, __builtin_popcount(diagNTU.hist),
                  diagACS.mv, diagACS.span, diagACS.swing, diagACS.raw, __builtin_popcount(diagACS.hist),
                  diagBAT.mv, diagBAT.span, diagBAT.swing, diagBAT.raw, __builtin_popcount(diagBAT.hist),
                  batDividerRatio);
    if (phValid && diagPH.mv >= 3050) {
      Serial.printf("[USV-PH-DIAG] Cam bien pH (GPIO 7) DA KET NOI (%umV, Vref7=%.2fV -> pH=%.2f). Neu den do thu 2 tren mach PH-4502C dang sang (ap Po > 3.08V), hay xoay nhe bien tro xanh sat giac BNC nguoc chieu kim dong ho ~3-5 vong de ha Po ve ~2500mV!\n",
                    diagPH.mv, phRefVolt7, valPH);
    }
    const char* batDiagTxt = "OK (DANG DO PIN CHUAN)";
    if (diagBAT.fault == ADC_SATURATED) batDiagTxt = "KEP TRAN ADC (>3.05V) -> Kiem tra: da cam qua mach chia ap chua? Chan '+' de trong chua? Day '-' da noi GND chua?";
    else if (!batValid) batDiagTxt = "CHUA CAM PIN (0V) -> Kiem tra: Pin da noi vao coc VCC/GND cua mach chia ap chua?";
    Serial.printf("[USV-BAT-DIAG] Pin GPIO 3: %umV (raw: %u) | He so: %.2f | %s\n",
                  diagBAT.mv, diagBAT.raw, batDividerRatio, batDiagTxt);
  }
}

void updateAllSensors() {
  unsigned long now = millis();

  // 1. Nhiệt độ nước DS18B20 BẤT ĐỒNG BỘ (GPIO 6) — Nếu không thấy cảm biến -> Báo CHƯA CẮM (0.0°C)
  if (now - lastTempReqTick >= 750) {
    float t = ds18b20.getTempCByIndex(0);
    if (t > -20.0f && t < 85.0f && t != DEVICE_DISCONNECTED_C) {
      valTemp      = ds18b20Valid ? (valTemp * 0.72f + t * 0.28f) : t;
      ds18b20Valid = true;
    } else {
      ds18b20Valid = false;
      valTemp      = 0.0f; // Không báo nhiệt độ ảo khi rút DS18B20!
    }
    ds18b20.requestTemperatures();
    lastTempReqTick = now;
  }

  // 2. Cảm biến pH (GPIO 7) — Nhận diện chuẩn xác mạch PH-4502C đang cắm (kể cả khi mạch 5V xuất áp ~3.08V)
  float mvPH = 0.0f;
  bool phReadOk = readAdcWithFloatingCheck(PIN_PH, mvPH, &diagPH);
  phValid = updateAdcPresence(diagPH, phReadOk && mvPH >= 120.0f);
  float filtPH = 0.0f;
  if (phValid && mvPH >= 120.0f) {
    float vPH     = mvPH * 0.001f;
    lastRawPhVolt = vPH;
    float slope   = (phSlopeVPerUnit > 0.05f && phSlopeVPerUnit < 0.40f) ? phSlopeVPerUnit : 0.18f;
    // Nếu mạch PH-4502C cấp nguồn 5V chưa vặn biến trở hạ áp (vPH >= 3.05V trong khi phRefVolt7 vẫn ở 2.50V mặc định),
    // tự động lấy điểm cân bằng trung tính của mạch 5V (~3.08V) để không báo sai pH 3.76 (Axit giả)!
    float effRef7 = (vPH >= 3.02f && fabsf(phRefVolt7 - 2.50f) < 0.05f) ? 3.084f : phRefVolt7;
    float rawPH   = constrain(7.0f + ((effRef7 - vPH) / slope) + phCalibOffset, 0.0f, 14.0f);
    filtPH        = kalmanPH.update(rawPH);
  } else {
    kalmanPH.init = false;
    filtPH = 0.0f;
  }

  // 3. Cảm biến TDS bù nhiệt (GPIO 4) — Kiểm tra phần cứng thực tế + cắt nhiễu nền khi để khô
  float mvTDS = 0.0f;
  tdsValid = updateAdcPresence(diagTDS, readAdcWithFloatingCheck(PIN_TDS, mvTDS, &diagTDS));
  float filtTDS = 0.0f;
  float filtEC  = 0.0f;
  if (tdsValid) {
    bool tdsDryHum = (mvTDS < 190.0f && diagTDS.span >= 45);
    if (mvTDS >= 55.0f && !tdsDryHum) {
      float vTDS    = mvTDS * 0.001f;
      float refTemp = ds18b20Valid ? valTemp : 25.0f;
      float vComp   = vTDS / (1.0f + 0.02f * (refTemp - 25.0f));
      float rawTDS  = max(0.0f, (float)((133.42f * pow(vComp, 3) - 255.86f * pow(vComp, 2) + 857.39f * vComp) * 0.5f));
      rawTDS       *= constrain(tdsCalibFactor, 0.50f, 2.00f);
      filtTDS       = kalmanTDS.update(rawTDS);
    } else {
      // Khi đầu dò TDS để khô ngoài không khí (< 35mV) -> TDS = 0 ppm
      filtTDS       = kalmanTDS.update(0.0f);
      if (filtTDS < 1.5f) filtTDS = 0.0f;
    }
    filtEC = calcECuScm(filtTDS);
  } else {
    kalmanTDS.init = false;
    filtTDS = 0.0f;
    filtEC  = 0.0f;
  }

  // 4. Cảm biến Độ đục NTU (GPIO 5) — Chuẩn hóa theo điện áp nền thực tế của module (~2750..2850mV khi trong suốt)
  float mvNTU = 0.0f;
  ntuValid = updateAdcPresence(diagNTU, readAdcWithFloatingCheck(PIN_TURBIDITY, mvNTU, &diagNTU));
  float filtNTU = 0.0f;
  if (ntuValid) {
    if (mvNTU > 0.0f) {
      float rawNTU = 0.0f;
      if (mvNTU >= 2250.0f) {
        // Vùng nước trong / để ngoài không khí sạch (2250..3084mV) -> 0..5 NTU (chống nhảy số 95..220 NTU do gợn nguồn 5V USB)
        rawNTU = (float)map((long)constrain(mvNTU, 2250.0f, 3084.0f), 3084, 2250, 0, 5);
      } else {
        // Vùng nước có độ đục thực tế (480..2250mV) -> 5..1200 NTU
        rawNTU = (float)map((long)constrain(mvNTU, 480.0f, 2250.0f), 2250, 480, 5, 1200) + ntuCalibOffset;
      }
      rawNTU   = max(0.0f, rawNTU);
      filtNTU  = kalmanNTU.update(rawNTU);
    } else {
      filtNTU  = valNTU;
    }
  } else {
    kalmanNTU.init = false;
    filtNTU = 0.0f;
  }

  // 5. Oxy hòa tan ảo (DO) & VN-WQI — Chỉ tính khi có ít nhất 1 cảm biến nước đang cắm
  float calcDO = 0.0f;
  int   wqi    = 0;
  if (ds18b20Valid || phValid || tdsValid || ntuValid) {
    calcDO = estimateVirtualDO(valTemp, filtTDS, filtPH, filtNTU);
    wqi    = calcWaterQualityIndex(filtPH, calcDO, filtTDS, filtNTU, valTemp);
  }

  // 6. Cảm biến dòng ACS712 20A (GPIO 8) — Chốt điểm 0A tức thì ngay mẫu hợp lệ đầu tiên khi motor nghỉ
  float mvACS = 0.0f;
  bool acsSampleOk = readAdcWithFloatingCheck(PIN_ACS712, mvACS, &diagACS) && (mvACS >= 400.0f && mvACS <= 3150.0f);
  acs712Valid = updateAdcPresence(diagACS, acsSampleOk);
  static bool acsZeroInitDone = false;
  static int acsInitSamples = 0;
  static float acsInitSumMv = 0.0f;
  float filtAmp = 0.0f;
  if (acs712Valid) {
    if (acsSampleOk) {
      float vACS = mvACS * 0.001f;
      if (!acsZeroInitDone) {
        if (curMotor == 0 && winchMode == 0 && mvACS >= 1200.0f && mvACS <= 2700.0f) {
          acsInitSumMv += vACS;
          acsInitSamples++;
          if (acsInitSamples >= 20) {
            acsZeroVolt     = acsInitSumMv / 20.0f;
            acsZeroInitDone = true;
            kalmanAmp.reset(0.0f);
            Serial.printf("[USV-ACS] Da chot diem 0A on dinh: %.3f V (20 mau)\n", acsZeroVolt);
          }
        }
      } else {
        if (curMotor == 0 && winchMode == 0) {
          acsZeroVolt = acsZeroVolt * 0.995f + vACS * 0.005f;
        }
      }
      float signedAmp = (vACS - acsZeroVolt) / 0.100f;
      float fSigned   = kalmanAmp.update(signedAmp);
      filtAmp = abs(fSigned);
      float deadzone = (curMotor == 0 && winchMode == 0) ? 0.70f : 0.12f;
      if (filtAmp < deadzone) filtAmp = 0.0f;
    } else {
      filtAmp = valAmp;
    }
  } else {
    acsZeroInitDone = false;
    acsInitSamples  = 0;
    acsInitSumMv    = 0.0f;
    kalmanAmp.init  = false;
    filtAmp         = 0.0f;
  }

  // 7. Đo áp Pin Chính Xác tại GPIO 3 (Hỗ trợ Pin 1S: 2.8..4.35V | Pin 2S: 5.8..8.7V | Pin 3S: 9.0..13.2V)
  // Tuyệt đối không gán số ảo nếu chưa cắm Pin vào cảm biến hoặc chưa cắm dây GPIO 3!
  float mvBat = 0.0f;
  bool batAdcOk = readAdcWithFloatingCheck(PIN_BAT_3S, mvBat, &diagBAT);
  // ĐIỀU KIỆN PIN HỢP LỆ (Hỗ trợ cả Pin 1S, 2S, 3S qua cầu phân áp):
  // 1. Phải có điện áp thực tế >= 65mV (tránh nhầm với áp trôi nổi khi rút pin).
  // 2. Kể cả khi kẹp trần bão hòa (>3.07V khi pin đầy hoặc sạc), vẫn ghi nhận pin đang cắm và đo chính xác!
  bool batSampleOk = batAdcOk && (mvBat >= 65.0f);
  batValid = updateAdcPresence(diagBAT, batSampleOk);
  float filtBat = 0.0f;
  if (batValid) {
    lastRawBatPinV = mvBat * 0.001f;
    float rawBat   = lastRawBatPinV * batDividerRatio;
    filtBat        = kalmanBat.update(rawBat);
  } else {
    lastRawBatPinV = 0.0f;
    kalmanBat.init = false;
    filtBat        = 0.0f;
  }

  sensorsInitialized = true;

  // 8. Đọc 14 thanh ghi MPU6050 (Accel + Die Temp + Gyro) & Bộ lọc Bù 6 Trục (Complementary Filter)
  if (!hasMPU6050 && (now - lastI2cCheckTick >= 2500)) {
    lastI2cCheckTick = now;
    scanAndRecoverI2C();
  }

  float newRoll     = 0.0f;
  float newPitch    = 0.0f;
  float newMpuTemp  = 0.0f;

  if (hasMPU6050) {
    Wire.beginTransmission(mpuAddr);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)mpuAddr, (size_t)14, true) == 14) {
      int16_t ax   = (Wire.read() << 8) | Wire.read();
      int16_t ay   = (Wire.read() << 8) | Wire.read();
      int16_t az   = (Wire.read() << 8) | Wire.read();
      int16_t tRaw = (Wire.read() << 8) | Wire.read();
      int16_t gx   = (Wire.read() << 8) | Wire.read();
      int16_t gy   = (Wire.read() << 8) | Wire.read();
      int16_t gz   = (Wire.read() << 8) | Wire.read();

      if (ax == 0 && ay == 0 && az == 0) {
        wakeMPU6050At(mpuAddr);
      } else {
        unsigned long nowUs = micros();
        float dt = (lastMpuReadUs > 0) ? ((nowUs - lastMpuReadUs) * 1e-6f) : 0.05f;
        lastMpuReadUs = nowUs;
        if (dt <= 0.0f || dt > 0.5f) dt = 0.05f;

        float dieTemp = ((float)tRaw / 340.0f) + 36.53f;
        if (dieTemp > 5.0f && dieTemp < 85.0f) {
          newMpuTemp = (valMpuTemp > 1.0f) ? (valMpuTemp * 0.8f + dieTemp * 0.2f) : dieTemp;
        }

        float absZ     = sqrt((float)az * az + 0.01f * (float)ax * ax);
        float accRoll  = atan2((float)ay, absZ) * 57.29578f;
        float accPitch = atan2((float)(-ax), sqrt((float)ay * ay + (float)az * az)) * 57.29578f;

        float gyroRollRate  = (float)gx / 131.0f;
        float gyroPitchRate = (float)gy / 131.0f;
        float gyroYawRate   = (float)gz / 131.0f;

        rawRollUncomp  = 0.88f * (rawRollUncomp  + gyroRollRate * dt)  + 0.12f * accRoll;
        rawPitchUncomp = 0.88f * (rawPitchUncomp + gyroPitchRate * dt) + 0.12f * accPitch;

        if (!imuBootTareDone) {
          imuWarmupSamples++;
          if (imuWarmupSamples == 1) {
            rawRollUncomp  = accRoll;
            rawPitchUncomp = accPitch;
          } else if (imuWarmupSamples >= 12) {
            imuBootTareDone = true;
            if (rollTareOffset == 0.0f && pitchTareOffset == 0.0f) {
              rollTareOffset  = rawRollUncomp;
              pitchTareOffset = rawPitchUncomp;
            }
          }
        }

        newRoll  = rawRollUncomp  - rollTareOffset;
        newPitch = rawPitchUncomp - pitchTareOffset;
        lastGyroYawRate = gyroYawRate;

        if (!hasGY271 && abs(gyroYawRate) > 0.35f) {
          valHeading += gyroYawRate * dt;
          if (valHeading < 0.0f)    valHeading += 360.0f;
          if (valHeading >= 360.0f) valHeading -= 360.0f;
        }
      }
    } else {
      hasMPU6050 = false;
    }
  }

  // 9. Đọc La bàn số GY-271 thực tế (Tự động hỗ trợ cả QMC5883L 0x0D & HMC5883L 0x1E)
  static unsigned long lastCompassCheckMs = 0;
  if (!hasGY271 && (now - lastCompassCheckMs >= 2000)) {
    lastCompassCheckMs = now;
    autoDetectAndInitCompass();
  }
  static unsigned long lastCompassReadUs = 0;
  if (hasGY271) {
    int16_t mx = 0, my = 0, mz = 0;
    bool magReadOk = false;

    Wire.beginTransmission(gy271Addr);
    if (gy271Addr == 0x0D) {
      Wire.write(0x00);
      if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)0x0D, (size_t)6, true) == 6) {
        mx = (int16_t)(Wire.read() | (Wire.read() << 8));
        my = (int16_t)(Wire.read() | (Wire.read() << 8));
        mz = (int16_t)(Wire.read() | (Wire.read() << 8));
        magReadOk = true;
      }
    } else {
      Wire.write(0x03);
      if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)0x1E, (size_t)6, true) == 6) {
        mx = (int16_t)((Wire.read() << 8) | Wire.read());
        mz = (int16_t)((Wire.read() << 8) | Wire.read());
        my = (int16_t)((Wire.read() << 8) | Wire.read());
        magReadOk = true;
      }
    }

    if (magReadOk && (mx != 0 || my != 0)) {
      unsigned long nowUs = micros();
      float dtMag = (lastCompassReadUs > 0) ? ((nowUs - lastCompassReadUs) * 1e-6f) : 0.05f;
      lastCompassReadUs = nowUs;
      if (dtMag <= 0.0f || dtMag > 0.5f) dtMag = 0.05f;

      // S-09: Hiệu chuẩn Hard-Iron (Offset) & Soft-Iron (Scale) trước khi bù nghiêng
      float cmx = ((float)mx - magOffsetX) * magScaleX;
      float cmy = ((float)my - magOffsetY) * magScaleY;
      float cmz = ((float)mz - magOffsetZ) * magScaleZ;

      float h = 0.0f;
      if (hasMPU6050 && imuBootTareDone) {
        // Bù nghiêng 3D bằng Roll & Pitch từ MPU6050 (Tilt Compensation):
        // Chiếu vector từ trường Trái Đất lên mặt phẳng nằm ngang thực tế
        float phi   = newRoll  * 0.0174532925f; // Roll (rad)
        float theta = newPitch * 0.0174532925f; // Pitch (rad)
        float sinP  = sinf(phi),   cosP = cosf(phi);
        float sinT  = sinf(theta), cosT = cosf(theta);

        float Xh = cmx * cosT + cmy * sinP * sinT + cmz * cosP * sinT;
        float Yh = cmy * cosP - cmz * sinP;
        h = atan2f(Yh, Xh) * 57.29578f;
      } else {
        h = atan2f(cmy, cmx) * 57.29578f;
      }
      if (h < 0.0f)    h += 360.0f;
      if (h >= 360.0f) h -= 360.0f;

      // Bộ lọc bù kết hợp (Complementary Fusion) giữa La bàn GY-271 & Con quay Gyro Yaw Rate:
      // Triệt tiêu xung nhiễu từ trường của motor/tời & bám chuyển động quay tức thì không trễ
      if (valHeading == 0.0f) {
        valHeading = h;
      } else {
        float dH = h - valHeading;
        while (dH > 180.0f)  dH -= 360.0f;
        while (dH < -180.0f) dH += 360.0f;
        float gyroInteg = hasMPU6050 ? (lastGyroYawRate * dtMag) : 0.0f;
        valHeading += 0.15f * dH + gyroInteg;
        if (valHeading < 0.0f)    valHeading += 360.0f;
        if (valHeading >= 360.0f) valHeading -= 360.0f;
      }
    }
  }

  // 11. Đọc Cảm biến áp suất thủy tĩnh MS5837 (I2C 0x76) & Encoder AS5600 (I2C 0x36)
  static unsigned long lastDepthSensorCheckMs = 0;
  if (!hasMS5837 && !hasAS5600 && (now - lastDepthSensorCheckMs >= 3000)) {
    lastDepthSensorCheckMs = now;
    autoDetectDepthSensors();
  }
  if (hasMS5837) {
    updateMS5837();
  }
  if (hasAS5600) {
    updateAS5600();
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
        double rawLat = gps.location.lat();
        double rawLng = gps.location.lng();
        // S-07: Bộ lọc vị trí GPS Median-3 + EMA (alpha = 0.25) chống nhảy tọa độ 2..5m khi giữ Neo Ảo DP
        static double latHist[3] = {0.0, 0.0, 0.0};
        static double lngHist[3] = {0.0, 0.0, 0.0};
        static uint8_t gpsHistCnt = 0;
        latHist[0] = latHist[1]; latHist[1] = latHist[2]; latHist[2] = rawLat;
        lngHist[0] = lngHist[1]; lngHist[1] = lngHist[2]; lngHist[2] = rawLng;
        if (gpsHistCnt < 3) gpsHistCnt++;
        double medLat = rawLat, medLng = rawLng;
        if (gpsHistCnt >= 3) {
          double a = latHist[0], b = latHist[1], c0 = latHist[2];
          medLat = max(min(a, b), min(max(a, b), c0));
          double x = lngHist[0], y = lngHist[1], z = lngHist[2];
          medLng = max(min(x, y), min(max(x, y), z));
        }
        filtGpsLat = (filtGpsLat == 0.0) ? medLat : (0.75 * filtGpsLat + 0.25 * medLat);
        filtGpsLng = (filtGpsLng == 0.0) ? medLng : (0.75 * filtGpsLng + 0.25 * medLng);

        portENTER_CRITICAL(&dataMux);
        gpsLat = filtGpsLat;
        gpsLng = filtGpsLng;
        gpsFixValid = true;
        lastGpsFixMs = now;
        portEXIT_CRITICAL(&dataMux);
      }
      gpsSpeedKmph = gps.speed.isValid() ? gps.speed.kmph() : 0.0f;
      gpsAltM      = gps.altitude.isValid() ? gps.altitude.meters() : 0.0f;
      gpsSats      = gps.satellites.isValid() ? (uint8_t)constrain(gps.satellites.value(), 0UL, 99UL) : 0;
      gpsHdop      = (gps.hdop.isValid() && gps.hdop.hdop() > 0.1f) ? gps.hdop.hdop() : 1.5f;
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
  }

  // Kiểm tra timeout module SIM 4G thực tế
  if (simModemOnline && (now - lastSimOkMs > 25000)) {
    simModemOnline = false;
  }

  // 10B. Đọc nhiệt độ chip nội ESP32-S3 (On-die Temperature Sensor)
  float curS3T = temperatureRead();
  if (curS3T > -30.0f && curS3T < 115.0f) {
    valS3ChipTemp = (valS3ChipTemp > 1.0f) ? (valS3ChipTemp * 0.90f + curS3T * 0.10f) : curS3T;
  }

  // 11. Tự chẩn đoán phần cứng 11 linh kiện thực tế
  updateHardwareSelfDiagnosis();

  uint8_t alerts = ALERT_NONE;
  if (jamState != JAM_NORMAL)                              alerts |= ALERT_MOTOR_JAM;
  if (batValid && isBatteryLow(filtBat))                   alerts |= ALERT_LOW_BATTERY;
  if (hasMPU6050 && (abs(newRoll) > 45.0f || abs(newPitch) > 45.0f)) alerts |= ALERT_HIGH_TILT;

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
  }
  if (evaluateWaterQuality(filtPH, filtTDS, filtNTU, valTemp) > 0)   alerts |= ALERT_WATER_QUAL;
  if (mudBottomLatched)                                    alerts |= ALERT_WINCH_LIMIT;
  int waterSensorCount = (ds18b20Valid ? 1 : 0) + (phValid ? 1 : 0) +
                         (tdsValid ? 1 : 0) + (ntuValid ? 1 : 0);
  if (!ds18b20Valid || !hasMPU6050 || waterSensorCount < 2) alerts |= ALERT_SENSOR_FAULT;
  if (headingHoldActive)                                   alerts |= ALERT_HEADING_HOLD;
  if (is4GFallback)                                        alerts |= ALERT_4G_FALLBACK;

  homeDistanceM = calcGpsDistanceMeters(gpsLat, gpsLng, homeGpsLat, homeGpsLng);
  flags2 = FLAG2_NONE;
  if (returnHomeActive)                         flags2 |= FLAG2_RETURN_HOME;
  if (gpsFixValid)                              flags2 |= FLAG2_GPS_FIX;
  if (homeGpsLat != 0.0 || homeGpsLng != 0.0)    flags2 |= FLAG2_HOME_SAVED;
  if (simModemOnline)                           flags2 |= FLAG2_SIM_ONLINE;
  if (autoStage != AUTO_STAGE_OFF && autoStage != AUTO_STAGE_COMPLETED)
                                                flags2 |= FLAG2_AUTO_RUNNING;
  if (mudBottomLatched)                         flags2 |= FLAG2_MUD_LATCHED;
  if (isBatteryCritical(filtBat))               flags2 |= FLAG2_LOW_BAT_CRIT;
  if (nvsCalibrated)                            flags2 |= FLAG2_CALIBRATED;
  if (virtualAnchorActive)                      flags2 |= FLAG2_VIRTUAL_ANCHOR;
  if (headingHoldActive)                        flags2 |= FLAG2_HEADING_HOLD;

  portENTER_CRITICAL(&dataMux);
  valPH            = filtPH;
  valTDS           = filtTDS;
  valNTU           = filtNTU;
  valDO            = calcDO;
  valEC            = filtEC;
  valWQI           = wqi;
  valAmp           = filtAmp;
  valVolt3S        = filtBat;
  valRoll          = newRoll;
  valPitch         = newPitch;
  valMpuTemp       = newMpuTemp;
  systemAlertFlags = alerts;
  portEXIT_CRITICAL(&dataMux);
}

// =========================================================================
// 6. MÁY TRẠNG THÁI CHỐNG KẸT CHÂN VỊT & QUẢN LÝ TỜI / AUTO 3 TẦNG
// =========================================================================
void updateAntiJamProtection() {
  unsigned long now = millis();

  switch (jamState) {
    case JAM_NORMAL:
      if (valAmp > CURRENT_JAM_LIMIT_A && abs(curMotor) > 30) {
        if (overAmpStart == 0) {
          overAmpStart = now;
        } else if (now - overAmpStart >= 250) {
          jamConsecutiveStrikes++;
          Serial.printf("[USV-WARN] Phat hien ket chan vit (>2.8A, lan #%u)! Kich hoat dao chieu go rac...\n", jamConsecutiveStrikes);
          driveMotor(0);
          curMotor = 0;
          if (jamConsecutiveStrikes >= 3) {
            // Khóa bảo vệ cấp 3: Đã thử gỡ 3 lần không thành công -> Khóa cứng ngắt động cơ để bảo vệ cuộn dây và L298N
            jamState = JAM_LOCKED_WAIT_RELEASE;
            jamTimerTick = now + 2000UL;
            cmdMotor = 0;
            returnHomeActive = false;
            headingHoldActive = false;
            Serial.println(F("[USV-ERR] *** KHOA AN TOAN DONG CO: Ket chan vit 3 lan lien tiep! Cat nguon tranh chay mach. ***"));
          } else {
            jamState = JAM_STOP_WAIT;
            jamTimerTick = now;
          }
        }
      } else {
        overAmpStart = 0;
        if (abs(curMotor) > 30 && valAmp <= CURRENT_JAM_LIMIT_A) {
          if (lastNormalRunTick == 0) lastNormalRunTick = now;
          else if (now - lastNormalRunTick >= 5000UL) {
            jamConsecutiveStrikes = 0; // Đã chạy bình thường > 5s -> reset số lần kẹt
          }
        } else {
          lastNormalRunTick = 0;
        }
      }
      break;

    case JAM_STOP_WAIT:
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
      break;

    case JAM_LOCKED_WAIT_RELEASE:
      driveMotor(0);
      curMotor = 0;
      // Bắt buộc người lái phải trả cần ga về 0 (Neutral) VÀ chờ ít nhất 3 giây để tránh tự khởi động lại khi chưa an toàn
      if (cmdMotor == 0 && (now - jamTimerTick >= 3000)) {
        jamState = JAM_NORMAL;
        overAmpStart = 0;
      }
      break;
  }
}

void startAutoProfiling() {
  if (autoStage != AUTO_STAGE_OFF && autoStage != AUTO_STAGE_COMPLETED) return;
  Serial.println("[USV-AUTO] Bat dau quy trinh Auto do 3 tang nuoc (Co bu ap & giam toc mem)...");
  mudBottomLatched = false;
  autoStage        = AUTO_STAGE_HOMING_START;
  autoStageTick    = millis();
  driveWinch(WINCH_CMD_HOME);
}

void stopAutoProfiling() {
  autoCmdLatched = false;
  if (autoStage != AUTO_STAGE_OFF) {
    autoStage = AUTO_STAGE_OFF;
    driveWinch(WINCH_CMD_STOP);
    Serial.println("[USV-AUTO] Da dung che do Auto do 3 tang.");
  }
}

void updateWinchAndAutoProfile() {
  unsigned long now = millis();
  updateWinchDepthEstimate();

  // 1. Xác nhận ngắt CT1 (Active-LOW: Khi đang hạ tời mà CT1 chạm đáy đóng xuống GND -> Nhấc nhẹ 5cm)
  if (trigBottomMud) {
    trigBottomMud = false;
    if (winchMode == 1 && digitalRead(PIN_CT1_BOTTOM) == LOW) {
      mudBottomLatched   = true;
      winchServo.write(45);
      winchMode          = 0;
      isReliftingFromMud = true;
      reliftStart        = now;
      Serial.println("[USV-WINCH] CT1(38) Cham bun day! Dang nhac nhe 5cm...");

      if (autoStage == AUTO_STAGE_MOVE_BOT) {
        autoStage     = AUTO_STAGE_LIFT_BOT;
        autoStageTick = now;
      }
    }
  }

  if (isReliftingFromMud && (now - reliftStart >= MUD_RELIFT_TIME_MS)) {
    isReliftingFromMud = false;
    winchServo.write(90);
    winchMode = 0;
  }

  // 2. Xác nhận ngắt CT2 (Về sát gầm sàn Home -> Phanh dừng ngay & Reset mốc 0.00m)
  if (trigTopHome || (winchMode == 2 && digitalRead(PIN_CT2_TOP_HOME) == LOW)) {
    trigTopHome = false;
    winchServo.write(90);
    winchMode   = 0;
    winchDepthM = 0.00f;
  }

  // 3. Bảo vệ chống chạy tời quá thời gian tối đa (25 giây)
  if (winchMode != 0 && (now - winchStartTick >= WINCH_MAX_RUN_TIME_MS)) {
    Serial.println("[USV-WARN] Qua thoi gian chay toi an toan (25s)! Tu dong dung toi.");
    driveWinch(WINCH_CMD_STOP);
    if (autoStage != AUTO_STAGE_OFF) {
      autoStage     = AUTO_STAGE_RETURN_HOME;
      autoStageTick = now;
    }
  }

  // Toi tung nac do phan giai cao 10cm
  if (winchInchUntilMs > 0 && now >= winchInchUntilMs) {
    winchInchUntilMs = 0;
    driveWinch(WINCH_CMD_STOP);
    Serial.println("[USV-WINCH] Hoan tat nac ha/thu toi 10cm.");
  }

  // Tự động giữ vị trí Neo Ảo Điện tử (Dynamic Positioning - Marine Station Keeping - S-07)
  // Ma trận ưu tiên: Manual < Auto 3 tầng < DP < RTH < Low-battery derating < E-Stop
  static bool anchorCorrecting = false;
  if (virtualAnchorActive) {
    bool gpsQualityOk = gpsFixValid && (gpsSats >= 6) && (gpsHdop <= 2.5f);
    if (!gpsQualityOk) {
      // Mất GPS FIX hoặc độ chính xác thấp (sats < 6 hoặc HDOP > 2.5) -> dừng động cơ an toàn thay vì chạy mù.
      cmdMotor = 0;
      anchorCorrecting = false;
      headingHoldActive = false;
    } else if (!returnHomeActive && (autoStage == AUTO_STAGE_OFF || autoStage == AUTO_STAGE_COMPLETED)) {
      float anchorDist = calcGpsDistanceMeters(gpsLat, gpsLng, virtualAnchorLat, virtualAnchorLng);
      // Vùng trễ Hysteresis: kích hoạt khi trôi > dpActivateRadiusM (2.5m), dừng đẩy khi vào bán kính an toàn <= dpSafeRadiusM (1.5m)
      if (anchorDist > dpActivateRadiusM) {
        anchorCorrecting = true;
      } else if (anchorDist <= dpSafeRadiusM) {
        anchorCorrecting = false;
      }

      if (anchorCorrecting) {
        float tBearing = calcGpsBearingDegrees(gpsLat, gpsLng, virtualAnchorLat, virtualAnchorLng);
        targetHeadingDeg  = tBearing;
        headingHoldActive = true;

        // Tính góc lệch hướng mũi thuyền so với hướng trạm Neo
        float hdgErr = fabsf(calcHeadingShortestError(targetHeadingDeg, valHeading));
        if (hdgErr > 55.0f) {
          // Mũi thuyền đang quay lệch lớn -> ga nhẹ 65 để bánh lái xoay thuyền trước, tránh lao xa điểm neo!
          cmdMotor = 65;
        } else {
          // Đã hướng về điểm Neo -> ga tỷ lệ theo khoảng cách trôi (từ 75 đến 150)
          float propThrust = 75.0f + constrain((anchorDist - dpSafeRadiusM) * 25.0f, 0.0f, 75.0f);
          cmdMotor = (int)propThrust;
        }
      } else {
        cmdMotor = 0;
        headingHoldActive = false; // Trong vùng neo an toàn: thả trôi êm dịu, không ghì servo bánh lái
      }
    }
  } else {
    anchorCorrecting = false;
  }

  // P3-3.3: Máy trạng thái Sục rửa đầu dò tự động 30s Non-blocking (10s bơm rửa + 10s sục khí + 10s ổn định)
  if (cleanState != CLEAN_IDLE) {
    unsigned long cElapsed = now - cleanStepStartMs;
    if (cleanState == CLEAN_WASH_PUMP && cElapsed >= 10000UL) {
      cleanState = CLEAN_AIR_FLUSH;
      cleanStepStartMs = now;
      Serial.println("[USV-CLEAN] Buoc 2/3: Suc khi lam sach mang bam dau do (10s)...");
    } else if (cleanState == CLEAN_AIR_FLUSH && cElapsed >= 10000UL) {
      cleanState = CLEAN_SETTLE;
      cleanStepStartMs = now;
      Serial.println("[USV-CLEAN] Buoc 3/3: Cho on dinh dien cuc truoc khi doc lai (10s)...");
    } else if (cleanState == CLEAN_SETTLE && cElapsed >= 10000UL) {
      cleanState = CLEAN_IDLE;
      Serial.println("[USV-CLEAN] Hoan tat chu trinh suc rua dau do 30s!");
    }
  }

  // P3-3.4: Máy trạng thái Bơm lấy mẫu nước tự động tại tầng hiện tại Non-blocking (5s tráng ống + 15s lấy mẫu)
  if (sampleState != SAMPLE_IDLE) {
    unsigned long sElapsed = now - sampleStepStartMs;
    if (sampleState == SAMPLE_FLUSH_PIPE && sElapsed >= 5000UL) {
      sampleState = SAMPLE_FILL_BOTTLE;
      sampleStepStartMs = now;
      Serial.printf("[USV-SAMPLE] Buoc 2/2: Dang bom lay mau nuoc vao chai #%u tai do sau %.2fm (15s)...\n",
                    (unsigned int)sampleBottleIdx, winchDepthM);
    } else if (sampleState == SAMPLE_FILL_BOTTLE && sElapsed >= 15000UL) {
      sampleState = SAMPLE_IDLE;
      Serial.printf("[USV-SAMPLE] HOAN TAT LAY MAU CHAI #%u | DoSau=%.2fm | GPS=(%.5f,%.5f) | pH=%.2f TDS=%.0f NTU=%.0f\n",
                    (unsigned int)sampleBottleIdx, winchDepthM, gpsLat, gpsLng, valPH, valTDS, valNTU);
    }
  }

  // 4. Máy trạng thái Auto đo 3 tầng nước (có giảm tốc mềm P-Control khi gần tới mốc)
  if (autoStage == AUTO_STAGE_OFF || autoStage == AUTO_STAGE_COMPLETED) return;

  unsigned long surfTargetMs = getVoltageCompensatedDuration(1500);
  unsigned long midTargetMs  = getVoltageCompensatedDuration(2500);
  unsigned long elapsed      = now - autoStageTick;

  switch (autoStage) {
    case AUTO_STAGE_HOMING_START:
      if (digitalRead(PIN_CT2_TOP_HOME) == LOW || elapsed >= 4000) {
        driveWinch(WINCH_CMD_STOP);
        mudBottomLatched = false;
        winchDepthM      = 0.00f;
        autoStage        = AUTO_STAGE_MOVE_SURF;
        autoStageTick    = now;
        driveWinchVariable(WINCH_CMD_DOWN, 45);
      }
      break;

    case AUTO_STAGE_MOVE_SURF:
      if (elapsed + 350 >= surfTargetMs && elapsed < surfTargetMs) {
        driveWinchVariable(WINCH_CMD_DOWN, 20);
      }
      if (elapsed >= surfTargetMs || mudBottomLatched) {
        driveWinch(WINCH_CMD_STOP);
        winchDepthM   = 0.30f;
        autoStage     = AUTO_STAGE_WAIT_SURF;
        autoStageTick = now;
      }
      break;

    case AUTO_STAGE_WAIT_SURF:
      if (elapsed >= 2500) {
        saveWaterLayerSample(layerSurface, "l1");
        Serial.printf("[USV-AUTO] Tang Mat (0.30m): pH=%.2f, TDS=%.0f, NTU=%.0f, Temp=%.1f, DO=%.2fmg/L\n",
                      layerSurface.ph, layerSurface.tds, layerSurface.ntu, layerSurface.temp, layerSurface.doMgL);
        autoStage     = AUTO_STAGE_MOVE_MID;
        autoStageTick = now;
        driveWinchVariable(WINCH_CMD_DOWN, 45);
      }
      break;

    case AUTO_STAGE_MOVE_MID:
      if (elapsed + 400 >= midTargetMs && elapsed < midTargetMs) {
        driveWinchVariable(WINCH_CMD_DOWN, 20);
      }
      if (elapsed >= midTargetMs || mudBottomLatched) {
        driveWinch(WINCH_CMD_STOP);
        winchDepthM   = 1.00f;
        autoStage     = AUTO_STAGE_WAIT_MID;
        autoStageTick = now;
      }
      break;

    case AUTO_STAGE_WAIT_MID:
      if (elapsed >= 2500) {
        saveWaterLayerSample(layerMiddle, "l2");
        Serial.printf("[USV-AUTO] Tang Giua (1.00m): pH=%.2f, TDS=%.0f, NTU=%.0f, Temp=%.1f, DO=%.2fmg/L\n",
                      layerMiddle.ph, layerMiddle.tds, layerMiddle.ntu, layerMiddle.temp, layerMiddle.doMgL);
        if (mudBottomLatched) {
          autoStage     = AUTO_STAGE_WAIT_BOT;
          autoStageTick = now;
        } else {
          autoStage     = AUTO_STAGE_MOVE_BOT;
          autoStageTick = now;
          driveWinchVariable(WINCH_CMD_DOWN, 40);
        }
      }
      break;

    case AUTO_STAGE_MOVE_BOT:
      if (mudBottomLatched || elapsed >= 3500) {
        driveWinch(WINCH_CMD_STOP);
        winchDepthM   = 1.80f;
        autoStage     = AUTO_STAGE_WAIT_BOT;
        autoStageTick = now;
      }
      break;

    case AUTO_STAGE_LIFT_BOT:
      if (!isReliftingFromMud) {
        autoStage     = AUTO_STAGE_WAIT_BOT;
        autoStageTick = now;
      }
      break;

    case AUTO_STAGE_WAIT_BOT:
      if (elapsed >= 2500) {
        saveWaterLayerSample(layerBottom, "l3");
        Serial.printf("[USV-AUTO] Tang Day (1.80m): pH=%.2f, TDS=%.0f, NTU=%.0f, Temp=%.1f, DO=%.2fmg/L\n",
                      layerBottom.ph, layerBottom.tds, layerBottom.ntu, layerBottom.temp, layerBottom.doMgL);
        if (loraReady) {
          char prfBuf[190];
          int len = snprintf(prfBuf, sizeof(prfBuf) - 5,
                             "PRF:%.2f,%.0f,%.0f,%.1f;%.2f,%.0f,%.0f,%.1f;%.2f,%.0f,%.0f,%.1f",
                             layerSurface.ph, layerSurface.tds, layerSurface.ntu, layerSurface.temp,
                             layerMiddle.ph,  layerMiddle.tds,  layerMiddle.ntu,  layerMiddle.temp,
                             layerBottom.ph,  layerBottom.tds,  layerBottom.ntu,  layerBottom.temp);
          if (len > 0) {
            if (len >= (int)sizeof(prfBuf) - 5) len = (int)sizeof(prfBuf) - 6;
            uint8_t crc = calcKeyedCRC8(prfBuf, (size_t)len);
            snprintf(prfBuf + len, 5, "*%02X", crc);
            LoRa.beginPacket();
            LoRa.print(prfBuf);
            bool prfTxOk = LoRa.endPacket();
            if (prfTxOk) { loraTxOkCnt++; loraTxFailCnt = 0; } else { loraTxFailCnt++; }
            // KHONG goi LoRa.parsePacket() ngay sau TX de khong nuot goi lenh dang cho.
            lastLoraTx = millis();
            Serial.printf("[USV-TX-PRF] %s (%s)\n", prfBuf, prfTxOk ? "OK" : "FAIL");
          }
        }
        pendingGsProfile = true;
        autoStage        = AUTO_STAGE_RETURN_HOME;
        autoStageTick    = now;
        driveWinch(WINCH_CMD_HOME);
      }
      break;

    case AUTO_STAGE_RETURN_HOME:
      if (digitalRead(PIN_CT2_TOP_HOME) == LOW || elapsed >= 6000) {
        driveWinch(WINCH_CMD_STOP);
        winchDepthM     = 0.00f;
        autoStage       = AUTO_STAGE_COMPLETED;
        // GIU LATCH = true: Trạm Bờ còn gửi W=3 trong 1-2 gói nữa trước khi nhận
        // được gói COMPLETED. Nếu nhả latch ngay, Auto sẽ tự khởi động lại lần nữa.
        // Latch chỉ nhả khi nhận W != 3 (lệnh DỪNG) ở applyIncomingCommand().
        autoCmdLatched  = true;
        Serial.println("[USV-AUTO] Hoan tat do 3 tang! Da thu long ve Home (0.00m) an toan.");
      }
      break;

    default:
      break;
  }
}

// =========================================================================
// 7. XỬ LÝ LỆNH ĐIỀU KHIỂN TRÊN CORE 1 (AN TOÀN ĐA NHIỆM TUYỆT ĐỐI)
// =========================================================================
void tareImuAnglesNow() {
  portENTER_CRITICAL(&dataMux);
  rollTareOffset  = rawRollUncomp;
  pitchTareOffset = rawPitchUncomp;
  valRoll         = 0.0f;
  valPitch        = 0.0f;
  portEXIT_CRITICAL(&dataMux);
  nvsPrefs.putFloat("r_ofs", rollTareOffset);
  nvsPrefs.putFloat("p_ofs", pitchTareOffset);
  Serial.printf("[USV-IMU] Da Tare can bang goc MPU6050 ve (0.0, 0.0)! Offset=(%.2f, %.2f)\n",
                rollTareOffset, pitchTareOffset);
}

// Bộ điều khiển PID Giữ hướng tự động (Autopilot Compass Heading Hold) trên Core 1
void updateAutopilotHeadingHold() {
  if (!headingHoldActive) return;
  unsigned long now = millis();
  if (now - lastPidTickMs < 40) return; // Chu kỳ PID 25Hz (40ms)
  float dt = (lastPidTickMs > 0) ? ((now - lastPidTickMs) * 0.001f) : 0.04f;
  lastPidTickMs = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.04f;

  // Nếu người lái chủ động bẻ bánh lái qua Joystick/Switch (> 8 độ khỏi vị trí 90°), ưu tiên tay lái người dùng
  if (abs(cmdRudder - 90) > 8) {
    rudderServo.write(cmdRudder);
    return;
  }

  float curHdg, yawRate;
  portENTER_CRITICAL(&dataMux);
  curHdg  = valHeading;
  yawRate = lastGyroYawRate;
  portEXIT_CRITICAL(&dataMux);

  float err = calcHeadingShortestError(targetHeadingDeg, curHdg);
  if (fabsf(err) < 1.2f) err = 0.0f; // Vùng chết nhỏ chống rung Servo

  pidHeadingIntegral = constrain(pidHeadingIntegral + err * dt, -25.0f, 25.0f);
  const float kp = 1.15f;
  const float ki = 0.12f;
  const float kd = 0.28f;
  float corr = kp * err + ki * pidHeadingIntegral - kd * yawRate;
  int autoRudder = constrain((int)(90.0f + corr + 0.5f), 45, 135);
  rudderServo.write(autoRudder);
}

// Bảo vệ đa tầng Pin LiPo: Tier 1 (Cảnh báo + Giảm ga), Tier 2 (Auto Return-To-Home GPS), Tier 3 (Ngắt khẩn cấp Motor & Winch)
void updateMultiTierBatteryFailsafes() {
  if (!batValid || valVolt3S < 2.0f) {
    lowBatTier2Start = 0;
    lowBatTier3Start = 0;
    batTier3CutoffActive = false;
    return;
  }

  unsigned long now = millis();
  float cellV = calcBatteryCellVolt(valVolt3S);

  // Tier 3: Khẩn cấp cắt toàn bộ động cơ & tời (< 3.15V/cell trong 2s)
  if (cellV < BAT_CELL_TIER3_CUTOFF_V) {
    if (lowBatTier3Start == 0) {
      lowBatTier3Start = now;
    } else if (now - lowBatTier3Start >= BAT_TIER3_CUTOFF_HOLD_MS) {
      if (!batTier3CutoffActive) {
        batTier3CutoffActive = true;
        cmdMotor = 0;
        curMotor = 0;
        driveMotor(0);
        driveWinch(WINCH_CMD_STOP);
        returnHomeActive = false;
        headingHoldActive = false;
        systemAlertFlags |= ALERT_LOW_BATTERY;
        flags2 |= FLAG2_LOW_BAT_CRIT;
        Serial.printf("[USV-BAT] *** TIER 3 KHAN CAP ***: Pin %.2fV (%.2fV/cell) duoi nguong an toan! CAT DONG CO & TOI de duy tri 3.3V LoRa/GPS beacon!\n",
                      valVolt3S, cellV);
      }
    }
  } else {
    lowBatTier3Start = 0;
    if (batTier3CutoffActive && cellV > BAT_CELL_TIER3_CUTOFF_V + 0.15f) {
      batTier3CutoffActive = false;
      Serial.println("[USV-BAT] Dien ap pin phuc hoi, bo khoa Tier 3.");
    }
  }

  // Tier 2: Tự hành quay về Home GPS (< 3.26V/cell trong 4s)
  if (!batTier3CutoffActive && cellV < BAT_CELL_TIER2_RTH_V) {
    if (lowBatTier2Start == 0) {
      lowBatTier2Start = now;
    } else if (now - lowBatTier2Start >= BAT_TIER2_RTH_HOLD_MS) {
      if (!batTier2RthTriggered && !returnHomeActive) {
        if (gpsFixValid && (homeGpsLat != 0.0 || homeGpsLng != 0.0) && homeDistanceM > 4.0f) {
          batTier2RthTriggered = true;
          returnHomeActive = true;
          headingHoldActive = true;
          pidHeadingIntegral = 0.0f;
          lastPidTickMs = 0;
          cmdMotor = RETURN_HOME_MOTOR_PWM;
          if (winchDepthM > 0.05f && winchMode == 0 && !isReliftingFromMud) {
            driveWinch(WINCH_CMD_HOME);
          }
          Serial.printf("[USV-BAT] *** TIER 2 AUTO-RTH ***: Pin yeu %.2fV (%.2fV/cell)! Tu dong Return-To-Home GPS ve (%.5f, %.5f), cach %.1fm\n",
                        valVolt3S, cellV, homeGpsLat, homeGpsLng, homeDistanceM);
        }
      }
    }
  } else if (cellV >= BAT_CELL_TIER2_RTH_V + 0.10f) {
    lowBatTier2Start = 0;
    batTier2RthTriggered = false;
  }
}

// Máy trạng thái Tự hành quay về Home GPS (RTH FSM - Mục 3.2: IDLE -> ARMED -> TURN_HOME -> CRUISE -> APPROACH -> ARRIVED | ABORT)
void updateReturnHome() {
  if (!returnHomeActive) {
    rthState = RTH_IDLE;
    return;
  }

  unsigned long now = millis();
  if (rthState == RTH_IDLE) {
    rthState          = RTH_ARMED;
    rthStartMs        = now;
    rthGpsLossStartMs = 0;
  }

  // Timeout hành trình RTH tối đa 180 giây -> ABORT an toàn
  if (now - rthStartMs > 180000UL) {
    rthState          = RTH_ABORT;
    returnHomeActive  = false;
    headingHoldActive = false;
    cmdMotor          = 0;
    driveMotor(0);
    Serial.println("[USV-HOME] ABORT RTH: Qua thoi gian hanh trinh 180s! Ngat dong co & phat SOS.");
    return;
  }

  // Kiểm tra Home hợp lệ & xử lý mất GPS tạm thời (giữ 5s trước khi ABORT)
  if (homeGpsLat == 0.0 && homeGpsLng == 0.0) {
    rthState          = RTH_ABORT;
    returnHomeActive  = false;
    headingHoldActive = false;
    cmdMotor          = 0;
    driveMotor(0);
    Serial.println("[USV-HOME] ABORT RTH: Chua luu toa do Home GPS hop le!");
    return;
  }

  if (!gpsFixValid) {
    if (rthGpsLossStartMs == 0) rthGpsLossStartMs = now;
    cmdMotor = 0;
    driveMotor(0);
    if (now - rthGpsLossStartMs >= 5000UL) {
      rthState          = RTH_ABORT;
      returnHomeActive  = false;
      headingHoldActive = false;
      Serial.println("[USV-HOME] ABORT RTH: Mat GPS FIX > 5s! Ngat dong co an toan.");
    }
    return;
  }
  rthGpsLossStartMs = 0;

  homeDistanceM = calcGpsDistanceMeters(gpsLat, gpsLng, homeGpsLat, homeGpsLng);

  if (homeDistanceM <= RETURN_HOME_RADIUS_M) {
    rthState          = RTH_ARRIVED;
    returnHomeActive  = false;
    headingHoldActive = false;
    cmdMotor          = 0;
    driveMotor(0);
    rudderServo.write(90);
    Serial.printf("[USV-HOME] RTH_ARRIVED: DA VE TOI HOME (con %.2fm). Tu dong dung dong co.\n", homeDistanceM);
    return;
  }

  targetHeadingDeg  = calcGpsBearingDegrees(gpsLat, gpsLng, homeGpsLat, homeGpsLng);
  headingHoldActive = true;
  float hdgErr = fabsf(calcHeadingShortestError(targetHeadingDeg, valHeading));

  if (hdgErr > 35.0f) {
    rthState = RTH_TURN_HOME;
    cmdMotor = 75; // Xoay mũi về hướng Home trước khi tăng ga hành trình
  } else if (homeDistanceM < 10.0f) {
    rthState = RTH_APPROACH;
    cmdMotor = 90; // Giảm ga khi còn < 10m để tránh lao qua điểm Neo
  } else {
    rthState = RTH_CRUISE;
    cmdMotor = RETURN_HOME_MOTOR_PWM;
  }

  // An toàn: vừa chạy về vừa thu lồng cảm biến về Home (tránh kéo lê dây tời).
  if (winchDepthM > 0.05f && winchMode == 0 && !isReliftingFromMud) {
    driveWinch(WINCH_CMD_HOME);
  }
}

bool applyIncomingCommand(const char* rawCmd, int rssi, float snr, const char* srcTag) {
  unsigned long now = millis();

  if (strncmp(rawCmd, "GURL:", 5) == 0) {
    portENTER_CRITICAL(&dataMux);
    copyStringSafe(googleScriptUrl, sizeof(googleScriptUrl), rawCmd + 5);
    char* star = strchr(googleScriptUrl, '*');
    if (star) *star = '\0';
    portEXIT_CRITICAL(&dataMux);
    nvsPrefs.putString("gs_url", googleScriptUrl);
    lastCmdRxTime = now;
    Serial.printf("[USV-GSHEETS] Da cap nhat Google Script URL moi: %s\n", googleScriptUrl);
    return true;
  }

  if (strncmp(rawCmd, "TARE", 4) == 0) {
    tareImuAnglesNow();
    lastCmdRxTime = now;
    return true;
  }

  if (strncmp(rawCmd, "CBAT:", 5) == 0) {
    float targetBatV = strtof(rawCmd + 5, nullptr);
    if (targetBatV >= 2.0f && targetBatV <= 25.0f && lastRawBatPinV >= 0.15f) {
      batDividerRatio = targetBatV / lastRawBatPinV;
      nvsPrefs.putFloat("bat_rat", batDividerRatio);
      kalmanBat.reset(targetBatV);
      valVolt3S = targetBatV;
      nvsCalibrated = true;
      Serial.printf("[USV-BAT] Da hieu chuan Pin theo VOM = %.2fV (He so chia ap moi = %.4f)\n",
                    targetBatV, batDividerRatio);
    } else {
      Serial.println("[USV-BAT] Tu choi hieu chuan: dien ap VOM ngoai dai 2..25V hoac chua doc duoc ADC Pin!");
    }
    lastCmdRxTime = now;
    return true;
  }

  if (strncmp(rawCmd, "CMD:", 4) != 0) return false;

  const char* starPtr = strchr(rawCmd, '*');
  if (starPtr != nullptr) {
    size_t payloadLen = (size_t)(starPtr - rawCmd);
    uint8_t expectedCrc = calcKeyedCRC8(rawCmd, payloadLen);
    uint8_t rxCrc = (uint8_t)strtoul(starPtr + 1, nullptr, 16);
    if (expectedCrc != rxCrc) {
      Serial.printf("[USV-SEC] Tu choi goi CMD sai CRC-8 (Nhan:0x%02X != Tinh:0x%02X)\n", rxCrc, expectedCrc);
      return false;
    }
  } else if (strcmp(srcTag, "LORA") == 0) {
    // Gói điều khiển qua LoRa BẮT BUỘC phải có CRC-8 để chống nhiễu/giả mạo.
    Serial.println("[USV-SEC] Tu choi goi CMD LoRa thieu CRC-8!");
    return false;
  }

  int m = 0, r = 90, w = 0;
  if (sscanf(rawCmd + 4, "%d,%d,%d", &m, &r, &w) != 3) return false;

  triggerLedFlash(0, 45, 45, 90); // Chop Xanh Cyan bao vua nhan lenh dieu khien RX

  // Cac ma lenh "cau hinh / bao cao" KHONG duoc lam thay doi chuyen dong hien tai.
  // Chi cac lenh dieu khien that su (tien/lui, lai, toi, GOTO_HOME, STOP_ALL...) moi ap dung M/R/W.
  bool applyMotion  = true;
  bool centerRudder = false;

  // Cac ma lenh may rong duoc biet tu Tram Bo / Joystick / Web Dashboard:
  if (w == WINCH_CMD_TARE_IMU) {
    tareImuAnglesNow();
    w = 0;
    applyMotion = false;
  } else if (w == WINCH_CMD_HEADING_HOLD_ON) {
    portENTER_CRITICAL(&dataMux);
    headingHoldActive  = true;
    targetHeadingDeg   = valHeading;
    portEXIT_CRITICAL(&dataMux);
    pidHeadingIntegral = 0.0f;
    lastPidTickMs      = 0;
    Serial.printf("[USV-AUTOPILOT] BAT Giu Huong PID tai goc %.1f do!\n", targetHeadingDeg);
    w = 0;
    applyMotion  = false;
    centerRudder = true;
  } else if (w == WINCH_CMD_HEADING_HOLD_OFF) {
    headingHoldActive  = false;
    returnHomeActive   = false;
    pidHeadingIntegral = 0.0f;
    Serial.println("[USV-AUTOPILOT] TAT Giu Huong PID.");
    w = 0;
    applyMotion  = false;
    centerRudder = true;
  } else if (w == WINCH_CMD_SET_HOME_GPS) {
    if (gpsFixValid && (gpsLat != 0.0 || gpsLng != 0.0)) {
      homeGpsLat = gpsLat;
      homeGpsLng = gpsLng;
      nvsPrefs.putFloat("home_lat", (float)homeGpsLat);
      nvsPrefs.putFloat("home_lng", (float)homeGpsLng);
      Serial.printf("[USV-GPS] Da luu diem Neo Home GPS: (%.5f, %.5f)\n", homeGpsLat, homeGpsLng);
    } else {
      Serial.println("[USV-GPS] Tu choi luu Home: GPS chua co toa do hop le (khong luu 0,0)!");
    }
    w = 0;
    applyMotion = false;
  } else if (w == WINCH_CMD_SET_HEADING) {
    if (m >= 0 && m <= 359) {
      headingHoldActive  = true;
      targetHeadingDeg   = (float)m;
      pidHeadingIntegral = 0.0f;
      lastPidTickMs      = 0;
      Serial.printf("[USV-AUTOPILOT] Dat huong muc tieu = %d do.\n", m);
    }
    m = 0;
    w = 0;
    applyMotion  = false;
    centerRudder = true;
  } else if (w == WINCH_CMD_GOTO_HOME) {
    if (homeGpsLat != 0.0 || homeGpsLng != 0.0) {
      stopAutoProfiling();
      returnHomeActive = true;
      headingHoldActive = true;
      pidHeadingIntegral = 0.0f;
      lastPidTickMs = 0;
      cmdMotor = RETURN_HOME_MOTOR_PWM;
      m = RETURN_HOME_MOTOR_PWM;
      Serial.printf("[USV-HOME] Bat dau tu hanh ve Home (%.5f, %.5f)...\n", homeGpsLat, homeGpsLng);
    } else {
      returnHomeActive = false;
      m = 0;
      Serial.println("[USV-HOME] Tu choi GOTO_HOME: chua luu Home GPS!");
    }
    w = 0;
  } else if (w == WINCH_CMD_STOP_ALL) {
    headingHoldActive = false;
    returnHomeActive  = false;
    virtualAnchorActive = false; // Dung ca Neo Ao, neu khong DP se tu chay lai ngay
    autoCmdLatched    = false;
    stopAutoProfiling();
    m = 0;
    r = 90;
    w = 0;
    driveMotor(0);
    driveWinch(WINCH_CMD_STOP);
    Serial.println("[USV-STOP] DA DUNG TOAN BO: dong co, toi, autopilot va auto 3 tang.");
  } else if (w == WINCH_CMD_REPORT_STATUS) {
    // Chi gui telemetry ngay, tuyet doi khong dong vao dong co/banh lai/toi.
    pendingTelemetryReply = true;
    cmdReceivedAtTick     = now;
    w = 0;
    applyMotion = false;
  } else if (w == WINCH_CMD_EMERGENCY_STOP) {
    headingHoldActive = false;
    returnHomeActive  = false;
    virtualAnchorActive = false; // Phanh khan cap phai huy ca Neo Ao
    autoCmdLatched    = false;
    stopAutoProfiling();
    m = 0;
    r = 90;
    w = WINCH_CMD_HOME; // Tang thu long cam bien ve Home an toan!
    Serial.println("[USV-EMERG] KICH HOAT PHANH KHAN CAP & THU LONG VE HOME!");
  } else if (w == WINCH_CMD_INCH_DOWN) {
    autoCmdLatched = false;
    stopAutoProfiling();
    if (!isReliftingFromMud && digitalRead(PIN_CT1_BOTTOM) == HIGH) {
      driveWinch(WINCH_CMD_DOWN);
      winchInchUntilMs = now + 900; // Ha dung 900ms (~10cm)
      Serial.println("[USV-WINCH] Ha toi tung nac 10cm...");
    }
    w = 0;
    applyMotion = false; // Lenh nac toi KHONG duoc dong vao dong co/banh lai
  } else if (w == WINCH_CMD_INCH_UP) {
    autoCmdLatched = false;
    stopAutoProfiling();
    if (digitalRead(PIN_CT2_TOP_HOME) == HIGH) {
      driveWinch(WINCH_CMD_HOME);
      winchInchUntilMs = now + 900; // Thu dung 900ms (~10cm)
      Serial.println("[USV-WINCH] Thu toi tung nac 10cm...");
    }
    w = 0;
    applyMotion = false;
  } else if (w == WINCH_CMD_VIRTUAL_ANCHOR_ON) {
    if (gpsFixValid && (gpsLat != 0.0 || gpsLng != 0.0)) {
      virtualAnchorLat = gpsLat;
      virtualAnchorLng = gpsLng;
      virtualAnchorActive = true;
      Serial.printf("[USV-ANCHOR] KICH HOAT NEO AO tai (%.5f, %.5f)!\n", virtualAnchorLat, virtualAnchorLng);
    } else {
      Serial.println("[USV-ANCHOR] Tu choi Neo Ao: Chua co toa do GPS fix!");
    }
    w = 0;
    applyMotion = false;
  } else if (w == WINCH_CMD_VIRTUAL_ANCHOR_OFF) {
    virtualAnchorActive = false;
    driveMotor(0);   // Neo Ao truoc do co the dang day thuyen -> phai dung dong co
    cmdMotor = 0;
    headingHoldActive = false;
    rudderServo.write(90);
    Serial.println("[USV-ANCHOR] TAT Che do Neo Ao & Tra lai banh lai ve 90 do.");
    w = 0;
    applyMotion = false;
  } else if (w == WINCH_CMD_TAKE_SAMPLE) {
    sampleBottleIdx   = (uint8_t)constrain(m > 0 ? m : (sampleBottleIdx + 1), 1, 8);
    sampleState       = SAMPLE_FLUSH_PIPE;
    sampleStepStartMs = now;
    Serial.printf("[USV-SAMPLE] Bat dau quy trinh lay mau nuoc chai #%u tai do sau %.2fm (5s trang ong + 15s bom)...\n",
                  (unsigned int)sampleBottleIdx, winchDepthM);
    w = 0;
    applyMotion = false;
  } else if (w == WINCH_CMD_CLEAN_SENSOR) {
    cleanState       = CLEAN_WASH_PUMP;
    cleanStepStartMs = now;
    Serial.println("[USV-CLEAN] Bat dau quy trinh suc rua dau do tu dong 30s (10s rua + 10s khi + 10s on dinh)...");
    w = 0;
    applyMotion = false;
  } else if (w == WINCH_CMD_CALIB_PH_2PT) {
    // m = 4 / 400 (dung dịch chuẩn pH 4.00) hoặc m = 7 / 686 / 700 (dung dịch chuẩn pH 6.86 / 7.00)
    float targetPh = (m > 100) ? (m * 0.01f) : ((m >= 1 && m <= 14) ? (float)m : 7.00f);
    if (fabsf(targetPh - 7.0f) < 0.35f && lastRawPhVolt > 0.2f) {
      phRefVolt7 = lastRawPhVolt;
      phCalibOffset = targetPh - 7.0f;
      reqSaveNvsCalib = true;
      Serial.printf("[USV-CAL] Da luu diem chuan pH %.2f tai V=%.3fV\n", targetPh, phRefVolt7);
    } else if (fabsf(targetPh - 4.0f) < 0.35f && lastRawPhVolt > 0.2f) {
      float dv = fabsf(lastRawPhVolt - phRefVolt7);
      if (dv > 0.15f) {
        phSlopeVPerUnit = constrain(dv / (7.0f - targetPh), 0.10f, 0.30f);
        reqSaveNvsCalib = true;
        Serial.printf("[USV-CAL] Da hieu chuan 2 diem pH %.2f: Slope=%.4f V/pH\n", targetPh, phSlopeVPerUnit);
      }
    }
    w = 0;
    applyMotion = false;
  } else if (w == WINCH_CMD_SET_DP_RADIUS) {
    // m = bán kính tính bằng dm (ví dụ 15 -> 1.5m, từ 5..50 -> 0.5m..5.0m)
    float radM = constrain((float)m * 0.1f, 0.5f, 5.0f);
    dpSafeRadiusM     = radM;
    dpActivateRadiusM = radM + 1.0f;
    Serial.printf("[USV-ANCHOR] Cap nhat ban kinh Neo Ao DP: safe=%.1fm, activate=%.1fm\n",
                  dpSafeRadiusM, dpActivateRadiusM);
    w = 0;
    applyMotion = false;
  } else if (w == WINCH_CMD_SET_TX_INTERVAL) {
    loraTxIntervalMs = (unsigned long)constrain(m, 500, 5000);
    Serial.printf("[USV-LORA] Cap nhat chu ky phat Telemetry: %lu ms\n", loraTxIntervalMs);
    w = 0;
    applyMotion = false;
  }

  lastCmdRxTime = now;

  if (applyMotion) {
    // Người lái chủ động bẻ lái/ga bằng tay -> hủy Neo Ảo để không bị DP giành quyền.
    if (virtualAnchorActive && (m != 0 || r != 90)) {
      virtualAnchorActive = false;
      Serial.println("[USV-ANCHOR] Nhan lenh lai tay -> TU DONG TAT Neo Ao.");
    }
    // Người lái can thiệp tay -> tự động nhường quyền điều khiển ngay lập tức
    if ((returnHomeActive || headingHoldActive) && (m != 0 || abs(r - 90) > 8)) {
      returnHomeActive = false;
      headingHoldActive = false;
      Serial.println("[USV-OVR] Nhan lenh lai tay -> CAN THIEP KHAN CAP: Tu dong huy Return-To-Home & Giu Huong!");
    }
    cmdMotor  = constrain(m, -255, 255);
    cmdRudder = constrain(r, 45, 135);
    cmdWinch  = constrain(w, 0, 3);

    if (!headingHoldActive || abs(cmdRudder - 90) > 8) {
      rudderServo.write(cmdRudder);
    }

    if (cmdWinch == WINCH_CMD_AUTO_3LVL) {
      if (!autoCmdLatched) {
        autoCmdLatched = true;
        startAutoProfiling();
      }
    } else if (cmdWinch == WINCH_CMD_DOWN || cmdWinch == WINCH_CMD_HOME) {
      autoCmdLatched = false;
      stopAutoProfiling();
      if (!isReliftingFromMud) {
        driveWinch(cmdWinch);
      }
    } else {
      autoCmdLatched = false;
      if (digitalRead(PIN_CT1_BOTTOM) == HIGH) {
        mudBottomLatched = false;
      }
      // W=0 (DUNG) tu Tram Bo cung phai dung Auto do 3 tang.
      // Neu khong, sau khi nguoi dung bam DUNG tren Web, Auto van tiep tuc chay ngam.
      if (autoStage != AUTO_STAGE_OFF) {
        stopAutoProfiling();
      }
      if (!isReliftingFromMud) {
        driveWinch(WINCH_CMD_STOP);
      }
    }
  } else if (centerRudder) {
    cmdRudder = 90;
    rudderServo.write(90);
  }

  pendingTelemetryReply = true;
  cmdReceivedAtTick     = now;
  if (applyMotion) {
    Serial.printf("[USV-RX %s] Nhan lenh CMD: M=%d, R=%d, W=%d (RSSI=%ddBm, SNR=%.1fdB)\n",
                  srcTag, cmdMotor, cmdRudder, cmdWinch, rssi, snr);
  } else {
    Serial.printf("[USV-RX %s] Nhan lenh dac biet (W=%d) - KHONG doi chuyen dong (M=%d, R=%d, W=%d)\n",
                  srcTag, w, cmdMotor, cmdRudder, cmdWinch);
  }
  return true;
}

// =========================================================================
// 8. GỬI TELEMETRY LORA (30 TRƯỜNG DỮ LIỆU KHKT + CRC-8) & 4G DUAL-LINK
// =========================================================================
void sendLoRaTelemetry() {
  if (!loraReady) return;

  int ct1Mud  = mudBottomLatched ? 1 : 0;
  int ct2Home = (digitalRead(PIN_CT2_TOP_HOME) == LOW || winchDepthM <= 0.02f) ? 1 : 0;
  usvTxSeq++;

  float ph, tds, ntu, temp, amp, bat, roll, pitch, hdg, doVal, mpuT, ecVal, depthM;
  double lat, lng;
  uint8_t alerts;
  uint16_t f2;
  uint16_t hwMask;
  int wqi;
  uint8_t sats;
  float spdKmph, altM, homeDist, tgtHdg;

  portENTER_CRITICAL(&dataMux);
  ph     = valPH;
  tds    = valTDS;
  ntu    = valNTU;
  temp   = valTemp;
  amp    = valAmp;
  bat    = valVolt3S;
  roll   = valRoll;
  pitch  = valPitch;
  hdg    = valHeading;
  lat    = gpsLat;
  lng    = gpsLng;
  doVal  = valDO;
  hwMask = hwHealthMask;
  alerts = systemAlertFlags;
  f2     = flags2;
  wqi    = valWQI;
  mpuT   = valMpuTemp;
  ecVal  = valEC;
  depthM = winchDepthM;
  float s3ChipT = valS3ChipTemp;
  portEXIT_CRITICAL(&dataMux);

  sats     = gpsSats;
  spdKmph  = gpsSpeedKmph;
  altM     = gpsAltM;
  homeDist = homeDistanceM;
  tgtHdg   = targetHeadingDeg;

  char txBuf[LORA_PACKET_MAX_LEN];
  int len = snprintf(
    txBuf, sizeof(txBuf) - 5,
    "USV:%.2f,%.0f,%.0f,%.1f,%.2f,%.2f,%.1f,%d,%d,%.1f,%.0f,%.5f,%.5f,%d,%u,%lu,%.2f,%u,%d,%.1f,%.0f,%.2f,%u,%.1f,%u,%.2f,%.1f,%.1f,%u,%.0f",
    ph, tds, ntu, temp, amp, bat, roll, ct1Mud, ct2Home,
    pitch, hdg, lat, lng, (int)autoStage,
    (unsigned int)alerts, (unsigned long)usvTxSeq,
    doVal, (unsigned int)hwMask,
    wqi, mpuT, ecVal, depthM, (unsigned int)adcFaultMask, s3ChipT,
    (unsigned int)sats, spdKmph, altM, homeDist, (unsigned int)f2, tgtHdg
  );
  if (len <= 0) return;
  if (len >= (int)sizeof(txBuf) - 5) len = (int)sizeof(txBuf) - 6;
  uint8_t crc = calcKeyedCRC8(txBuf, (size_t)len);
  snprintf(txBuf + len, 5, "*%02X", crc);

  LoRa.beginPacket();
  LoRa.print(txBuf);
  bool txOk = LoRa.endPacket();
  if (txOk) { loraTxOkCnt++; loraTxFailCnt = 0; } else { loraTxFailCnt++; }
  // KHÔNG gọi LoRa.parsePacket() ngay sau khi TX: có thể nuốt mất gói lệnh đang chờ.
  lastLoraTx = millis();
  triggerLedFlash(0, 0, 50, 70); // Chớp Xanh Dương báo phát gói tin LoRa TX
  Serial.printf("[USV-TX #%lu | WQI:%d | S3-Chip:%.1fC | Core0:%uHz Core1:%uHz | TX:%s] %s\n",
                (unsigned long)usvTxSeq, wqi, s3ChipT, core0Hz, core1Hz,
                txOk ? "OK" : "FAIL", txBuf);
}

void checkAndRecoverLoRa() {
  if (millis() - lastLoraHealthMs < 10000) return;
  lastLoraHealthMs = millis();

  // Nếu TX thất bại liên tiếp nhiều lần -> coi như SPI/radio treo, ép khởi tạo lại
  if (loraReady && loraTxFailCnt >= 10) {
    Serial.printf("[USV-RECOVERY] LoRa TX that bai %lu lan lien tiep -> Khoi tao lai!\n",
                  (unsigned long)loraTxFailCnt);
    loraReady = false;
    loraTxFailCnt = 0;
  }

  if (!loraReady) {
    digitalWrite(LORA_RST_PIN, LOW);
    delay(10);
    digitalWrite(LORA_RST_PIN, HIGH);
    delay(10);
    if (LoRa.begin(LORA_FREQ_HZ)) {
      LoRa.setSignalBandwidth(LORA_BW_HZ);
      LoRa.setSpreadingFactor(LORA_SF);
      LoRa.setCodingRate4(5);
      LoRa.setSyncWord(LORA_SYNC_WORD);
      LoRa.setPreambleLength(8);
      LoRa.setTxPower(LORA_TX_POWER_DBM);
      LoRa.enableCrc();
      LoRa.parsePacket();
      loraReady = true;
      Serial.println("[USV-RECOVERY] Da khoi phuc module LoRa Ra-02 thanh cong!");
    }
  }
}

void updateSim4GDualLink() {
  unsigned long now = millis();

  // Sao chép cấu hình & mẫu 3 tầng sang bản cục bộ để tránh race condition giữa Core 0 và Core 1
  char urlLocal[180];
  WaterLayerSample s1, s2, s3;
  float vPH, vTDS, vNTU, vTemp, vAmp, vBat, vRoll, vPitch, vHdg, vDO;
  int vWQI;
  double vLat, vLng;
  uint16_t vHwMask;
  uint8_t vAlerts, vSats;
  uint16_t vFlags2;
  float vSpd, vAlt, vHomeDist, vTgtHdg, vEC, vDepth;
  portENTER_CRITICAL(&dataMux);
  copyStringSafe(urlLocal, sizeof(urlLocal), googleScriptUrl);
  s1 = layerSurface; s2 = layerMiddle; s3 = layerBottom;
  vPH = valPH; vTDS = valTDS; vNTU = valNTU; vTemp = valTemp;
  vAmp = valAmp; vBat = valVolt3S; vRoll = valRoll; vPitch = valPitch;
  vHdg = valHeading; vDO = valDO; vWQI = valWQI;
  vLat = gpsLat; vLng = gpsLng; vHwMask = hwHealthMask;
  vAlerts = systemAlertFlags; vFlags2 = flags2;
  vSats = gpsSats; vSpd = gpsSpeedKmph; vAlt = gpsAltM;
  vHomeDist = homeDistanceM; vTgtHdg = targetHeadingDeg;
  vEC = valEC; vDepth = winchDepthM;
  portEXIT_CRITICAL(&dataMux);

  static const uint32_t SIM_BAUDS[] = {9600, 115200, 38400, 57600};
  static uint8_t simBaudIdx = 0;
  static unsigned long lastBaudTryMs = 0;

  while (Serial2.available() > 0) {
    char c = (char)Serial2.read();
    if (c == '\n' || c == '\r') {
      if (simRxIdx > 0) {
        simRxLine[simRxIdx] = '\0';
        Serial.printf("[SIM4G-RX] '%s'\n", simRxLine);
        if (strstr(simRxLine, "OK") != nullptr || strstr(simRxLine, "+CSQ:") != nullptr || 
            strstr(simRxLine, "RDY") != nullptr || strstr(simRxLine, "+CPIN") != nullptr || 
            strstr(simRxLine, "Ready") != nullptr || strstr(simRxLine, "+NETOPEN") != nullptr ||
            strstr(simRxLine, "PB DONE") != nullptr) {
          if (!simModemOnline) {
            Serial.printf("[SIM4G-UART] DONG BO THANH CONG o toc do %lu bps! Modem da Online.\n",
                          (unsigned long)SIM_BAUDS[simBaudIdx]);
          }
          simModemOnline = true;
          lastSimOkMs    = now;
        }
        // S-04: Chỉ nhận +HTTPACTION khi đang thực sự chờ; nếu HTTP 200 thì đọc body AT+HTTPREAD để xác nhận "OK_"
        if (simGsState == SIM_GS_WAIT_RESPONSE && strncmp(simRxLine, "+HTTPACTION:", 12) == 0) {
          int method = 0, status = 0, dataLen = 0;
          if (sscanf(simRxLine + 12, "%d,%d,%d", &method, &status, &dataLen) >= 2) {
            simLastHttpCode = status;
            simLastDataLen  = dataLen;
            if (status == 200) {
              if (dataLen > 0) {
                simGsState    = SIM_GS_HTTPREAD;
                simGsStepTick = now;
              } else {
                simGsSuccessCnt++;
                Serial.printf("[SIM4G-GSHEET] Day du lieu len Google Sheets THANH CONG (HTTP %d, Tong: #%lu)!\n",
                              status, (unsigned long)simGsSuccessCnt);
                simGsState    = SIM_GS_HTTPTERM;
                simGsStepTick = now;
              }
            } else {
              simGsFailCnt++;
              Serial.printf("[SIM4G-GSHEET] Day that bai (HTTP %d, Loi: #%lu). Kiem tra SSL/REDIR/APN/URL.\n",
                            status, (unsigned long)simGsFailCnt);
              simGsState    = SIM_GS_HTTPTERM;
              simGsStepTick = now;
            }
          }
        } else if (simGsState == SIM_GS_WAIT_READ) {
          if (strstr(simRxLine, "OK_") != nullptr || strstr(simRxLine, "OK") != nullptr) {
            if (strstr(simRxLine, "OK_") != nullptr) {
              simGsSuccessCnt++;
              Serial.printf("[SIM4G-GSHEET] Xac nhan body '%s' THANH CONG (HTTP 200, Tong: #%lu)!\n",
                            simRxLine, (unsigned long)simGsSuccessCnt);
              simGsState    = SIM_GS_HTTPTERM;
              simGsStepTick = now;
            }
          }
        }
        char* cmdPos = strstr(simRxLine, "CMD:");
        if (cmdPos != nullptr && !hasPendingCore0Cmd) {
          portENTER_CRITICAL(&dataMux);
          strncpy(pendingCore0Cmd, cmdPos, sizeof(pendingCore0Cmd) - 1);
          pendingCore0Cmd[sizeof(pendingCore0Cmd) - 1] = '\0';
          hasPendingCore0Cmd = true;
          portEXIT_CRITICAL(&dataMux);
        }
        simRxIdx = 0;
      }
    } else if (simRxIdx < sizeof(simRxLine) - 1) {
      simRxLine[simRxIdx++] = c;
    }
  }

  // Đọc cổng USB Serial0 từng ký tự Non-blocking 100% (Không bao giờ block luồng!)
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (usbRxIdx > 0) {
        usbRxLine[usbRxIdx] = '\0';
        if (strncmp(usbRxLine, "SET_URL:", 8) == 0) {
          portENTER_CRITICAL(&dataMux);
          strncpy(googleScriptUrl, usbRxLine + 8, sizeof(googleScriptUrl) - 1);
          googleScriptUrl[sizeof(googleScriptUrl) - 1] = '\0';
          reqSaveNvsUrl = true;
          portEXIT_CRITICAL(&dataMux);
          Serial.printf("[USV-CFG] Da luu Google Script URL moi: %s\n", googleScriptUrl);
        } else if (strncmp(usbRxLine, "AT:", 3) == 0) {
          Serial.printf("[SIM4G-TX] Gui lenh AT: %s\n", usbRxLine + 3);
          Serial2.println(usbRxLine + 3);
        } else if (strncmp(usbRxLine, "TEST_4G", 7) == 0) {
          Serial.println("[SIM4G-TEST] Kiem tra toan bo mang 4G LTE va thong tin SIM...");
          Serial2.println("AT+COPS?");
          Serial2.println("AT+CEREG?");
          Serial2.println("AT+NETOPEN");
          Serial2.println("AT+IPADDR");
        } else if (strncmp(usbRxLine, "CALL:", 5) == 0) {
          char phone[20];
          copyStringSafe(phone, sizeof(phone), usbRxLine + 5);
          Serial.printf("[SIM4G-CALL] Dang thuc hien cuoc goi toi so: %s\n", phone);
          Serial2.printf("ATD%s;\r\n", phone);
        } else if (strncmp(usbRxLine, "HANGUP", 6) == 0) {
          Serial.println("[SIM4G-CALL] Ngat cuoc goi...");
          Serial2.println("ATH");
        } else if (strncmp(usbRxLine, "CAL_PH7", 7) == 0) {
          portENTER_CRITICAL(&dataMux);
          snprintf(pendingCore0Cmd, sizeof(pendingCore0Cmd), "CMD:7,90,%d", WINCH_CMD_CALIB_PH_2PT);
          hasPendingCore0Cmd = true;
          portEXIT_CRITICAL(&dataMux);
        } else if (strncmp(usbRxLine, "CAL_PH4", 7) == 0) {
          portENTER_CRITICAL(&dataMux);
          snprintf(pendingCore0Cmd, sizeof(pendingCore0Cmd), "CMD:4,90,%d", WINCH_CMD_CALIB_PH_2PT);
          hasPendingCore0Cmd = true;
          portEXIT_CRITICAL(&dataMux);
        } else if (strncmp(usbRxLine, "CAL_MAG:", 8) == 0) {
          float ox = 0, oy = 0, oz = 0, sx = 1, sy = 1, sz = 1;
          if (sscanf(usbRxLine + 8, "%f,%f,%f,%f,%f,%f", &ox, &oy, &oz, &sx, &sy, &sz) >= 3) {
            portENTER_CRITICAL(&dataMux);
            magOffsetX = ox; magOffsetY = oy; magOffsetZ = oz;
            if (sx > 0.1f) magScaleX = sx;
            if (sy > 0.1f) magScaleY = sy;
            if (sz > 0.1f) magScaleZ = sz;
            reqSaveNvsCalib = true;
            portEXIT_CRITICAL(&dataMux);
            Serial.printf("[USV-CAL] Hieu chuan La ban Hard/Soft-Iron: Ofs(%.1f,%.1f,%.1f) Scl(%.2f,%.2f,%.2f)\n",
                          magOffsetX, magOffsetY, magOffsetZ, magScaleX, magScaleY, magScaleZ);
          }
        } else if (strncmp(usbRxLine, "CAL_PH:", 7) == 0) {
          float targetPH = strtof(usbRxLine + 7, nullptr);
          if (targetPH >= 4.0f && targetPH <= 10.0f) {
            portENTER_CRITICAL(&dataMux);
            phCalibOffset += (targetPH - valPH);
            reqSaveNvsCalib = true;
            portEXIT_CRITICAL(&dataMux);
          }
        } else if (strncmp(usbRxLine, "CAL_BAT:", 8) == 0) {
          portENTER_CRITICAL(&dataMux);
          snprintf(pendingCore0Cmd, sizeof(pendingCore0Cmd), "CBAT:%s", usbRxLine + 8);
          hasPendingCore0Cmd = true;
          portEXIT_CRITICAL(&dataMux);
        } else if (strncmp(usbRxLine, "CAL_IMU", 7) == 0 || strncmp(usbRxLine, "TARE", 4) == 0) {
          portENTER_CRITICAL(&dataMux);
          strcpy(pendingCore0Cmd, "TARE");
          hasPendingCore0Cmd = true;
          portEXIT_CRITICAL(&dataMux);
        } else if (strncmp(usbRxLine, "CAL_TDS:", 8) == 0) {
          float targetTDS = strtof(usbRxLine + 8, nullptr);
          if (targetTDS > 0.0f && valTDS > 1.0f) {
            tdsCalibFactor = constrain(tdsCalibFactor * (targetTDS / valTDS), 0.50f, 2.00f);
            reqSaveNvsCalib = true;
            Serial.printf("[USV-CAL] Hieu chuan TDS: %.0f ppm -> he so %.3f\n", targetTDS, tdsCalibFactor);
          }
        } else if (strncmp(usbRxLine, "CAL_NTU:", 8) == 0) {
          float targetNTU = strtof(usbRxLine + 8, nullptr);
          if (targetNTU >= 0.0f && valNTU > 0.0f) {
            ntuCalibOffset = constrain(ntuCalibOffset + (targetNTU - valNTU), -500.0f, 500.0f);
            reqSaveNvsCalib = true;
            Serial.printf("[USV-CAL] Hieu chuan NTU: %.0f -> offset %.1f\n", targetNTU, ntuCalibOffset);
          }
        } else if (strncmp(usbRxLine, "SMS:", 4) == 0) {
          copyStringSafe(smsAlertNumber, sizeof(smsAlertNumber), usbRxLine + 4);
          reqSaveNvsUrl = true; // dùng chung cờ lưu cấu hình
          Serial.printf("[USV-CFG] So dien thoai canh bao: %s\n", smsAlertNumber);
        } else if (strncmp(usbRxLine, "STATUS", 6) == 0) {
          reqStatusPrint = true;
        } else if (strncmp(usbRxLine, "REBOOT", 6) == 0) {
          reqReboot = true;
        } else if (strncmp(usbRxLine, "FACTORY_RESET", 13) == 0) {
          reqFactoryReset = true;
        } else if (strncmp(usbRxLine, "SAVE", 4) == 0) {
          reqSaveNvsCalib = true;
        } else if (strncmp(usbRxLine, "GOTO_HOME", 9) == 0) {
          portENTER_CRITICAL(&dataMux);
          strcpy(pendingCore0Cmd, "CMD:0,90,10");
          hasPendingCore0Cmd = true;
          portEXIT_CRITICAL(&dataMux);
        } else if (strncmp(usbRxLine, "STOP_ALL", 8) == 0) {
          portENTER_CRITICAL(&dataMux);
          strcpy(pendingCore0Cmd, "CMD:0,90,11");
          hasPendingCore0Cmd = true;
          portEXIT_CRITICAL(&dataMux);
        } else if (strncmp(usbRxLine, "SET_HOME", 8) == 0) {
          portENTER_CRITICAL(&dataMux);
          strcpy(pendingCore0Cmd, "CMD:0,90,6");
          hasPendingCore0Cmd = true;
          portEXIT_CRITICAL(&dataMux);
        } else if (strncmp(usbRxLine, "REPORT", 6) == 0) {
          portENTER_CRITICAL(&dataMux);
          strcpy(pendingCore0Cmd, "CMD:0,90,13");
          hasPendingCore0Cmd = true;
          portEXIT_CRITICAL(&dataMux);
        } else if (strncmp(usbRxLine, "SET_HEADING:", 12) == 0) {
          int deg = constrain(atoi(usbRxLine + 12), 0, 359);
          portENTER_CRITICAL(&dataMux);
          snprintf(pendingCore0Cmd, sizeof(pendingCore0Cmd), "CMD:%d,90,12", deg);
          hasPendingCore0Cmd = true;
          portEXIT_CRITICAL(&dataMux);
        }
        usbRxIdx = 0;
      }
    } else if (usbRxIdx < sizeof(usbRxLine) - 1) {
      usbRxLine[usbRxIdx++] = c;
    }
  }

  // Khởi tạo module SIM 4G theo từng bước: tắt echo, kiểm tra SIM, APN, mở mạng data.
  if (!simModemOnline) {
    if (now - simInitTick >= 2500) {
      simInitTick = now;
      // Tự động quét vòng các tốc độ baud (9600, 115200, 38400, 57600)
      Serial2.begin(SIM_BAUDS[simBaudIdx], SERIAL_8N1, SIM_RX2_PIN, SIM_TX2_PIN);
      Serial.printf("[SIM4G-UART] Dang quet Baud %lu bps (GPIO RX=%d, TX=%d). Gui 'AT'...\n",
                    (unsigned long)SIM_BAUDS[simBaudIdx], SIM_RX2_PIN, SIM_TX2_PIN);
      Serial2.println("AT");
      simBaudIdx = (simBaudIdx + 1) % 4;
      simInitStep = 0;
    }
  } else {
    // Khi modem đã phản hồi online, chạy tuần tự các bước cấu hình mạng
    if (simInitStep < 8 && (now - simInitTick >= 1200)) {
      simInitTick = now;
      switch (simInitStep) {
        case 0: Serial2.println("ATE0"); break;
        case 1: Serial2.println("AT+CPIN?"); break;
        case 2: Serial2.println("AT+CSQ"); break;
        case 3: Serial2.println("AT+CGATT?"); break;
        case 4: Serial2.printf("AT+CGDCONT=1,\"IP\",\"%s\"\r\n", SIM_APN); break;
        case 5: Serial2.println("AT+NETOPEN"); break;
        case 6: Serial2.println("AT+HTTPINIT"); break;
        case 7:
          simNetOpen = true;
          Serial.printf("[SIM4G] Da cau hinh APN '%s' va mo data. San sang day Cloud.\n", SIM_APN);
          break;
      }
      simInitStep++;
    } else if (simGsState == SIM_GS_IDLE && (now - lastSimPingMs >= 10000)) {
      lastSimPingMs = now;
      Serial2.println("AT+CSQ");
    }
  }

  bool loraLost = (now - lastCmdRxTime > LORA_FALLBACK_4G_MS);
  is4GFallback  = loraLost && simModemOnline;
  unsigned long gsInterval = is4GFallback ? 8000UL : GSHEETS_PUSH_INTERVAL_MS;

  bool urlConfigured = isValidGoogleScriptUrl(urlLocal);

  switch (simGsState) {
    case SIM_GS_IDLE:
      if (pendingGsProfile || (now - lastSimTx >= gsInterval)) {
        lastSimTx        = now;
        currentGsIsPrf   = pendingGsProfile;
        pendingGsProfile = false;

        if (urlConfigured) {
          simGsState    = SIM_GS_TERM_PREV;
          simGsStepTick = now;
        } else {
          Serial2.println("AT+CSQ");
        }
      }
      break;

    case SIM_GS_TERM_PREV:
      if (now - simGsStepTick >= 100) {
        Serial2.println("AT+HTTPTERM");
        simGsState    = SIM_GS_HTTPINIT;
        simGsStepTick = now;
      }
      break;

    case SIM_GS_HTTPINIT:
      if (now - simGsStepTick >= 250) {
        Serial2.println("AT+HTTPINIT");
        simGsState    = SIM_GS_SSL_CFG;
        simGsStepTick = now;
      }
      break;

    case SIM_GS_SSL_CFG:
      // S-04: Cấu hình SSL/TLS 1.2 & bỏ qua xác thực CA cho SIM A7680C khi gọi HTTPS Google Apps Script
      if (now - simGsStepTick >= 150) {
        Serial2.println("AT+CSSLCFG=\"sslversion\",0,3");
        Serial2.println("AT+CSSLCFG=\"authmode\",0,0");
        simGsState    = SIM_GS_HTTPSSL;
        simGsStepTick = now;
      }
      break;

    case SIM_GS_HTTPSSL:
      if (now - simGsStepTick >= 150) {
        Serial2.println("AT+HTTPSSL=1");
        simGsState    = SIM_GS_HTTPPARA_REDIR;
        simGsStepTick = now;
      }
      break;

    case SIM_GS_HTTPPARA_REDIR:
      // S-04: Bật tự động theo dõi HTTP 302 Redirect từ script.google.com sang script.googleusercontent.com
      if (now - simGsStepTick >= 150) {
        Serial2.println("AT+HTTPPARA=\"REDIR\",1");
        simGsState    = SIM_GS_HTTPPARA_URL;
        simGsStepTick = now;
      }
      break;

    case SIM_GS_HTTPPARA_URL:
      if (now - simGsStepTick >= 300) {
        char fullUrl[760];
        bool alarmNow = ((vAlerts & (ALERT_MOTOR_JAM | ALERT_HIGH_TILT | ALERT_WATER_QUAL | ALERT_LOW_BATTERY)) != 0) &&
                        (now - lastAlarmPushMs >= GSHEETS_ALARM_COOLDOWN_MS);
        if (alarmNow) lastAlarmPushMs = now;

        if (currentGsIsPrf) {
          snprintf(
            fullUrl, sizeof(fullUrl),
            "AT+HTTPPARA=\"URL\",\"%s?mode=profile&ph1=%.2f&tds1=%.0f&ntu1=%.0f&t1=%.1f&do1=%.2f"
            "&ph2=%.2f&tds2=%.0f&ntu2=%.0f&t2=%.1f&do2=%.2f"
            "&ph3=%.2f&tds3=%.0f&ntu3=%.0f&t3=%.1f&do3=%.2f&src=SIM4G_S3&fw=%s\"",
            urlLocal,
            s1.ph, s1.tds, s1.ntu, s1.temp, s1.doMgL,
            s2.ph, s2.tds, s2.ntu, s2.temp, s2.doMgL,
            s3.ph, s3.tds, s3.ntu, s3.temp, s3.doMgL,
            FW_VERSION
          );
        } else {
          int hwPct = 0;
          for (int i = 0; i < HW_TOTAL_COMPONENTS; i++) if (vHwMask & (1U << i)) hwPct++;
          hwPct = (hwPct * 100) / HW_TOTAL_COMPONENTS;
          snprintf(
            fullUrl, sizeof(fullUrl),
            "AT+HTTPPARA=\"URL\",\"%s?mode=tele&seq=%lu&ph=%.2f&do=%.2f&tds=%.0f&ntu=%.0f"
            "&temp=%.1f&watt=%.1f&amp=%.2f&bat=%.2f&roll=%.1f&pitch=%.1f&hdg=%.0f"
            "&lat=%.5f&lng=%.5f&hw=%d&hwm=%u&alr=%u&wqi=%d&alarm=%d&flags2=%u&fw=%s&src=%s"
            "&ec=%.0f&dep=%.2f&sats=%u&spd=%.2f&alt=%.1f&homed=%.1f&tgthdg=%.0f\"",
            urlLocal, (unsigned long)usvTxSeq,
            vPH, vDO, vTDS, vNTU, vTemp, vBat * vAmp, vAmp, vBat,
            vRoll, vPitch, vHdg, vLat, vLng, hwPct, (unsigned int)vHwMask, (unsigned int)vAlerts, vWQI,
            alarmNow ? 1 : 0, (unsigned int)vFlags2, FW_VERSION,
            is4GFallback ? "SIM4G_FALLBACK" : "SIM4G_S3",
            vEC, vDepth, (unsigned int)vSats, vSpd, vAlt, vHomeDist, vTgtHdg
          );
        }
        Serial2.println(fullUrl);
        simGsState    = SIM_GS_HTTPACTION;
        simGsStepTick = now;
      }
      break;

    case SIM_GS_HTTPACTION:
      if (now - simGsStepTick >= 350) {
        Serial2.println("AT+HTTPACTION=0");
        simGsState    = SIM_GS_WAIT_RESPONSE;
        simGsStepTick = now;
      }
      break;

    case SIM_GS_WAIT_RESPONSE:
      if (now - simGsStepTick >= SIM_HTTP_TIMEOUT_MS) {
        simGsFailCnt++;
        simGsState    = SIM_GS_HTTPTERM;
        simGsStepTick = now;
      }
      break;

    case SIM_GS_HTTPREAD:
      if (now - simGsStepTick >= 150) {
        Serial2.printf("AT+HTTPREAD=0,%d\r\n", constrain(simLastDataLen, 1, 128));
        simGsState    = SIM_GS_WAIT_READ;
        simGsStepTick = now;
      }
      break;

    case SIM_GS_WAIT_READ:
      if (now - simGsStepTick >= 2000) {
        // Nếu đã nhận HTTP 200 nhưng body trả về khác chuẩn, vẫn đóng phiên an toàn
        simGsSuccessCnt++;
        simGsState    = SIM_GS_HTTPTERM;
        simGsStepTick = now;
      }
      break;

    case SIM_GS_HTTPTERM:
      if (now - simGsStepTick >= 200) {
        Serial2.println("AT+HTTPTERM");
        simGsState = SIM_GS_IDLE;
      }
      break;
  }

  bool criticalEvent = (vAlerts & (ALERT_MOTOR_JAM | ALERT_HIGH_TILT | ALERT_WATER_QUAL | ALERT_LOW_BATTERY)) != 0;
  if (criticalEvent && !emergencyAlertSent) {
    emergencyAlertSent = true;
    Serial.printf("[SIM4G-ALARM] Canh bao khan cap: Flags=%u pH=%.2f DO=%.2f WQI=%d Bat=%.2fV\n",
                  (unsigned int)vAlerts, vPH, vDO, vWQI, vBat);
  } else if (!criticalEvent) {
    emergencyAlertSent = false;
  }
}

void updateSmartPowerManagement() {
  unsigned long now = millis();
  // Chỉ Deep Sleep khi ĐÃ CẮM PIN và điện áp MỖI CELL thực sự kiệt (< 3.10V/cell).
  // Cách này tránh Deep Sleep nhầm với pack 2S đang đầy (8.2..8.75V).
  if (batValid && isBatteryCritical(valVolt3S)) {
    if (lowBatDangerStart == 0) {
      lowBatDangerStart = now;
      Serial.printf("[USV-POWER] Phat hien pin kiet: %.2fV (%.2fV/cell). Cho %lu giay...\n",
                    valVolt3S, calcBatteryCellVolt(valVolt3S),
                    (unsigned long)(BAT_DEEP_SLEEP_HOLD_MS / 1000));
    } else if (now - lowBatDangerStart >= BAT_DEEP_SLEEP_HOLD_MS) {
      reqEnterDeepSleep = true;
    }
  } else {
    lowBatDangerStart = 0;
  }
}

// =========================================================================
// 9B. TIỆN ÍCH CHẨN ĐOÁN QUA USB SERIAL (STATUS / FACTORY_RESET)
// =========================================================================
void printStatusReport() {
  int hwCount = 0;
  for (int i = 0; i < HW_TOTAL_COMPONENTS; i++) if (hwHealthMask & (1U << i)) hwCount++;
  Serial.println(F("\n================ CHEMSTATION AI — STATUS ================"));
  Serial.printf("FW:%s | Protocol:%d | Boot:#%lu | Uptime:%lu s\n",
                FW_VERSION, PROTOCOL_VERSION, (unsigned long)bootCount, millis() / 1000UL);
  Serial.printf("Battery: %.2fV (%d%%) | valid:%d | cells:%d | cell:%.2fV\n",
                valVolt3S, calcBattery3SPercent(valVolt3S), (int)batValid,
                calcBatteryCellCount(valVolt3S), calcBatteryCellVolt(valVolt3S));
  Serial.printf("Water: pH=%.2f TDS=%.0f NTU=%.0f Temp=%.1f DO=%.2f WQI=%d\n",
                valPH, valTDS, valNTU, valTemp, valDO, valWQI);
  Serial.printf("IMU: Roll=%.1f Pitch=%.1f Heading=%.0f | MPU:%d GY271:%d\n",
                valRoll, valPitch, valHeading, (int)hasMPU6050, (int)hasGY271);
  Serial.printf("GPS: fix:%d sats:%u hdop:%.1f lat=%.5f lng=%.5f alt=%.1fm spd=%.2fkm/h\n",
                (int)gpsFixValid, (unsigned int)gpsSats, gpsHdop, gpsLat, gpsLng, gpsAltM, gpsSpeedKmph);
  Serial.printf("Home: %.5f,%.5f | dist=%.1fm | returnHome:%d (FSM:%u)\n",
                homeGpsLat, homeGpsLng, homeDistanceM, (int)returnHomeActive, (unsigned int)rthState);
  Serial.printf("Winch: mode=%d depth=%.2fm mud=%d | AutoStage:%d\n",
                winchMode, winchDepthM, (int)mudBottomLatched, (int)autoStage);
  Serial.printf("Motor: cmd=%d cur=%d (MaxSafe:%d) | Rudder:%d | HeadingHold:%d target=%.0f\n",
                cmdMotor, curMotor, MOTOR_MAX_SAFE_PWM, cmdRudder, (int)headingHoldActive, targetHeadingDeg);
  Serial.printf("HW mask: 0x%03X (%d/%d) | ADC fault: 0x%03X | Alerts: 0x%02X\n",
                (unsigned int)hwHealthMask, hwCount, HW_TOTAL_COMPONENTS,
                (unsigned int)adcFaultMask, (unsigned int)systemAlertFlags);
  Serial.printf("LoRa: ready=%d TX_OK=%lu TX_FAIL=%lu | TxInterval:%lu ms\n",
                (int)loraReady, (unsigned long)loraTxOkCnt, (unsigned long)loraTxFailCnt, (unsigned long)loraTxIntervalMs);
  Serial.printf("SIM4G: online=%d net=%d http=%d ok=%lu fail=%lu | APN:%s\n",
                (int)simModemOnline, (int)simNetOpen, simLastHttpCode,
                (unsigned long)simGsSuccessCnt, (unsigned long)simGsFailCnt, SIM_APN);
  Serial.printf("Google URL: %s\n", googleScriptUrl);
  Serial.printf("Calib: pH_ofs=%.3f pH_v7=%.3f pH_slp=%.4f TDS=%.3f NTU=%.1f BatRatio=%.3f | SMS:%s\n",
                phCalibOffset, phRefVolt7, phSlopeVPerUnit, tdsCalibFactor, ntuCalibOffset, batDividerRatio,
                smsAlertNumber[0] ? smsAlertNumber : "(chua dat)");
  Serial.println(F("=========================================================\n"));
}

void factoryResetNvs() {
  Serial.println(F("[USV-CFG] DANG XOA TOAN BO CAU HINH NVS VA KHOI DONG LAI..."));
  nvsPrefs.clear();
  nvsPrefs.end();
  delay(300);
  ESP.restart();
}

void sensorAndCloudTaskCore0(void* pvParameters) {
  (void)pvParameters;
  TickType_t lastWakeTime = xTaskGetTickCount();
  for (;;) {
    core0LoopCnt++;
    esp_task_wdt_reset();
    enforceHardwareMotorSafetyGuard();
    updateAllSensors();
    updateSim4GDualLink();
    updateSmartPowerManagement();
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(50));
  }
}

// =========================================================================
// 10. SETUP & VÒNG LẶP THỜI GIAN THỰC CORE 1 (LORA RF, MOTOR PID & WINCH FSM)
// =========================================================================
void setup() {
  // H-05: Ép tất cả chân công suất & chân strapping (GPIO 45/46/40/41/42) xuống LOW ngay lập tức tại dòng 1 của setup()
  // trước khi khởi tạo bất kỳ ngoại vi nào để chống giật cơ cấu chấp hành / kích nổ nhầm van CO2 khi cấp nguồn!
  pinMode(PIN_CO2_TRIGGER, OUTPUT);
  digitalWrite(PIN_CO2_TRIGGER, LOW);
  pinMode(PIN_SIREN_BUZZER, OUTPUT);
  digitalWrite(PIN_SIREN_BUZZER, LOW);
  pinMode(PIN_MOTOR_PWM, OUTPUT);
  digitalWrite(PIN_MOTOR_PWM, LOW);
  pinMode(PIN_MOTOR_IN1, OUTPUT);
  digitalWrite(PIN_MOTOR_IN1, LOW);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  digitalWrite(PIN_MOTOR_IN2, LOW);

  setBuoyRgbLed(35, 35, 35); // Trắng: Đang khởi động & Tự chẩn đoán phần cứng
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  adcAttachPin(PIN_BAT_3S);        analogSetPinAttenuation(PIN_BAT_3S, ADC_11db);
  adcAttachPin(PIN_TDS);           analogSetPinAttenuation(PIN_TDS, ADC_11db);
  adcAttachPin(PIN_TURBIDITY);     analogSetPinAttenuation(PIN_TURBIDITY, ADC_11db);
  adcAttachPin(PIN_PH);            analogSetPinAttenuation(PIN_PH, ADC_11db);
  adcAttachPin(PIN_ACS712);        analogSetPinAttenuation(PIN_ACS712, ADC_11db);

  // Khởi tạo RTC Pull-Down trên toàn bộ 5 chân ADC1 (GPIO 3, 4, 5, 7, 8)
  // để xả sạch điện tích trôi nổi về GND khi chân hở mạch/chưa cắm linh kiện thật!
  const gpio_num_t adc_pins[] = {GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_7, GPIO_NUM_8};
  for (int i = 0; i < 5; i++) {
    rtc_gpio_init(adc_pins[i]);
    rtc_gpio_pullup_dis(adc_pins[i]);
    rtc_gpio_pulldown_en(adc_pins[i]);
  }

  nvsPrefs.begin(NVS_NAMESPACE_BUOY, false);
  bootCount = nvsPrefs.getUInt("boot_cnt", 0) + 1;
  nvsPrefs.putUInt("boot_cnt", bootCount);
  esp_reset_reason_t rr = esp_reset_reason();
  const char* rrTxt = "UNKNOWN";
  switch (rr) {
    case ESP_RST_POWERON:  rrTxt = "POWERON"; break;
    case ESP_RST_EXT:      rrTxt = "EXT_PIN"; break;
    case ESP_RST_SW:       rrTxt = "SOFTWARE"; break;
    case ESP_RST_PANIC:    rrTxt = "PANIC/CRASH"; break;
    case ESP_RST_INT_WDT:  rrTxt = "INT_WDT"; break;
    case ESP_RST_TASK_WDT: rrTxt = "TASK_WDT"; break;
    case ESP_RST_WDT:      rrTxt = "WDT"; break;
    case ESP_RST_DEEPSLEEP:rrTxt = "DEEPSLEEP"; break;
    case ESP_RST_BROWNOUT: rrTxt = "BROWNOUT"; break;
    case ESP_RST_SDIO:     rrTxt = "SDIO"; break;
    default: break;
  }
  Serial.printf("\n[USV-BOOT] FW %s | Lan khoi dong #%lu | Ly do reset: %s\n",
                FW_VERSION, (unsigned long)bootCount, rrTxt);
  if (rr == ESP_RST_BROWNOUT) {
    brownoutCooldownUntil = millis() + 6000UL;
    Serial.println(F("[USV-POWER] *** CANH BAO: KHOI DONG LAI DO BROWNOUT (SAP NGUON)! Khoa dong co 6 giay on dinh... ***"));
  }

  if (nvsPrefs.isKey("gs_url")) {
    String savedUrl = nvsPrefs.getString("gs_url", DEFAULT_GOOGLE_SCRIPT_URL);
    copyStringSafe(googleScriptUrl, sizeof(googleScriptUrl), savedUrl.c_str());
  }
  if (nvsPrefs.isKey("ph_ofs")) {
    phCalibOffset = nvsPrefs.getFloat("ph_ofs", 0.0f);
    nvsCalibrated = true;
  }
  if (nvsPrefs.isKey("ph_v7")) {
    phRefVolt7 = constrain(nvsPrefs.getFloat("ph_v7", 2.50f), 1.0f, 3.2f);
  }
  if (nvsPrefs.isKey("ph_slp")) {
    phSlopeVPerUnit = constrain(nvsPrefs.getFloat("ph_slp", 0.18f), 0.08f, 0.35f);
  }
  if (nvsPrefs.isKey("mx_ofs")) {
    magOffsetX = nvsPrefs.getFloat("mx_ofs", 0.0f);
    magOffsetY = nvsPrefs.getFloat("my_ofs", 0.0f);
    magOffsetZ = nvsPrefs.getFloat("mz_ofs", 0.0f);
    magScaleX  = constrain(nvsPrefs.getFloat("mx_scl", 1.0f), 0.2f, 5.0f);
    magScaleY  = constrain(nvsPrefs.getFloat("my_scl", 1.0f), 0.2f, 5.0f);
    magScaleZ  = constrain(nvsPrefs.getFloat("mz_scl", 1.0f), 0.2f, 5.0f);
  }
  if (nvsPrefs.isKey("bat_rat")) {
    batDividerRatio = nvsPrefs.getFloat("bat_rat", 4.30f);
    // H-03 & P0-5: Cho phép dải hiệu chuẩn cầu phân áp hợp lệ 1.5..8.5 (mặc định chuẩn 33k/10k = 4.30f)
    if (batDividerRatio < 1.5f || batDividerRatio > 8.5f) {
      batDividerRatio = 4.30f;
      nvsPrefs.putFloat("bat_rat", batDividerRatio);
    }
    nvsCalibrated = true;
  }
  if (nvsPrefs.isKey("tds_fac")) {
    tdsCalibFactor = nvsPrefs.getFloat("tds_fac", 1.00f);
    nvsCalibrated = true;
  }
  if (nvsPrefs.isKey("ntu_ofs")) {
    ntuCalibOffset = nvsPrefs.getFloat("ntu_ofs", 0.00f);
    nvsCalibrated = true;
  }
  if (nvsPrefs.isKey("sms_num")) {
    String num = nvsPrefs.getString("sms_num", "");
    copyStringSafe(smsAlertNumber, sizeof(smsAlertNumber), num.c_str());
  }
  if (nvsPrefs.isKey("home_lat")) {
    homeGpsLat = nvsPrefs.getFloat("home_lat", 0.0f);
    homeGpsLng = nvsPrefs.getFloat("home_lng", 0.0f);
  }
  if (nvsPrefs.isKey("r_ofs")) {
    rollTareOffset  = nvsPrefs.getFloat("r_ofs", 0.0f);
    pitchTareOffset = nvsPrefs.getFloat("p_ofs", 0.0f);
  }
  // Chỉ nạp lại 3 tầng nước nếu người dùng ĐÃ TỪNG ĐO THẬT (có cờ "_ok").
  loadWaterLayerSample(layerSurface, "l1");
  loadWaterLayerSample(layerMiddle,  "l2");
  loadWaterLayerSample(layerBottom,  "l3");

  pinMode(PIN_MOTOR_IN1, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  ledcSetup(MOTOR_LEDC_CH, MOTOR_LEDC_FREQ, MOTOR_LEDC_RES);
  ledcAttachPin(PIN_MOTOR_PWM, MOTOR_LEDC_CH);
  driveMotor(0);

  // Cả 2 công tắc hành trình CT1 (38) và CT2 (39) đều dùng INPUT_PULLUP, kích hoạt mức LOW (FALLING)
  pinMode(PIN_CT1_BOTTOM, INPUT_PULLUP);
  pinMode(PIN_CT2_TOP_HOME, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_CT1_BOTTOM), isrBottomMud, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_CT2_TOP_HOME), isrTopHome, FALLING);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  winchServo.setPeriodHertz(50);
  rudderServo.setPeriodHertz(50);
  winchServo.attach(PIN_SERVO_WINCH, 500, 2400);
  rudderServo.attach(PIN_SERVO_RUDDER, 500, 2400);
  winchServo.write(90);
  rudderServo.write(90);

  ds18b20.begin();
  ds18b20.setWaitForConversion(false);
  ds18b20.requestTemperatures();
  lastTempReqTick = millis();

  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_SCL_PIN, INPUT_PULLUP);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
  Wire.setTimeOut(15);
  scanAndRecoverI2C(true);

  Serial2.begin(9600, SERIAL_8N1, SIM_RX2_PIN, SIM_TX2_PIN);
  Serial1.begin(9600, SERIAL_8N1, GPS_RX1_PIN, GPS_TX1_PIN);
  Serial2.println("AT");
  simInitStep = 0;
  simInitTick = millis();

  pinMode(LORA_NSS_PIN, OUTPUT);
  digitalWrite(LORA_NSS_PIN, HIGH);
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_NSS_PIN);
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
    loraReady = true;
    Serial.println("[USV] LoRa Ra-02 (NSS=15, SCK=11) SAN SANG! (CRC HW: ON)");
  } else {
    Serial.println("[USV-ERR] Khong tim thay module LoRa Ra-02!");
  }

  esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
  esp_task_wdt_add(NULL);

  xTaskCreatePinnedToCore(
    sensorAndCloudTaskCore0,
    "SensorCloudCore0",
    12288,
    nullptr,
    2,
    &core0TaskHandle,
    0
  );
  if (core0TaskHandle) {
    esp_task_wdt_add(core0TaskHandle);
  }

  // Kết nối Wi-Fi STA "Mang bao tri" trực tiếp trên ESP32-S3
  if (strlen(DEFAULT_STA_WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(DEFAULT_STA_WIFI_SSID, DEFAULT_STA_WIFI_PASS);
    Serial.printf("[USV-WIFI] Dang ket noi Wi-Fi '%s'...\n", DEFAULT_STA_WIFI_SSID);
  }

  lastCmdRxTime   = millis();
  lastHzStatMs    = millis();
  lastDepthCalcMs = millis();
  triggerLedFlash(0, 50, 0, 600); // Xanh Lá 600ms: Khởi động thành công!
}

void loop() {
  esp_task_wdt_reset();
  updateBuoyStatusLed();
  core1LoopCnt++;
  unsigned long now = millis();

  if (now - lastHzStatMs >= 1000) {
    core0Hz      = (uint16_t)core0LoopCnt;
    core1Hz      = (uint16_t)constrain(core1LoopCnt, 1UL, 9999UL);
    core0LoopCnt = 0;
    core1LoopCnt = 0;
    lastHzStatMs = now;
  }

  // Xử lý yêu cầu lưu NVS hoặc lệnh từ Core 0 chuyển sang (100% Thread-Safe trên Core 1)
  if (reqSaveNvsUrl) {
    reqSaveNvsUrl = false;
    nvsPrefs.putString("gs_url", googleScriptUrl);
    Serial.printf("[USV-GSHEETS] Da luu Google Script URL vao NVS: %s\n", googleScriptUrl);
  }
  if (reqSaveNvsCalib) {
    reqSaveNvsCalib = false;
    nvsPrefs.putFloat("ph_ofs", phCalibOffset);
    nvsPrefs.putFloat("ph_v7",  phRefVolt7);
    nvsPrefs.putFloat("ph_slp", phSlopeVPerUnit);
    nvsPrefs.putFloat("mx_ofs", magOffsetX);
    nvsPrefs.putFloat("my_ofs", magOffsetY);
    nvsPrefs.putFloat("mz_ofs", magOffsetZ);
    nvsPrefs.putFloat("mx_scl", magScaleX);
    nvsPrefs.putFloat("my_scl", magScaleY);
    nvsPrefs.putFloat("mz_scl", magScaleZ);
    nvsPrefs.putFloat("tds_fac", tdsCalibFactor);
    nvsPrefs.putFloat("ntu_ofs", ntuCalibOffset);
    nvsPrefs.putFloat("bat_rat", batDividerRatio);
    nvsPrefs.putString("sms_num", smsAlertNumber);
    nvsCalibrated = true;
    Serial.printf("[USV-CAL] Da luu NVS: pH_ofs=%.3f pH_v7=%.3f pH_slp=%.4f TDS=%.3f NTU=%.1f BatRatio=%.3f\n",
                  phCalibOffset, phRefVolt7, phSlopeVPerUnit, tdsCalibFactor, ntuCalibOffset, batDividerRatio);
  }
  if (reqStatusPrint) {
    reqStatusPrint = false;
    printStatusReport();
  }
  if (reqFactoryReset) {
    reqFactoryReset = false;
    factoryResetNvs();
  }
  if (reqReboot) {
    reqReboot = false;
    Serial.println("[USV-CFG] Yeu cau khoi dong lai tu USB Serial...");
    delay(100);
    ESP.restart();
  }
  if (hasPendingCore0Cmd) {
    char localCmd[180];
    portENTER_CRITICAL(&dataMux);
    copyStringSafe(localCmd, sizeof(localCmd), pendingCore0Cmd);
    hasPendingCore0Cmd = false;
    portEXIT_CRITICAL(&dataMux);
    applyIncomingCommand(localCmd, -65, 10.0f, "4G");
  }
  if (reqEnterDeepSleep) {
    reqEnterDeepSleep = false;
    Serial.printf("[USV-POWER] Pin kiet (%.2fV, %.2fV/cell)! Luu NVS va vao Deep Sleep %llu giay...\n",
                  valVolt3S, calcBatteryCellVolt(valVolt3S), (unsigned long long)BAT_DEEP_SLEEP_SEC);
    driveMotor(0);
    driveWinch(WINCH_CMD_STOP);
    nvsPrefs.putFloat("last_bat", valVolt3S);
    nvsPrefs.putFloat("home_lat", (float)homeGpsLat);
    nvsPrefs.putFloat("home_lng", (float)homeGpsLng);
    if (loraReady) LoRa.sleep();
    delay(50);
    esp_deep_sleep(BAT_DEEP_SLEEP_SEC * 1000000ULL);
  }

  // 1. Cập nhật máy trạng thái Tời, Đo độ sâu cáp, Công tắc hành trình & Auto đo 3 tầng
  updateWinchAndAutoProfile();



  if (loraReady) {
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0) {
      char rxBuf[LORA_PACKET_MAX_LEN];
      size_t idx = 0;
      while (LoRa.available() && idx < sizeof(rxBuf) - 1) {
        rxBuf[idx++] = (char)LoRa.read();
      }
      rxBuf[idx] = '\0';
      int rxRssi = LoRa.packetRssi();
      float rxSnr = LoRa.packetSnr();
      while (LoRa.available()) LoRa.read();

      Serial.printf("[USV-RAW-RX len=%d] RSSI:%d SNR:%.1f | %s\n", packetSize, rxRssi, rxSnr, rxBuf);

      applyIncomingCommand(rxBuf, rxRssi, rxSnr, "LORA");
    }
  }

  // 3. Cơ chế Fail-safe mất sóng điều khiển:
  // 3A. Mất lệnh ngắn (> 2.2s): Dừng khẩn cấp động cơ tránh đâm va trôi tự do (Chỉ áp dụng khi lái tay, KHÔNG hủy Neo Ảo)
  if (!returnHomeActive && !virtualAnchorActive && now - lastCmdRxTime > FAILSAFE_TIMEOUT_MS) {
    if (cmdMotor != 0 || cmdRudder != 90) {
      cmdMotor  = 0;
      curMotor  = 0;
      cmdRudder = 90;
      driveMotor(0);
      rudderServo.write(90);
      if (autoStage == AUTO_STAGE_OFF && winchMode == 1) {
        driveWinch(WINCH_CMD_STOP);
      }
    }
  }

  // 3B. S-05: Mất tín hiệu kép (LoRa > FAILSAFE_RADIO_BLACKOUT_RTH_MS = 25s VÀ 4G Cloud > 25s): Tự động Return-To-Home GPS
  bool loraLost = (now - lastCmdRxTime > FAILSAFE_RADIO_BLACKOUT_RTH_MS);
  bool simLost  = (!simModemOnline || (now - lastSimOkMs > FAILSAFE_RADIO_BLACKOUT_RTH_MS));
  if (!batTier3CutoffActive && !returnHomeActive && loraLost && simLost) {
    if (gpsFixValid && (homeGpsLat != 0.0 || homeGpsLng != 0.0) && homeDistanceM > 5.0f) {
      virtualAnchorActive = false; // Mất sóng kéo dài -> hủy Neo Ảo để tự động quay về bờ
      returnHomeActive   = true;
      rthState           = RTH_ARMED;
      rthStartMs         = now;
      headingHoldActive  = true;
      pidHeadingIntegral = 0.0f;
      lastPidTickMs      = 0;
      cmdMotor           = RETURN_HOME_MOTOR_PWM;
      if (winchDepthM > 0.05f && winchMode == 0 && !isReliftingFromMud) {
        driveWinch(WINCH_CMD_HOME);
      }
      Serial.printf("[USV-FAILSAFE] Mat song LoRa & 4G > %lus! Tu dong kich hoat Return-To-Home GPS ve (%.5f, %.5f), cach %.1fm\n",
                    (unsigned long)(FAILSAFE_RADIO_BLACKOUT_RTH_MS / 1000UL), homeGpsLat, homeGpsLng, homeDistanceM);
    }
  }

  // 4. Cập nhật Failsafe Pin Đa Tầng, Tốc độ động cơ Soft-Start, Tự hành về Home, Autopilot PID & Bảo vệ quá dòng
  updateMultiTierBatteryFailsafes();
  updateReturnHome();
  updateMotorSoftStart();
  updateAutopilotHeadingHold();
  updateAntiJamProtection();

  // 5. S-06: Gửi gói Telemetry về Trạm bờ (Phản hồi đồng bộ 35ms hoặc định kỳ loraTxIntervalMs mặc định 1000ms)
  if ((pendingTelemetryReply && (now - cmdReceivedAtTick >= 35)) ||
      (now - lastLoraTx >= loraTxIntervalMs)) {
    pendingTelemetryReply = false;
    sendLoRaTelemetry();
  }

  // 6. Tự phục hồi LoRa nếu treo SPI
  checkAndRecoverLoRa();
  delay(2);
}
