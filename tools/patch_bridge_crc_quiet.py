with open('gsheets_live_bridge.py', 'r', encoding='utf-8') as f:
    code = f.read()

old_crc_check = """            m = USV_RE.search(line)
            if m:
                payload, crc_hex = m.group(1), m.group(2)
                if not args.no_crc and not verify_crc(payload, crc_hex):
                    log.warning("CRC sai -> bỏ gói: %s", payload[:80])
                    continue"""

new_crc_check = """            m = USV_RE.search(line)
            if m:
                payload, crc_hex = m.group(1), m.group(2)
                # Bỏ qua dòng tóm tắt thông số [GCS-RX ...] vốn không mang đuôi CRC *XX
                if crc_hex is None and ("[GCS-RX" in line or "[GCS-CFG" in line or "[GCS-BOOT" in line):
                    continue
                if not args.no_crc and not verify_crc(payload, crc_hex):
                    log.warning("CRC sai -> bỏ gói: %s", payload[:80])
                    continue"""

if old_crc_check in code:
    code = code.replace(old_crc_check, new_crc_check, 1)
    print("Fixed false-positive CRC check successfully!")
else:
    print("Warning: old_crc_check not found!")

with open('gsheets_live_bridge.py', 'w', encoding='utf-8') as f:
    f.write(code)
print("Finished updating gsheets_live_bridge.py!")
