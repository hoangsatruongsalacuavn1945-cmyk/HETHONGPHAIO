import os
import openpyxl
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.utils import get_column_letter
from PIL import Image, ImageDraw, ImageFont

# Thư mục lưu ảnh tạm
IMG_DIR = "component_images"
os.makedirs(IMG_DIR, exist_ok=True)

def get_system_font(size=14, bold=False):
    """Nạp font hệ thống hỗ trợ tiếng Việt có dấu cho Pillow trên mọi HĐH"""
    candidates = [
        "C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf" if bold else "/System/Library/Fonts/Supplemental/Arial.ttf",
        "segoeuib.ttf" if bold else "segoeui.ttf",
        "arial.ttf"
    ]
    for p in candidates:
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except Exception:
                pass
    return ImageFont.load_default()

def smart_truncate(text, max_chars=38):
    """Cắt chuỗi theo từ nguyên vẹn, tuyệt đối không cắt cụt giữa chừng một từ tiếng Việt"""
    if len(text) <= max_chars:
        return text
    words = text.split(" ")
    out = ""
    for w in words:
        if len(out) + len(w) + 1 <= max_chars - 1:
            out = f"{out} {w}".strip()
        else:
            break
    return out + "…"

# Danh sách 21 linh kiện Phao USV
USV_COMPONENTS = [
    {
        "stt": 1,
        "name": "ESP32-S3-WROOM-1 DevKitC-1",
        "category": "Vi Điều Khiển Trung Tâm",
        "specs": "Dual-Core 240MHz Xtensa LX7, 512KB SRAM, 8MB Flash, Wi-Fi 2.4GHz + BLE 5.0",
        "pinout": "ADC1 (GPIO 3,4,5,7,8), I2C (GPIO 1,2), SPI (GPIO 11,13,14,15,21,47), UART1/2, PWM (GPIO 9,10,16,40,12)",
        "function": "Bộ não trung tâm của phao: Chạy FreeRTOS 2 nhân xử lý song song, lọc Kalman 1D cho 5 kênh cảm biến, thuật toán Autopilot PID giữ hướng la bàn, tự hành quay về Home GPS, máy trạng thái đo sâu 3 tầng nước và kích hoạt Hardware Task Watchdog 8s chống treo.",
        "badge_color": "#1E3A8A",
        "tag": "MCU 240MHz"
    },
    {
        "stt": 2,
        "name": "Module LoRa Ra-02 (SX1278 433MHz)",
        "category": "Viễn Thông Vô Tuyến Cự Ly Xa",
        "specs": "Tần số 433MHz, Băng thông 125kHz, Spreading Factor SF7, Công suất phát 17dBm (50mW), Độ nhạy -148dBm",
        "pinout": "SPI: NSS=GPIO 15, SCK=GPIO 11, MISO=GPIO 13, MOSI=GPIO 14, RST=GPIO 21, DIO0=GPIO 47",
        "function": "Đường truyền viễn thông chính xuyên vật cản và mặt nước cự ly 1-3km: Truyền gói Telemetry thời gian thực 30 trường dữ liệu (tốc độ 1Hz, bộ đệm mở rộng 256-byte) về Trạm bờ GCS, kèm mã kiểm tra lỗi CRC-8 chống nhiễu và nhận lệnh điều khiển Joystick từ xa.",
        "badge_color": "#0284C7",
        "tag": "LORA 433MHz"
    },
    {
        "stt": 3,
        "name": "Module SIM 4G LTE A7680C",
        "category": "Viễn Thông Di Động & Cloud",
        "specs": "LTE Cat-1 đa băng tần, UART 115200 baud, hỗ trợ HTTP/HTTPS/TCP/IP, gửi nhận tin nhắn SMS",
        "pinout": "UART2: RX=GPIO 42, TX=GPIO 41 (Nguồn xung 5V/3A độc lập)",
        "function": "Kênh truyền thông dự phòng kép (Dual-Link Fallback): Tự động kích hoạt khi mất sóng LoRa > 5s để đẩy trực tiếp dữ liệu quan trắc lên Google Sheets Cloud và gửi tin nhắn SMS cảnh báo khẩn cấp tới điện thoại khi gặp sự cố lật phao/pin kiệt.",
        "badge_color": "#6366F1",
        "tag": "4G LTE CAT-1"
    },
    {
        "stt": 4,
        "name": "Module Định Vị Vệ Tinh GPS NEO-6M",
        "category": "Định Vị Toàn Cầu & Dẫn Đường",
        "specs": "50 kênh vệ tinh GPS L1 C/A, độ chính xác < 2.5m CEP, tần số cập nhật 1Hz - 5Hz, ăng-ten gốm tích hợp",
        "pinout": "UART1: RX=GPIO 18, TX=GPIO 17 (Nguồn 3.3V/5V)",
        "function": "Cung cấp tọa độ địa lý thực tế (Kinh độ, Vĩ độ), độ cao và tốc độ di chuyển: Phục vụ lưu điểm mốc gốc Home GPS, tính toán khoảng cách và góc phương vị (bearing) cho bộ điều khiển tự hành quay về trạm (Return-To-Home) và giữ vị trí Neo Ảo (Virtual Anchor).",
        "badge_color": "#059669",
        "tag": "GPS NEO-6M"
    },
    {
        "stt": 5,
        "name": "Cảm Biến Đo pH PH-4502C + Điện Cực E-201-C",
        "category": "Cảm Biến Chất Lượng Nước",
        "specs": "Dải đo 0.00 - 14.00 pH, độ phân giải 0.01 pH, điện áp ngõ ra analog 0.0V - 3.0V, thời gian đáp ứng < 1 phút",
        "pinout": "ADC1: GPIO 7 (chân Po, đọc qua analogReadMilliVolts)",
        "function": "Đo độ chua/kiềm của nguồn nước trong hồ/sông: Phát hiện sự cố nước thải công nghiệp hoặc hiện tượng phú dưỡng, tích hợp thuật toán phát hiện hở mạch/bão hòa áp trần (>3050mV) chống báo giả, lọc nhiễu Kalman 1D và lưu điểm chuẩn vào NVS.",
        "badge_color": "#D97706",
        "tag": "pH 0-14"
    },
    {
        "stt": 6,
        "name": "Cảm Biến Tổng Khoáng Hòa Tan TDS",
        "category": "Cảm Biến Chất Lượng Nước",
        "specs": "Dải đo 0 - 1000 ppm (mg/L), độ chính xác ±10% F.S, điện cực chống nước 2 kim titan, ngõ ra analog 0 - 2.3V",
        "pinout": "ADC1: GPIO 4",
        "function": "Đo tổng hàm lượng chất rắn và ion khoáng hòa tan trong nước: Đánh giá độ sạch của nguồn nước, quy đổi độ dẫn điện EC thực tế (µS/cm) và kết hợp nhiệt độ DS18B20 để ước lượng mức bão hòa Oxy hòa tan lý thuyết (DO_sat).",
        "badge_color": "#0D9488",
        "tag": "TDS 0-1000ppm"
    },
    {
        "stt": 7,
        "name": "Cảm Biến Độ Đục Nước Turbidity Sensor",
        "category": "Cảm Biến Chất Lượng Nước",
        "specs": "Dải đo 0 - 3000 NTU, bước sóng quang hồng ngoại 940nm, ngõ ra analog AO 0 - 4.5V",
        "pinout": "ADC1: GPIO 5 (kèm mạch chia áp bảo vệ 3.3V)",
        "function": "Đo độ trong/đục và nồng độ phù sa, bùn đất lơ lửng trong hồ: Phát hiện nguồn nước bị khuấy đục do sạt lở hoặc dòng chảy ngầm, tự động hiệu chuẩn điểm nước trong tuyệt đối trong bộ nhớ NVS.",
        "badge_color": "#EA580C",
        "tag": "NTU 0-3000"
    },
    {
        "stt": 8,
        "name": "Cảm Biến Nhiệt Độ Nước Chống Nước DS18B20",
        "category": "Cảm Biến Chất Lượng Nước",
        "specs": "Dải đo -55°C đến +125°C, độ chính xác ±0.5°C (-10°C đến +85°C), vỏ bọc thép không gỉ phi 6mm, chuẩn Dallas 1-Wire",
        "pinout": "1-Wire: GPIO 6 (kèm trở kéo pull-up 4.7kΩ lên 3.3V)",
        "function": "Đo nhiệt độ từng tầng nước chính xác cao với cơ chế đọc bất đồng bộ phi chặn (Non-blocking 750ms): Dùng để tự động bù nhiệt cho phép đo pH/TDS, tính độ bão hòa DO lý thuyết và tính chỉ số chất lượng nước VN-WQI chuẩn Bộ TN&MT.",
        "badge_color": "#4F46E5",
        "tag": "DS18B20 1-WIRE"
    },
    {
        "stt": 9,
        "name": "Cảm Biến Góc Nghiêng & Gia Tốc 6 Trục MPU6050",
        "category": "Cảm Biến Quán Tính & Cân Bằng Tàu",
        "specs": "Con quay hồi chuyển 3 trục Gyroscope (±250°/s) + Gia tốc kế 3 trục Accelerometer (±2g), giao tiếp I2C 100kHz",
        "pinout": "I2C chung: SDA=GPIO 1, SCL=GPIO 2 (Địa chỉ 0x68)",
        "function": "Giám sát góc nghiêng Roll và Pitch của thân thuyền với bộ lọc bù 6 trục: Cân bằng góc Tare 0.0° qua lệnh LoRa từ xa, tự động ngắt động cơ tức thì nếu phao bị sóng đánh nghiêng dốc > 60° để chống lật úp, đo nhiệt độ chip nội.",
        "badge_color": "#9333EA",
        "tag": "IMU MPU6050"
    },
    {
        "stt": 10,
        "name": "Cảm Biến La Bàn Số 3 Trục GY-271 (QMC5883L / HMC5883L)",
        "category": "Cảm Biến Định Hướng Không Gian",
        "specs": "Dải đo từ trường Trái Đất ±8 Gauss, độ phân giải 16-bit, tần số cập nhật 50Hz, tự động nhận diện chip 0x0D & 0x1E",
        "pinout": "I2C chung: SDA=GPIO 1, SCL=GPIO 2 (Địa chỉ 0x0D hoặc 0x1E)",
        "function": "Xác định góc phương vị la bàn từ trường (Heading 0° - 360°) đối chiếu hướng Bắc thực tế: Tín hiệu phản hồi chủ đạo cho bộ điều khiển Autopilot PID bẻ lái tự hành, giúp thuyền bơi thẳng tắp theo hướng mục tiêu bất chấp sóng gió tạt ngang.",
        "badge_color": "#0284C7",
        "tag": "COMPASS GY271"
    },
    {
        "stt": 11,
        "name": "Cảm Biến Đo Dòng Điện Hall ACS712 20A",
        "category": "Cảm Biến Bảo Vệ Dòng & An Toàn",
        "specs": "Dải đo dòng DC -20A đến +20A, độ nhạy 100mV/A, cách ly quang điện 2.1kVRMS, điện áp điểm 0 VCC/2 (1.65V)",
        "pinout": "ADC1: GPIO 8",
        "function": "Đo dòng tải tiêu thụ tức thời của động cơ đẩy L298N: Phát hiện kẹt chân vịt (Jamming > 2.8A) để kích hoạt chu trình tự động đảo chiều gỡ rác và khóa bảo vệ cấp 3 chống cháy cuộn dây motor và nổ mạch cầu H.",
        "badge_color": "#DC2626",
        "tag": "ACS712 20A"
    },
    {
        "stt": 12,
        "name": "Mạch Phân Áp Đo Pin 3S LiPo (Cầu Trở 33kΩ / 10kΩ)",
        "category": "Cảm Biến Giám Sát Nguồn Pin 3S",
        "specs": "Tỉ số chia áp 4.30:1, điện áp đo 0.0V - 12.60V (hiệu chuẩn eFuse Vref qua analogReadMilliVolts)",
        "pinout": "ADC1: GPIO 3 (P+ Pin nối qua trở 33kΩ xuống GPIO 3, trở 10kΩ xuống GND)",
        "function": "Giám sát điện áp nguồn pin LiPo 3S chính xác cao: Thuật toán nhận diện số cell (1S/2S/3S), tính toán % dung lượng thực tế theo đường cong điện hóa và điều phối bảo vệ 4 tầng (Tier 1: Giảm ga, Tier 2: Tự hành về Home, Tier 3: Ngắt tải động lực, Tier 4: Deep Sleep).",
        "badge_color": "#16A34A",
        "tag": "BAT 3S 12.6V"
    },
    {
        "stt": 13,
        "name": "Công Tắc Hành Trình Đáy Bùn CT1 (KW12-3)",
        "category": "Cảm Biến Giới Hạn Hành Trình",
        "specs": "Tiếp điểm cơ học chịu tải 5A 250VAC, kích hoạt mức LOW (Falling Interrupt), có lọc dội tín hiệu 45ms",
        "pinout": "GPIO 38 (Active-LOW: COM nối GND, NO nối GPIO 38)",
        "function": "Phát hiện lồng cảm biến chạm đáy hồ bùn trong chế độ thả tời tự động: Lập tức ngắt tời hạ và kích hoạt tời nhấc nhẹ lồng lên 5cm để tránh cắm ngập vào lớp bùn đặc gây nghẹt và vỡ điện cực thủy tinh pH.",
        "badge_color": "#B45309",
        "tag": "CT1 BOTTOM"
    },
    {
        "stt": 14,
        "name": "Công Tắc Hành Trình Điểm Gốc Home CT2 (KW12-3)",
        "category": "Cảm Biến Giới Hạn Hành Trình",
        "specs": "Tiếp điểm cơ học chịu tải 5A 250VAC, kích hoạt mức LOW (Falling Interrupt), có lọc dội tín hiệu 45ms",
        "pinout": "GPIO 39 (Active-LOW: COM nối GND, NO nối GPIO 39)",
        "function": "Xác nhận điểm gốc mốc 0.00m khi thu lồng cảm biến về sát gầm sàn phao: Ngắt tời thu ngay tức thì, đồng thời reset bộ đếm độ sâu về 0.00m để bảo vệ cáp không bị kéo căng quá mức làm đứt dây hoặc gãy giá treo.",
        "badge_color": "#047857",
        "tag": "CT2 TOP HOME"
    },
    {
        "stt": 15,
        "name": "Mạch Cầu H L298N (Khóa An Toàn 65% PWM) / Khuyến Nghị BTS7960 43A",
        "category": "Cơ Cấu Chấp Hành & Động Lực",
        "specs": "L298N: Dòng liên tục 2A (Giới hạn phần mềm 65% PWM = 165/255) | Khuyến nghị nâng cấp BTS7960 43A (R_DS(on) 16mΩ) cho động cơ 775",
        "pinout": "ENA=GPIO 16 (Xung LEDC PWM 5kHz 8-bit), IN1=GPIO 40, IN2=GPIO 12",
        "function": "Điều khiển tốc độ vô cấp và chiều quay động cơ đẩy chính: Khóa trần PWM 65% (165/255) khi dùng L298N, giữ dừng 800ms bắt buộc trước khi đảo chiều (triệt tiêu dòng khởi động ngược 2x I_stall), kết hợp Soft-start ramp 200ms và TVS SMBJ15A + tụ 100nF/470µF chống Back-EMF.",
        "badge_color": "#991B1B",
        "tag": "L298N DUAL H-BRIDGE"
    },
    {
        "stt": 16,
        "name": "Động Cơ Đẩy DC 12V 775/550 + Chân Vịt 3 Cánh Chống Rác",
        "category": "Cơ Cấu Động Lực Thủy Khí",
        "specs": "Điện áp danh định 12V DC, tốc độ không tải 8000 RPM, trục thép chống nước, chân vịt 3 cánh đường kính 36mm",
        "pinout": "Nối vào 2 cọc công suất OUT1 & OUT2 của mạch L298N",
        "function": "Cung cấp lực đẩy phản lực đưa phao USV di chuyển tuần tra trên hồ, thực hiện nhiệm vụ bơi tự hành theo hải trình định vị GPS, tự bơi về bờ khi mất sóng và giữ vị trí Neo Ảo (Dynamic Positioning).",
        "badge_color": "#1E293B",
        "tag": "DC 12V MOTOR"
    },
    {
        "stt": 17,
        "name": "Servo Tời Quay Vô Cấp 360° MG996R",
        "category": "Cơ Cấu Tời Khảo Sát Độ Sâu",
        "specs": "Góc quay 360° liên tục vô cấp, mô-men xoắn 11kg.cm (ở 6V), toàn bộ bánh răng kim loại chịu lực cao, xung PWM 50Hz",
        "pinout": "GPIO 9 (Xung PWM điều khiển 500µs - 2400µs), nguồn 5V riêng từ mạch Buck",
        "function": "Kéo thả tang cuộn cáp nâng/hạ lồng đo 4 cảm biến nước: Phục vụ chế độ tự động khảo sát phân tầng chất lượng nước theo chiều sâu (Mặt 0.3m, Giữa 1.0m, Đáy 1.8m) và hạ/thu từng nấc độ phân giải cao 10cm.",
        "badge_color": "#334155",
        "tag": "WINCH SERVO 360"
    },
    {
        "stt": 18,
        "name": "Servo Bẻ Lái Bánh Lái MG90S",
        "category": "Cơ Cấu Chấp Hành & Dẫn Hướng",
        "specs": "Góc quay 0° - 180° (giới hạn an toàn 45° - 135°), mô-men xoắn 2.2kg.cm, bánh răng kim loại chống mòn, xung PWM 50Hz",
        "pinout": "GPIO 10 (Xung PWM điều khiển), nguồn 5V",
        "function": "Điều khiển góc bẻ bánh lái thủy lực phía sau đuôi phao (45°: Rẽ trái hết lái, 90°: Đi thẳng, 135°: Rẽ phải hết lái): Đáp ứng lệnh lái tức thì từ Joystick trạm bờ và vòng lặp bẻ lái tự động của bộ PID Heading Hold.",
        "badge_color": "#475569",
        "tag": "RUDDER SERVO 90S"
    },
    {
        "stt": 19,
        "name": "Bộ Nguồn Pin LiPo 3S 11.1V - 12.6V (3000mAh - 5200mAh 30C)",
        "category": "Nguồn Cung Cấp Năng Lượng",
        "specs": "Điện áp danh định 11.1V (sạc đầy 12.6V), dòng xả liên tục 30C - 50C, giắc cắm chuẩn XT60, có mạch bảo vệ BMS",
        "pinout": "Cực Dương P+ cấp vào mạch L298N, mạch phân áp GPIO 3 và đầu vào mạch Buck 5V; Cực Âm P- nối GND chung",
        "function": "Trái tim năng lượng của toàn bộ hệ thống phao: Cấp dòng xả tức thời lớn cho động cơ đẩy và mạch servo tời, duy trì hoạt động quan trắc bền bỉ từ 4 đến 8 giờ liên tục ngoài hiện trường khắc nghiệt.",
        "badge_color": "#15803D",
        "tag": "LIPO 3S 12.6V"
    },
    {
        "stt": 20,
        "name": "Hệ Thống Nguồn Phân Tầng 3 Nhánh Buck DC-DC 5V Độc Lập + E-Stop",
        "category": "Nguồn Cung Cấp Năng Lượng",
        "specs": "Cầu chì chính 20A + Công tắc E-Stop nấm đỏ cơ khí + 3 nhánh Buck: Buck #1 (5V/2A Logic), Buck #2 (6V/5A Servo), Buck #3 (4.2V/3A SIM 4G)",
        "pinout": "IN+ nối Pin 3S, IN- nối GND; OUT+ cấp vào chân 5V ESP32-S3 và mạch servo/cảm biến, OUT- nối GND",
        "function": "Hạ áp ổn định từ nguồn pin 12V xuống nguồn 5.0V sạch, chống nhiễu gợn sóng cho vi điều khiển ESP32-S3, mạch SIM 4G và các cảm biến đo lường nhạy cảm.",
        "badge_color": "#047857",
        "tag": "BUCK 5V 3A"
    },
    {
        "stt": 21,
        "name": "Đèn LED RGB WS2812 Thông Minh (Onboard GPIO 48)",
        "category": "Chỉ Báo Trạng Thái Thị Giác",
        "specs": "LED kỹ thuật số WS2812B địa chỉ hóa 1 chân dữ liệu, hiển thị 16 triệu màu, tích hợp sẵn trên ESP32-S3",
        "pinout": "GPIO 48",
        "function": "Hiển thị trạng thái hoạt động thực tế bằng mắt thường từ xa: Xanh lá (Bình thường), Xanh dương (Phát LoRa), Cyan (Chạy motor/nhận lệnh), Vàng (Tời đang hoạt động), Đỏ (Báo động nguy hiểm/kẹt chân vịt/pin yếu), Tím (Thiếu linh kiện).",
        "badge_color": "#EC4899",
        "tag": "RGB WS2812"
    }
]

# Danh sách 7 linh kiện Trạm bờ GCS
GCS_COMPONENTS = [
    {
        "stt": 1,
        "name": "ESP32 Dev Module (ESP32-WROOM-32D / D0WD-V3)",
        "category": "Vi Điều Khiển Trung Tâm Trạm Bờ",
        "specs": "Dual-Core 240MHz Xtensa LX6, 320KB SRAM, 4MB Flash, Wi-Fi 802.11 b/g/n + Bluetooth 4.2 BR/EDR/BLE",
        "pinout": "Cổng USB Serial CP2102/CH340 (COM11/COM12), VSPI bus (GPIO 5,15,16,18,19,23,26), ADC1 (GPIO 34,35), Switch (GPIO 12,13,14,22,25,27,32,33)",
        "function": "Bộ não trung tâm của Trạm bờ GCS: Thu nhận giải mã gói tin LoRa 433MHz, vẽ đồ họa tốc độ cao lên màn hình màu IPS TFT ST7789, phát Wi-Fi Access Point Web Dashboard và trung chuyển dữ liệu Cloud qua WiFi/4G.",
        "badge_color": "#1E3A8A",
        "tag": "GCS MCU 240MHz"
    },
    {
        "stt": 2,
        "name": "Màn Hình Màu IPS TFT 2.0 inch ST7789 (240x320)",
        "category": "Giao Diện Hiển Thị Hiện Trường",
        "specs": "Tấm nền IPS góc nhìn rộng 178°, độ phân giải 240x320 Pixel, chuẩn giao tiếp SPI tần số cao 27MHz - 40MHz",
        "pinout": "VSPI chung: CS=GPIO 5, DC=GPIO 2, RST=GPIO 4, MOSI=GPIO 23, SCK=GPIO 18",
        "function": "Màn hình tác chiến cầm tay hiển thị 5 trang giao diện chuyên nghiệp: Trang 0 (HUD buồng lái kim đo 4 chỉ số nước), Trang 1 (Đồ thị sóng Telemetry 120 điểm), Trang 2 (Khảo sát cột nước 3 tầng), Trang 3 (Ma trận kiểm tra 11 linh kiện), Trang 4 (Radar 360° vệ tinh và góc la bàn).",
        "badge_color": "#0284C7",
        "tag": "TFT IPS 240x320"
    },
    {
        "stt": 3,
        "name": "Module LoRa Ra-02 (SX1278 433MHz - Trạm Bờ)",
        "category": "Viễn Thông Vô Tuyến Trạm Bờ",
        "specs": "Tần số 433MHz, chia sẻ bus VSPI với màn hình TFT, quản lý phần cứng bằng chân chọn chip NSS độc quyền",
        "pinout": "VSPI chung: NSS=GPIO 15, RST=GPIO 16, DIO0=GPIO 26, SCK=GPIO 18, MISO=GPIO 19, MOSI=GPIO 23",
        "function": "Thu nhận gói tin Telemetry 30 trường từ phao USV với tần số 1Hz, kiểm tra mã sai số CRC-8 trên toàn khung 256-byte và phát các gói lệnh điều khiển Joystick, lệnh tự động đo 3 tầng, lệnh Neo Ảo và lệnh dừng khẩn cấp xuống phao.",
        "badge_color": "#0D9488",
        "tag": "GCS LORA 433"
    },
    {
        "stt": 4,
        "name": "Cần Điều Khiển Joystick 2 Trục Analog + Nút Bấm SW",
        "category": "Giao Diện Lái Thủ Công Vô Cấp",
        "specs": "2 biến trở góc 10kΩ độc lập (trục X và trục Y), góc xoay 60°, điện áp ngõ ra 0.0V - 3.3V, nút nhấn kim loại tích hợp",
        "pinout": "ADC1: VRY=GPIO 34 (Ga tiến/lùi), VRX=GPIO 35 (Lái trái/phải), SW=GPIO 13 (Nút nhấn)",
        "function": "Cho phép phi công lái thuyền USV vô cấp siêu mượt: Hỗ trợ 3 cấp tốc độ (ECO 60%, CRUISE 82%, TURBO 100%), tích hợp thuật toán tự cân bằng điểm 0 điện áp và cơ chế tự động ngắt ga an toàn khi phát hiện đứt/rút dây.",
        "badge_color": "#D97706",
        "tag": "JOYSTICK 2-AXIS"
    },
    {
        "stt": 5,
        "name": "Dàn Công Tắc Gạt & Nút Nhấn Tác Chiến (SW1 - SW7)",
        "category": "Phím Bấm Chức Năng Hiện Trường",
        "specs": "7 nút bấm kim loại tiếp điểm cơ học chống dội, kích hoạt mức LOW (INPUT_PULLUP)",
        "pinout": "SW1(GPIO 25), SW2(GPIO 27), SW3(GPIO 14), SW4(GPIO 12), SW5(GPIO 32), SW6(GPIO 33), SW7(GPIO 22)",
        "function": "Điều khiển thao tác nhanh không cần mở laptop: Chuyển trang màn hình TFT (SW1), Tare cân bằng góc nghiêng 0.0° (SW2), bẻ lái trái/phải thủ công (SW3/SW4), hạ/thu tời khẩn cấp (SW5/SW6), kích hoạt chu trình tự động đo 3 tầng nước (SW7).",
        "badge_color": "#6366F1",
        "tag": "7 TACTICAL SW"
    },
    {
        "stt": 6,
        "name": "Bộ Nguồn Pin Sạc Li-ion 18650 2S (7.4V - 8.4V)",
        "category": "Nguồn Cung Cấp Năng Lượng Trạm Bờ",
        "specs": "2 cell pin Li-ion 18650 mắc nối tiếp dung lượng 2600mAh - 3400mAh, tích hợp mạch sạc cân bằng Type-C BMS",
        "pinout": "Cực Dương nối qua công tắc nguồn vào chân VIN bo ESP32; Cực Âm nối GND chung",
        "function": "Cung cấp nguồn điện di động cho toàn bộ Trạm bờ cầm tay hoạt động độc lập ngoài bờ hồ/sông từ 6 đến 12 giờ liên tục mà không cần nguồn điện lưới 220V.",
        "badge_color": "#16A34A",
        "tag": "BAT 18650 2S"
    },
    {
        "stt": 7,
        "name": "Cổng Giao Tiếp USB-to-UART CP2102 / CH340",
        "category": "Giao Tiếp Máy Tính & Nạp Firmware",
        "specs": "Chuẩn USB 2.0 Full-Speed, tốc độ truyền baud 115200 bps, tự động Reset & nạp code qua chân DTR/RTS",
        "pinout": "Cổng Micro-USB / Type-C nối trực tiếp vào máy tính điều hành",
        "function": "Cầu nối dữ liệu trực tiếp với phần mềm máy tính (Live Bridge Daemon): Đẩy dữ liệu quan trắc thời gian thực lên Google Sheets qua HTTP POST JSON, phát bản tin Discord Webhook và xuất file log dữ liệu CSV offline song song.",
        "badge_color": "#475569",
        "tag": "USB CH340/CP2102"
    }
]

# Danh mục Ma Trận Chân Cắm GPIO & Tự Động Chẩn Đoán (Sheet 3 - 45 Chân)
GPIO_MATRIX = [
    # Phao USV (ESP32-S3)
    (1, "Phao USV (ESP32-S3)", "GPIO 1", "I2C SDA (Chung: MPU6050 + GY-271)", "I2C (100kHz, 3.3V)", "Tự động phát 9 xung clock cứu bus treo, Auto-detect chip 0x0D (QMC5883L) & 0x1E (HMC5883L)", "HOẠT ĐỘNG TỐT (Có Bảo Vệ)"),
    (2, "Phao USV (ESP32-S3)", "GPIO 2", "I2C SCL (Chung: MPU6050 + GY-271)", "I2C (100kHz, 3.3V)", "Kiểm tra mức HIGH trở kéo pull-up, timeout 15ms chống deadlock bus I2C", "HOẠT ĐỘNG TỐT (Có Bảo Vệ)"),
    (3, "Phao USV (ESP32-S3)", "GPIO 3 (Alt: GPIO 20)", "PIN_BAT_3S (Đo áp Pin 3S LiPo - Lưu ý Strapping JTAG)", "ADC1 Ch2 (0-3.14V)", "Đọc analogReadMilliVolts(3) hiệu chuẩn eFuse (tỉ số 4.035:1, dải NVS 1.5..8.5); hỗ trợ #define USE_SAFE_ALT_PINOUT tránh chân Strapping", "HOẠT ĐỘNG TỐT (Có Bảo Vệ)"),
    (4, "Phao USV (ESP32-S3)", "GPIO 4", "PIN_TDS (Cảm biến tổng khoáng hòa tan)", "ADC1 Ch3 (0-2.3V)", "Phát hiện hở mạch, tự động bù nhiệt độ từ DS18B20 và nội suy EC (µS/cm)", "HOẠT ĐỘNG TỐT"),
    (5, "Phao USV (ESP32-S3)", "GPIO 5", "PIN_TURBIDITY (Cảm biến độ đục NTU)", "ADC1 Ch4 (0-3.3V)", "Cầu phân áp an toàn 3.3V, lưu mốc hiệu chuẩn nước cất 0 NTU trong bộ nhớ NVS", "HOẠT ĐỘNG TỐT"),
    (6, "Phao USV (ESP32-S3)", "GPIO 6", "PIN_DS18B20 (Cảm biến nhiệt độ nước)", "1-Wire (Pull-up 4.7kΩ)", "Đọc bất đồng bộ Non-blocking 750ms, kiểm tra mã CRC ROM chống nhiễu", "HOẠT ĐỘNG TỐT"),
    (7, "Phao USV (ESP32-S3)", "GPIO 7", "PIN_PH (Cảm biến pH PH-4502C)", "ADC1 Ch6 (0-3.0V)", "Chống bão hòa áp trần >3050mV, khóa cảnh báo giả khi đầu dò hở mạch (kẹt 3.76)", "HOẠT ĐỘNG TỐT (Có Bảo Vệ)"),
    (8, "Phao USV (ESP32-S3)", "GPIO 8", "PIN_ACS712 (Cảm biến dòng động cơ)", "ADC1 Ch7 (1.65V ref)", "Tự động lấy mẫu điểm 0 acsZeroVolt khi khởi động, khóa ngắt 3-strikes khi > 2.8A", "HOẠT ĐỘNG TỐT"),
    (9, "Phao USV (ESP32-S3)", "GPIO 9", "PIN_SERVO_WINCH (Servo tời 360° MG996R)", "LEDC PWM (50Hz)", "Xung 500-2400µs, tự ngắt an toàn sau 25s chống kẹt cơ khí gây cháy cuộn dây", "HOẠT ĐỘNG TỐT"),
    (10, "Phao USV (ESP32-S3)", "GPIO 10", "PIN_SERVO_RUDDER (Servo lái MG90S)", "LEDC PWM (50Hz)", "Giới hạn góc bẻ lái an toàn 45°-135°, điều khiển bởi bộ PID Heading Hold", "HOẠT ĐỘNG TỐT"),
    (11, "Phao USV (ESP32-S3)", "GPIO 11", "LORA_SCK_PIN (LoRa Ra-02 SX1278)", "SPI Bus (3.3V)", "Xung nhịp SPI phần cứng, kiểm tra thanh ghi RegVersion == 0x12", "HOẠT ĐỘNG TỐT"),
    (12, "Phao USV (ESP32-S3)", "GPIO 12", "PIN_MOTOR_IN2 (Cầu H L298N IN2)", "GPIO Out (3.3V)", "Thời gian chết Dead-time 50µs chống ngắn mạch dẫn thông khi đảo chiều", "HOẠT ĐỘNG TỐT"),
    (13, "Phao USV (ESP32-S3)", "GPIO 13", "LORA_MISO_PIN (LoRa Ra-02 SX1278)", "SPI Bus (3.3V)", "Đường dữ liệu vào Master In Slave Out tốc độ cao", "HOẠT ĐỘNG TỐT"),
    (14, "Phao USV (ESP32-S3)", "GPIO 14", "LORA_MOSI_PIN (LoRa Ra-02 SX1278)", "SPI Bus (3.3V)", "Đường dữ liệu ra Master Out Slave In tốc độ cao", "HOẠT ĐỘNG TỐT"),
    (15, "Phao USV (ESP32-S3)", "GPIO 15", "LORA_NSS_PIN (LoRa Ra-02 CS/NSS)", "GPIO Out (3.3V)", "Chân chọn chip độc quyền, tự động tái khởi tạo SPI khi phát hiện treo truyền thông", "HOẠT ĐỘNG TỐT"),
    (16, "Phao USV (ESP32-S3)", "GPIO 16", "PIN_MOTOR_PWM (Cầu H L298N ENA)", "LEDC PWM (5kHz)", "Điều áp 8-bit, Soft-start ramp dốc 200ms triệt tiêu xung phản kháng Back-EMF", "HOẠT ĐỘNG TỐT"),
    (17, "Phao USV (ESP32-S3)", "GPIO 17", "GPS_TX1_PIN (Nối chân RXD GPS NEO-6M)", "UART1 TX (9600 baud)", "Truyền lệnh cấu hình tần số cập nhật bản tin NMEA", "HOẠT ĐỘNG TỐT (Có Bảo Vệ)"),
    (18, "Phao USV (ESP32-S3)", "GPIO 18", "GPS_RX1_PIN (Nối chân TXD GPS NEO-6M)", "UART1 RX (9600 baud)", "Tách biệt trạng thái UART OK (giao tiếp chip tốt) và trạng thái chờ GPS Fix (trong nhà)", "HOẠT ĐỘNG TỐT (Có Bảo Vệ)"),
    (19, "Phao USV (ESP32-S3)", "GPIO 21", "LORA_RST_PIN (LoRa Ra-02 Reset)", "GPIO Out (3.3V)", "Kích mức LOW 10ms khôi phục phần cứng LoRa khi mất sóng", "HOẠT ĐỘNG TỐT"),
    (20, "Phao USV (ESP32-S3)", "GPIO 38", "PIN_CT1_BOTTOM (Công tắc chạm đáy bùn)", "GPIO In (Pull-up 3.3V)", "Ngắt ngoài FALLING, chống dội 45ms, tự động nhấc lồng cảm biến lên 5cm", "HOẠT ĐỘNG TỐT"),
    (21, "Phao USV (ESP32-S3)", "GPIO 39", "PIN_CT2_TOP_HOME (Công tắc điểm gốc Home)", "GPIO In (Pull-up 3.3V)", "Ngắt ngoài FALLING, dừng kéo căng cáp và reset độ sâu tời về 0.00m", "HOẠT ĐỘNG TỐT"),
    (22, "Phao USV (ESP32-S3)", "GPIO 40", "PIN_MOTOR_IN1 (Cầu H L298N IN1)", "GPIO Out (3.3V)", "Phối hợp đảo chiều động cơ đẩy tự động gỡ rác chân vịt", "HOẠT ĐỘNG TỐT"),
    (23, "Phao USV (ESP32-S3)", "GPIO 41", "SIM_TX2_PIN (Nối chân RXD SIM A7680C)", "UART2 TX (115200)", "Truyền lệnh AT phi chặn, đẩy dữ liệu HTTP POST Cloud, gửi tin nhắn SMS khẩn cấp", "HOẠT ĐỘNG TỐT (Có Bảo Vệ)"),
    (24, "Phao USV (ESP32-S3)", "GPIO 42", "SIM_RX2_PIN (Nối chân TXD SIM A7680C)", "UART2 RX (115200)", "Máy trạng thái bất đồng bộ tự động Re-sync khi sụt áp hoặc mất sóng 4G", "HOẠT ĐỘNG TỐT (Có Bảo Vệ)"),
    (25, "Phao USV (ESP32-S3)", "GPIO 47", "LORA_DIO0_PIN (LoRa Ra-02 DIO0)", "GPIO In (Interrupt)", "Ngắt phần cứng báo có gói tin mới trong bộ đệm FIFO radio", "HOẠT ĐỘNG TỐT"),
    (26, "Phao USV (ESP32-S3)", "GPIO 48", "WS2812_PIN (Đèn LED RGB thông minh)", "Kỹ thuật số 1 dây", "Hiển thị 6 mã màu trạng thái thực chiến từ xa bằng mắt thường", "HOẠT ĐỘNG TỐT"),
    (27, "Phao USV (ESP32-S3)", "GPIO 45 (Alt: GPIO 43)", "PIN_CO2_TRIGGER (Phao cứu sinh CO2 - Strapping VDD_SPI)", "GPIO Out (Active-HIGH)", "Ép OUTPUT LOW ngay dòng 1 của setup() trước Serial.begin(); khuyến nghị mạch RC + NPN hoặc chuyển sang GPIO 43 qua USE_SAFE_ALT_PINOUT", "HOẠT ĐỘNG TỐT (Có Bảo Vệ)"),
    (28, "Phao USV (ESP32-S3)", "GPIO 46 (Alt: GPIO 44)", "PIN_SIREN_BUZZER (Còi báo động 120dB - Strapping Boot)", "GPIO Out (Active-HIGH)", "Ép OUTPUT LOW ngay dòng 1 của setup() chống kẹt chế độ Download Boot; hỗ trợ chuyển sang GPIO 44 qua USE_SAFE_ALT_PINOUT", "HOẠT ĐỘNG TỐT (Có Bảo Vệ)"),
    # Trạm bờ GCS (ESP32 Dev Module)
    (27, "Trạm Bờ GCS (ESP32)", "GPIO 2", "TFT_DC_PIN (Màn hình IPS ST7789)", "GPIO Out (3.3V)", "Phân biệt byte Lệnh (Command) và Dữ liệu (Data) vẽ đồ họa màn hình", "HOẠT ĐỘNG TỐT"),
    (28, "Trạm Bờ GCS (ESP32)", "GPIO 4", "TFT_RST_PIN (Màn hình IPS ST7789)", "GPIO Out (3.3V)", "Khởi động cứng lại màn hình TFT khi có xung đột nhiễu SPI", "HOẠT ĐỘNG TỐT"),
    (29, "Trạm Bờ GCS (ESP32)", "GPIO 5", "TFT_CS_PIN (Màn hình IPS ST7789 CS)", "VSPI CS (3.3V)", "Chọn chip màn hình TFT độc quyền trên bus VSPI (kèm Mutex bảo vệ)", "HOẠT ĐỘNG TỐT"),
    (30, "Trạm Bờ GCS (ESP32)", "GPIO 12", "SW4_RUDDER_RIGHT (Nút bẻ lái phải)", "GPIO In (Pull-up 3.3V)", "Tiếp điểm chống dội 50ms, bẻ bánh lái sang phải thủ công", "HOẠT ĐỘNG TỐT"),
    (31, "Trạm Bờ GCS (ESP32)", "GPIO 13", "JOY_SW_PIN (Nút nhấn cần Joystick)", "GPIO In (Pull-up 3.3V)", "Chuyển đổi 3 chế độ tốc độ: ECO (60%) -> CRUISE (82%) -> TURBO (100%)", "HOẠT ĐỘNG TỐT"),
    (32, "Trạm Bờ GCS (ESP32)", "GPIO 14", "SW3_RUDDER_LEFT (Nút bẻ lái trái)", "GPIO In (Pull-up 3.3V)", "Tiếp điểm chống dội 50ms, bẻ bánh lái sang trái thủ công", "HOẠT ĐỘNG TỐT"),
    (33, "Trạm Bờ GCS (ESP32)", "GPIO 15", "LORA_NSS_PIN (LoRa Ra-02 NSS GCS)", "VSPI NSS (3.3V)", "Chọn chip LoRa trạm bờ, tách biệt hoàn toàn khi đang vẽ đồ họa IPS", "HOẠT ĐỘNG TỐT"),
    (34, "Trạm Bờ GCS (ESP32)", "GPIO 16", "LORA_RST_PIN (LoRa Ra-02 RST GCS)", "GPIO Out (3.3V)", "Reset phần cứng module LoRa trạm bờ khi treo", "HOẠT ĐỘNG TỐT"),
    (35, "Trạm Bờ GCS (ESP32)", "GPIO 18", "VSPI_SCK_PIN (Chung: TFT + LoRa)", "VSPI SCK (27MHz)", "Xung nhịp bus chia sẻ tốc độ cao giữa màn hình và bộ thu phát vô tuyến", "HOẠT ĐỘNG TỐT"),
    (36, "Trạm Bờ GCS (ESP32)", "GPIO 19", "VSPI_MISO_PIN (LoRa Ra-02 MISO)", "VSPI MISO (3.3V)", "Thu gói Telemetry 30 trường (bộ đệm 256-byte) từ phao USV tần số 1Hz", "HOẠT ĐỘNG TỐT"),
    (37, "Trạm Bờ GCS (ESP32)", "GPIO 22", "SW7_PAGE_AUTO (Nút kích hoạt Auto 3 tầng)", "GPIO In (Pull-up 3.3V)", "Kích hoạt chu trình tự động khảo sát mặt cắt nước 3 tầng từ xa", "HOẠT ĐỘNG TỐT"),
    (38, "Trạm Bờ GCS (ESP32)", "GPIO 23", "VSPI_MOSI_PIN (Chung: TFT + LoRa)", "VSPI MOSI (3.3V)", "Phát gói lệnh điều khiển Joystick và đẩy pixel đồ họa ra màn hình TFT", "HOẠT ĐỘNG TỐT"),
    (39, "Trạm Bờ GCS (ESP32)", "GPIO 25", "SW1_PAGE_NEXT (Nút chuyển trang TFT)", "GPIO In (Pull-up 3.3V)", "Lật vòng quanh 5 trang màn hình tác chiến hiện trường", "HOẠT ĐỘNG TỐT"),
    (40, "Trạm Bờ GCS (ESP32)", "GPIO 26", "LORA_DIO0_PIN (LoRa Ra-02 DIO0 GCS)", "GPIO In (Interrupt)", "Ngắt nhận gói tin vô tuyến và kiểm định toàn vẹn mã CRC-8", "HOẠT ĐỘNG TỐT"),
    (41, "Trạm Bờ GCS (ESP32)", "GPIO 27", "SW2_TARE_IMU (Nút cân bằng góc 0.0°)", "GPIO In (Pull-up 3.3V)", "Bắn lệnh Tare MPU6050 cân bằng mặt phẳng chuẩn từ xa", "HOẠT ĐỘNG TỐT"),
    (42, "Trạm Bờ GCS (ESP32)", "GPIO 32", "SW5_WINCH_DOWN (Nút hạ tời thủ công)", "GPIO In (Pull-up 3.3V)", "Hạ lồng cảm biến từng nấc 10cm theo ý muốn người vận hành", "HOẠT ĐỘNG TỐT"),
    (43, "Trạm Bờ GCS (ESP32)", "GPIO 33", "SW6_WINCH_HOME (Nút thu tời về Home)", "GPIO In (Pull-up 3.3V)", "Kéo lồng cảm biến thu sát về gầm sàn phao an toàn", "HOẠT ĐỘNG TỐT"),
    (44, "Trạm Bờ GCS (ESP32)", "GPIO 34", "JOY1_VRY_PIN (Joystick trục Y - Ga tiến/lùi)", "ADC1 Ch6 (0-3.3V)", "Tự động lấy điểm 0 trung tính, ngắt ga an toàn khi phát hiện tuột dây", "HOẠT ĐỘNG TỐT"),
    (45, "Trạm Bờ GCS (ESP32)", "GPIO 35", "JOY1_VRX_PIN (Joystick trục X - Lái trái/phải)", "ADC1 Ch7 (0-3.3V)", "Điều khiển góc bẻ bánh lái vô cấp siêu mượt", "HOẠT ĐỘNG TỐT")
]

def hex_to_rgb(hex_str):
    hex_str = hex_str.lstrip('#')
    return tuple(int(hex_str[i:i+2], 16) for i in (0, 2, 4))

def create_component_card(comp, filename):
    """Tạo ảnh minh họa linh kiện chuyên nghiệp bằng Pillow, không bao giờ cắt cụt từ tiếng Việt"""
    width, height = 260, 180
    im = Image.new("RGB", (width, height), color=(15, 23, 42))
    draw = ImageDraw.Draw(im)

    font_title = get_system_font(13, bold=True)
    font_body = get_system_font(10, bold=False)
    font_body_bold = get_system_font(12, bold=True)
    font_small = get_system_font(10, bold=False)

    main_color = hex_to_rgb(comp["badge_color"])
    draw.rectangle([0, 0, width-1, height-1], outline=main_color, width=3)
    draw.rectangle([3, 3, width-4, 38], fill=main_color)
    draw.text((12, 10), comp["tag"], fill=(255, 255, 255), font=font_title)
    draw.rectangle([20, 50, width-20, 130], outline=(51, 65, 85), fill=(30, 41, 59), width=2)

    tag = comp["tag"]
    if "MCU" in tag:
        draw.rectangle([80, 65, 180, 115], fill=(15, 23, 42), outline=main_color, width=2)
        draw.text((95, 80), "ESP32", fill=(0, 245, 212), font=font_body_bold)
        for x in range(85, 180, 15):
            draw.line([x, 58, x, 65], fill=(148, 163, 184), width=2)
            draw.line([x, 115, x, 122], fill=(148, 163, 184), width=2)
    elif "LORA" in tag or "4G" in tag or "GPS" in tag:
        draw.line([130, 120, 130, 75], fill=(0, 245, 212), width=3)
        draw.ellipse([125, 70, 135, 80], fill=(245, 158, 11))
        draw.arc([115, 60, 145, 90], start=200, end=340, fill=main_color, width=2)
        draw.arc([105, 50, 155, 100], start=200, end=340, fill=main_color, width=2)
        draw.text((70, 105), tag.split()[0], fill=(255, 255, 255), font=font_small)
    elif "BAT" in tag or "LIPO" in tag:
        draw.rectangle([70, 75, 175, 110], fill=(22, 163, 74), outline=(255, 255, 255), width=2)
        draw.rectangle([175, 85, 185, 100], fill=(255, 255, 255))
        bat_txt = "2S 8.4V" if ("2S" in tag or "18650" in comp["name"]) else "3S 12.6V"
        draw.text((85, 82), bat_txt, fill=(255, 255, 255), font=font_body_bold)
    elif "pH" in tag or "TDS" in tag or "NTU" in tag or "1-WIRE" in tag:
        draw.rectangle([115, 60, 145, 105], fill=(59, 130, 246), outline=(255, 255, 255), width=2)
        draw.polygon([(115, 105), (145, 105), (130, 125)], fill=(234, 88, 12))
        draw.text((55, 80), tag.split()[0], fill=(0, 245, 212), font=font_body_bold)
    elif "MOTOR" in tag or "SERVO" in tag or "WINCH" in tag:
        draw.ellipse([100, 65, 160, 125], fill=(71, 85, 105), outline=main_color, width=3)
        draw.ellipse([120, 85, 140, 105], fill=(245, 158, 11))
        draw.text((65, 70), "M", fill=(255, 255, 255), font=font_body_bold)
    else:
        draw.rectangle([70, 65, 190, 120], fill=(30, 41, 59), outline=main_color, width=2)
        draw.text((80, 82), tag[:12], fill=(0, 245, 212), font=font_body_bold)

    cat_short = smart_truncate(comp["category"], max_chars=38)
    draw.text((10, 138), cat_short, fill=(203, 213, 225), font=font_body)
    draw.text((10, 156), f"ID: #{comp['stt']} • Real HW Verified", fill=(100, 116, 139), font=font_small)

    im.save(filename, "PNG")
    return filename

def generate_excel():
    wb = openpyxl.Workbook()
    wb.remove(wb.active)

    font_title = Font(name="Segoe UI", size=15, bold=True, color="FFFFFF")
    font_subtitle = Font(name="Segoe UI", size=10, italic=True, color="CBD5E1")
    font_header = Font(name="Segoe UI", size=11, bold=True, color="FFFFFF")
    font_data = Font(name="Segoe UI", size=10, color="0F172A")
    font_bold = Font(name="Segoe UI", size=10, bold=True, color="0F172A")
    font_code = Font(name="Consolas", size=9.5, bold=True, color="1E3A8A")
    font_pass = Font(name="Segoe UI", size=9.5, bold=True, color="047857")
    font_guard = Font(name="Segoe UI", size=9.5, bold=True, color="0369A1")

    fill_title_usv = PatternFill(start_color="0F172A", end_color="0F172A", fill_type="solid")
    fill_header_usv = PatternFill(start_color="1E3A8A", end_color="1E3A8A", fill_type="solid")
    fill_header_gcs = PatternFill(start_color="065F46", end_color="065F46", fill_type="solid")
    fill_header_gpio = PatternFill(start_color="334155", end_color="334155", fill_type="solid")

    fill_row_even = PatternFill(start_color="F8FAFC", end_color="F8FAFC", fill_type="solid")
    fill_row_odd = PatternFill(start_color="FFFFFF", end_color="FFFFFF", fill_type="solid")
    fill_pass = PatternFill(start_color="DCFCE7", end_color="DCFCE7", fill_type="solid")
    fill_guard = PatternFill(start_color="E0F2FE", end_color="E0F2FE", fill_type="solid")

    thin_border = Border(
        left=Side(style='thin', color='CBD5E1'),
        right=Side(style='thin', color='CBD5E1'),
        top=Side(style='thin', color='CBD5E1'),
        bottom=Side(style='thin', color='CBD5E1')
    )

    align_center = Alignment(horizontal="center", vertical="center", wrap_text=True)
    align_left = Alignment(horizontal="left", vertical="center", wrap_text=True)
    align_top_left = Alignment(horizontal="left", vertical="top", wrap_text=True)

    headers = [
        "STT", "HÌNH ẢNH MINH HỌA", "TÊN LINH KIỆN / THIẾT BỊ",
        "PHÂN LOẠI HỆ THỐNG", "THÔNG SỐ KỸ THUẬT CHI TIẾT",
        "CHÂN KẾT NỐI PHẦN CỨNG (PINOUT)", "CÔNG DỤNG & NHIỆM VỤ TRONG VẬN HÀNH THỰC TẾ"
    ]

    # SHEET 1: PHAO USV
    ws1 = wb.create_sheet(title="Phao_USV_Quan_Trac")
    ws1.views.sheetView[0].showGridLines = True
    ws1.freeze_panes = "A4"

    ws1.merge_cells("A1:G1")
    ws1["A1"] = "HỆ THỐNG PHAO QUAN TRẮC NƯỚC TỰ HÀNH CHEMSTATION AI (USV BUOY)"
    ws1["A1"].font = font_title
    ws1["A1"].fill = fill_title_usv
    ws1["A1"].alignment = align_center

    ws1.merge_cells("A2:G2")
    ws1["A2"] = "BẢNG DANH MỤC LINH KIỆN, SƠ ĐỒ KẾT NỐI CHÂN & CÔNG DỤNG CHI TIẾT 100% PHẦN CỨNG THỰC TẾ"
    ws1["A2"].font = font_subtitle
    ws1["A2"].fill = fill_title_usv
    ws1["A2"].alignment = align_center

    ws1.row_dimensions[1].height = 35
    ws1.row_dimensions[2].height = 22
    ws1.row_dimensions[3].height = 30

    for col_idx, h in enumerate(headers, 1):
        cell = ws1.cell(row=3, column=col_idx, value=h)
        cell.font = font_header
        cell.fill = fill_header_usv
        cell.alignment = align_center
        cell.border = thin_border

    current_row = 4
    for item in USV_COMPONENTS:
        ws1.row_dimensions[current_row].height = 110
        img_path = os.path.join(IMG_DIR, f"usv_{item['stt']}.png")
        create_component_card(item, img_path)

        img = openpyxl.drawing.image.Image(img_path)
        img.width = 145
        img.height = 100
        ws1.add_image(img, f"B{current_row}")

        ws1.cell(row=current_row, column=1, value=item["stt"]).alignment = align_center
        ws1.cell(row=current_row, column=2, value="").alignment = align_center
        ws1.cell(row=current_row, column=3, value=item["name"]).alignment = align_left
        ws1.cell(row=current_row, column=4, value=item["category"]).alignment = align_left
        ws1.cell(row=current_row, column=5, value=item["specs"]).alignment = align_top_left
        ws1.cell(row=current_row, column=6, value=item["pinout"]).alignment = align_top_left
        ws1.cell(row=current_row, column=7, value=item["function"]).alignment = align_top_left

        fill_color = fill_row_even if current_row % 2 == 0 else fill_row_odd
        for c in range(1, 8):
            cell = ws1.cell(row=current_row, column=c)
            cell.border = thin_border
            cell.fill = fill_color
            if c in (1, 3):
                cell.font = font_bold
            elif c == 6:
                cell.font = font_code
            else:
                cell.font = font_data
        current_row += 1

    ws1.auto_filter.ref = f"A3:G{current_row - 1}"
    for i, w in enumerate([8, 22, 28, 25, 38, 35, 55], 1):
        ws1.column_dimensions[get_column_letter(i)].width = w

    # SHEET 2: TRẠM BỜ GCS
    ws2 = wb.create_sheet(title="Tram_Bo_GCS")
    ws2.views.sheetView[0].showGridLines = True
    ws2.freeze_panes = "A4"

    ws2.merge_cells("A1:G1")
    ws2["A1"] = "HỆ THỐNG TRẠM BỜ ĐIỀU KHIỂN & GIÁM SÁT MẶT ĐẤT (GCS GROUND STATION)"
    ws2["A1"].font = font_title
    ws2["A1"].fill = PatternFill(start_color="064E3B", end_color="064E3B", fill_type="solid")
    ws2["A1"].alignment = align_center

    ws2.merge_cells("A2:G2")
    ws2["A2"] = "DANH MỤC THIẾT BỊ ĐIỀU HÀNH, MÀN HÌNH IPS TFT 2.0 INCH, JOYSTICK & DÀN CÔNG TẮC TÁC CHIẾN"
    ws2["A2"].font = font_subtitle
    ws2["A2"].fill = PatternFill(start_color="064E3B", end_color="064E3B", fill_type="solid")
    ws2["A2"].alignment = align_center

    ws2.row_dimensions[1].height = 35
    ws2.row_dimensions[2].height = 22
    ws2.row_dimensions[3].height = 30

    for col_idx, h in enumerate(headers, 1):
        cell = ws2.cell(row=3, column=col_idx, value=h)
        cell.font = font_header
        cell.fill = fill_header_gcs
        cell.alignment = align_center
        cell.border = thin_border

    current_row = 4
    for item in GCS_COMPONENTS:
        ws2.row_dimensions[current_row].height = 110
        img_path = os.path.join(IMG_DIR, f"gcs_{item['stt']}.png")
        create_component_card(item, img_path)

        img = openpyxl.drawing.image.Image(img_path)
        img.width = 145
        img.height = 100
        ws2.add_image(img, f"B{current_row}")

        ws2.cell(row=current_row, column=1, value=item["stt"]).alignment = align_center
        ws2.cell(row=current_row, column=2, value="").alignment = align_center
        ws2.cell(row=current_row, column=3, value=item["name"]).alignment = align_left
        ws2.cell(row=current_row, column=4, value=item["category"]).alignment = align_left
        ws2.cell(row=current_row, column=5, value=item["specs"]).alignment = align_top_left
        ws2.cell(row=current_row, column=6, value=item["pinout"]).alignment = align_top_left
        ws2.cell(row=current_row, column=7, value=item["function"]).alignment = align_top_left

        fill_color = fill_row_even if current_row % 2 == 0 else fill_row_odd
        for c in range(1, 8):
            cell = ws2.cell(row=current_row, column=c)
            cell.border = thin_border
            cell.fill = fill_color
            if c in (1, 3):
                cell.font = font_bold
            elif c == 6:
                cell.font = font_code
            else:
                cell.font = font_data
        current_row += 1

    ws2.auto_filter.ref = f"A3:G{current_row - 1}"
    for i, w in enumerate([8, 22, 28, 25, 38, 35, 55], 1):
        ws2.column_dimensions[get_column_letter(i)].width = w

    # SHEET 3: MA TRẬN CHÂN CẮM GPIO & CHẨN ĐOÁN PHẦN CỨNG (45 CHÂN)
    ws3 = wb.create_sheet(title="So_Do_Dau_Noi_GPIO")
    ws3.views.sheetView[0].showGridLines = True
    ws3.freeze_panes = "A4"

    ws3.merge_cells("A1:G1")
    ws3["A1"] = "MA TRẬN ĐẤU NỐI CHÂN GPIO & CƠ CHẾ TỰ ĐỘNG CHẨN ĐOÁN PHẦN CỨNG"
    ws3["A1"].font = font_title
    ws3["A1"].fill = PatternFill(start_color="1E293B", end_color="1E293B", fill_type="solid")
    ws3["A1"].alignment = align_center

    ws3.merge_cells("A2:G2")
    ws3["A2"] = "TỔNG HỢP 45 ĐƯỜNG TÍN HIỆU ESP32-S3 (PHAO) & ESP32 DEVKIT (TRẠM BỜ GCS)"
    ws3["A2"].font = font_subtitle
    ws3["A2"].fill = PatternFill(start_color="1E293B", end_color="1E293B", fill_type="solid")
    ws3["A2"].alignment = align_center

    ws3.row_dimensions[1].height = 35
    ws3.row_dimensions[2].height = 22
    ws3.row_dimensions[3].height = 30

    gpio_headers = [
        "STT", "PHÂN HỆ HỆ THỐNG", "SỐ HIỆU CHÂN GPIO",
        "MODULE / TÍN HIỆU KẾT NỐI", "CHUẨN GIAO TIẾP & ĐIỆN ÁP",
        "CƠ CHẾ BẢO VỆ & TỰ ĐỘNG CHẨN ĐOÁN (HEALTH DIAGNOSTICS)",
        "TRẠNG THÁI CẤU HÌNH"
    ]
    for col_idx, h in enumerate(gpio_headers, 1):
        cell = ws3.cell(row=3, column=col_idx, value=h)
        cell.font = font_header
        cell.fill = fill_header_gpio
        cell.alignment = align_center
        cell.border = thin_border

    current_row = 4
    for item in GPIO_MATRIX:
        ws3.row_dimensions[current_row].height = 36
        for c_idx in range(1, 8):
            ws3.cell(row=current_row, column=c_idx, value=item[c_idx - 1])

        ws3.cell(row=current_row, column=1).alignment = align_center
        ws3.cell(row=current_row, column=2).alignment = align_left
        ws3.cell(row=current_row, column=3).alignment = align_center
        ws3.cell(row=current_row, column=4).alignment = align_left
        ws3.cell(row=current_row, column=5).alignment = align_left
        ws3.cell(row=current_row, column=6).alignment = align_left
        ws3.cell(row=current_row, column=7).alignment = align_center

        fill_color = fill_row_even if current_row % 2 == 0 else fill_row_odd
        is_guard = "BẢO VỆ" in item[6]
        for c in range(1, 8):
            cell = ws3.cell(row=current_row, column=c)
            cell.border = thin_border
            cell.fill = fill_color
            if c == 1:
                cell.font = font_bold
            elif c == 3:
                cell.font = font_code
            elif c == 7:
                cell.font = font_guard if is_guard else font_pass
                cell.fill = fill_guard if is_guard else fill_pass
            else:
                cell.font = font_data
        current_row += 1

    ws3.auto_filter.ref = f"A3:G{current_row - 1}"
    for i, w in enumerate([8, 24, 18, 36, 26, 56, 24], 1):
        ws3.column_dimensions[get_column_letter(i)].width = w

    output_filename = "Danh_Sach_Linh_Kien_USV_Chemstations.xlsx"
    wb.save(output_filename)
    print(f"Xuat file Excel thanh cong: {output_filename}")

if __name__ == "__main__":
    generate_excel()
