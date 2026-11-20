import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

with open('gsheets_live_bridge.py', 'r', encoding='utf-8', errors='ignore') as f:
    code = f.read()

# 1. Define Discord Webhook Default & Functions
discord_block = '''
DEFAULT_DISCORD_WEBHOOK = ""

def send_discord_report(data, src="LoRa", webhook_url=DEFAULT_DISCORD_WEBHOOK):
    if not webhook_url or not webhook_url.startswith("https://discord.com/api/webhooks/"):
        return False
    import urllib.request
    import json
    import datetime

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
    
    bat_pct = int(max(0.0, min(100.0, (bat - 11.1) / (12.6 - 11.1) * 100.0))) if bat >= 9.0 else 0
    
    if wqi >= 75:
        wqi_status = "🟢 MỨC A: RẤT TỐT (Nước cấp sinh hoạt)"
        embed_color = 0x06D6A0
    elif wqi >= 50:
        wqi_status = "🟡 MỨC B: TRUNG BÌNH (Bảo tồn thủy sinh)"
        embed_color = 0xFFD166
    elif wqi > 0:
        wqi_status = "🔴 MỨC C: Ô NHIỄM (Cần xử lý)"
        embed_color = 0xFF4757
    else:
        wqi_status = "⚪ Đang hiệu chuẩn cảm biến"
        embed_color = 0x00F5D4
    
    hw_names = ["DS18B20", "pH", "TDS", "NTU", "ACS712", "Bat3S", "MPU6050", "GY271", "LoRa", "SIM4G", "GPS"]
    ok_count = sum(1 for i in range(len(hw_names)) if (hwm & (1 << i)))
    missing = [hw_names[i] for i in range(len(hw_names)) if not (hwm & (1 << i))]
    hw_str = f"**{ok_count}/11 OK**" + (f" (Thiếu: {', '.join(missing)})" if missing else "")

    payload = {
        "username": "USV ChemStation AI - Quan Trắc 3D",
        "avatar_url": "https://raw.githubusercontent.com/FortAwesome/Font-Awesome/master/svgs/solid/water.svg",
        "embeds": [{
            "title": f"🌊 BÁO CÁO QUAN TRẮC USV AQUA-TACTICAL | GÓI #{seq}",
            "description": f"**Trạng thái:** {wqi_status}\\n**Kênh truyền:** {src} (RSSI: {rssi} dBm, SNR: {snr:.1f} dB)",
            "color": embed_color,
            "fields": [
                {
                    "name": "🧪 CHỈ SỐ NƯỚC THỰC TẾ",
                    "value": f"• **Độ pH:** `{ph:.2f}`\\n• **Oxy hòa tan (DO):** `{do_mgl:.2f} mg/L`\\n• **Khoáng TDS:** `{tds:.0f} ppm` (EC: `{ec:.0f} µS/cm`)\\n• **Độ đục NTU:** `{ntu:.0f} NTU`\\n• **Nhiệt độ nước:** `{temp:.1f}°C` (S3 Chip: `{s3_temp:.0f}°C`)",
                    "inline": True
                },
                {
                    "name": "⚡ NGUỒN PIN & NĂNG LƯỢNG 3S",
                    "value": f"• **Điện áp:** `{bat:.2f} V` ({bat_pct}%)\\n• **Dòng tải ACS712:** `{amp:.2f} A`\\n• **Công suất:** `{watt:.1f} W`\\n• **Độ sâu lồng tời:** `{depth:.2f} m`",
                    "inline": True
                },
                {
                    "name": "🧭 HẢI TRÌNH & GÓC NGHIÊNG THÂN TÀU",
                    "value": f"• **Góc nghiêng:** `Roll: {roll:+.1f}°` | `Pitch: {pitch:+.1f}°`\\n• **Hướng mũi La bàn:** `{hdg:.0f}°`\\n• **Vận tốc GPS:** `{spd:.1f} km/h` ({sats} vệ tinh)\\n• **Tọa độ:** `{lat:.5f}°N, {lng:.5f}°E`",
                    "inline": False
                },
                {
                    "name": "🛠️ KIỂM ĐỊNH PHẦN CỨNG 11 LINH KIỆN",
                    "value": hw_str,
                    "inline": True
                },
                {
                    "name": "🔗 TRUY CẬP TRỰC TIẾP",
                    "value": f"[🌐 **Xem Bản Đồ 3D WebGL Lòng Hồ**](http://192.168.4.1/3d)\\n[📊 **Mở Bảng Tính Google Sheets**](https://docs.google.com/spreadsheets/d/{DEFAULT_SPREADSHEET_ID}/edit)",
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
        log.warning("Lỗi gửi Discord Webhook: %s", e)
        return False

def send_discord_profile_report(prf, src="LoRa", webhook_url=DEFAULT_DISCORD_WEBHOOK):
    if not webhook_url or not webhook_url.startswith("https://discord.com/api/webhooks/"):
        return False
    import urllib.request
    import json
    import datetime

    payload = {
        "username": "USV ChemStation AI - Quan Trắc 3D",
        "avatar_url": "https://raw.githubusercontent.com/FortAwesome/Font-Awesome/master/svgs/solid/layer-group.svg",
        "embeds": [{
            "title": "🌊 HOÀN TẤT KHẢO SÁT 3 TẦNG NƯỚC (3D BATHYMETRY PROFILE)",
            "description": f"Phao USV vừa hoàn thành chu trình tự động hạ tời đo sâu 3 tầng nước (Nguồn: {src}).",
            "color": 0x00BBF9,
            "fields": [
                {
                    "name": "1️⃣ TẦNG MẶT (Độ sâu 0.3m)",
                    "value": f"• **pH:** `{prf.get('ph1', 0.0):.2f}` | **DO:** `{prf.get('do1', 0.0):.2f} mg/L`\\n• **TDS:** `{prf.get('tds1', 0.0):.0f} ppm` | **NTU:** `{prf.get('ntu1', 0.0):.0f}`\\n• **Nhiệt độ:** `{prf.get('t1', 0.0):.1f}°C` | **WQI:** `{prf.get('wqi1', 0)}`",
                    "inline": False
                },
                {
                    "name": "2️⃣ TẦNG GIỮA (Độ sâu 1.0m)",
                    "value": f"• **pH:** `{prf.get('ph2', 0.0):.2f}` | **DO:** `{prf.get('do2', 0.0):.2f} mg/L`\\n• **TDS:** `{prf.get('tds2', 0.0):.0f} ppm` | **NTU:** `{prf.get('ntu2', 0.0):.0f}`\\n• **Nhiệt độ:** `{prf.get('t2', 0.0):.1f}°C` | **WQI:** `{prf.get('wqi2', 0)}`",
                    "inline": False
                },
                {
                    "name": "3️⃣ TẦNG ĐÁY (Độ sâu 1.8m)",
                    "value": f"• **pH:** `{prf.get('ph3', 0.0):.2f}` | **DO:** `{prf.get('do3', 0.0):.2f} mg/L`\\n• **TDS:** `{prf.get('tds3', 0.0):.0f} ppm` | **NTU:** `{prf.get('ntu3', 0.0):.0f}`\\n• **Nhiệt độ:** `{prf.get('t3', 0.0):.1f}°C` | **WQI:** `{prf.get('wqi3', 0)}`",
                    "inline": False
                },
                {
                    "name": "🌐 TRỰC QUAN HÓA 3D & DỮ LIỆU ĐÁM MÂY",
                    "value": f"[🌐 **Xem Mô Hình 3D Lòng Hồ WebGL**](http://192.168.4.1/3d)\\n[📊 **Mở Bảng Tính Profile_3Tang**](https://docs.google.com/spreadsheets/d/{DEFAULT_SPREADSHEET_ID}/edit#gid=Profile_3Tang)",
                    "inline": False
                }
            ],
            "footer": {
                "text": "USV ChemStation AI v4.3-DISCORD • Khảo Sát Thể Tích Đa Tầng Tự Động"
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
        log.warning("Lỗi gửi Discord Profile Webhook: %s", e)
        return False
'''

# Insert discord_block before build_arg_parser
if "DEFAULT_DISCORD_WEBHOOK" not in code:
    code = code.replace("def build_arg_parser():", discord_block + "\ndef build_arg_parser():", 1)

# Add CLI arguments
old_args = 'ap.add_argument("--interval", type=float, default=5.0,'
new_args = '''ap.add_argument("--discord-webhook", default=DEFAULT_DISCORD_WEBHOOK,
                    help="URL Discord Webhook để gửi báo cáo quan trắc")
    ap.add_argument("--discord-interval", type=float, default=60.0,
                    help="Khoảng cách giây giữa 2 lần gửi Discord (mặc định 60s)")
    ap.add_argument("--no-discord", action="store_true", help="Tắt gửi thông báo Discord")
    ap.add_argument("--interval", type=float, default=5.0,'''

if old_args in code and "--discord-webhook" not in code:
    code = code.replace(old_args, new_args, 1)

# In main loop, trigger Discord
old_loop_init = "last_seq_ts = 0.0"
new_loop_init = "last_seq_ts = 0.0\n    last_discord_ts = 0.0"
if old_loop_init in code and "last_discord_ts" not in code:
    code = code.replace(old_loop_init, new_loop_init, 1)

old_tele_trigger = "last_upload = now"
new_tele_trigger = '''last_upload = now
            if not args.no_discord and args.discord_webhook and (now - last_discord_ts >= args.discord_interval or args.once):
                if send_discord_report(data, src, args.discord_webhook):
                    log.info("Đã gửi báo cáo chi tiết gói #%s về Discord!", seq)
                last_discord_ts = now'''
if old_tele_trigger in code and "send_discord_report(data" not in code:
    code = code.replace(old_tele_trigger, new_tele_trigger, 1)

with open('gsheets_live_bridge.py', 'w', encoding='utf-8') as f:
    f.write(code)

print("Updated gsheets_live_bridge.py with Discord integration!")
