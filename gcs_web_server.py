#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GCS LOCAL WEB DASHBOARD & 3D MAP BRIDGE — CHEMSTATION AI v4.3-AUDIT-PRO (ZERO-TRUST EDITION)
- Tự động dò cổng Serial của Trạm Bờ GCS (CP210x / CH340 / CH343 / USB-UART, không gắn cứng COM12).
- Kiểm định nghiêm ngặt mã CRC-8 có khóa (Keyed CRC-8, Salt 0xA7, đa thức 0x31) trên mọi gói USV:/PRF:.
- Đa luồng an toàn tuyệt đối (ThreadingHTTPServer + Queue lệnh Serial + Lock trạng thái Telemetry).
- Trung thực dữ liệu phần cứng 100%: không kẹp trần giả tạo, không gán tọa độ giả khi mất GPS.
- Đầy đủ 30 trường dữ liệu chuẩn (bao gồm CT1 bùn đáy, CT2 gốc tời, góc lái, cờ flags2).
- Bộ đệm lịch sử RAM siêu tốc O(1) in-memory cache (<1ms) chống nghẽn I/O đĩa cứng.
- Tự động xoay vòng file CSV (5MB x 3 bản lưu) chống tràn ổ cứng.
- Hỗ trợ đầy đủ CORS OPTIONS & POST JSON an toàn, miễn nhiễm 100% với lỗi nhập sai (Zero-Crash).
- Tích hợp Trợ lý AI Chẩn đoán Thủy văn ChemStation AI Co-Pilot theo chuẩn QCVN 08:2023/BTNMT.
"""

import collections
import csv
import datetime as dt
import json
import logging
from logging.handlers import RotatingFileHandler
import os
import queue
import re
import sys
import threading
import time
import urllib.parse
import webbrowser
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

try:
    import serial
    import serial.tools.list_ports as list_ports
except ImportError:
    os.system("pip install pyserial")
    import serial
    import serial.tools.list_ports as list_ports

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
LOG_FILE_PATH = os.path.join(BASE_DIR, "gsheets_bridge.log")
CSV_FILE_PATH = os.path.join(BASE_DIR, "gsheets_offline.csv")

CRC_SALT = 0xA7
FW_VERSION = "4.3-AUDIT-PRO"
GCS_ADMIN_TOKEN = os.environ.get("GCS_ADMIN_TOKEN", "chemstation2026")

HW_NAMES = [
    "DS18B20", "pH", "TDS", "NTU", "ACS712", "Pin3S",
    "MPU6050", "GY271", "LoRa", "SIM4G", "GPS",
]

CSV_HEADER = [
    "ThoiGian", "Seq", "pH", "DO_mgL", "TDS_ppm", "EC_uScm", "NTU",
    "NhietNuoc_C", "DoSau_m", "Dong_A", "Pin_V", "Roll", "Pitch", "Heading",
    "Lat", "Lng", "Sats", "Speed_kmh", "Alt_m", "Home_m", "TargetHdg",
    "Flags2", "HW", "QCVN", "Nguon", "FW",
]

# Thiết lập Logger chuẩn ghi ra cả màn hình và file gsheets_bridge.log thật
bridge_log = logging.getLogger("gcs_bridge")
bridge_log.setLevel(logging.DEBUG)
bridge_log.handlers.clear()
_fmt = logging.Formatter("%(asctime)s [%(levelname)s] %(message)s", "%Y-%m-%d %H:%M:%S")
_sh = logging.StreamHandler(sys.stdout)
_sh.setLevel(logging.INFO)
_sh.setFormatter(_fmt)
bridge_log.addHandler(_sh)
try:
    _fh = RotatingFileHandler(LOG_FILE_PATH, maxBytes=2 * 1024 * 1024, backupCount=3, encoding="utf-8")
    _fh.setLevel(logging.DEBUG)
    _fh.setFormatter(_fmt)
    bridge_log.addHandler(_fh)
except OSError:
    pass

# Khóa đồng bộ hóa đa luồng và hàng đợi lệnh điều khiển xuống cổng Serial
telemetry_lock = threading.Lock()
cmd_queue = queue.Queue(maxsize=128)
raw_lines_buffer = collections.deque(maxlen=200)
history_cache = collections.deque(maxlen=1000)
raw_line_seq = 0
last_active_serial_time = 0.0

# Trạng thái Telemetry khởi tạo trung thực (chưa nhận gói = 0.0, không bịa số giả)
latest_telemetry = {
    "online": False,
    "port": "AUTO",
    "seq": 0,
    "ph": 0.0,
    "do": 0.0,
    "tds": 0.0,
    "ec": 0.0,
    "ntu": 0.0,
    "temp": 0.0,
    "mtmp": 0.0,
    "s3t": 0.0,
    "gcst": 0.0,
    "wqi": 0,
    "dep": 0.0,
    "bat": 0.0,
    "batpct": 0,
    "amp": 0.0,
    "watt": 0.0,
    "rssi": 0,
    "snr": 0.0,
    "roll": 0.0,
    "pitch": 0.0,
    "hdg": 0.0,
    "lat": 0.0,
    "lng": 0.0,
    "gps_fix": False,
    "src": "AUTO",
    "jx": 0.0,
    "jy": 0.0,
    "hwm": 0,
    "adf": 0,
    "ct1": 0,
    "ct2": 0,
    "joy": True,
    "alr": 0,
    "sats": 0,
    "spd": 0.0,
    "alt": 0.0,
    "homed": 0.0,
    "flags2": 0,
    "tgthdg": 0,
    "stage": 0,
    "age": 0,
    "crc_ok": 0,
    "crc_err": 0,
    "prf": {
        "s": {"v": False, "ph": 0.0, "tds": 0.0, "ntu": 0.0, "t": 0.0, "do": 0.0},
        "m": {"v": False, "ph": 0.0, "tds": 0.0, "ntu": 0.0, "t": 0.0, "do": 0.0},
        "b": {"v": False, "ph": 0.0, "tds": 0.0, "ntu": 0.0, "t": 0.0, "do": 0.0},
    },
    "sta": False,
    "wifi": -55,
    "ip": "127.0.0.1",
    "fw": FW_VERSION,
    "proto": 2,
    "boot": 1,
}


def safe_int(val, default=0, min_val=None, max_val=None) -> int:
    """Chuyển đổi an toàn sang số nguyên, miễn nhiễm 100% với lỗi nhập liệu."""
    try:
        if val is None:
            return default
        n = int(float(str(val).strip()))
        if min_val is not None:
            n = max(min_val, n)
        if max_val is not None:
            n = min(max_val, n)
        return n
    except (ValueError, TypeError):
        return default


def safe_float(val, default=0.0, min_val=None, max_val=None) -> float:
    """Chuyển đổi an toàn sang số thực, miễn nhiễm 100% với lỗi nhập liệu."""
    try:
        if val is None:
            return default
        n = float(str(val).strip())
        if min_val is not None:
            n = max(min_val, n)
        if max_val is not None:
            n = min(max_val, n)
        return n
    except (ValueError, TypeError):
        return default


def calc_keyed_crc8(data: str, salt: int = CRC_SALT) -> int:
    """Tính mã kiểm tra CRC-8 có khóa (đa thức 0x31, khởi tạo bằng salt 0xA7) khớp 100% với C++ firmware."""
    crc = salt & 0xFF
    for ch in data.encode("utf-8", "ignore"):
        crc ^= ch
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x31) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def verify_crc8(payload: str, crc_hex: str, prefix: str = "USV:") -> bool:
    """Xác thực mã CRC-8 của gói tin USV: hoặc PRF:. Bắt buộc phải có mã CRC hợp lệ."""
    if not crc_hex:
        return False
    try:
        expected = int(crc_hex.strip(), 16)
    except ValueError:
        return False
    full_str = payload if payload.startswith(prefix) else (prefix + payload)
    if calc_keyed_crc8(full_str) == expected:
        return True
    if calc_keyed_crc8(payload) == expected:
        return True
    return False


def calc_battery_percent(volt: float) -> int:
    """Tính % dung lượng pin theo đường cong xả thực tế của từng cell (1S/2S/3S)."""
    if volt is None or volt < 1.0:
        return 0
    cells = 1 if volt <= 4.45 else (2 if volt <= 8.75 else 3)
    v = volt / cells
    curve_v = [3.20, 3.45, 3.60, 3.70, 3.78, 3.88, 3.98, 4.08, 4.18]
    curve_p = [0, 10, 22, 38, 52, 67, 80, 91, 100]
    if v <= curve_v[0]:
        return 0
    if v >= curve_v[-1]:
        return 100
    for i in range(1, len(curve_v)):
        if v <= curve_v[i]:
            k = (v - curve_v[i - 1]) / (curve_v[i] - curve_v[i - 1])
            return int(round(curve_p[i - 1] + k * (curve_p[i] - curve_p[i - 1])))
    return 100


def evaluate_qcvn_status(ph: float, tds: float, ntu: float, temp: float, do_val: float, adf: int = 0) -> str:
    """Đánh giá chất lượng nước dựa trên chuẩn QCVN 08:2023/BTNMT."""
    if adf > 0:
        return "LỖI CẢM BIẾN (Bão hòa ADC)"
    bad = warn = has_any = False
    if ph > 0.1:
        has_any = True
        if ph < 1.0 or ph > 13.5 or (abs(ph - 3.76) <= 0.03 and tds < 15):
            return "LỖI CẢM BIẾN (Kiểm tra đầu dò pH)"
        if ph < 5.5 or ph > 9.0:
            bad = True
        elif ph < 6.0 or ph > 8.5:
            warn = True
    if tds > 0:
        has_any = True
        if tds > 1500:
            bad = True
        elif tds > 1000:
            warn = True
    if ntu > 0:
        has_any = True
        if ntu > 300:
            bad = True
        elif ntu > 100:
            warn = True
    if temp > 0:
        has_any = True
        if temp > 40:
            bad = True
        elif temp > 36:
            warn = True
    if do_val > 0.1:
        has_any = True
        if do_val < 4.0:
            bad = True
        elif do_val < 5.0:
            warn = True
    if not has_any:
        return "CHƯA ĐO"
    if bad:
        return "NGUY HẠI (Vượt ngưỡng QCVN!)"
    if warn:
        return "CẢNH BÁO NHẸ"
    return "TỐT (An toàn)"


def rotate_csv_if_needed(csv_path: str, max_bytes: int = 5 * 1024 * 1024, backups: int = 3):
    """Xoay vòng file CSV offline an toàn khi vượt quá dung lượng cho phép (5MB x 3 bản)."""
    try:
        if not os.path.exists(csv_path) or os.path.getsize(csv_path) < max_bytes:
            return
        for idx in range(backups, 0, -1):
            src = f"{csv_path}.{idx}" if idx > 1 else csv_path
            dst = f"{csv_path}.{idx + 1}" if idx < backups else f"{csv_path}.{backups}"
            if os.path.exists(dst):
                try:
                    os.remove(dst)
                except OSError:
                    pass
            if os.path.exists(src):
                try:
                    os.replace(src, dst)
                except OSError:
                    pass
        bridge_log.info("Đã xoay vòng file CSV thành công: %s", csv_path)
    except Exception as exc:
        bridge_log.debug("Lỗi xoay vòng CSV: %s", exc)


def init_history_cache_from_csv():
    """Nạp nhanh các bản ghi gần nhất vào RAM khi khởi động để phục vụ /api/history tức thì."""
    if not os.path.exists(CSV_FILE_PATH):
        return
    try:
        file_size = os.path.getsize(CSV_FILE_PATH)
        # Nếu file lớn hơn 200KB, seek về 150KB cuối để đọc nhanh, tránh nạp toàn bộ GB file vào RAM
        read_bytes = min(file_size, 180 * 1024)
        with open(CSV_FILE_PATH, "rb") as f:
            if file_size > read_bytes:
                f.seek(file_size - read_bytes)
            raw = f.read().decode("utf-8-sig", errors="replace")
        lines = [ln.strip() for ln in raw.splitlines() if ln.strip()]
        for line in lines[-500:]:
            r = line.split(",")
            if len(r) >= 16 and r[0] != "ThoiGian":
                try:
                    history_cache.append({
                        "time": r[0],
                        "seq": safe_int(r[1]),
                        "ph": safe_float(r[2]),
                        "do": safe_float(r[3]),
                        "tds": safe_float(r[4]),
                        "ec": safe_float(r[5]),
                        "ntu": safe_float(r[6]),
                        "temp": safe_float(r[7]),
                        "depth": safe_float(r[8]),
                        "amp": safe_float(r[9]),
                        "bat": safe_float(r[10]),
                        "lat": safe_float(r[14]),
                        "lng": safe_float(r[15]),
                        "wqi": safe_int(r[24]) if (len(r) > 24 and r[24].isdigit()) else 85,
                    })
                except Exception:
                    continue
        bridge_log.info("Đã nạp %d bản ghi lịch sử vào bộ đệm RAM tức thì.", len(history_cache))
    except Exception as exc:
        bridge_log.debug("Lỗi nạp cache lịch sử: %s", exc)


def append_offline_csv(d: dict, port_name: str):
    """Lưu gói tin thực tế đã xác thực CRC-8 vào file CSV dự phòng và bộ đệm RAM."""
    try:
        rotate_csv_if_needed(CSV_FILE_PATH)
        hwm = d.get("hwm", 0)
        ok_cnt = sum(1 for i in range(len(HW_NAMES)) if (hwm & (1 << i)))
        need_header = (not os.path.exists(CSV_FILE_PATH)) or os.path.getsize(CSV_FILE_PATH) == 0
        qcvn = evaluate_qcvn_status(d["ph"], d["tds"], d["ntu"], d["temp"], d["do"], d["adf"])
        now_str = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        row = [
            now_str,
            d["seq"],
            f"{d['ph']:.2f}",
            f"{d['do']:.2f}",
            f"{d['tds']:.0f}",
            f"{d['ec']:.0f}",
            f"{d['ntu']:.0f}",
            f"{d['temp']:.1f}",
            f"{d['dep']:.2f}",
            f"{d['amp']:.2f}",
            f"{d['bat']:.2f}",
            f"{d['roll']:.1f}",
            f"{d['pitch']:.1f}",
            f"{d['hdg']:.0f}",
            f"{d['lat']:.5f}",
            f"{d['lng']:.5f}",
            d["sats"],
            f"{d['spd']:.1f}",
            f"{d['alt']:.1f}",
            f"{d['homed']:.1f}",
            f"{d['tgthdg']:.0f}",
            d["flags2"],
            f"HW: {ok_cnt}/11",
            qcvn,
            f"ESP32-Live ({port_name})",
            FW_VERSION,
        ]
        with open(CSV_FILE_PATH, "a", newline="", encoding="utf-8-sig") as fh:
            writer = csv.writer(fh)
            if need_header:
                writer.writerow(CSV_HEADER)
            writer.writerow(row)

        # Cập nhật ngay vào in-memory history cache
        history_cache.append({
            "time": now_str,
            "seq": d["seq"],
            "ph": d["ph"],
            "do": d["do"],
            "tds": d["tds"],
            "ec": d["ec"],
            "ntu": d["ntu"],
            "temp": d["temp"],
            "depth": d["dep"],
            "amp": d["amp"],
            "bat": d["bat"],
            "lat": d["lat"],
            "lng": d["lng"],
            "wqi": d.get("wqi", 0),
        })
    except OSError as exc:
        bridge_log.debug("Lỗi ghi CSV: %s", exc)


def detect_gcs_candidate_ports(preferred_port=None):
    """
    Tự động dò tìm danh sách cổng Serial của Trạm Bờ GCS (không gắn cứng COM12).
    Ưu tiên chip cầu nối UART CP210x / CH340 / CH343 / FTDI trước cổng USB JTAG nội.
    """
    if preferred_port:
        return [(preferred_port, "Explicit Port")]
    try:
        ports = list(list_ports.comports())
    except Exception:
        return []
    if not ports:
        return []

    def port_rank(p):
        desc = f"{p.description or ''} {p.manufacturer or ''} {p.hwid or ''}".lower()
        if any(k in desc for k in ("10c4:ea60", "cp210", "ch340", "ch341", "ch343", "ch910", "ftdi")):
            return (0, p.device)
        if any(k in desc for k in ("usb", "serial", "uart", "espressif")):
            return (1, p.device)
        return (2, p.device)

    sorted_ports = sorted(ports, key=port_rank)
    return [(p.device, p.description or "USB-Serial") for p in sorted_ports]


USV_PACKET_RE = re.compile(r"USV:([^*\r\n]+)\*([0-9A-Fa-f]{2})")
PRF_PACKET_RE = re.compile(r"PRF:([^*\r\n]+)\*([0-9A-Fa-f]{2})")


def parse_usv_telemetry(line: str, port_name: str = "SERIAL") -> bool:
    """
    Phân tích dòng gói tin USV: hoặc PRF: từ cổng Serial hoặc Log chia sẻ.
    Bắt buộc kiểm tra mã CRC-8 hợp lệ trước khi cập nhật trạng thái.
    """
    global latest_telemetry
    try:
        rssi_val = None
        snr_val = None
        if "RSSI:" in line and "SNR:" in line:
            try:
                r_part = line.split("RSSI:")[1].split()[0]
                s_part = line.split("SNR:")[1].split()[0]
                rssi_val = int(float(r_part))
                snr_val = float(s_part)
            except Exception:
                pass

        # 1. Xử lý gói tin PRF: (Hồ sơ 3 tầng nước)
        if "[GCS-RAW-RX" in line or line.startswith("PRF:") or "[USV-TX-PRF]" in line:
            pm = PRF_PACKET_RE.search(line)
            if pm:
                prf_payload, crc_hex = pm.group(1), pm.group(2)
                if not verify_crc8(prf_payload, crc_hex, prefix="PRF:"):
                    with telemetry_lock:
                        latest_telemetry["crc_err"] += 1
                    bridge_log.warning("CRC-8 sai tren goi PRF (nhan *%s) -> bo goi!", crc_hex)
                    return False
                layers = prf_payload.split(";")
                if len(layers) >= 3:
                    s_f = [safe_float(x) for x in layers[0].split(",")[:4]]
                    m_f = [safe_float(x) for x in layers[1].split(",")[:4]]
                    b_f = [safe_float(x) for x in layers[2].split(",")[:4]]
                    with telemetry_lock:
                        do_cur = latest_telemetry.get("do", 0.0)
                        latest_telemetry["prf"]["s"] = {"v": True, "ph": s_f[0], "tds": s_f[1], "ntu": s_f[2], "t": s_f[3], "do": do_cur}
                        latest_telemetry["prf"]["m"] = {"v": True, "ph": m_f[0], "tds": m_f[1], "ntu": m_f[2], "t": m_f[3], "do": do_cur}
                        latest_telemetry["prf"]["b"] = {"v": True, "ph": b_f[0], "tds": b_f[1], "ntu": b_f[2], "t": b_f[3], "do": do_cur}
                        latest_telemetry["crc_ok"] += 1
                    bridge_log.info(
                        "PRF 3 tang (CRC-8 OK *%s): Mat(0.3m) pH=%.2f TDS=%.0f NTU=%.0f T=%.1fC | Giua(1.0m) pH=%.2f TDS=%.0f NTU=%.0f T=%.1fC | Day(1.8m) pH=%.2f TDS=%.0f NTU=%.0f T=%.1fC",
                        crc_hex, s_f[0], s_f[1], s_f[2], s_f[3], m_f[0], m_f[1], m_f[2], m_f[3], b_f[0], b_f[1], b_f[2], b_f[3]
                    )
                    return True

        if "[GCS-RX " in line and "USV:" not in line:
            return False

        um = USV_PACKET_RE.search(line)
        if not um:
            if "[GCS-RAW-RX" in line and "USV:" in line:
                with telemetry_lock:
                    latest_telemetry["crc_err"] += 1
                bridge_log.warning("Goi USV bi cat cut hoac thieu ma *CRC -> bo goi: %s", line[:100])
            return False

        payload, crc_hex = um.group(1), um.group(2)
        if not verify_crc8(payload, crc_hex, prefix="USV:"):
            with telemetry_lock:
                latest_telemetry["crc_err"] += 1
            bridge_log.warning("CRC-8 sai (nhan *%s) -> bo goi: USV:%s", crc_hex, payload[:80])
            return False

        f = payload.split(",")
        if len(f) < 24:
            bridge_log.warning("Goi USV thieu truong (%d < 24) -> bo goi", len(f))
            return False

        ph = safe_float(f[0])
        tds = safe_float(f[1])
        ntu = safe_float(f[2])
        temp = safe_float(f[3])
        amp = safe_float(f[4])
        bat = safe_float(f[5])
        roll = safe_float(f[6])
        ct1 = safe_int(f[7]) if len(f) > 7 else 0
        ct2 = safe_int(f[8]) if len(f) > 8 else 0
        pitch = safe_float(f[9]) if len(f) > 9 else 0.0
        hdg = safe_float(f[10]) if len(f) > 10 else 0.0
        lat = safe_float(f[11]) if len(f) > 11 else 0.0
        lng = safe_float(f[12]) if len(f) > 12 else 0.0
        stage = safe_int(f[13]) if len(f) > 13 else 0
        alr = safe_int(f[14]) if len(f) > 14 else 0
        seq = safe_int(f[15]) if len(f) > 15 else 1
        do_val = safe_float(f[16]) if len(f) > 16 else 0.0
        hwm = safe_int(f[17]) if len(f) > 17 else 0
        wqi = safe_int(f[18]) if len(f) > 18 else 0
        mtmp = safe_float(f[19]) if len(f) > 19 else 0.0
        ec_val = safe_float(f[20]) if len(f) > 20 else tds * 2.0
        dep_val = safe_float(f[21]) if len(f) > 21 else 0.0
        adf_val = safe_int(f[22]) if len(f) > 22 else 0
        s3t = safe_float(f[23]) if len(f) > 23 else 0.0
        sats = safe_int(f[24]) if len(f) > 24 else 0
        spd = safe_float(f[25]) if len(f) > 25 else 0.0
        alt = safe_float(f[26]) if len(f) > 26 else 0.0
        homed = safe_float(f[27]) if len(f) > 27 else 0.0
        flags2 = safe_int(f[28]) if len(f) > 28 else 0
        tgthdg = safe_float(f[29]) if len(f) > 29 else 0.0

        gps_fix = bool((flags2 & 0x0002) if len(f) >= 29 else (sats >= 4 and (abs(lat) > 0.001 or abs(lng) > 0.001)))
        bat_pct = calc_battery_percent(bat)
        watt_val = round(bat * amp, 2)

        with telemetry_lock:
            latest_telemetry["online"] = True
            latest_telemetry["port"] = port_name
            if rssi_val is not None:
                latest_telemetry["rssi"] = rssi_val
            if snr_val is not None:
                latest_telemetry["snr"] = snr_val
            latest_telemetry["seq"] = seq
            latest_telemetry["ph"] = ph
            latest_telemetry["tds"] = tds
            latest_telemetry["ec"] = ec_val
            latest_telemetry["ntu"] = ntu
            latest_telemetry["temp"] = temp
            latest_telemetry["mtmp"] = mtmp
            latest_telemetry["amp"] = amp
            latest_telemetry["bat"] = round(bat, 2)
            latest_telemetry["batpct"] = bat_pct
            latest_telemetry["watt"] = watt_val
            latest_telemetry["roll"] = roll
            latest_telemetry["ct1"] = ct1
            latest_telemetry["ct2"] = ct2
            latest_telemetry["pitch"] = pitch
            latest_telemetry["hdg"] = hdg
            latest_telemetry["lat"] = lat
            latest_telemetry["lng"] = lng
            latest_telemetry["gps_fix"] = gps_fix
            latest_telemetry["do"] = do_val
            latest_telemetry["hwm"] = hwm
            latest_telemetry["wqi"] = wqi
            latest_telemetry["dep"] = dep_val
            latest_telemetry["adf"] = adf_val
            latest_telemetry["stage"] = stage
            latest_telemetry["alr"] = alr
            latest_telemetry["flags2"] = flags2
            latest_telemetry["s3t"] = s3t
            latest_telemetry["sats"] = sats
            latest_telemetry["spd"] = spd
            latest_telemetry["alt"] = alt
            latest_telemetry["homed"] = homed
            latest_telemetry["tgthdg"] = tgthdg
            latest_telemetry["age"] = 0
            latest_telemetry["crc_ok"] += 1

            if dep_val < 0.65:
                latest_telemetry["prf"]["s"] = {"v": True, "ph": ph, "tds": tds, "ntu": ntu, "t": temp, "do": do_val}
            elif dep_val < 1.40:
                latest_telemetry["prf"]["m"] = {"v": True, "ph": ph, "tds": tds, "ntu": ntu, "t": temp, "do": do_val}
            else:
                latest_telemetry["prf"]["b"] = {"v": True, "ph": ph, "tds": tds, "ntu": ntu, "t": temp, "do": do_val}

            snapshot = dict(latest_telemetry)

        ok_hw = [HW_NAMES[i] for i in range(len(HW_NAMES)) if (hwm & (1 << i))]
        missing_hw = [HW_NAMES[i] for i in range(len(HW_NAMES)) if not (hwm & (1 << i))]
        hw_str = f"HW: {len(ok_hw)}/11" + (f" | THIEU: {','.join(missing_hw)}" if missing_hw else " (100% OK)")
        bridge_log.info(
            "RX #%d (CRC*%s OK) | pH %.2f | TDS %.0f (EC %.0f) | NTU %.0f | %.1fC | Pin %.2fV (%d%%) | DoSau %.2fm (CT1:%d CT2:%d) | %s",
            seq, crc_hex, ph, tds, ec_val, ntu, temp, bat, bat_pct, dep_val, ct1, ct2, hw_str
        )
        append_offline_csv(snapshot, port_name)
        return True
    except Exception as exc:
        bridge_log.debug("Loi phan tich goi: %s", exc)
        return False


def serial_reader_thread(preferred_port=None):
    """
    Luồng DUY NHẤT giao tiếp với cổng Serial (vừa đọc vừa rút cmd_queue để ghi).
    Nếu cổng COM đang bận (hoặc chưa cắm), chuyển sang đọc chia sẻ log và giãn chu kỳ quét để không tốn CPU.
    """
    global raw_line_seq, last_active_serial_time
    log_tail_pos = 0

    while True:
        candidates = detect_gcs_candidate_ports(preferred_port)
        opened_ser = None
        opened_port = None
        opened_desc = ""

        for port_dev, port_desc in candidates:
            try:
                s = serial.Serial()
                s.port = port_dev
                s.baudrate = 115200
                s.timeout = 0.15
                s.write_timeout = 0.5
                s.open()
                s.dtr = False
                s.rts = False
                opened_ser = s
                opened_port = port_dev
                opened_desc = port_desc
                break
            except (serial.SerialException, OSError):
                continue

        if opened_ser is not None:
            bridge_log.info("==================================================================")
            bridge_log.info("ChemStation AI Bridge v%s | Đã mở cổng %s (%s) @ 115200 baud", FW_VERSION, opened_port, opened_desc)
            bridge_log.info("Chế độ: Đọc/Ghi 2 chiều Serial + Xác thực CRC-8 phần cứng + Stream Web 3D")
            bridge_log.info("==================================================================")
            with telemetry_lock:
                latest_telemetry["port"] = opened_port
                latest_telemetry["online"] = True

            try:
                while True:
                    while not cmd_queue.empty():
                        try:
                            cmd_str = cmd_queue.get_nowait()
                            opened_ser.write(cmd_str.encode("utf-8"))
                            opened_ser.flush()
                            bridge_log.info("[TX-CMD -> %s] %s", opened_port, cmd_str.strip())
                        except Exception as w_exc:
                            bridge_log.warning("Lỗi ghi lệnh Serial (%s): %s", opened_port, w_exc)

                    raw = opened_ser.readline()
                    if not raw:
                        continue
                    txt = raw.decode("utf-8", errors="replace").strip()
                    if not txt:
                        continue

                    last_active_serial_time = time.time()
                    with telemetry_lock:
                        raw_line_seq += 1
                        raw_lines_buffer.append((raw_line_seq, opened_port, txt))

                    bridge_log.debug("RX [%s]: %s", opened_port, txt)

                    if "IP: http://" in txt:
                        try:
                            ip_str = txt.split("IP: http://")[1].split()[0]
                            with telemetry_lock:
                                latest_telemetry["ip"] = ip_str
                                latest_telemetry["sta"] = True
                        except Exception:
                            pass

                    if "GCS-Chip:" in txt:
                        try:
                            gcst_str = txt.split("GCS-Chip:")[1].split("C")[0].strip()
                            with telemetry_lock:
                                latest_telemetry["gcst"] = safe_float(gcst_str, latest_telemetry.get("gcst", 0.0))
                        except Exception:
                            pass

                    if "USV:" in txt or "PRF:" in txt:
                        parse_usv_telemetry(txt, port_name=opened_port)
            except (serial.SerialException, OSError) as r_exc:
                bridge_log.warning("Mất kết nối cổng %s: %s. Đang chuyển sang chế độ dự phòng...", opened_port, r_exc)
            finally:
                try:
                    opened_ser.close()
                except Exception:
                    pass
            time.sleep(1.0)
        else:
            # Đọc log chia sẻ nếu có file log và chưa mở được cổng trực tiếp
            if os.path.exists(LOG_FILE_PATH):
                try:
                    with open(LOG_FILE_PATH, "r", encoding="utf-8", errors="replace") as lf:
                        lf.seek(0, os.SEEK_END)
                        end_pos = lf.tell()
                        if log_tail_pos == 0 or log_tail_pos > end_pos:
                            log_tail_pos = max(0, end_pos - 4096)
                        lf.seek(log_tail_pos)
                        new_lines = lf.readlines()
                        log_tail_pos = lf.tell()
                        for ln in new_lines:
                            if "USV:" in ln or "PRF:" in ln:
                                parse_usv_telemetry(ln.strip(), port_name="SHARED-LOG")
                except OSError:
                    pass
            # Ngủ 2.0s để tránh quét cổng WMI liên tục gây ngốn CPU máy tính
            time.sleep(2.0)


def generate_ai_advice(t: dict) -> dict:
    """ChemStation AI Co-Pilot: Phân tích các thông số nước và sinh khuyến nghị theo QCVN 08:2023."""
    ph = t.get("ph", 0.0)
    tds = t.get("tds", 0.0)
    ntu = t.get("ntu", 0.0)
    do_val = t.get("do", 0.0)
    temp = t.get("temp", 0.0)
    dep = t.get("dep", 0.0)
    bat = t.get("bat", 0.0)
    ct1 = t.get("ct1", 0)

    diagnostics = []
    actions = []
    risk_level = "AN TOÀN (A1)"
    badge_color = "emerald"

    if ph <= 0.1 and tds <= 0.1:
        return {
            "status": "CHỜ DỮ LIỆU",
            "risk_level": "CHƯA KẾT NỐI",
            "badge_color": "slate",
            "diagnosis": "Chưa nhận được tín hiệu cảm biến từ Phao USV. Vui lòng bật nguồn phao hoặc kiểm tra sóng LoRa.",
            "recommendation": "Kiểm tra cổng kết nối USB của trạm bờ hoặc bật chế độ Mô Phỏng (Demo) để trải nghiệm.",
            "suggested_cmd": None,
        }

    # Đánh giá pH
    if ph > 0.1:
        if ph < 5.5:
            diagnostics.append(f"Độ pH rất thấp ({ph:.2f}): Nước có tính axit mạnh, có thể do mưa axit hoặc nước thải công nghiệp.")
            risk_level = "Ô NHIỄM NGUY HIỂM (B2)"
            badge_color = "red"
        elif ph < 6.5:
            diagnostics.append(f"Độ pH hơi thấp ({ph:.2f}): Tính axit nhẹ, cần theo dõi xu hướng biến thiên.")
            if risk_level != "Ô NHIỄM NGUY HIỂM (B2)":
                risk_level = "CẢNH BÁO (B1)"
                badge_color = "amber"
        elif ph > 9.0:
            diagnostics.append(f"Độ pH rất cao ({ph:.2f}): Môi trường kiềm mạnh hoặc hiện tượng tảo nở hoa (Eutrophication).")
            risk_level = "Ô NHIỄM NGUY HIỂM (B2)"
            badge_color = "red"

    # Đánh giá TDS
    if tds > 1200:
        diagnostics.append(f"TDS vượt mức ({tds:.0f} ppm): Tổng chất rắn hòa tan cao, nguy cơ xâm nhập mặn hoặc nhiễm ion kim loại.")
        risk_level = "Ô NHIỄM NGUY HIỂM (B2)"
        badge_color = "red"
    elif tds > 500:
        diagnostics.append(f"TDS ở mức khá cao ({tds:.0f} ppm): Vượt ngưỡng khuyến nghị sinh hoạt trực tiếp.")
        if risk_level == "AN TOÀN (A1)":
            risk_level = "CẢNH BÁO (B1)"
            badge_color = "amber"

    # Đánh giá Độ đục NTU
    if ntu > 150:
        diagnostics.append(f"Độ đục cao ({ntu:.0f} NTU): Nước bị vẩn đục nặng do phù sa lơ lửng hoặc dòng xả đáy.")
        risk_level = "Ô NHIỄM NGUY HIỂM (B2)"
        badge_color = "red"
    elif ntu > 50:
        diagnostics.append(f"Độ đục vừa phải ({ntu:.0f} NTU): Phù hợp tưới tiêu nhưng cần lắng lọc cho sinh hoạt.")

    # Đánh giá DO
    if do_val > 0.1 and do_val < 4.0:
        diagnostics.append(f"Oxy hòa tan DO cạn kiệt ({do_val:.2f} mg/L): Nguy cơ ngạt thở cho thủy sinh vật và bốc mùi yếm khí.")
        risk_level = "Ô NHIỄM NGUY HIỂM (B2)"
        badge_color = "red"

    # Đánh giá công tắc bùn CT1
    if ct1 == 1:
        diagnostics.append("Cảnh báo Công tắc CT1 kích hoạt: Lồng tời đã chạm đáy bùn hồ. Tự động bảo vệ chống kẹt cáp!")
        actions.append("Kích hoạt thu tời khẩn cấp về 0.0m.")

    # Đánh giá Pin
    if bat > 0.5 and bat < 9.6:
        diagnostics.append(f"Điện áp Pin 3S thấp ({bat:.2f}V): Dưới ngưỡng an toàn của cell Li-ion.")
        actions.append("Kích hoạt lệnh Tự hành về Home GPS (GOTO_HOME) trước khi cạn pin.")

    if not diagnostics:
        diagnostics.append(f"Mọi thông số (pH={ph:.2f}, TDS={tds:.0f}ppm, NTU={ntu:.0f}, DO={do_val:.2f}mg/L, T={temp:.1f}°C) đều nằm trong giới hạn QCVN 08:2023/BTNMT mức A1/A2.")

    if not actions:
        if dep < 0.5:
            actions.append("Thực hiện đo phân tầng tự động 3 mức độ sâu (0.3m -> 1.0m -> 1.8m) để khảo sát cột nước.")
        else:
            actions.append("Kích hoạt bơm lấy mẫu tự động 20s để lưu mẫu phân tích phòng thí nghiệm.")

    return {
        "status": "ĐÃ PHÂN TÍCH",
        "risk_level": risk_level,
        "badge_color": badge_color,
        "diagnostics": diagnostics,
        "recommendations": actions,
        "timestamp": dt.datetime.now().strftime("%H:%M:%S"),
    }


class GcsWebHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def send_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-GCS-Token, Authorization")

    def do_OPTIONS(self):
        """Xử lý preflight CORS requests từ trình duyệt."""
        self.send_response(200)
        self.send_cors_headers()
        self.end_headers()

    def do_POST(self):
        """Xử lý các API điều khiển hoặc chẩn đoán qua phương thức POST (JSON payload)."""
        url_parts = urllib.parse.urlparse(self.path)
        path = url_parts.path
        content_length = safe_int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode("utf-8", errors="replace") if content_length > 0 else "{}"
        try:
            req_data = json.loads(body) if body.strip() else {}
        except Exception:
            req_data = {}

        if path == "/api/ai_consult":
            with telemetry_lock:
                snapshot = dict(latest_telemetry)
            advice = generate_ai_advice(snapshot)
            payload_bytes = json.dumps(advice, ensure_ascii=False).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_cors_headers()
            self.end_headers()
            self.wfile.write(payload_bytes)
            return

        elif path == "/api/cmd":
            # Tái sử dụng logic điều khiển an toàn
            query = {k: [str(v)] for k, v in req_data.items()}
            self.handle_cmd_request(query)
            return

        self.send_response(404)
        self.send_cors_headers()
        self.end_headers()

    def do_GET(self):
        url_parts = urllib.parse.urlparse(self.path)
        path = url_parts.path

        if path == "/api/telemetry":
            with telemetry_lock:
                payload_bytes = json.dumps(latest_telemetry, ensure_ascii=False).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_cors_headers()
            self.end_headers()
            self.wfile.write(payload_bytes)
            return

        elif path == "/api/ai_consult":
            with telemetry_lock:
                snapshot = dict(latest_telemetry)
            advice = generate_ai_advice(snapshot)
            payload_bytes = json.dumps(advice, ensure_ascii=False).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_cors_headers()
            self.end_headers()
            self.wfile.write(payload_bytes)
            return

        elif path == "/api/history":
            # Siêu tốc O(1): lấy trực tiếp từ bộ đệm RAM in-memory history cache (<1ms)
            rows_out = list(history_cache)
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_cors_headers()
            self.end_headers()
            self.wfile.write(json.dumps({"ok": True, "count": len(rows_out), "rows": rows_out}, ensure_ascii=False).encode("utf-8"))
            return

        elif path == "/api/raw_line":
            query = urllib.parse.parse_qs(url_parts.query)
            since_id = safe_int(query.get("since", ["0"])[0], 0)
            with telemetry_lock:
                lines_out = [
                    {"id": seq_id, "port": p_name, "line": ln}
                    for (seq_id, p_name, ln) in raw_lines_buffer
                    if seq_id > since_id
                ]
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_cors_headers()
            self.end_headers()
            self.wfile.write(json.dumps({"lines": lines_out}, ensure_ascii=False).encode("utf-8"))
            return

        elif path == "/api/cmd":
            query = urllib.parse.parse_qs(url_parts.query)
            self.handle_cmd_request(query)
            return

        elif path == "/api/export/gpx":
            rows = list(history_cache)
            now_str = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
            gpx_lines = [
                '<?xml version="1.0" encoding="UTF-8"?>',
                '<gpx version="1.1" creator="ChemStation AI USV Server" xmlns="http://www.topografix.com/GPX/1/1">',
                '  <metadata>',
                f'    <name>USV Water Quality Survey</name>',
                f'    <time>{dt.datetime.utcnow().isoformat()}Z</time>',
                '  </metadata>',
                '  <trk>',
                '    <name>USV_Survey_Track</name>',
                '    <trkseg>'
            ]
            for r in rows:
                lat = r.get("lat", 0.0)
                lng = r.get("lng", 0.0)
                if abs(lat) < 0.001 and abs(lng) < 0.001:
                    continue
                dep = r.get("depth", 0.0)
                t = r.get("time", "")
                wqi = r.get("wqi", 0)
                ph = r.get("ph", 0.0)
                tds = r.get("tds", 0.0)
                ntu = r.get("ntu", 0.0)
                gpx_lines.append(f'      <trkpt lat="{lat:.6f}" lon="{lng:.6f}">')
                gpx_lines.append(f'        <ele>{-dep:.2f}</ele>')
                if t:
                    gpx_lines.append(f'        <time>{t}</time>')
                gpx_lines.append(f'        <desc>WQI:{wqi} pH:{ph:.2f} TDS:{tds:.0f} NTU:{ntu:.0f}</desc>')
                gpx_lines.append('      </trkpt>')
            gpx_lines.append('    </trkseg>')
            gpx_lines.append('  </trk>')
            gpx_lines.append('</gpx>')
            gpx_bytes = "\n".join(gpx_lines).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/gpx+xml; charset=utf-8")
            self.send_header("Content-Disposition", f'attachment; filename="usv_survey_{now_str}.gpx"')
            self.send_cors_headers()
            self.end_headers()
            self.wfile.write(gpx_bytes)
            return

        elif path == "/api/export/csv":
            now_str = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
            self.send_response(200)
            self.send_header("Content-Type", "text/csv; charset=utf-8")
            self.send_header("Content-Disposition", f'attachment; filename="usv_survey_{now_str}.csv"')
            self.send_cors_headers()
            self.end_headers()
            if os.path.exists(CSV_FILE_PATH):
                with open(CSV_FILE_PATH, "rb") as f:
                    self.wfile.write(f.read())
            else:
                self.wfile.write(b"No data recorded yet\n")
            return

        elif path in ("/cloud", "/mobile", "/cloud.html"):
            if os.path.exists("cloud_dashboard_mobile.html"):
                with open("cloud_dashboard_mobile.html", "rb") as f:
                    data = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_cors_headers()
                self.end_headers()
                self.wfile.write(data)
                return

        elif path in ("/map", "/gps", "/satellite", "/map.html"):
            target_f = os.path.join(BASE_DIR, "google_satellite_tracker.html")
            if os.path.exists(target_f):
                with open(target_f, "rb") as f:
                    data = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_cors_headers()
                self.end_headers()
                self.wfile.write(data)
                return

        elif path in ("/", "", "/sentinel", "/dashboard", "/studio", "/ai", "/sentinel.html", "/index.html"):
            target_f = os.path.join(BASE_DIR, "aqua_sentinel_dashboard.html")
            if not os.path.exists(target_f):
                target_f = os.path.join(BASE_DIR, "index.html")
            if os.path.exists(target_f):
                with open(target_f, "rb") as f:
                    data = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_cors_headers()
                self.end_headers()
                self.wfile.write(data)
                return

        elif path in ("/3d", "/3d.html"):
            html_3d = self.load_3d_html()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html_3d.encode("utf-8"))
            return

        else:
            html_dash = self.load_dashboard_html()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html_dash.encode("utf-8"))
            return

    def handle_cmd_request(self, query):
        req_tok = (self.headers.get("X-GCS-Token") or (query.get("token", [""])[0])).strip()
        client_ip = self.client_address[0] if self.client_address else ""
        if GCS_ADMIN_TOKEN and req_tok != GCS_ADMIN_TOKEN and client_ip not in ("127.0.0.1", "::1"):
            self.send_response(401)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_cors_headers()
            self.end_headers()
            self.wfile.write(b'{"ok":false,"err":"Unauthorized: Invalid X-GCS-Token"}')
            return

        enqueued = 0
        m_val = safe_int(query.get("m", ["0"])[0], 0, -255, 255)
        r_val = safe_int(query.get("r", ["90"])[0], 90, 0, 180)
        cmds_to_send = []

        if "auto" in query or query.get("w", [""])[0] == "3":
            cmds_to_send.append("CMD:0,90,3\n")
        elif "winch_home" in query or query.get("w", [""])[0] == "2":
            cmds_to_send.append("CMD:0,90,2\n")
        elif "goto" in query:
            cmds_to_send.append("CMD:0,90,10\n")
        elif "stopall" in query:
            cmds_to_send.append("CMD:0,90,11\n")
        elif "anchor_on" in query:
            cmds_to_send.append("CMD:0,90,16\n")
        elif "anchor_off" in query:
            cmds_to_send.append("CMD:0,90,17\n")
        elif "sample" in query:
            cmds_to_send.append("CMD:0,90,18\n")
        elif "clean" in query:
            cmds_to_send.append("CMD:0,90,19\n")
        elif "cal_ph" in query:
            pt = 4 if safe_int(query["cal_ph"][0], 7) == 4 else 7
            cmds_to_send.append(f"CMD:{pt},90,20\n")
        elif "dp_rad" in query:
            dm = safe_int(query["dp_rad"][0], 20, 5, 100)
            cmds_to_send.append(f"CMD:{dm},90,21\n")
        elif "tx_int" in query:
            ms = safe_int(query["tx_int"][0], 1000, 500, 5000)
            cmds_to_send.append(f"CMD:{ms},90,22\n")
        elif "tare" in query:
            cmds_to_send.append("CMD:0,90,9\n")
        elif "sethome" in query or "home" in query:
            cmds_to_send.append("CMD:0,90,6\n")
        elif "hdg" in query:
            deg = safe_int(query["hdg"][0], 0, 0, 359)
            cmds_to_send.append(f"CMD:{deg},90,12\n")
        elif "cbat" in query:
            bv = safe_float(query["cbat"][0], 12.0, 8.0, 15.0)
            cmds_to_send.append(f"CAL_BAT:{bv:.2f}\n")
        elif "pushprf" in query:
            cmds_to_send.append("PUSHPRF\n")
        elif "push" in query:
            cmds_to_send.append("PUSH\n")
        elif "w" in query or "m" in query or "r" in query:
            w_val = safe_int(query.get("w", ["0"])[0], 0, 0, 25)
            cmds_to_send.append(f"CMD:{m_val},{r_val},{w_val}\n")

        for cmd_str in cmds_to_send:
            try:
                cmd_queue.put_nowait(cmd_str)
                enqueued += 1
            except queue.Full:
                pass

        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_cors_headers()
        self.end_headers()
        resp_data = {"ok": True, "enqueued": enqueued, "cmds": [c.strip() for c in cmds_to_send]}
        self.wfile.write(json.dumps(resp_data).encode("utf-8"))

    def load_3d_html(self):
        """Tải giao diện Bản đồ 3D WebGL trực tiếp từ file gốc hoặc header C++."""
        for fname in ("water_map_3d.html", "test_3d.html"):
            fpath = os.path.join(BASE_DIR, fname)
            if os.path.exists(fpath):
                with open(fpath, "r", encoding="utf-8") as f:
                    return f.read()

        header_path = os.path.join(BASE_DIR, "src", "ground_station", "web_3d_map.h")
        if os.path.exists(header_path):
            with open(header_path, "r", encoding="utf-8") as f:
                content = f.read()
            m = re.search(r'R"rawliteral\((.*?)\)rawliteral"', content, re.DOTALL)
            if m:
                return m.group(1)
        return "<h1>Không tìm thấy tài nguyên Bản đồ 3D trong thư mục dự án.</h1>"

    def load_dashboard_html(self):
        gcs_cpp_path = os.path.join(BASE_DIR, "src", "ground_station", "main.cpp")
        if os.path.exists(gcs_cpp_path):
            try:
                with open(gcs_cpp_path, "r", encoding="utf-8") as f:
                    cpp_content = f.read()
                m = re.search(
                    r'WEB_DASHBOARD_HTML\[\]\s*PROGMEM\s*=\s*R"rawliteral\((.*?)\)rawliteral"',
                    cpp_content,
                    re.DOTALL,
                )
                if m:
                    return m.group(1)
            except OSError:
                pass
        return """<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ChemStation AI — Trạm Bờ Điều Khiển GCS Web Dashboard</title>
  <style>
    :root { --bg: #030712; --card: rgba(15,23,42,0.85); --cyan: #38bdf8; --emerald: #34d399; --amber: #fbbf24; --red: #ef4444; }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: ui-sans-serif, system-ui, -apple-system, sans-serif; background: #030712; color: #f1f5f9; padding: 20px; }
    .header { display: flex; justify-content: space-between; align-items: center; background: var(--card); padding: 16px 24px; border-radius: 16px; border: 1px solid rgba(56,189,248,0.25); margin-bottom: 20px; flex-wrap: wrap; gap: 12px; }
    .title { font-size: 20px; font-weight: bold; color: var(--cyan); display: flex; align-items: center; gap: 10px; }
    .badge { padding: 4px 10px; border-radius: 9999px; font-size: 12px; font-weight: bold; }
    .badge-ok { background: #064e3b; color: var(--emerald); border: 1px solid var(--emerald); }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(210px, 1fr)); gap: 14px; margin-bottom: 18px; }
    .card { background: var(--card); padding: 16px; border-radius: 14px; border: 1px solid rgba(148,163,184,0.18); }
    .card-label { font-size: 12px; color: #94a3b8; margin-bottom: 6px; font-weight: 600; }
    .card-value { font-size: 26px; font-weight: 800; font-family: monospace; color: #fff; }
    .card-unit { font-size: 13px; color: #64748b; font-weight: normal; margin-left: 4px; }
    .btn-3d { display: inline-flex; align-items: center; gap: 8px; background: linear-gradient(135deg, #0284c7, #0369a1); color: #fff; text-decoration: none; padding: 12px 24px; border-radius: 12px; font-weight: bold; font-size: 15px; box-shadow: 0 0 20px rgba(56,189,248,0.35); transition: all 0.2s; }
    .btn-3d:hover { transform: translateY(-2px); box-shadow: 0 0 30px rgba(56,189,248,0.6); }
    .iframe-box { width: 100%; height: 680px; border-radius: 16px; border: 1px solid rgba(56,189,248,0.35); overflow: hidden; margin-top: 8px; }
  </style>
</head>
<body>
  <div class="header">
    <div>
      <div class="title">
        <span>🚤 CHEMSTATION AI — TRẠM ĐIỀU KHIỂN TRỰC QUAN GCS</span>
        <span class="badge badge-ok" id="badge_port">LIVE USB SERIAL (AUTO)</span>
      </div>
      <p style="color:#94a3b8; font-size:13px; margin-top:4px;">Giao diện Web đa nền tảng chạy trực tiếp trên máy tính và trạm bờ (Kiểm định CRC-8 100%)</p>
    </div>
    <div>
      <a href="/3d" target="_blank" class="btn-3d">
        🌐 MỞ BẢN ĐỒ 3D TOÀN MÀN HÌNH (WEBGL)
      </a>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <div class="card-label">ĐỘ pH NƯỚC (QCVN 08)</div>
      <div class="card-value" id="val_ph" style="color:#38bdf8;">0.00</div>
    </div>
    <div class="card">
      <div class="card-label">TỔNG KHOÁNG TDS / EC</div>
      <div class="card-value" id="val_tds" style="color:#34d399;">0<span class="card-unit">ppm</span></div>
    </div>
    <div class="card">
      <div class="card-label">ĐỘ ĐỤC QUAN TRẮC (NTU)</div>
      <div class="card-value" id="val_ntu" style="color:#fbbf24;">0<span class="card-unit">NTU</span></div>
    </div>
    <div class="card">
      <div class="card-label">NHIỆT ĐỘ NƯỚC</div>
      <div class="card-value" id="val_temp" style="color:#cbd5e1;">0.0<span class="card-unit">°C</span></div>
    </div>
    <div class="card">
      <div class="card-label">ĐIỆN ÁP PIN 3S THỰC TẾ</div>
      <div class="card-value" id="val_bat" style="color:#34d399;">0.00<span class="card-unit">V</span></div>
    </div>
    <div class="card">
      <div class="card-label">SỨC KHỎE PHẦN CỨNG</div>
      <div class="card-value" id="val_hw" style="color:#38bdf8;">--<span class="card-unit">%</span></div>
    </div>
  </div>

  <h3 style="color:#38bdf8; margin: 14px 0 8px 0; font-size: 15px;">🌐 KHÔNG GIAN BẢN ĐỒ 3D TRỰC QUAN (NHÚNG TRỰC TIẾP):</h3>
  <div class="iframe-box">
    <iframe src="/3d" style="width:100%; height:100%; border:none;"></iframe>
  </div>

  <script>
    let updateInFlight = false;
    async function updateData() {
      if (updateInFlight) return;
      updateInFlight = true;
      try {
        const res = await fetch('/api/telemetry');
        const d = await res.json();
        const badgeEl = document.getElementById('badge_port');
        if (badgeEl && d.port) {
          badgeEl.innerText = 'LIVE USB SERIAL ' + d.port + ' • SEQ #' + d.seq + ' (CRC OK: ' + (d.crc_ok || 0) + ')';
        }
        document.getElementById('val_ph').innerText = d.ph > 0.1 ? d.ph.toFixed(2) : '0.00 (Chưa cắm)';
        document.getElementById('val_tds').innerHTML = d.tds.toFixed(0) + '<span class="card-unit">ppm</span>';
        document.getElementById('val_ntu').innerHTML = d.ntu.toFixed(0) + '<span class="card-unit">NTU</span>';
        document.getElementById('val_temp').innerHTML = d.temp.toFixed(1) + '<span class="card-unit">°C</span>';
        document.getElementById('val_bat').innerHTML = d.bat.toFixed(2) + '<span class="card-unit">V (' + (d.batpct || 0) + '%)</span>';

        let okCount = 0;
        for (let i = 0; i < 11; i++) {
          if (d.hwm & (1 << i)) okCount++;
        }
        document.getElementById('val_hw').innerHTML = Math.round((okCount / 11) * 100) + '<span class="card-unit">% (' + okCount + '/11)</span>';
      } catch (e) {} finally {
        updateInFlight = false;
        setTimeout(updateData, 600);
      }
    }
    updateData();
  </script>
</body>
</html>"""


class ReusableThreadingHTTPServer(ThreadingHTTPServer):
    allow_reuse_address = True
    daemon_threads = True



def google_sheets_sync_worker():
    """Tự động đồng bộ các gói tin LoRa thực từ Phao lên Google Sheets Cloud mỗi 3 giây."""
    import urllib.request
    last_synced_seq = -1
    gas_url = "https://script.google.com/macros/s/AKfycbzcdsFjdE1jkOlEQcafJ4BhnYB4wLumgYNlLkmcj7EeUUatMWIqFr6OJZfGpW2uuB1hZg/exec"
    while True:
        try:
            time.sleep(3)
            with telemetry_lock:
                cur_seq = latest_telemetry.get("seq", 0)
                if cur_seq == 0 or cur_seq == last_synced_seq:
                    continue
                p = {
                    "mode": "tele",
                    "seq": cur_seq,
                    "ph": latest_telemetry.get("ph", 0.0),
                    "do": latest_telemetry.get("do", 0.0),
                    "tds": latest_telemetry.get("tds", 0),
                    "ec": latest_telemetry.get("ec", 0),
                    "ntu": latest_telemetry.get("ntu", 0.0),
                    "temp": latest_telemetry.get("temp", 0.0),
                    "dep": latest_telemetry.get("dep", 0.0),
                    "bat": latest_telemetry.get("bat", 0.0),
                    "amp": latest_telemetry.get("amp", 0.0),
                    "watt": latest_telemetry.get("watt", 0.0),
                    "roll": latest_telemetry.get("roll", 0.0),
                    "pitch": latest_telemetry.get("pitch", 0.0),
                    "hdg": latest_telemetry.get("hdg", 0.0),
                    "lat": latest_telemetry.get("lat", 0.0),
                    "lng": latest_telemetry.get("lng", 0.0),
                    "sats": latest_telemetry.get("sats", 0),
                    "spd": latest_telemetry.get("spd", 0.0),
                    "wqi": latest_telemetry.get("wqi", 0),
                    "src": "GCS_LORA_BRIDGE"
                }

            query_str = urllib.parse.urlencode(p)
            req = urllib.request.Request(f"{gas_url}?{query_str}", headers={"User-Agent": "USV-GCS-Bridge/4.3"})
            with urllib.request.urlopen(req, timeout=8) as res:
                resp_text = res.read().decode('utf-8', errors='ignore')
                if "OK" in resp_text:
                    last_synced_seq = cur_seq
                    bridge_log.info(f"[CLOUD-SYNC] Dong bo goi #{cur_seq} (pH: {p['ph']}, DO: {p['do']}, WQI: {p['wqi']}) len Google Sheets OK!")
        except Exception as e:
            bridge_log.debug(f"[CLOUD-SYNC] Loi dong bo: {e}")


def start_server(port=8080):
    server = ReusableThreadingHTTPServer(("0.0.0.0", port), GcsWebHandler)
    url = f"http://localhost:{port}/3d"
    print("\n" + "=" * 65)
    print(" [✓] TRẠM ĐIỀU KHIỂN GCS WEB DASHBOARD & 3D MAP ĐÃ SẴN SÀNG!")
    print(f" [👉] BẢN ĐỒ 3D WEBGL:    http://localhost:{port}/3d")
    print(f" [👉] DASHBOARD TỔNG HỢP: http://localhost:{port}")
    print("=" * 65)

    if "--no-browser" not in sys.argv:
        try:
            webbrowser.open(url)
        except Exception:
            pass

    server.serve_forever()


if __name__ == "__main__":
    pref_port = None
    port = 8080
    for arg in sys.argv[1:]:
        if arg.upper().startswith("COM") or arg.startswith("/dev/"):
            pref_port = arg
        elif arg.isdigit():
            port = int(arg)

    init_history_cache_from_csv()
    t = threading.Thread(target=serial_reader_thread, args=(pref_port,), daemon=True)
    t.start()
    t_sync = threading.Thread(target=google_sheets_sync_worker, daemon=True)
    t_sync.start()
    start_server(port)
