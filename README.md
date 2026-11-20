# ChemStation AI v4.3-AUDIT-PRO — Hệ thống Phao quan trắc nước tự hành (USV) & Trạm bờ (GCS)

Firmware ESP32-S3 (phao) + ESP32 (trạm bờ) cho đề tài KHKT: đo pH / TDS / EC / NTU / DO / nhiệt độ,
điều khiển L298N bằng joystick, tự đo 3 tầng nước, tự hành về Home GPS, và đẩy dữ liệu lên
Google Sheets qua Wi-Fi hoặc SIM 4G.

> **Nguyên tắc dữ liệu:** Không bao giờ báo giá trị ảo. Linh kiện chưa cắm → hiển thị
> `CHUA CAM` / `THIEU` / `0`, không dùng giá trị mặc định giả (7.0 pH, 28 °C, 10.9574°…).

---

## 1. Tính năng chính

| Nhóm | Tính năng |
|---|---|
| Đo nước | pH (PH-4502C), TDS, EC, độ đục NTU, DO ước lượng, nhiệt độ DS18B20, chỉ số VN-WQI |
| Điện | Đo pin chính xác theo từng cell (tự nhận diện 1S/2S/3S), dòng ACS712, công suất W, điện năng Wh |
| Cơ khí | L298N + joystick vô cấp, 7 công tắc, servo bánh lái, servo tời MG996R, chống kẹt chân vịt, chống đứt cáp |
| Tự động | Auto đo 3 tầng (0.3 m – 1.0 m – 1.8 m), nhích tời `INCH_DOWN`/`INCH_UP` 10 cm, sục rửa đầu dò 30 s (`CLEAN_SENSOR`), bơm lấy mẫu 20 s (`TAKE_SAMPLE`) |
| Autopilot | PID giữ hướng la bàn, Neo Ảo `VIRTUAL_ANCHOR` (Dynamic Positioning), `GOTO_HOME` FSM 6 bước theo GPS (Median-3 + EMA + HDOP), `STOP_ALL`, `SET_HEADING` |
| GPS & IMU | NEO-6M (vệ tinh, tốc độ, độ cao, HDOP, khoảng cách Home) + MPU6050 & La bàn GY-271 (Hiệu chuẩn Hard-Iron/Soft-Iron 3D) |
| Liên lạc | LoRa Ra-02 433 MHz (bắt buộc CRC-8 có khóa, RSSI & SNR, gói < 250 B, chu kỳ 1 Hz), dự phòng 4G SSL/TLS (`AT+HTTPSSL=1`), RTH sau 25 s mất sóng |
| Bản đồ 3D | Không gian 3D WebGL (`/3d`) hiển thị trực tiếp cột nước 3 tầng, địa hình lòng hồ, thanh trượt lịch sử (`Time Slider /api/history`), tách biệt hoàn toàn chế độ Demo |
| Bảo mật & Cloud | Xác thực Quản trị (`X-GCS-Token` / `/api/login`), OTA kiểm tra Magic Byte `0xE9` + MD5 + Rollback, xoay vòng tự động `gsheets_offline.csv` (5 MB) & `gsheets_bridge.log` (2 MB), hàng đợi Retry bền vững `gsheets_retry_queue.json` |
| Chẩn đoán | Bitmask 11 linh kiện, phát hiện ADC vượt dải, nhiệt độ chip S3/GCS, log Serial |

---

## 2. Cấu trúc thư mục

```
.
├─ include/usv_common.h        # Giao thức, bitmask, helper dùng chung 2 firmware
├─ src/usv_buoy/main.cpp       # Firmware phao ESP32-S3
├─ src/ground_station/main.cpp # Firmware trạm bờ ESP32 + TFT
├─ platformio.ini              # 2 môi trường build
├─ google_sheets_script.gs     # Google Apps Script (Web App + email cảnh báo)
├─ gsheets_live_bridge.py      # Cầu nối Serial -> Google Sheets trên PC
└─ README.md
```

---

## 3. Sơ đồ chân

### 3.1 Phao — ESP32-S3-WROOM-1

| Chức năng | GPIO | Ghi chú |
|---|---|---|
| Pin 3S (cầu 33k/10k) | 3 | ADC1 |
| TDS | 4 | ADC1 |
| Độ đục NTU | 5 | ADC1 (AO) |
| DS18B20 | 6 | 1-Wire, trở 4.7k lên 3.3 V |
| pH | 7 | ADC1 (Po) |
| ACS712 | 8 | ADC1 |
| I2C SDA / SCL | 1 / 2 | MPU6050 0x68 + GY-271 0x0D |
| CT1 chống chùng bùn | 38 | NO → GND |
| CT2 Home | 39 | NO → GND |
| LoRa NSS / SCK / MISO / MOSI | 15 / 11 / 13 / 14 | Ra-02 433 MHz |
| LoRa RST / DIO0 | 21 / 47 | |
| Servo tời | 9 | MG996R 360° |
| Servo bánh lái | 10 | MG90S |
| L298N ENA / IN1 / IN2 | 16 / 40 / 12 | PWM LEDC ch 4 |
| SIM4G TX2 / RX2 | 41 / 42 | Chéo TX↔RX với module |
| GPS TX1 / RX1 | 17 / 18 | Chéo với NEO-6M |
| LED RGB WS2812 | 48 | Báo trạng thái |

### 3.2 Trạm bờ — ESP32 DevKit + TFT 240×320 (ILI9342)

| Chức năng | GPIO |
|---|---|
| TFT CS / DC / RST | 5 / 2 / 4 |
| TFT MOSI / MISO / SCLK | 23 / 19 / 18 |
| LoRa NSS / RST / DIO0 | 15 / 16 / 17 |
| Joystick VRY / VRX | 34 / 35 |
| Joystick 2 VRY (tời) | 36 |
| Nút joystick | 39 |
| SW1…SW7 | 32, 33, 25, 26, 27, 14, 13 |
| SIM4G RX2 / TX2 | 21 / 22 |

---

## 4. Build & nạp firmware

Yêu cầu: [PlatformIO](https://platformio.org/) (VS Code extension hoặc CLI).

```bash
# Phao ESP32-S3
pio run -e esp32-s3-devkitc-1
pio run -e esp32-s3-devkitc-1 -t upload

# Trạm bờ ESP32
pio run -e esp32dev_gcs
pio run -e esp32dev_gcs -t upload
```

> **Quan trọng (trạm bờ):** `platformio.ini` dùng `board_build.partitions = min_spiffs.csv`
> để có 2 slot OTA (1.875 MB/slot). Lần đầu sau khi đổi bảng phân vùng phải nạp qua USB;
> các lần sau có thể nạp OTA tại `http://192.168.4.1/update`.

Cổng nạp mặc định: phao `COM13`, trạm bờ `COM11` — sửa `upload_port` trong `platformio.ini`
hoặc dùng `pio run -e ... -t upload --upload-port COMx`.

---

## 5. Trạm bờ: Wi-Fi, Web Dashboard, OTA

Sau khi khởi động, trạm bờ phát Access Point:

- SSID: `ChemStation-AI-GCS`
- Mật khẩu: `12345678`
- Địa chỉ: `http://192.168.4.1` hoặc `http://chemstation.local` (mDNS)

### 5.1 Cấu hình

Vào mục **Đồng Bộ Google Sheets & Cấu Hình Trạm Bờ** trên dashboard:

- **Google Apps Script URL**: dán URL `/exec` sau khi deploy (mục 6).
- **Wi-Fi STA**: SSID/mật khẩu Wi-Fi nhà hoặc hotspot điện thoại. Trạm bờ chạy
  AP+STA song song; mất STA sẽ tự kết nối lại mỗi 15 s.
- Cấu hình được lưu vào NVS (`chem_gcs`), giữ nguyên sau khi reboot.
- Nút **SYNC URL XUỐNG PHAO** gửi URL mới xuống phao qua LoRa (`GURL:`), phao lưu NVS.

### 5.2 Nút điều khiển từ xa

| Nút | Lệnh LoRa | Chức năng |
|---|---|---|
| LƯU HOME GPS | `CMD:0,90,6` | Lưu vị trí neo hiện tại (chỉ khi GPS fix) |
| TỰ VỀ HOME | `CMD:0,90,10` | Autopilot GPS quay về Home |
| DỪNG TẤT CẢ | `CMD:0,90,11` | Dừng động cơ, tời, autopilot, auto 3 tầng |
| GIỮ HƯỚNG BẬT/TẮT | `4` / `5` | PID khóa hướng la bàn |
| ĐẶT HƯỚNG | `CMD:<0..359>,90,12` | Đặt hướng mục tiêu |
| BÁO CÁO NGAY | `CMD:0,90,13` | Phao gửi telemetry ngay |
| PUSH TELE / PUSH 3 TẦNG | HTTP | Đẩy thủ công lên Google Sheets |

### 5.3 OTA

Mở `http://192.168.4.1/update`, chọn `firmware.bin` (trong `.pio/build/esp32dev_gcs/`),
bấm **BẮT ĐẦU NẠP**. Không tắt nguồn trong lúc nạp.

### 5.4 Web API

| Endpoint | Method | Mô tả |
|---|---|---|
| `/api/telemetry` | GET | JSON đầy đủ (telemetry + GPS + cloud + system) |
| `/api/cmd?m=&r=&w=` | GET | Điều khiển L298N (m: -255..255, r: 45..135, w: 0..9) |
| `/api/cmd?page=0..2` | GET | Đổi trang TFT |
| `/api/cmd?gear=0..2` | GET | ECO / CRUISE / TURBO |
| `/api/cmd?caljoy=1` | GET | Hiệu chuẩn tâm joystick |
| `/api/cmd?cbat=<V>` | GET | Hiệu chuẩn pin từ xa |
| `/api/cmd?tare=1` | GET | Tare góc MPU6050 |
| `/api/cmd?home=1` | GET | Lưu Home GPS |
| `/api/cmd?goto=1` | GET | Tự về Home |
| `/api/cmd?stopall=1` | GET | Dừng tất cả |
| `/api/cmd?hdg_on=1` / `hdg_off=1` | GET | Bật/tắt giữ hướng |
| `/api/cmd?hdg=<0..359>` | GET | Đặt hướng mục tiêu |
| `/api/cmd?report=1` | GET | Yêu cầu phao báo trạng thái |
| `/api/cmd?push=tele` / `push=profile` | GET | Đẩy Google Sheets |
| `/api/cmd?sync=1` | GET | Đồng bộ URL xuống phao |
| `/api/config` | GET/POST | Đọc/lưu URL + Wi-Fi STA (form-urlencoded) |
| `/api/reboot` | GET | Khởi động lại |
| `/api/factory_reset` | GET | Xóa NVS cấu hình trạm bờ |
| `/update` | GET/POST | Trang và endpoint OTA |

Ví dụ:

```bash
curl "http://192.168.4.1/api/cmd?m=200&r=60&w=0"
curl -X POST "http://192.168.4.1/api/config" \
     -d "url=https://script.google.com/macros/s/XXXX/exec&ssid=Nha&pass=12345678"
```

---

## 6. Google Sheets (Apps Script)

1. Tạo Google Sheets mới → **Tiện ích mở rộng → Apps Script**.
2. Dán toàn bộ `google_sheets_script.gs` → **Lưu**.
3. **Triển khai → Tạo mục triển khai mới → Ứng dụng web**:
   - Execute as: **Me**
   - Who has access: **Anyone**
4. Copy URL `/exec`, dán vào dashboard trạm bờ (hoặc `--url` của bridge).
5. (Tùy chọn) **Project Settings → Script properties**:
   - `ALARM_EMAIL` — email nhận cảnh báo.
   - `SPREADSHEET_ID` — chỉ cần khi script không gắn trực tiếp vào Sheet.
   - `ALARM_COOLDOWN_SEC` (mặc định 900), `DEDUPE_WINDOW_SEC` (mặc định 5).

Script tự tạo 2 sheet:

- `Telemetry_Realtime` — 30 cột: thời gian, seq, pH, DO, TDS, EC, NTU, nhiệt độ, độ sâu,
  công suất, dòng, áp pin, roll, pitch, heading, lat, lng, vệ tinh, tốc độ, độ cao,
  cách Home, hướng mục tiêu, flags2, RSSI, %HW, WQI, cảnh báo, QCVN, nguồn, FW.
- `Profile_3Tang` — 3 tầng × (pH, TDS, NTU, nhiệt, DO) + nguồn + FW.

Kiểm tra nhanh cấu hình: mở `<URL>/exec?mode=status` → JSON thống kê số dòng,
email cảnh báo đã cấu hình hay chưa.

> **Bảo mật:** URL `/exec` là “mật khẩu”. Ai có URL đều ghi được dữ liệu.
> Không chia sẻ công khai; có thể thu hồi bằng cách tạo deployment mới.

---

## 7. Cầu nối Python (Serial → Google Sheets)

Dùng khi trạm bờ cắm USB vào PC và muốn đẩy dữ liệu trực tiếp (không cần Wi-Fi/4G trên thiết bị).

```bash
pip install pyserial

python gsheets_live_bridge.py --list-ports
python gsheets_live_bridge.py --url "https://script.google.com/macros/s/XXXX/exec"
python gsheets_live_bridge.py --port COM11 --url "..." --interval 5 --verbose
python gsheets_live_bridge.py --url "..." --once
python gsheets_live_bridge.py --url "..." --print-only
```

Đặc điểm:

- Tự dò cổng ESP32 (CP210x / CH340 / CH9102 / USB-Serial), không hard-code COM.
- Không dùng `shell=True`; upload bằng HTTPS GET tới Apps Script.
- Kiểm tra CRC-8 có khóa (`*XX`) trước khi gửi.
- Tự mở lại cổng khi mất kết nối (backoff 1→30 s).
- Upload lỗi → ghi `gsheets_offline.csv` để không mất dữ liệu.
- Gói `PRF:` được đẩy vào sheet `Profile_3Tang`.
- Log ra màn hình + `gsheets_bridge.log`.

---

## 8. Giao thức LoRa

### 8.1 Telemetry `USV:` — 30 trường, phân cách dấu phẩy

```
USV:ph,tds,ntu,temp,amp,bat,roll,ct1,ct2,pitch,hdg,lat,lng,stage,alerts,seq,do,hw,wqi,mpuT,ec,depth,adcFault,s3ChipT,sats,gpsSpeed,gpsAlt,homeDist,flags2,targetHeading*XX
```

| # | Trường | Ý nghĩa |
|---|---|---|
| 1–6 | ph, tds, ntu, temp, amp, bat | Giá trị đo thực (0 nếu chưa cắm) |
| 7–10 | roll, ct1, ct2, pitch | IMU + công tắc hành trình |
| 11–13 | hdg, lat, lng | La bàn + GPS |
| 14–16 | stage, alerts, seq | Trạng thái auto, cờ cảnh báo, số gói |
| 17–19 | do, hw, wqi | DO ước lượng, bitmask 11 linh kiện, VN-WQI |
| 20–23 | mpuT, ec, depth, adcFault | Nhiệt MPU, EC, độ sâu tời, ADC vượt dải |
| 24 | s3ChipT | Nhiệt chip ESP32-S3 |
| 25–28 | sats, gpsSpeed, gpsAlt, homeDist | GPS mở rộng |
| 29–30 | flags2, targetHeading | Cờ trạng thái 2 + hướng mục tiêu |

Trạm bờ vẫn đọc được gói 24 trường của firmware cũ (các trường mới = 0).

### 8.2 Bảng 3 tầng `PRF:`

```
PRF:ph1,tds1,ntu1,t1;ph2,tds2,ntu2,t2;ph3,tds3,ntu3,t3*XX
```

### 8.3 Lệnh điều khiển `CMD:m,r,w`

`m` = PWM động cơ (-255…255), `r` = góc bánh lái (45…135), `w` = mã tời/đặc biệt:

| w | Lệnh |
|---|---|
| 0 | Dừng tời |
| 1 | Hạ tời |
| 2 | Thu tời về Home |
| 3 | Auto đo 3 tầng |
| 4 / 5 | Bật / tắt giữ hướng PID |
| 6 | Lưu Home GPS |
| 8 | Dừng khẩn cấp + thu lồng |
| 9 | Tare MPU6050 |
| 10 | GOTO_HOME |
| 11 | STOP_ALL |
| 12 | SET_HEADING (`m` = 0…359) |
| 13 | REPORT_STATUS |

Lệnh khác: `GURL:<url>` (đồng bộ URL), `CBAT:<volt>` (hiệu chuẩn pin).

### 8.4 CRC-8 có khóa

```c
uint8_t crc = 0xA7;
for each byte b: crc ^= b; 8 lần: crc = (crc & 0x80) ? (crc<<1)^0x31 : crc<<1;
```

Gói có dạng `<payload>*XX` (XX = hex). Sai CRC → bỏ gói, tăng `crcErrorCnt`.

### 8.5 Bitmask phần cứng (11 bit)

| Bit | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Linh kiện | DS18B20 | pH | TDS | NTU | ACS712 | Pin3S | MPU6050 | GY271 | LoRa | SIM4G | GPS |

### 8.6 `flags2`

`0x01` RETURN_HOME · `0x02` GPS_FIX · `0x04` HOME_SAVED · `0x08` SIM_ONLINE ·
`0x10` AUTO_RUNNING · `0x20` MUD_LATCHED · `0x40` LOW_BAT_CRIT · `0x80` CALIBRATED

---

## 9. Lệnh Serial chẩn đoán

### 9.1 Phao (115200 baud, USB CDC)

| Lệnh | Chức năng |
|---|---|
| `STATUS` | In báo cáo đầy đủ (pin, nước, IMU, GPS, Home, LoRa, 4G, NVS) |
| `REBOOT` | Khởi động lại |
| `FACTORY_RESET` | Xóa toàn bộ NVS rồi khởi động lại |
| `SAVE` | Lưu hiệu chuẩn hiện tại |
| `SET_URL:<url>` | Đổi Google Script URL |
| `CAL_PH:<pH>` | Hiệu chuẩn pH (4.0…10.0) |
| `CAL_TDS:<ppm>` | Hiệu chuẩn TDS |
| `CAL_NTU:<NTU>` | Hiệu chuẩn độ đục |
| `CAL_BAT:<V>` | Hiệu chuẩn áp pin (đo bằng VOM) |
| `CAL_IMU` / `TARE` | Tare góc nghiêng |
| `SMS:<số>` | Số điện thoại cảnh báo |
| `GOTO_HOME` / `STOP_ALL` / `SET_HOME` / `REPORT` | Lệnh autopilot |
| `SET_HEADING:<0..359>` | Đặt hướng mục tiêu |

### 9.2 Trạm bờ

| Lệnh | Chức năng |
|---|---|
| `STATUS` | In trạng thái LoRa, Wi-Fi, Google Sheets, phao |
| `PAGE:0..2` | Đổi trang TFT |
| `CAL_JOY` | Hiệu chuẩn tâm joystick |
| `SCAN_ADC` | Đọc nhanh các kênh ADC |
| `CAL_BAT:<V>` | Gửi hiệu chuẩn pin xuống phao |
| `CMD:<m>,<r>,<w>` | Gửi lệnh đặc biệt xuống phao |
| `PUSH` / `PUSHPRF` | Đẩy telemetry / profile lên Sheets |
| `SYNC_URL` | Đồng bộ URL xuống phao |

---

## 10. Xử lý sự cố

| Hiện tượng | Kiểm tra |
|---|---|
| Web không mở | Đã nối Wi-Fi `ChemStation-AI-GCS` chưa? Thử `http://192.168.4.1` |
| `CHUA CAM` ở mọi cảm biến | Kiểm tra nguồn 3.3 V, mass chung, dây ADC; chạy `SCAN_ADC` |
| ADC báo `VUOT DAI` | Điện áp vào GPIO > 3.05 V → thêm cầu chia áp |
| Không thấy phao | Kiểm tra LoRa NSS/SCK/MISO/MOSI, anten, `RSSI` trong log |
| Google Sheets không ghi | Mở `<URL>/exec?mode=ping` phải trả `PONG`; kiểm tra quyền “Anyone” |
| OTA báo lỗi | Đã nạp USB với `min_spiffs.csv` lần đầu chưa? File phải là `firmware.bin` |
| Pin báo % sai | Dùng `CAL_BAT:<V>` với số đo VOM; kiểm tra cầu 33k/10k |
| Tời chạy mãi | Kiểm tra CT1/CT2 (NO→GND), thời gian tối đa 25 s |

---

## 11. Thay đổi chính v4.2

- Giao thức telemetry mở rộng 24 → **30 trường** (GPS, Home, flags2, hướng mục tiêu),
  vẫn tương thích ngược.
- Thêm lệnh `GOTO_HOME`, `STOP_ALL`, `SET_HEADING`, `REPORT_STATUS`.
- Phao: bỏ `LoRa.parsePacket()` sau TX (tránh nuốt lệnh), tự khởi tạo lại LoRa sau
  10 lần TX lỗi, bật CRC phần cứng, NVS khôi phục đúng mẫu 3 tầng.
- Pin: ngưỡng theo từng cell (1S/2S/3S), sửa cảnh báo pin yếu và Deep Sleep.
- Phao: hiệu chuẩn TDS/NTU, lưu Home GPS, báo cáo STATUS qua Serial, APN 4G đầy đủ.
- Trạm bờ: parser 30 trường + `PRF:`, đẩy profile đúng 1 lần/chu trình, scheduler
  Wi-Fi/4G không mất gói, tự kết nối lại Wi-Fi, mDNS, OTA, `/api/config`.
- Dashboard: thẻ GPS/Autopilot, nút điều khiển từ xa, cấu hình Wi-Fi/URL, trạng thái cloud.
- Apps Script: bỏ toàn bộ giá trị mặc định ảo, chống ghi trùng, email cảnh báo,
  hỗ trợ JSON POST, `mode=status`.
- Python bridge: argparse, tự dò cổng, không `shell=True`, CRC, retry, CSV dự phòng.
