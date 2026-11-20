with open('gsheets_live_bridge.py', 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

new_lines = []
for i, line in enumerate(lines):
    new_lines.append(line)
    if 'last_seq_ts = now' in line and i > 800:
        indent = ' ' * (len(line) - len(line.lstrip()))
        discord_call = (
            f"{indent}if not args.no_discord and args.discord_webhook and (now - last_discord_ts >= args.discord_interval or args.once):\n"
            f"{indent}    if send_discord_report(data, src, args.discord_webhook):\n"
            f"{indent}        log.info(\"Da gui bao cao USV goi #%s ve Discord!\", seq)\n"
            f"{indent}    last_discord_ts = now\n"
        )
        new_lines.append(discord_call)
        print(f"Inserted Discord call after line {i+1} with indent {len(indent)}")

with open('gsheets_live_bridge.py', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)
print("Updated successfully!")
