import json
import urllib.request
import datetime

DEFAULT_DISCORD_WEBHOOK = ""
SPREADSHEET_ID = "1IVcoOZhhrgMh_IOLvDm1-ssyUtHGmMhFSyZD3fYICDY"

def send_discord_profile_report(webhook_url=DEFAULT_DISCORD_WEBHOOK):
    payload = {
        "username": "USV ChemStation AI - Quan Trắc 3D",
        "avatar_url": "https://raw.githubusercontent.com/FortAwesome/Font-Awesome/master/svgs/solid/layer-group.svg",
        "embeds": [{
            "title": "🌊 HOÀN TẤT KHẢO SÁT 3 TẦNG NƯỚC (3D BATHYMETRY PROFILE)",
            "description": "Phao USV vừa hoàn thành chu trình tự động hạ tời đo sâu 3 tầng nước tại hồ quan trắc.",
            "color": 0x00BBF9, # Neon Cyan/Blue
            "fields": [
                {
                    "name": "1️⃣ TẦNG MẶT (Độ sâu 0.3m)",
                    "value": "• **pH:** `7.15` | **Oxy (DO):** `6.10 mg/L`\n• **Khoáng TDS:** `18 ppm` (EC: `36 µS/cm`)\n• **Độ đục:** `12 NTU` (Nước trong)\n• **Nhiệt độ:** `29.2°C` | **WQI:** `52` (Đạt chuẩn)",
                    "inline": False
                },
                {
                    "name": "2️⃣ TẦNG GIỮA (Độ sâu 1.0m)",
                    "value": "• **pH:** `7.02` | **Oxy (DO):** `5.60 mg/L`\n• **Khoáng TDS:** `24 ppm` (EC: `48 µS/cm`)\n• **Độ đục:** `28 NTU` (Nước trong)\n• **Nhiệt độ:** `28.7°C` | **WQI:** `50` (Trung bình)",
                    "inline": False
                },
                {
                    "name": "3️⃣ TẦNG ĐÁY (Độ sâu 1.8m)",
                    "value": "• **pH:** `6.84` | **Oxy (DO):** `4.80 mg/L`\n• **Khoáng TDS:** `62 ppm` (EC: `124 µS/cm`)\n• **Độ đục:** `185 NTU` (Đục cao / Cận bùn đáy)\n• **Nhiệt độ:** `27.9°C` | **WQI:** `46` (Cảnh báo)",
                    "inline": False
                },
                {
                    "name": "📊 PHÂN TÍCH PHÂN TẦNG NHIỆT & CHẤT LƯỢNG",
                    "value": "• **Chênh lệch nhiệt độ (ΔT):** `1.3°C` từ mặt xuống đáy hồ.\n• **Độ đục:** Tăng vọt từ `12 NTU` (mặt) lên `185 NTU` (đáy) do dòng đối lưu bùn hữu cơ.\n• **Oxy hòa tan (DO):** Giảm từ `6.10 mg/L` xuống `4.80 mg/L` ở tầng đáy.",
                    "inline": False
                },
                {
                    "name": "🌐 TRỰC QUAN HÓA 3D & DỮ LIỆU ĐÁM MÂY",
                    "value": f"[🌐 **Xem Mô Hình 3D Lòng Hồ WebGL**](http://192.168.4.1/3d)\n[📊 **Mở Bảng Tính Profile_3Tang trên Google Sheets**](https://docs.google.com/spreadsheets/d/{SPREADSHEET_ID}/edit#gid=Profile_3Tang)",
                    "inline": False
                }
            ],
            "footer": {
                "text": "USV ChemStation AI v4.3-DISCORD • Khảo Sát Thể Tích Đa Tầng Tự Động"
            },
            "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat()
        }]
    }

    req = urllib.request.Request(
        webhook_url,
        data=json.dumps(payload).encode('utf-8'),
        headers={'Content-Type': 'application/json', 'User-Agent': 'ChemStation-GCS/4.3'}
    )
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            return resp.status in (200, 204)
    except Exception as e:
        print(f"Error: {e}")
        return False

if __name__ == "__main__":
    ok = send_discord_profile_report()
    print("Ket qua gui Discord 3D Profile Report:", "THANH CONG (HTTP 204)" if ok else "THAT BAI")
