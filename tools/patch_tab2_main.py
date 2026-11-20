import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

with open('src/ground_station/main.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

# 1. Update Tab 2 markup with responsive iframe and refresh buttons
old_tab2 = """<!-- TAB 2: BẢN ĐỒ 3D WEBGL -->
<div id="view_map3d" class="tab-content">
  <div class="card" style="padding:6px">
    <div class="card-title" style="padding:6px 8px">
      <span>BẢN ĐỒ 3D WEBGL KHÔNG GIAN HỒ NƯỚC & PHAO USV THỜI GIAN THỰC</span>
      <button onclick="window.open('/3d','_blank')" class="btn-cyan" style="padding:4px 10px;font-size:0.75rem">⛶ MỞ TOÀN MÀN HÌNH</button>
    </div>
    <iframe id="frame3D" src="/3d" style="width:100%;height:680px;border:none;border-radius:8px;background:#030712"></iframe>
  </div>
</div>"""

new_tab2 = """<!-- TAB 2: BẢN ĐỒ 3D WEBGL -->
<div id="view_map3d" class="tab-content">
  <div class="card" style="padding:6px">
    <div class="card-title" style="padding:6px 8px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:6px">
      <span>BẢN ĐỒ 3D WEBGL KHÔNG GIAN HỒ NƯỚC & PHAO USV THỜI GIAN THỰC</span>
      <div style="display:flex;gap:6px">
        <button onclick="document.getElementById('frame3D').src='/3d'" class="btn-blue" style="padding:4px 10px;font-size:0.75rem">🔄 TẢI LẠI 3D</button>
        <button onclick="window.open('/3d','_blank')" class="btn-cyan" style="padding:4px 10px;font-size:0.75rem">⛶ MỞ TAB TOÀN MÀN HÌNH</button>
      </div>
    </div>
    <iframe id="frame3D" src="/3d" style="width:100%;height:calc(100vh - 165px);min-height:580px;border:none;border-radius:8px;background:#030712"></iframe>
  </div>
</div>"""

if old_tab2 in text:
    text = text.replace(old_tab2, new_tab2, 1)
    print("Updated Tab 2 HTML in main.cpp")
else:
    print("old_tab2 not found")

# 2. Update switchTab function to ensure iframe loads and resizes
old_switch = """// --- Tab Switching ---
function switchTab(t){
  ['cockpit','map3d','config'].forEach(k=>{
    document.getElementById('view_'+k).className='tab-content'+(k===t?' active':'');
    document.getElementById('tab_'+k).className='tab-btn'+(k===t?' active':'');
  });
  if(t==='map3d'){
    const f=document.getElementById('frame3D');
    if(f&&!f.src)f.src='/3d';
  }
}"""

new_switch = """// --- Tab Switching ---
function switchTab(t){
  ['cockpit','map3d','config'].forEach(k=>{
    document.getElementById('view_'+k).className='tab-content'+(k===t?' active':'');
    document.getElementById('tab_'+k).className='tab-btn'+(k===t?' active':'');
  });
  if(t==='map3d'){
    const f=document.getElementById('frame3D');
    if(f && (!f.src || f.src==='about:blank' || !f.getAttribute('src'))){
      f.src='/3d';
    }
  }
}"""

if old_switch in text:
    text = text.replace(old_switch, new_switch, 1)
    print("Updated switchTab in main.cpp")
else:
    print("old_switch not found")

with open('src/ground_station/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("Updated main.cpp successfully!")
