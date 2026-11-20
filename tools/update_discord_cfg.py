import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

webhook_url = ""

# 1. Update include/usv_common.h
with open('include/usv_common.h', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

target = '#define DEFAULT_DISCORD_WEBHOOK_URL ""'
replacement = f'#define DEFAULT_DISCORD_WEBHOOK_URL "{webhook_url}"'

if target in text:
    text = text.replace(target, replacement, 1)
    with open('include/usv_common.h', 'w', encoding='utf-8') as f:
        f.write(text)
    print("Updated include/usv_common.h with Discord Webhook URL")
else:
    print("Target in usv_common.h not found")

# 2. Update google_sheets_script.gs
with open('google_sheets_script.gs', 'r', encoding='utf-8', errors='ignore') as f:
    gs_text = f.read()

gs_target = 'DISCORD_WEBHOOK_URL: "",'
gs_replacement = f'DISCORD_WEBHOOK_URL: "{webhook_url}",'

if gs_target in gs_text:
    gs_text = gs_text.replace(gs_target, gs_replacement, 1)
    with open('google_sheets_script.gs', 'w', encoding='utf-8') as f:
        f.write(gs_text)
    print("Updated google_sheets_script.gs with Discord Webhook URL")
else:
    print("Target in google_sheets_script.gs not found")
