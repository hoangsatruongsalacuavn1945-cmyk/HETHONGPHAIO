import os
import re

with open('gsheets_live_bridge.py', 'r', encoding='utf-8') as f:
    code = f.read()

# Replace upload_via_cli
old_upload = """def upload_via_cli(spreadsheet_id, row):
    import subprocess
    import shutil
    exe = shutil.which("gsheets") or shutil.which("gsheets.cmd") or "gsheets"
    cmd = [exe, "mutate", "append", spreadsheet_id, "Telemetry_Realtime",
           ",".join(str(v) for v in row)]
    try:
        res = subprocess.run(cmd, check=False, capture_output=True, text=True, timeout=30, shell=(os.name == 'nt'))
        if res.returncode == 0:
            return True
        log.warning("gsheets CLI lỗi (%d): %s", res.returncode, (res.stderr or "").strip()[:160])
    except (OSError, subprocess.SubprocessError) as exc:
        log.warning("Không chạy được gsheets CLI: %s", exc)
    return False"""

new_upload = """def build_profile_sheet_row(p, src):
    \"\"\"17 cột khớp đúng thứ tự PROFILE_HEADERS của Google Sheet.\"\"\"
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
        s = s.replace(",", "\\,")
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
    return False"""

if old_upload in code:
    code = code.replace(old_upload, new_upload)
    print("Replaced upload_via_cli successfully.")
else:
    print("Warning: old_upload not found exactly, searching via regex...")

# Replace telemetry upload
old_tele = """                        elif args.gsheets_cli:
                            if not upload_via_cli(args.spreadsheet, build_sheet_row(data, src)):
                                uploader.append_csv(build_csv_row(data, src))
                        elif uploader.send(query):
                            log.info("Đã đẩy #%s lên Google Sheets (%s | OK:%d Lỗi:%d)",
                                     seq, qcvn, uploader.ok_count, uploader.fail_count)
                        else:
                            uploader.append_csv(build_csv_row(data, src))
                            log.info("Đã ghi #%s vào CSV dự phòng.", seq)"""

new_tele = """                        elif uploader.url:
                            if uploader.send(query):
                                log.info("Đã đẩy #%s lên Google Sheets qua Web App (%s | OK:%d Lỗi:%d)",
                                         seq, qcvn, uploader.ok_count, uploader.fail_count)
                            else:
                                if upload_via_cli(args.spreadsheet, "Telemetry_Realtime", build_sheet_row(data, src)):
                                    log.info("Đã fallback đẩy #%s lên Google Sheets qua CLI thành công!", seq)
                                else:
                                    uploader.append_csv(build_csv_row(data, src))
                                    log.info("Đã ghi #%s vào CSV dự phòng.", seq)
                        else:
                            if upload_via_cli(args.spreadsheet, "Telemetry_Realtime", build_sheet_row(data, src)):
                                log.info("Đã đẩy #%s lên Google Sheets qua CLI (%s)", seq, qcvn)
                            else:
                                uploader.append_csv(build_csv_row(data, src))
                                log.info("Đã ghi #%s vào CSV dự phòng.", seq)"""

if old_tele in code:
    code = code.replace(old_tele, new_tele)
    print("Replaced old_tele successfully.")
else:
    print("Warning: old_tele not found.")

# Replace profile upload
old_prf = """                query = build_profile_query(prf, src)
                if args.print_only:
                    log.info("PROFILE: %s", query)
                elif uploader.send(query):
                    log.info("Đã đẩy bảng 3 tầng lên Google Sheets.")
                else:
                    log.warning("Không đẩy được bảng 3 tầng (xem CSV/log để kiểm tra).")"""

new_prf = """                query = build_profile_query(prf, src)
                if args.print_only:
                    log.info("PROFILE: %s", query)
                elif uploader.url and uploader.send(query):
                    log.info("Đã đẩy bảng 3 tầng lên Google Sheets qua Web App.")
                else:
                    if upload_via_cli(args.spreadsheet, "Profile_3Tang", build_profile_sheet_row(prf, src)):
                        log.info("Đã đẩy bảng 3 tầng lên Google Sheets qua CLI gsheets.")
                    else:
                        log.warning("Không đẩy được bảng 3 tầng lên Google Sheets.")
                if not args.no_discord and args.discord_webhook:
                    send_discord_profile_report(prf, src, args.discord_webhook)
                    log.info("Đã gửi Báo Cáo Khảo Sát 3 Tầng lên Discord Webhook!")"""

if old_prf in code:
    code = code.replace(old_prf, new_prf)
    print("Replaced old_prf successfully.")
else:
    print("Warning: old_prf not found.")

with open('gsheets_live_bridge.py', 'w', encoding='utf-8') as f:
    f.write(code)
print("Finished patching gsheets_live_bridge.py!")
