import json
import urllib.request
import datetime

DEFAULT_DISCORD_WEBHOOK = ""
SPREADSHEET_ID = "1IVcoOZhhrgMh_IOLvDm1-ssyUtHGmMhFSyZD3fYICDY"

def send_discord_report(data, src="LoRa", webhook_url=DEFAULT_DISCORD_WEBHOOK, custom_title=None):
    if not webhook_url or not webhook_url.startswith("https://discord.com/api/webhooks/"):
        return False

    seq = data.get("seq", 0)
    ph = data.get("ph", 0.0)
    do_mgl = data.get("do", 0.0)
    tds = data.get("tds", 0.0)
    ec = data.get("ec", 0.0)
    ntu = data.get("ntu", 0.0)
    temp = data.get("temp", 0.0)
    bat = data.get("bat", 0.0)
    amp = data.get("amp", 0.0)
    watt = bat * amp
    roll = data.get("roll", 0.0)
    pitch = data.get("pitch", 0.0)
    hdg = data.get("heading", 0.0)
    depth = data.get("depth", 0.0)
    sats = data.get("sats", 0)
    spd = data.get("spd", 0.0)
    lat = data.get("lat", 0.0)
    lng = data.get("lng", 0.0)
    wqi = data.get("wqi", 0)
    rssi = data.get("rssi", 0)
    snr = data.get("snr", 0.0)
    s3_temp = data.get("s3_chip_temp", 0.0)
    hwm = data.get("hwm", 0)
    
    # Tính phần trăm pin 3S
    bat_pct = int(max(0.0, min(100.0, (bat - 11.1) / (12.6 - 11.1) * 100.0))) if bat >= 9.0 else 0
    
    # Phân loại chất lượng nước theo VN-WQI
    if wqi >= 75:
        wqi_status = "🟢 MỨC A: RẤT TỐT (Nước cấp sinh hoạt)"
        embed_color = 0x06D6A0 # Emerald green
    elif wqi >= 50:
        wqi_status = "🟡 MỨC B: TRUNG BÌNH (Bảo tồn thủy sinh)"
        embed_color = 0xFFD166 # Amber
    elif wqi > 0:
        wqi_status = "🔴 MỨC C: Ô NHIỄM (Cần xử lý)"
        embed_color = 0xFF4757 # Red
    else:
        wqi_status = "⚪ Đang hiệu chuẩn cảm biến"
        embed_color = 0x00F5D4 # Cyan
    
    # Kiểm tra linh kiện 11 module
    hw_names = ["DS18B20", "pH", "TDS", "NTU", "ACS712", "Bat3S", "MPU6050", "GY271", "LoRa", "SIM4G", "GPS"]
    ok_count = 0
    missing = []
    for i, name in enumerate(hw_names):
        if (hwm & (1 << i)) != 0:
            ok_count += 1
        else:
            missing.append(name)
    hw_str = f"**{ok_count}/11 OK**"
    if missing:
        hw_str += f" (Thiếu: {', '.join(missing)})"

    title = custom_title or f"🌊 BÁO CÁO QUAN TRẮC USV AQUA-TACTICAL | GÓI TIN #{seq}"

    payload = {
        "username": "USV ChemStation AI - Quan Trắc 3D",
        "avatar_url": "https://raw.githubusercontent.com/FortAwesome/Font-Awesome/master/svgs/solid/water.svg",
        "embeds": [{
            "title": title,
            "description": f"**Trạng thái:** {wqi_status}\n**Kênh truyền:** {src} (RSSI: {rssi} dBm, SNR: {snr:.1f} dB)",
            "color": embed_color,
            "fields": [
                {
                    "name": "🧪 CHỈ SỐ NƯỚC THỰC TẾ",
                    "value": f"• **Độ pH:** `{ph:.2f}`\n• **Oxy hòa tan (DO):** `{do_mgl:.2f} mg/L`\n• **Khoáng TDS:** `{tds:.0f} ppm` (EC: `{ec:.0f} µS/cm`)\n• **Độ đục NTU:** `{ntu:.0f} NTU`\n• **Nhiệt độ nước:** `{temp:.1f}°C` (S3 Chip: `{s3_temp:.0f}°C`)",
                    "inline": True
                },
                {
                    "name": "⚡ NGUỒN PIN & NĂNG LƯỢNG 3S",
                    "value": f"• **Điện áp:** `{bat:.2f} V` ({bat_pct}%)\n• **Dòng tải ACS712:** `{amp:.2f} A`\n• **Công suất:** `{watt:.1f} W`\n• **Độ sâu lồng tời:** `{depth:.2f} m`",
                    "inline": True
                },
                {
                    "name": "🧭 HẢI TRÌNH & GÓC NGHIÊNG THÂN TÀU",
                    "value": f"• **Góc nghiêng:** `Roll: {roll:+.1f}°` | `Pitch: {pitch:+.1f}°`\n• **Hướng mũi La bàn:** `{hdg:.0f}°`\n• **Vận tốc GPS:** `{spd:.1f} km/h` ({sats} vệ tinh)\n• **Tọa độ:** `{lat:.5f}°N, {lng:.5f}°E`",
                    "inline": False
                },
                {
                    "name": "🛠️ KIỂM ĐỊNH PHẦN CỨNG 11 LINH KIỆN",
                    "value": hw_str,
                    "inline": True
                },
                {
                    "name": "🔗 TRUY CẬP TRỰC TIẾP",
                    "value": f"[🌐 **Xem Bản Đồ 3D WebGL Lòng Hồ**](http://192.168.4.1/3d)\n[📊 **Mở Bảng Tính Google Sheets**](https://docs.google.com/spreadsheets/d/{SPREADSHEET_ID}/edit)",
                    "inline": True
                }
            ],
            "footer": {
                "text": "USV ChemStation AI v4.3-DISCORD • Hệ Thống Quan Trắc Tự Hành Đa Tầng"
            },
            "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat()
        }]
    }

    try:
        req = urllib.request.Request(
            webhook_url,
            data=json.dumps(payload).encode('utf-8'),
            headers={'Content-Type': 'application/json', 'User-Agent': 'ChemStation-GCS/4.3'}
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            return resp.status in (200, 204)
    except Exception as e:
        print(f"Lỗi gửi Discord Webhook: {e}")
        return False

if __name__ == "__main__":
    sample_data = {
        "seq": 2050,
        "ph": 3.76,
        "do": 5.64,
        "tds": 5.0,
        "ec": 11.0,
        "ntu": 141.0,
        "temp": 29.5,
        "bat": 13.26,
        "amp": 0.22,
        "roll": -43.8,
        "pitch": 45.5,
        "heading": 281.0,
        "depth": 0.0,
        "sats": 0,
        "spd": 0.0,
        "lat": 0.0,
        "lng": 0.0,
        "wqi": 51,
        "rssi": -18,
        "snr": 9.8,
        "s3_chip_temp": 37.8,
        "hwm": 1407 # 9/11 LK (thieu GY271, SIM4G)
    }
    ok = send_discord_report(sample_data, src="LoRa 433MHz")
    print("Ket qua gui Discord Telemetry Report:", "THANH CONG (HTTP 204)" if ok else "THAT BAI")
