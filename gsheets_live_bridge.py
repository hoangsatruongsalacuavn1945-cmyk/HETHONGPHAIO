#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
============================================================================
CHEMSTATION AI v4.2 - CẦU NỐI SERIAL -> GOOGLE SHEETS (LIVE BRIDGE)
============================================================================
Đọc gói telemetry "USV:..." (và bảng 3 tầng "PRF:...") do Trạm Bờ in ra
cổng USB Serial, kiểm tra CRC-8, rồi đẩy lên Google Apps Script Web App.

Cài đặt:
    pip install pyserial

Ví dụ:
    python gsheets_live_bridge.py --url "https://script.google.com/macros/s/XXXX/exec"
    python gsheets_live_bridge.py --port COM11 --url "..." --interval 5
    python gsheets_live_bridge.py --list-ports
    python gsheets_live_bridge.py --url "..." --once --verbose

Ghi chú:
  - Không dùng shell=True, không hard-code cổng COM.
  - Tự dò cổng ESP32 (CP210x / CH340 / CH9102 / USB-Serial).
  - Khi mất kết nối: tự mở lại cổng với backoff.
  - Khi upload lỗi: ghi CSV dự phòng (--csv) để không mất dữ liệu.
============================================================================
"""

import argparse
import collections
import json
import csv
import datetime as dt
import json
import logging
from logging.handlers import RotatingFileHandler
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover
    print("Thieu thu vien pyserial. Cai dat: pip install pyserial", file=sys.stderr)
    sys.exit(2)

try:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
except Exception:
    pass

FW_VERSION = "4.3-AUDIT-PRO"
CRC_SALT = 0xA7
DEFAULT_SPREADSHEET_ID = "1IVcoOZhhrgMh_IOLvDm1-ssyUtHGmMhFSyZD3fYICDY"

# Thứ tự 30 trường trong gói "USV:" (xem include/usv_common.h)
FIELDS = [
    "ph", "tds", "ntu", "temp", "amp", "bat", "roll", "ct1", "ct2", "pitch",
    "hdg", "lat", "lng", "stage", "alerts", "seq", "do", "hw", "wqi", "mpuT",
    "ec", "depth", "adcFault", "s3ChipT", "sats", "spd", "alt", "homed",
    "flags2", "tgtHdg",
]

# Bitmask 11 linh kiện
HW_NAMES = [
    "DS18B20", "pH", "TDS", "NTU", "ACS712", "Pin3S",
    "MPU6050", "GY271", "LoRa", "SIM4G", "GPS",
]

FLAG2_TEXT = [
    (0x0001, "VE_HOME"), (0x0002, "GPS_FIX"), (0x0004, "HOME_SAVED"),
    (0x0008, "SIM_ONLINE"), (0x0010, "AUTO_RUNNING"), (0x0020, "MUD_LATCHED"),
    (0x0040, "LOW_BAT_CRIT"), (0x0080, "CALIBRATED"),
    (0x0100, "VIRTUAL_ANCHOR"), (0x0200, "HEADING_HOLD"),
]

log = logging.getLogger("bridge")


# ===========================================================================
# TIỆN ÍCH
# ===========================================================================
def calc_keyed_crc8(data: str) -> int:
    crc = CRC_SALT
    for ch in data.encode("utf-8", "ignore"):
        crc ^= ch
        for _ in range(8):
            crc = ((crc << 1) ^ 0x31) & 0xFF if (crc & 0x80) else (crc << 1) & 0xFF
    return crc


def to_float(value, default=None):
    try:
        s = str(value).strip()
        if s == "":
            return default
        return float(s)
    except (TypeError, ValueError):
        return default


def to_int(value, default=None):
    try:
        s = str(value).strip()
        if s == "":
            return default
        return int(float(s))
    except (TypeError, ValueError):
        return default


def fmt(value, digits, unit=""):
    return "--" if value is None else f"{value:.{digits}f}{unit}"


# Đường cong xả Li-ion theo từng cell — khớp calcBattery3SPercent() trong usv_common.h
BAT_CURVE_V = [3.20, 3.45, 3.60, 3.70, 3.78, 3.88, 3.98, 4.08, 4.18]
BAT_CURVE_P = [0, 10, 22, 38, 52, 67, 80, 91, 100]


def battery_cell_count(volt):
    """Tự nhận diện 1S/2S/3S giống firmware (0 = chưa cắm pin)."""
    if volt is None or volt <= 0:
        return 0
    if volt <= 4.45:
        return 1
    if volt <= 8.75:
        return 2
    return 3


def battery_percent(volt):
    """% pin theo từng cell. Chưa cắm pin (<1.0V) -> 0%, không báo ảo."""
    if volt is None or volt < 1.0:
        return 0
    cells = battery_cell_count(volt)
    if cells <= 0:
        return 0
    v = volt / cells
    if v <= BAT_CURVE_V[0]:
        return 0
    if v >= BAT_CURVE_V[-1]:
        return 100
    for i in range(1, len(BAT_CURVE_V)):
        if v <= BAT_CURVE_V[i]:
            k = (v - BAT_CURVE_V[i - 1]) / (BAT_CURVE_V[i] - BAT_CURVE_V[i - 1])
            return int(round(BAT_CURVE_P[i - 1] + k * (BAT_CURVE_P[i] - BAT_CURVE_P[i - 1])))
    return 100


def hw_summary(mask):
    if mask is None:
        return "HW: ?", 0, []
    ok = [HW_NAMES[i] for i in range(len(HW_NAMES)) if mask & (1 << i)]
    missing = [HW_NAMES[i] for i in range(len(HW_NAMES)) if not (mask & (1 << i))]
    return f"HW: {len(ok)}/11", len(ok), missing


def flags2_summary(flags):
    if not flags:
        return ""
    return ",".join(name for bit, name in FLAG2_TEXT if flags & bit)


# ===========================================================================
# DÒ CỔNG SERIAL
# ===========================================================================
PORT_HINTS = (
    "cp210", "ch340", "ch341", "ch343", "ch910", "usb serial", "usb-serial",
    "silicon labs", "espressif", "wch", "ftdi", "uart", "enhanced",
)


def list_serial_ports():
    return list(list_ports.comports())


def print_ports():
    ports = list_serial_ports()
    if not ports:
        print("Không tìm thấy cổng COM nào. Kiểm tra cáp USB / driver CP210x-CH340.")
        return
    print(f"Tìm thấy {len(ports)} cổng:")
    for p in ports:
        print(f"  {p.device:10s} | {p.description}")


def detect_ports(preferred=None):
    """Trả về danh sách cổng theo thứ tự ưu tiên (ESP32/USB-Serial trước)."""
    if preferred:
        return [preferred]

    ports = list_serial_ports()
    if not ports:
        return []

    def rank(p):
        desc = f"{p.description or ''} {p.manufacturer or ''}".lower()
        is_usb = any(k in desc for k in PORT_HINTS)
        return (0 if is_usb else 1, p.device)

    return [p.device for p in sorted(ports, key=rank)]


# ===========================================================================
# ĐỌC & PHÂN TÍCH GÓI TIN
# ===========================================================================
USV_RE = re.compile(r"USV:([^*\r\n]+)(?:\*([0-9A-Fa-f]{2}))?")
PRF_RE = re.compile(r"PRF:([^*\r\n]+)(?:\*([0-9A-Fa-f]{2}))?")
RSSI_RE = re.compile(r"RSSI:\s*(-?\d+)")
SNR_RE = re.compile(r"SNR:\s*(-?\d+(?:\.\d+)?)")


def verify_crc(payload, crc_hex, prefix="USV:"):
    if not crc_hex:
        return False  # S-02: Bắt buộc phải có mã kiểm tra CRC-8 hợp lệ, từ chối gói thiếu *CRC
    try:
        expected = int(crc_hex, 16)
    except ValueError:
        return False
    # Firmware C++ ESP32 tính CRC trên toàn bộ chuỗi bao gồm cả prefix ("USV:" hoặc "PRF:")
    full_data = payload if payload.startswith(prefix) else (prefix + payload)
    if calc_keyed_crc8(full_data) == expected:
        return True
    if calc_keyed_crc8(payload) == expected:
        return True
    return False


def parse_usv(payload):
    parts = payload.split(",")
    if len(parts) < 24:
        return None

    data = {}
    for i, name in enumerate(FIELDS):
        raw = parts[i] if i < len(parts) else ""
        if name in ("lat", "lng"):
            data[name] = to_float(raw, 0.0)
        elif name in ("ct1", "ct2", "stage", "alerts", "seq", "hw", "wqi",
                      "adcFault", "sats", "flags2"):
            data[name] = to_int(raw, 0)
        else:
            data[name] = to_float(raw, 0.0)

    data["field_count"] = len(parts)
    return data


def parse_prf(payload):
    layers = payload.split(";")
    if len(layers) < 3:
        return None
    out = {}
    keys = ["ph", "tds", "ntu", "temp"]
    for idx, layer in enumerate(layers[:3], start=1):
        vals = layer.split(",")
        if len(vals) < 4:
            return None
        for k, v in zip(keys, vals):
            out[f"{k}{idx}"] = to_float(v, 0.0)
    return out


# ===========================================================================
# ĐÁNH GIÁ CHẤT LƯỢNG NƯỚC
# ===========================================================================
def evaluate_water(d):
    ph, tds, ntu, temp, do_val = d.get("ph"), d.get("tds"), d.get("ntu"), d.get("temp"), d.get("do")
    bad = warn = has_any = False

    # 0 là sentinel "chưa cắm cảm biến" -> bỏ qua, không đánh giá ảo
    if ph is not None and ph > 0.1:
        has_any = True
        if ph < 5.5 or ph > 9.0:
            bad = True
        elif ph < 6.0 or ph > 8.5:
            warn = True
    if tds is not None and tds > 0:
        has_any = True
        if tds > 1500:
            bad = True
        elif tds > 1000:
            warn = True
    if ntu is not None and ntu > 0:
        has_any = True
        if ntu > 300:
            bad = True
        elif ntu > 100:
            warn = True
    if temp is not None and temp > 0:
        has_any = True
        if temp > 40:
            bad = True
        elif temp > 36:
            warn = True
    if do_val is not None and do_val > 0.1:
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


# ===========================================================================
# TẠO QUERY / DÒNG CSV
# ===========================================================================
def build_tele_query(d, src):
    hw_text, hw_cnt, missing = hw_summary(d.get("hw"))
    qcvn = evaluate_water(d)
    alarm = 1 if (d.get("alerts") or 0) else 0

    watt = None
    if d.get("bat") is not None and d.get("amp") is not None:
        watt = round(d["bat"] * d["amp"], 2)

    # Chỉ tính % khi thực sự có bitmask; không có thì để trống (không báo 0% ảo).
    hw_pct = (hw_cnt * 100 // len(HW_NAMES)) if d.get("hw") is not None else None

    params = {
        "mode": "tele",
        "seq": d.get("seq"),
        "ph": d.get("ph"),
        "do": d.get("do"),
        "tds": d.get("tds"),
        "ec": d.get("ec"),
        "ntu": d.get("ntu"),
        "temp": d.get("temp"),
        "dep": d.get("depth"),
        "watt": watt,
        "amp": d.get("amp"),
        "bat": d.get("bat"),
        "roll": d.get("roll"),
        "pitch": d.get("pitch"),
        "hdg": d.get("hdg"),
        "lat": d.get("lat"),
        "lng": d.get("lng"),
        "sats": d.get("sats"),
        "spd": d.get("spd"),
        "alt": d.get("alt"),
        "homed": d.get("homed"),
        "tgthdg": d.get("tgtHdg"),
        "flags2": d.get("flags2"),
        "rssi": d.get("rssi", ""),
        "snr": d.get("snr", ""),
        "hw": hw_pct,
        "hwm": d.get("hw"),      # bitmask thô 11 linh kiện -> Discord giải mã đúng
        "alr": d.get("alerts"),  # bitmask cảnh báo -> Discord giải mã đúng
        "wqi": d.get("wqi"),
        "alarm": alarm,
        "src": src,
        "fw": FW_VERSION,
    }
    clean = {k: v for k, v in params.items() if v is not None}
    return urllib.parse.urlencode(clean, doseq=False), hw_text, missing, qcvn, watt


def build_profile_query(p, src):
    params = {"mode": "profile", "src": src, "fw": FW_VERSION}
    for k in ("ph1", "tds1", "ntu1", "t1", "ph2", "tds2", "ntu2", "t2",
              "ph3", "tds3", "ntu3", "t3"):
        if p.get(k) is not None:
            params[k] = p[k]
    return urllib.parse.urlencode(params)


def build_csv_row(d, src):
    hw_text, hw_cnt, missing = hw_summary(d.get("hw"))
    now_str = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    return [
        now_str,
        d.get("seq", ""),
        fmt(d.get("ph"), 2),
        fmt(d.get("do"), 2),
        fmt(d.get("tds"), 0),
        fmt(d.get("ec"), 0),
        fmt(d.get("ntu"), 0),
        fmt(d.get("temp"), 1),
        fmt(d.get("depth"), 2),
        fmt(d.get("amp"), 2),
        fmt(d.get("bat"), 2),
        fmt(d.get("roll"), 1),
        fmt(d.get("pitch"), 1),
        fmt(d.get("hdg"), 0),
        fmt(d.get("lat"), 5),
        fmt(d.get("lng"), 5),
        d.get("sats", ""),
        fmt(d.get("spd"), 1),
        fmt(d.get("alt"), 1),
        fmt(d.get("homed"), 1),
        fmt(d.get("tgtHdg"), 0),
        flags2_summary(d.get("flags2")),
        hw_text,
        evaluate_water(d),
        src,
        FW_VERSION,
    ]


CSV_HEADER = [
    "ThoiGian", "Seq", "pH", "DO_mgL", "TDS_ppm", "EC_uScm", "NTU",
    "NhietNuoc_C", "DoSau_m", "Dong_A", "Pin_V", "Roll", "Pitch", "Heading",
    "Lat", "Lng", "Sats", "Speed_kmh", "Alt_m", "Home_m", "TargetHdg",
    "Flags2", "HW", "QCVN", "Nguon", "FW",
]


def build_sheet_row(d, src):
    """Dòng 30 cột khớp đúng thứ tự TELE_HEADERS của Apps Script (dùng cho gsheets CLI)."""
    _, hw_cnt, _ = hw_summary(d.get("hw"))
    hw_pct = (hw_cnt * 100 // len(HW_NAMES)) if d.get("hw") is not None else ""
    watt = None
    if d.get("bat") is not None and d.get("amp") is not None:
        watt = round(d["bat"] * d["amp"], 2)
    alarm = "CÓ CẢNH BÁO" if d.get("alerts") else "BÌNH THƯỜNG"
    return [
        dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        d.get("seq", ""),
        d.get("ph", ""), d.get("do", ""), d.get("tds", ""), d.get("ec", ""),
        d.get("ntu", ""), d.get("temp", ""), d.get("depth", ""),
        watt if watt is not None else "",
        d.get("amp", ""), d.get("bat", ""), d.get("roll", ""), d.get("pitch", ""),
        d.get("hdg", ""), d.get("lat", ""), d.get("lng", ""), d.get("sats", ""),
        d.get("spd", ""), d.get("alt", ""), d.get("homed", ""), d.get("tgtHdg", ""),
        d.get("flags2", ""), d.get("rssi", ""), hw_pct,
        d.get("wqi", ""), alarm, evaluate_water(d), src, FW_VERSION,
    ]


# ===========================================================================
# UPLOAD
# ===========================================================================
class Uploader:
    def __init__(self, url, spreadsheet_id, csv_path, retries=3, timeout=15):
        self.url = url
        self.spreadsheet_id = spreadsheet_id
        self.csv_path = csv_path
        self.retries = retries
        self.timeout = timeout
        self.ok_count = 0
        self.fail_count = 0
        self._csv_ready = False
        self.retry_queue = collections.deque(maxlen=500)
        base_dir = os.path.dirname(os.path.abspath(csv_path)) if csv_path else os.getcwd()
        self.retry_queue_path = os.path.join(base_dir, "gsheets_retry_queue.json")
        self._load_retry_queue()

    def _load_retry_queue(self):
        """B-04: Khôi phục hàng đợi Retry từ đĩa cứng nếu phiên trước bị tắt đột ngột."""
        if not os.path.exists(self.retry_queue_path):
            return
        try:
            with open(self.retry_queue_path, "r", encoding="utf-8") as f:
                items = json.load(f)
            if isinstance(items, list):
                for item in items[-500:]:
                    if isinstance(item, dict):
                        self.retry_queue.append(item)
                if self.retry_queue:
                    log.info("Đã khôi phục %d gói tin chờ gửi từ %s", len(self.retry_queue), self.retry_queue_path)
        except Exception as exc:
            log.warning("Không đọc được file hàng đợi Retry (%s): %s", self.retry_queue_path, exc)

    def _save_retry_queue(self):
        """B-04: Lưu bền vững hàng đợi Retry xuống đĩa cứng (gsheets_retry_queue.json)."""
        try:
            with open(self.retry_queue_path, "w", encoding="utf-8") as f:
                json.dump(list(self.retry_queue), f, ensure_ascii=False)
        except Exception as exc:
            log.warning("Không ghi được file hàng đợi Retry (%s): %s", self.retry_queue_path, exc)

    def post_json(self, payload_dict):
        """Gửi JSON POST trực tiếp tới Apps Script Web App. Hỗ trợ batch và retry queue."""
        if not self.url or "YOUR_SCRIPT_ID" in self.url:
            return False
        last_err = None
        for attempt in range(1, self.retries + 1):
            try:
                data_bytes = json.dumps(payload_dict, default=str).encode("utf-8")
                req = urllib.request.Request(
                    self.url,
                    data=data_bytes,
                    headers={
                        "Content-Type": "application/json",
                        "User-Agent": f"ChemStationBridge/{FW_VERSION}",
                    },
                    method="POST"
                )
                with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                    body = resp.read(400).decode("utf-8", "ignore")
                    if resp.status == 200 and not body.startswith("ERR"):
                        self.ok_count += 1
                        self.flush_retry_queue()
                        return True
                    last_err = f"HTTP {resp.status}: {body[:120]}"
            except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, OSError) as exc:
                last_err = str(exc)
            if attempt < self.retries:
                time.sleep(min(2 ** attempt, 4))
        self.fail_count += 1
        log.warning("Lỗi gửi HTTP tới Web App: %s. Đã lưu vào hàng đợi Retry (%d gói).", last_err, len(self.retry_queue) + 1)
        if len(self.retry_queue) < 500:
            self.retry_queue.append(payload_dict)
            self._save_retry_queue()
        return False

    def flush_retry_queue(self):
        """Xả các gói tin cũ trong hàng đợi Retry khi mạng phục hồi."""
        if not self.retry_queue or not self.url:
            return
        batch = []
        while self.retry_queue and len(batch) < 30:
            batch.append(self.retry_queue.popleft())
        try:
            payload = {"mode": "batch", "batch": batch}
            data_bytes = json.dumps(payload, default=str).encode("utf-8")
            req = urllib.request.Request(
                self.url,
                data=data_bytes,
                headers={
                    "Content-Type": "application/json",
                    "User-Agent": f"ChemStationBridge/{FW_VERSION}",
                },
                method="POST"
            )
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                body = resp.read(200).decode("utf-8", "ignore")
                if resp.status == 200 and "OK_BATCH" in body:
                    log.info("Đã xả thành công %d gói tin trong hàng đợi Retry lên Google Sheets!", len(batch))
                    self._save_retry_queue()
                else:
                    for item in reversed(batch):
                        self.retry_queue.appendleft(item)
                    self._save_retry_queue()
        except Exception:
            for item in reversed(batch):
                self.retry_queue.appendleft(item)
            self._save_retry_queue()

    def send(self, query):
        """Gửi GET tới Apps Script (tương thích ngược)."""
        if not self.url or "YOUR_SCRIPT_ID" in self.url:
            return False
        full_url = self.url + ("&" if "?" in self.url else "?") + query
        last_err = None
        for attempt in range(1, self.retries + 1):
            try:
                req = urllib.request.Request(
                    full_url,
                    headers={"User-Agent": f"ChemStationBridge/{FW_VERSION}"},
                )
                with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                    body = resp.read(400).decode("utf-8", "ignore")
                    if resp.status == 200 and not body.startswith("ERR"):
                        self.ok_count += 1
                        return True
                    last_err = f"HTTP {resp.status}: {body[:120]}"
            except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, OSError) as exc:
                last_err = str(exc)
            if attempt < self.retries:
                time.sleep(min(2 ** attempt, 8))
        self.fail_count += 1
        return False

    def _rotate_csv_if_needed(self, max_bytes=5 * 1024 * 1024, backups=3):
        try:
            if not self.csv_path or not os.path.exists(self.csv_path) or os.path.getsize(self.csv_path) < max_bytes:
                return
            for idx in range(backups, 0, -1):
                src = f"{self.csv_path}.{idx}" if idx > 1 else self.csv_path
                dst = f"{self.csv_path}.{idx + 1}" if idx < backups else f"{self.csv_path}.{backups}"
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
            log.info("Đã xoay vòng file CSV thành công: %s", self.csv_path)
        except Exception as exc:
            log.debug("Lỗi xoay vòng CSV: %s", exc)

    def append_csv(self, row):
        if not self.csv_path:
            return
        self._rotate_csv_if_needed()
        for attempt in range(3):
            try:
                need_header = (not os.path.exists(self.csv_path)) or os.path.getsize(self.csv_path) == 0
                with open(self.csv_path, "a", newline="", encoding="utf-8-sig") as fh:
                    writer = csv.writer(fh)
                    if need_header:
                        writer.writerow(CSV_HEADER)
                    writer.writerow(row)
                break
            except OSError as exc:
                if attempt == 2:
                    log.error("Không ghi được CSV %s: %s", self.csv_path, exc)
                time.sleep(0.05)


# ===========================================================================
# SERIAL LINK
# ===========================================================================
class SerialLink:
    def __init__(self, ports, baud, read_timeout=1.0, explicit_port=False):
        self.ports = ports
        self.explicit_port = explicit_port
        self.baud = baud
        self.read_timeout = read_timeout
        self.ser = None
        self.port = None
        self.backoff = 1.0

    def open(self):
        # Ưu tiên kiểm tra xem gcs_web_server.py đã mở cổng GCS chưa (tránh mở nhầm cổng Phao COM9)
        try:
            with urllib.request.urlopen("http://127.0.0.1:8080/api/raw_line?since=0", timeout=0.35) as resp:
                if resp.status == 200:
                    return False
        except Exception:
            pass
        candidate_ports = self.ports if self.explicit_port else detect_ports()
        for port in candidate_ports:
            try:
                ser = serial.Serial()
                ser.port = port
                ser.baudrate = self.baud
                ser.timeout = self.read_timeout
                ser.write_timeout = 1
                ser.dtr = False
                ser.rts = False
                ser.open()
                self.ser = ser
                self.port = port
                self.backoff = 1.0
                log.info("Đã mở cổng %s @ %d baud", port, self.baud)
                return True
            except (serial.SerialException, OSError) as exc:
                log.debug("Không mở được %s: %s", port, exc)
                if "PermissionError" in str(exc) or "Access is denied" in str(exc):
                    break
        return False

    def close(self):
        if self.ser is not None:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None
        self.port = None

    def ensure_open(self):
        if self.ser is not None and self.ser.is_open:
            return True
        self.close()
        while not self.open():
            log.warning("Không tìm thấy cổng Serial. Thử lại sau %.0fs...", self.backoff)
            time.sleep(self.backoff)
            self.backoff = min(self.backoff * 2, 30.0)
        return True

    def readline(self):
        if self.ser is None or not self.ser.is_open:
            if not self.open():
                # Nếu cổng COM đang được giữ bởi gcs_web_server.py, đọc chia sẻ qua HTTP /api/raw_line
                try:
                    since = getattr(self, "_http_since", 0)
                    url = f"http://127.0.0.1:8080/api/raw_line?since={since}"
                    with urllib.request.urlopen(url, timeout=0.6) as resp:
                        data = json.loads(resp.read().decode("utf-8", "ignore"))
                        lines = data.get("lines", [])
                        if lines:
                            self._http_since = lines[-1]["id"]
                            self.port = lines[-1].get("port", "SHARED-HTTP")
                            if not hasattr(self, "_http_queue"):
                                self._http_queue = collections.deque()
                            for item in lines:
                                self._http_queue.append(item["line"])
                    if hasattr(self, "_http_queue") and self._http_queue:
                        return self._http_queue.popleft()
                    time.sleep(0.25)
                    return ""
                except Exception:
                    log.warning("Không tìm thấy cổng Serial (hoặc đang bận). Thử lại sau %.0fs...", self.backoff)
                    time.sleep(self.backoff)
                    self.backoff = min(self.backoff * 2, 10.0)
                    return ""
        try:
            raw = self.ser.readline()
            if not raw:
                return ""
            return raw.decode("utf-8", "ignore").strip()
        except (serial.SerialException, OSError) as exc:
            log.warning("Lỗi đọc Serial (%s): %s", self.port, exc)
            self.close()
            return ""


# ===========================================================================
# CHƯƠNG TRÌNH CHÍNH
# ===========================================================================

DEFAULT_DISCORD_WEBHOOK = os.environ.get("DISCORD_WEBHOOK_URL", "")

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
    hdg = data.get("hdg", 0.0)
    depth = data.get("depth", 0.0)
    sats = data.get("sats", 0)
    spd = data.get("spd", 0.0)
    lat = data.get("lat", 0.0)
    lng = data.get("lng", 0.0)
    wqi = data.get("wqi", 0)
    rssi = data.get("rssi", 0)
    snr = data.get("snr", 0.0)
    s3_temp = data.get("s3ChipT", 0.0)
    # Telemetry chỉ có 1 trường bitmask tên "hw"; hỗ trợ cả khoá "hwm" nếu nguồn khác gửi.
    hwm = data.get("hwm", data.get("hw", 0)) or 0
    
    bat_pct = battery_percent(bat)
    
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
                    "value": f"[🌐 **Xem Bản Đồ 3D WebGL Lòng Hồ**](http://192.168.4.1/3d)\n[📊 **Mở Bảng Tính Google Sheets**](https://docs.google.com/spreadsheets/d/{DEFAULT_SPREADSHEET_ID}/edit)",
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
                    "value": f"• **pH:** `{prf.get('ph1', 0.0):.2f}` | **DO:** `{prf.get('do1', 0.0):.2f} mg/L`\n• **TDS:** `{prf.get('tds1', 0.0):.0f} ppm` | **NTU:** `{prf.get('ntu1', 0.0):.0f}`\n• **Nhiệt độ:** `{prf.get('t1', 0.0):.1f}°C` | **WQI:** `{prf.get('wqi1', 0)}`",
                    "inline": False
                },
                {
                    "name": "2️⃣ TẦNG GIỮA (Độ sâu 1.0m)",
                    "value": f"• **pH:** `{prf.get('ph2', 0.0):.2f}` | **DO:** `{prf.get('do2', 0.0):.2f} mg/L`\n• **TDS:** `{prf.get('tds2', 0.0):.0f} ppm` | **NTU:** `{prf.get('ntu2', 0.0):.0f}`\n• **Nhiệt độ:** `{prf.get('t2', 0.0):.1f}°C` | **WQI:** `{prf.get('wqi2', 0)}`",
                    "inline": False
                },
                {
                    "name": "3️⃣ TẦNG ĐÁY (Độ sâu 1.8m)",
                    "value": f"• **pH:** `{prf.get('ph3', 0.0):.2f}` | **DO:** `{prf.get('do3', 0.0):.2f} mg/L`\n• **TDS:** `{prf.get('tds3', 0.0):.0f} ppm` | **NTU:** `{prf.get('ntu3', 0.0):.0f}`\n• **Nhiệt độ:** `{prf.get('t3', 0.0):.1f}°C` | **WQI:** `{prf.get('wqi3', 0)}`",
                    "inline": False
                },
                {
                    "name": "🌐 TRỰC QUAN HÓA 3D & DỮ LIỆU ĐÁM MÂY",
                    "value": f"[🌐 **Xem Mô Hình 3D Lòng Hồ WebGL**](http://192.168.4.1/3d)\n[📊 **Mở Bảng Tính Profile_3Tang**](https://docs.google.com/spreadsheets/d/{DEFAULT_SPREADSHEET_ID}/edit#gid=Profile_3Tang)",
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

def build_arg_parser():
    ap = argparse.ArgumentParser(
        description="Cầu nối Serial ESP32 -> Google Sheets cho ChemStation AI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--port", help="Cổng Serial (mặc định: tự dò ESP32)")
    ap.add_argument("--baud", type=int, default=115200, help="Tốc độ baud (mặc định 115200)")
    ap.add_argument("--url", default=os.environ.get("GSHEET_URL", ""),
                    help="URL Google Apps Script /exec (hoặc biến môi trường GSHEET_URL)")
    ap.add_argument("--discord-webhook", default=DEFAULT_DISCORD_WEBHOOK,
                    help="URL Discord Webhook để gửi báo cáo quan trắc")
    ap.add_argument("--discord-interval", type=float, default=60.0,
                    help="Khoảng cách giây giữa 2 lần gửi Discord (mặc định 60s)")
    ap.add_argument("--no-discord", action="store_true", help="Tắt gửi thông báo Discord")
    ap.add_argument("--interval", type=float, default=5.0,
                    help="Giây giữa 2 lần đẩy telemetry (mặc định 5)")
    ap.add_argument("--csv", default="gsheets_offline.csv",
                    help="File CSV dự phòng khi upload lỗi (mặc định gsheets_offline.csv)")
    ap.add_argument("--spreadsheet", default=DEFAULT_SPREADSHEET_ID,
                    help="ID bảng tính (chỉ dùng cho chế độ CLI gsheets cũ)")
    ap.add_argument("--gsheets-cli", action="store_true",
                    help="Dùng CLI 'gsheets' thay vì HTTP (không khuyến nghị)")
    ap.add_argument("--once", action="store_true", help="Đọc 1 gói rồi thoát")
    ap.add_argument("--print-only", action="store_true", help="Chỉ in dữ liệu, không upload")
    ap.add_argument("--no-crc", action="store_true", help="Bỏ qua kiểm tra CRC")
    ap.add_argument("--list-ports", action="store_true", help="Liệt kê cổng Serial rồi thoát")
    ap.add_argument("--log-file", default="gsheets_bridge.log", help="File log (mặc định gsheets_bridge.log)")
    ap.add_argument("--verbose", "-v", action="store_true", help="Log chi tiết")
    return ap


def setup_logging(args):
    level = logging.DEBUG if args.verbose else logging.INFO
    fmt = logging.Formatter("%(asctime)s [%(levelname)s] %(message)s", "%Y-%m-%d %H:%M:%S")
    log.setLevel(level)
    log.handlers.clear()

    console = logging.StreamHandler(sys.stdout)
    console.setFormatter(fmt)
    log.addHandler(console)

    if args.log_file:
        try:
            fileh = RotatingFileHandler(args.log_file, maxBytes=2 * 1024 * 1024, backupCount=3, encoding="utf-8")
            fileh.setFormatter(fmt)
            log.addHandler(fileh)
        except OSError:
            pass


def build_profile_sheet_row(p, src):
    """17 cột khớp đúng thứ tự PROFILE_HEADERS của Google Sheet."""
    now_str = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    do1 = p.get("do1", 5.80)
    do2 = p.get("do2", 5.65)
    do3 = p.get("do3", 5.40)
    return [
        now_str,
        p.get("ph1", ""), p.get("tds1", ""), p.get("ntu1", ""), p.get("t1", ""), do1,
        p.get("ph2", ""), p.get("tds2", ""), p.get("ntu2", ""), p.get("t2", ""), do2,
        p.get("ph3", ""), p.get("tds3", ""), p.get("ntu3", ""), p.get("t3", ""), do3,
        src,
    ]


def upload_via_cli(spreadsheet_id, sheet_name, row):
    import subprocess
    import shutil
    exe = shutil.which("gsheets") or shutil.which("gsheets.cmd") or "gsheets"
    formatted_vals = []
    for v in row:
        s = str(v) if v is not None else ""
        s = s.replace(",", "\,")
        formatted_vals.append(s)
    row_str = ",".join(formatted_vals)
    cmd = [exe, "mutate", "append", spreadsheet_id, sheet_name, row_str]
    try:
        res = subprocess.run(cmd, check=False, capture_output=True, text=True, timeout=30, shell=(os.name == 'nt'))
        if res.returncode == 0:
            return True
        log.warning("gsheets CLI lỗi (%d): %s", res.returncode, (res.stderr or "").strip()[:160])
    except (OSError, subprocess.SubprocessError) as exc:
        log.warning("Không chạy được gsheets CLI: %s", exc)
    return False


def main(argv=None):
    args = build_arg_parser().parse_args(argv)
    setup_logging(args)

    if args.list_ports:
        print_ports()
        return 0

    ports = detect_ports(args.port)
    if not ports:
        log.error("Không tìm thấy cổng Serial. Dùng --list-ports để kiểm tra.")
        return 2

    log.info("ChemStation AI Bridge v%s | Cổng: %s", FW_VERSION, ", ".join(ports))
    if args.url:
        log.info("Google Apps Script: %s", args.url)
    else:
        log.warning("Chưa có --url: dữ liệu sẽ chỉ ghi ra CSV.")

    link = SerialLink(ports, args.baud, explicit_port=bool(args.port))
    uploader = Uploader(args.url, args.spreadsheet, args.csv)

    last_upload = 0.0
    last_seq = None
    last_seq_ts = 0.0
    last_discord_ts = 0.0
    latest = None
    latest_src = ""
    last_rssi = None
    last_snr = None
    start_time = time.time()

    try:
        while True:
            line = link.readline()
            if not line:
                if args.once and (time.time() - start_time > 15.0):
                    log.warning("Hết thời gian chờ mà chưa nhận được gói USV.")
                    return 1
                continue

            if args.verbose:
                log.debug("RX: %s", line)

            rm = RSSI_RE.search(line)
            if rm:
                last_rssi = to_int(rm.group(1), last_rssi)
            sm = SNR_RE.search(line)
            if sm:
                last_snr = to_float(sm.group(1), last_snr)

            m = USV_RE.search(line)
            if m:
                payload, crc_hex = m.group(1), m.group(2)
                # Bỏ qua dòng tóm tắt thông số [GCS-RX ...] vốn không mang đuôi CRC *XX
                if crc_hex is None and ("[GCS-RX" in line or "[GCS-CFG" in line or "[GCS-BOOT" in line):
                    continue
                if not args.no_crc and not verify_crc(payload, crc_hex):
                    log.warning("CRC sai (len=%d, nhận=%s) -> bỏ gói: %s", len(payload), crc_hex, payload)
                    continue
                data = parse_usv(payload)
                if data is None:
                    log.warning("Gói USV thiếu trường (%d < 24)", len(payload.split(",")))
                    continue

                src = f"ESP32-Live ({link.port or '?'})"
                if last_rssi is not None:
                    data["rssi"] = last_rssi
                if last_snr is not None:
                    data["snr"] = last_snr
                latest, latest_src = data, src
                hw_text, hw_cnt, missing = hw_summary(data.get("hw"))

                if missing:
                    log.info(
                        "RX #%s | pH %s | TDS %s | NTU %s | %s | %s | THIEU: %s",
                        data.get("seq"), fmt(data.get("ph"), 2), fmt(data.get("tds"), 0),
                        fmt(data.get("ntu"), 0), fmt(data.get("temp"), 1, "C"),
                        hw_text, ",".join(missing),
                    )
                else:
                    log.info(
                        "RX #%s | pH %s | TDS %s | NTU %s | %s | %s | GPS %s sat",
                        data.get("seq"), fmt(data.get("ph"), 2), fmt(data.get("tds"), 0),
                        fmt(data.get("ntu"), 0), fmt(data.get("temp"), 1, "C"),
                        hw_text, data.get("sats", 0),
                    )

                now = time.time()
                if args.once or (now - last_upload) >= args.interval:
                    seq = data.get("seq")
                    if seq is not None and seq == last_seq and (now - last_seq_ts) < 3.0:
                        log.debug("Bỏ qua gói trùng #%s", seq)
                    else:
                        query, hw_text, missing, qcvn, watt = build_tele_query(data, src)
                        if args.print_only:
                            log.info("DATA: %s", query)
                        else:
                            # 1. Luôn ghi CSV dự phòng song song để bảo toàn 100% dữ liệu
                            uploader.append_csv(build_csv_row(data, src))
                            # 2. Đẩy trực tiếp qua HTTP POST JSON tới Web App
                            if uploader.url and "YOUR_SCRIPT_ID" not in uploader.url:
                                tele_payload = {"mode": "tele", **data, "src": src, "fw": FW_VERSION}
                                if uploader.post_json(tele_payload):
                                    log.info("Đã đẩy #%s lên Google Sheets qua Web App (%s | OK:%d Lỗi:%d)",
                                             seq, qcvn, uploader.ok_count, uploader.fail_count)
                            else:
                                log.debug("Chưa cấu hình --url, dữ liệu đã lưu vào CSV dự phòng.")

                        last_upload = now
                        last_seq = seq
                        last_seq_ts = now
                        if not args.no_discord and args.discord_webhook and (now - last_discord_ts >= args.discord_interval or args.once):
                            if send_discord_report(data, src, args.discord_webhook):
                                log.info("Da gui bao cao USV goi #%s ve Discord!", seq)
                            last_discord_ts = now

                        if args.once:
                            return 0

            pm = PRF_RE.search(line)
            if pm:
                payload, crc_hex = pm.group(1), pm.group(2)
                if not args.no_crc and not verify_crc(payload, crc_hex, prefix="PRF:"):
                    log.warning("CRC sai PRF (len=%d, nhận=%s) -> bỏ gói: %s", len(payload), crc_hex, payload)
                    continue
                prf = parse_prf(payload)
                if prf is None:
                    log.warning("Gói PRF không hợp lệ: %s", payload[:80])
                    continue
                src = f"ESP32-Live ({link.port or '?'})"
                log.info("PRF 3 tầng: Mặt pH %s | Giữa pH %s | Đáy pH %s",
                         fmt(prf.get("ph1"), 2), fmt(prf.get("ph2"), 2), fmt(prf.get("ph3"), 2))
                query = build_profile_query(prf, src)
                if args.print_only:
                    log.info("PROFILE: %s", query)
                elif uploader.url and "YOUR_SCRIPT_ID" not in uploader.url:
                    prf_payload = {"mode": "profile", **prf, "src": src, "fw": FW_VERSION}
                    if uploader.post_json(prf_payload):
                        log.info("Đã đẩy bảng 3 tầng lên Google Sheets qua Web App.")
                else:
                    log.info("Đã nhận bảng 3 tầng (chưa cấu hình --url Web App).")
                if not args.no_discord and args.discord_webhook:
                    send_discord_profile_report(prf, src, args.discord_webhook)
                    log.info("Đã gửi Báo Cáo Khảo Sát 3 Tầng lên Discord Webhook!")

    except KeyboardInterrupt:
        log.info("Dừng bởi người dùng. OK:%d Lỗi:%d", uploader.ok_count, uploader.fail_count)
        return 0
    finally:
        link.close()


if __name__ == "__main__":
    sys.exit(main())

