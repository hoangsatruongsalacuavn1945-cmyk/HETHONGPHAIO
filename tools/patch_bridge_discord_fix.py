with open('gsheets_live_bridge.py', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

target = """            last_upload = now
            last_seq = seq
            last_seq_ts = now"""

replacement = """            last_upload = now
            last_seq = seq
            last_seq_ts = now
            if not args.no_discord and args.discord_webhook and (now - last_discord_ts >= args.discord_interval or args.once):
                if send_discord_report(data, src, args.discord_webhook):
                    log.info("Da gui bao cao USV goi #%s ve Discord!", seq)
                last_discord_ts = now"""

if target in text:
    text = text.replace(target, replacement, 1)
    with open('gsheets_live_bridge.py', 'w', encoding='utf-8') as f:
        f.write(text)
    print("Placed Discord trigger at line 837!")
else:
    print("Target not found!")
