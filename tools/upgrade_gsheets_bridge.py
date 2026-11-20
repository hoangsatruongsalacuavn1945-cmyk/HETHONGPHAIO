#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os
import sys

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

path = os.path.join(os.path.dirname(__file__), "..", "gsheets_live_bridge.py")
with open(path, "r", encoding="utf-8") as f:
    c = f.read()

target = """    def append_csv(self, row):
        if not self.csv_path:
            return
        try:
            need_header = (not os.path.exists(self.csv_path)) or os.path.getsize(self.csv_path) == 0
            with open(self.csv_path, "a", newline="", encoding="utf-8-sig") as fh:
                writer = csv.writer(fh)
                if need_header:
                    writer.writerow(CSV_HEADER)
                writer.writerow(row)
        except OSError as exc:
            log.error("Không ghi được CSV %s: %s", self.csv_path, exc)"""

replacement = """    def _rotate_csv_if_needed(self, max_bytes=5 * 1024 * 1024, backups=3):
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
                time.sleep(0.05)"""

if target in c:
    c = c.replace(target, replacement)
    with open(path, "w", encoding="utf-8") as f:
        f.write(c)
    print("[OK] Updated gsheets_live_bridge.py with 5MB CSV rotation and retry protection.")
else:
    print("[WARN] Target method not found in gsheets_live_bridge.py.")
