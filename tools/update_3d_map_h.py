import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

with open('src/ground_station/web_3d_map.h', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

# 1. Remove tailwind script
old_script = '<script src="https://www.gstatic.com/antigravity/web/dev/tailwindcss.min.js"></script>'
if old_script in content:
    content = content.replace(old_script, '<!-- 100% Offline Embedded CSS (Zero External CDN Dependency) -->')
    print("Removed external tailwindcss script")
else:
    print("Tailwind script tag not found exactly")

# 2. Fix hardcoded 192.168.4.1 API calls
content = content.replace("'http://192.168.4.1/api/telemetry'", "'/api/telemetry'")
content = content.replace("'http://192.168.4.1/api/cmd?", "'/api/cmd?")
content = content.replace('"http://192.168.4.1/api/telemetry"', '"/api/telemetry"')
content = content.replace('"http://192.168.4.1/api/cmd?', '"/api/cmd?')

# 3. Comprehensive pure CSS bundle
css_bundle = """
    /* === 100% OFFLINE EMBEDDED PURE CSS ENGINE === */
    :root {
      --bg-dark: #030712; --cyan: #38bdf8; --cyan-bright: #00f5d4; --emerald: #34d399;
      --amber: #fbbf24; --red: #ef4444; --slate-300: #cbd5e1; --slate-400: #94a3b8;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
    body, html { margin: 0; padding: 0; width: 100%; height: 100%; overflow: hidden; background: #030712; font-family: ui-sans-serif, system-ui, -apple-system, sans-serif; color: #f1f5f9; }
    canvas { display: block; width: 100%; height: 100%; }
    .glass-card { background: rgba(15, 23, 42, 0.88); backdrop-filter: blur(14px); -webkit-backdrop-filter: blur(14px); border: 1px solid rgba(56, 189, 248, 0.25); box-shadow: 0 10px 25px -5px rgba(0,0,0,0.6); }
    .glass-button { background: rgba(30, 41, 59, 0.85); backdrop-filter: blur(8px); border: 1px solid rgba(148, 163, 184, 0.25); color: #cbd5e1; cursor: pointer; transition: all 0.16s ease; }
    .glass-button:hover { background: rgba(56, 189, 248, 0.25); border-color: rgba(56, 189, 248, 0.6); color: #fff; transform: translateY(-1px); }
    .glass-button.active { background: #0284c7; border-color: #38bdf8; color: #fff; box-shadow: 0 0 14px rgba(56, 189, 248, 0.45); }
    
    /* Layout */
    .flex { display: flex; }
    .flex-col { flex-direction: column; }
    .flex-wrap { flex-wrap: wrap; }
    .items-center { align-items: center; }
    .items-baseline { align-items: baseline; }
    .justify-between { justify-content: space-between; }
    .justify-center { justify-content: center; }
    .grid { display: grid; }
    .grid-cols-2 { grid-template-columns: repeat(2, 1fr); }
    .hidden { display: none !important; }
    .relative { position: relative; }
    .absolute { position: absolute; }
    .fixed { position: fixed; }
    
    /* Positioning */
    .top-3 { top: 12px; } .left-3 { left: 12px; } .right-3 { right: 12px; } .bottom-3 { bottom: 12px; }
    .bottom-14 { bottom: 56px; } .top-20 { top: 76px; } .left-1\\/2 { left: 50%; } .top-1\\/2 { top: 50%; }
    .-translate-x-1\\/2 { transform: translateX(-50%); }
    .-translate-y-1\\/2 { transform: translateY(-50%); }
    .z-10 { z-index: 10; } .z-20 { z-index: 20; }
    .pointer-events-none { pointer-events: none; }
    .pointer-events-auto { pointer-events: auto; }
    .overflow-y-auto { overflow-y: auto; }
    
    /* Sizing */
    .w-full { width: 100%; } .h-full { height: 100%; }
    .w-64 { width: 260px; } .w-72 { width: 290px; } .w-80 { width: 320px; }
    .w-2 { width: 8px; } .h-2 { height: 8px; }
    .w-3 { width: 12px; } .h-3 { height: 12px; }
    .w-3\\.5 { width: 14px; } .h-3\\.5 { height: 14px; }
    .h-1\\.5 { height: 6px; } .h-2\\.5 { height: 10px; }
    
    /* Spacing */
    .p-1 { padding: 4px; } .p-2 { padding: 8px; } .p-2\\.5 { padding: 10px; } .p-3\\.5 { padding: 14px; } .p-4 { padding: 16px; }
    .px-1\\.5 { padding-left: 6px; padding-right: 6px; }
    .px-2 { padding-left: 8px; padding-right: 8px; }
    .px-2\\.5 { padding-left: 10px; padding-right: 10px; }
    .px-3 { padding-left: 12px; padding-right: 12px; }
    .px-4 { padding-left: 16px; padding-right: 16px; }
    .py-0\\.5 { padding-top: 2px; padding-bottom: 2px; }
    .py-1 { padding-top: 4px; padding-bottom: 4px; }
    .py-1\\.5 { padding-top: 6px; padding-bottom: 6px; }
    .py-2 { padding-top: 8px; padding-bottom: 8px; }
    .py-2\\.5 { padding-top: 10px; padding-bottom: 10px; }
    .mb-1 { margin-bottom: 4px; } .mb-2 { margin-bottom: 8px; } .mb-3 { margin-bottom: 12px; }
    .mt-0\\.5 { margin-top: 2px; } .mt-1 { margin-top: 4px; } .mr-1 { margin-right: 4px; }
    .pb-1\\.5 { padding-bottom: 6px; } .pb-2 { padding-bottom: 8px; }
    .pt-1 { padding-top: 4px; } .pt-2 { padding-top: 8px; }
    .pr-2 { padding-right: 8px; } .pl-1 { padding-left: 4px; }
    .gap-1 { gap: 4px; } .gap-1\\.5 { gap: 6px; } .gap-2 { gap: 8px; } .gap-3 { gap: 12px; } .gap-x-2 { column-gap: 8px; }
    .space-y-1\\.5 > * + * { margin-top: 6px; }
    .space-y-3 > * + * { margin-top: 12px; }
    
    /* Borders & Radius */
    .rounded { border-radius: 4px; } .rounded-lg { border-radius: 8px; }
    .rounded-xl { border-radius: 12px; } .rounded-2xl { border-radius: 16px; }
    .rounded-full { border-radius: 9999px; }
    .border { border: 1px solid rgba(255,255,255,0.12); }
    .border-b { border-bottom: 1px solid rgba(255,255,255,0.12); }
    .border-t { border-top: 1px solid rgba(255,255,255,0.12); }
    .border-r { border-right: 1px solid rgba(255,255,255,0.12); }
    .border-slate-700 { border-color: #334155; }
    .border-slate-700\\/60 { border-color: rgba(51, 65, 85, 0.6); }
    .border-slate-800 { border-color: #1e293b; }
    .border-cyan-400\\/40 { border-color: rgba(56, 189, 248, 0.4); }
    .border-cyan-800\\/60 { border-color: rgba(7, 89, 133, 0.6); }
    .border-emerald-700 { border-color: #047857; }
    .border-emerald-700\\/60 { border-color: rgba(4, 120, 87, 0.6); }
    
    /* Backgrounds */
    .bg-slate-900\\/40 { background: rgba(15, 23, 42, 0.4); }
    .bg-slate-900\\/50 { background: rgba(15, 23, 42, 0.5); }
    .bg-slate-900\\/60 { background: rgba(15, 23, 42, 0.6); }
    .bg-slate-700 { background: #334155; }
    .bg-cyan-950\\/80 { background: rgba(8, 47, 73, 0.8); }
    .bg-emerald-950 { background: #022c22; }
    .bg-amber-950 { background: #451a03; }
    .bg-emerald-400 { background: #34d399; }
    
    /* Typography */
    .text-\\[10px\\] { font-size: 10px; } .text-\\[11px\\] { font-size: 11px; }
    .text-xs { font-size: 12px; } .text-sm { font-size: 14px; }
    .text-base { font-size: 16px; } .text-3xl { font-size: 28px; }
    .font-mono { font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; }
    .font-normal { font-weight: 400; } .font-semibold { font-weight: 600; }
    .font-bold { font-weight: 700; } .font-black { font-weight: 900; }
    .uppercase { text-transform: uppercase; }
    .tracking-wide { letter-spacing: 0.5px; } .tracking-wider { letter-spacing: 1px; } .tracking-tight { letter-spacing: -0.5px; }
    
    /* Colors */
    .text-cyan-200 { color: #a5f3fc; } .text-cyan-300 { color: #67e8f9; } .text-cyan-400 { color: #38bdf8; }
    .text-emerald-300 { color: #6ee7b7; } .text-emerald-400 { color: #34d399; }
    .text-sky-300 { color: #7dd3fc; } .text-sky-400 { color: #38bdf8; }
    .text-amber-300 { color: #fcd34d; } .text-amber-400 { color: #fbbf24; }
    .text-red-400 { color: #f87171; }
    .text-slate-100 { color: #f1f5f9; } .text-slate-300 { color: #cbd5e1; }
    .text-slate-400 { color: #94a3b8; } .text-slate-500 { color: #64748b; }
    
    /* Animations */
    .animate-pulse { animation: pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite; }
    @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
    .animate-ping { animation: ping 1.2s cubic-bezier(0, 0, 0.2, 1) infinite; }
    @keyframes ping { 75%, 100% { transform: scale(2); opacity: 0; } }
    .shadow-lg { box-shadow: 0 10px 15px -3px rgba(0,0,0,0.4); }
    .shadow-2xl { box-shadow: 0 25px 50px -12px rgba(0,0,0,0.6); }
    .cursor-pointer { cursor: pointer; }

    /* Custom scrollbar */
    ::-webkit-scrollbar { width: 5px; height: 5px; }
    ::-webkit-scrollbar-thumb { background: rgba(56, 189, 248, 0.35); border-radius: 4px; }
    ::-webkit-scrollbar-track { background: transparent; }

    /* Mobile Responsive Overrides */
    @media (max-width: 860px) {
      header { flex-direction: column; align-items: stretch !important; gap: 6px !important; }
      header > div { justify-content: center; }
      aside.w-64 { width: calc(100% - 24px) !important; max-height: 28vh; top: auto !important; bottom: 68px !important; }
      aside.w-72 { display: none !important; } /* Hidden on small mobile to give room to 3D canvas */
      footer { width: calc(100% - 24px) !important; bottom: 8px !important; padding: 6px 8px !important; }
    }
"""

# Replace existing <style> tag content
style_start = content.find('<style>')
style_end = content.find('</style>')
if style_start != -1 and style_end != -1:
    content = content[:style_start] + '<style>' + css_bundle + '\n  ' + content[style_end:]
    print("Replaced <style> block with 100% offline self-contained pure CSS bundle")
else:
    print("Could not find <style> block")

with open('src/ground_station/web_3d_map.h', 'w', encoding='utf-8') as out:
    out.write(content)
print("Updated src/ground_station/web_3d_map.h successfully!")
