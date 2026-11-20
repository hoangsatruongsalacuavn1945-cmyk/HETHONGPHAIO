with open('gsheets_live_bridge.py', 'r', encoding='utf-8') as f:
    code = f.read()

old_func = """def verify_crc(payload, crc_hex):
    if not crc_hex:
        return True  # Không có CRC -> chấp nhận (tương thích FW cũ)
    try:
        expected = int(crc_hex, 16)
    except ValueError:
        return False
    return calc_keyed_crc8(payload) == expected"""

new_func = """def verify_crc(payload, crc_hex, prefix="USV:"):
    if not crc_hex:
        return True  # Không có CRC -> chấp nhận (tương thích FW cũ)
    try:
        expected = int(crc_hex, 16)
    except ValueError:
        return False
    # Firmware C++ ESP32 tính CRC trên toàn bộ chuỗi bao gồm cả prefix ("USV:" hoặc "PRF:")
    full_data = payload if payload.startswith(prefix) else (prefix + payload)
    return calc_keyed_crc8(full_data) == expected"""

code = code.replace(old_func, new_func)

# Also ensure in PRF check verify_crc is called with prefix="PRF:"
old_prf_call = """                if not args.no_crc and not verify_crc(payload, crc_hex):"""
new_prf_call = """                if not args.no_crc and not verify_crc(payload, crc_hex, prefix="PRF:"):"""

# Note: only replace the second occurrence (the PRF one)
parts = code.split(old_prf_call)
if len(parts) == 3:
    code = parts[0] + old_prf_call + parts[1] + new_prf_call + parts[2]
    print("Updated verify_crc for both USV and PRF successfully!")
else:
    print(f"Warning: expected 2 occurrences of old_prf_call, found {len(parts)-1}")

with open('gsheets_live_bridge.py', 'w', encoding='utf-8') as f:
    f.write(code)
print("Finished updating gsheets_live_bridge.py with exact CRC match!")
