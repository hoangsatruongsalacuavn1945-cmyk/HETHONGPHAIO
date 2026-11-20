import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

new_web_dashboard = r'''
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no,viewport-fit=cover">
<title>USV AQUA-TACTICAL | Trạm Điều Khiển Trung Tâm</title>
<style>
:root{
  --bg:#030712;--bg-card:rgba(15,23,42,0.88);--border:rgba(0,245,212,0.22);
  --cyan:#00f5d4;--blue:#00bbf9;--green:#06d6a0;--amber:#ffd166;--red:#ff4757;
  --text:#f8fafc;--sub:#94a3b8;--dark:#0b1329;
}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
body{background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,sans-serif;padding:10px;min-height:100vh;background-image:radial-gradient(ellipse at top,rgba(0,187,249,0.08) 0%,transparent 70%)}
.hdr{background:linear-gradient(135deg,#0a152e,#112347);border:1px solid var(--border);border-radius:12px;padding:10px 14px;margin-bottom:10px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;box-shadow:0 8px 24px rgba(0,0,0,0.5)}
.hdr-left{display:flex;align-items:center;gap:10px}
.pulse{width:10px;height:10px;border-radius:50%;background:var(--green);box-shadow:0 0 10px var(--green);animation:pulse 1.8s infinite}
@keyframes pulse{0%{opacity:0.4;transform:scale(0.85)}50%{opacity:1;transform:scale(1.2)}100%{opacity:0.4;transform:scale(0.85)}}
.title{font-size:1.05rem;font-weight:900;letter-spacing:1px;color:var(--cyan);text-shadow:0 0 12px rgba(0,245,212,0.3)}
.badges{display:flex;gap:6px;flex-wrap:wrap;align-items:center}
.badge{padding:4px 9px;border-radius:6px;font-size:0.75rem;font-weight:700;font-family:monospace;background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.12);display:inline-flex;align-items:center;gap:4px}
.badge.green{background:rgba(6,214,160,0.15);color:var(--green);border-color:var(--green)}
.badge.cyan{background:rgba(0,245,212,0.15);color:var(--cyan);border-color:var(--cyan)}
.badge.amber{background:rgba(255,209,102,0.15);color:var(--amber);border-color:var(--amber)}
.badge.red{background:rgba(255,71,87,0.15);color:var(--red);border-color:var(--red)}

.tabs{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin-bottom:10px}
.tab-btn{padding:10px 6px;border-radius:8px;font-size:0.82rem;font-weight:800;background:rgba(255,255,255,0.04);border:1px solid rgba(255,255,255,0.1);color:var(--sub);cursor:pointer;transition:all 0.2s;text-align:center;user-select:none}
.tab-btn.active{background:linear-gradient(90deg,rgba(0,245,212,0.22),rgba(0,187,249,0.22));border-color:var(--cyan);color:var(--cyan);box-shadow:0 0 16px rgba(0,245,212,0.28)}

.card{background:var(--bg-card);border:1px solid var(--border);border-radius:12px;padding:12px;margin-bottom:10px;box-shadow:0 8px 24px rgba(0,0,0,0.45);backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px)}
.card-title{font-size:0.82rem;font-weight:800;color:var(--cyan);text-transform:uppercase;letter-spacing:0.8px;margin-bottom:8px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:4px}
.grid-2{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:10px}
.grid-4{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:8px}

.m-pill{background:rgba(255,255,255,0.03);border:1px solid rgba(255,255,255,0.07);border-radius:8px;padding:8px 10px;transition:border-color 0.2s}
.m-pill:hover{border-color:rgba(0,245,212,0.3)}
.m-lbl{font-size:0.7rem;color:var(--sub);text-transform:uppercase;letter-spacing:0.5px;font-weight:600}
.m-val{font-size:1.45rem;font-weight:900;color:#fff;margin:2px 0;font-family:monospace;letter-spacing:-0.5px}
.m-sub{font-size:0.72rem;color:var(--sub)}

.btns{display:grid;gap:6px;margin-top:6px}
button{padding:9px 12px;border-radius:8px;border:1px solid rgba(255,255,255,0.12);background:rgba(255,255,255,0.06);color:#fff;font-weight:800;font-size:0.78rem;cursor:pointer;transition:all 0.15s;user-select:none}
button:hover{filter:brightness(1.18);transform:translateY(-1px)}
button:active{transform:translateY(0)}
.btn-cyan{background:var(--cyan)!important;color:#040714!important;border-color:var(--cyan)!important}
.btn-green{background:var(--green)!important;color:#040714!important;border-color:var(--green)!important}
.btn-blue{background:var(--blue)!important;color:#040714!important;border-color:var(--blue)!important}
.btn-amber{background:var(--amber)!important;color:#040714!important;border-color:var(--amber)!important}
.btn-red{background:var(--red)!important;color:#fff!important;border-color:var(--red)!important}

input{width:100%;box-sizing:border-box;margin-top:4px;padding:8px 10px;border-radius:6px;border:1px solid rgba(0,245,212,0.3);background:#070d1e;color:#fff;font-size:0.82rem;font-family:monospace}
canvas{display:block;border-radius:8px}
.tab-content{display:none}
.tab-content.active{display:block}

.hw-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:6px;margin-top:8px}
.hw-node{padding:7px 9px;border-radius:6px;font-size:0.75rem;font-family:monospace;display:flex;justify-content:space-between;align-items:center;background:rgba(255,255,255,0.03);border:1px solid rgba(255,255,255,0.08)}
.hw-node.ok{border-color:var(--green);color:var(--green);background:rgba(6,214,160,0.06)}
.hw-node.err{border-color:var(--red);color:var(--red);background:rgba(255,71,87,0.06)}
.gear-btn.act{background:var(--cyan);color:#040714;border-color:var(--cyan)}
</style>
</head>
<body>

<!-- Header Bar -->
<div class="hdr">
  <div class="hdr-left">
    <div class="pulse"></div>
    <div>
      <div class="title">USV AQUA-TACTICAL COMMAND</div>
      <div style="font-size:0.72rem;color:var(--sub)" id="header_status">Quan trắc nước đa tầng & Tự hành hải trình 3D</div>
    </div>
  </div>
  <div class="badges">
    <span class="badge green" id="badge_lora">📡 LORA: ON</span>
    <span class="badge cyan" id="badge_wifi">📶 STA: OFF</span>
    <span class="badge amber" id="badge_bat">🔋 PIN: --</span>
    <span class="badge" id="badge_gps">🛰️ GPS: 0 SV</span>
    <button id="btnAudio" onclick="toggleAudio()" style="padding:4px 9px;font-size:0.72rem">🔊 BẬT ÂM</button>
  </div>
</div>

<!-- Navigation Tabs -->
<div class="tabs">
  <div class="tab-btn active" onclick="switchTab('cockpit')" id="tab_cockpit">🕹️ BUỒNG LÁI COCKPIT</div>
  <div class="tab-btn" onclick="switchTab('map3d')" id="tab_map3d">🌐 BẢN ĐỒ 3D WEBGL</div>
  <div class="tab-btn" onclick="switchTab('config')" id="tab_config">⚙️ ĐỒNG BỘ & CẤU HÌNH</div>
</div>

<!-- TAB 1: COCKPIT HUD -->
<div id="view_cockpit" class="tab-content active">
  <div class="grid-2">
    <!-- Cột trái: Horizon Gyro & Chỉ số nước -->
    <div>
      <!-- Thẻ Chân trời nhân tạo & Góc nghiêng thân tàu -->
      <div class="card">
        <div class="card-title">
          <span>GÓC NGHIÊNG THÂN TÀU & CHÂN TRỜI (ATTITUDE HUD)</span>
          <span id="txt_gyro" style="font-family:monospace;font-size:0.75rem">Roll: 0.0° | Pitch: 0.0°</span>
        </div>
        <canvas id="horizonCanvas" style="width:100%;height:150px;background:#030712"></canvas>
        <div style="display:flex;justify-content:space-between;margin-top:6px;font-size:0.75rem;color:var(--sub);font-family:monospace">
          <span>La Bàn: <b id="txt_hdg" style="color:var(--cyan)">--°</b></span>
          <span>Vận Tốc: <b id="txt_spd" style="color:#fff">0.0 km/h</b></span>
          <span>Về Home: <b id="txt_homed" style="color:var(--green)">-- m</b></span>
        </div>
      </div>

      <!-- Thẻ 4 Chỉ số chất lượng nước tinh gọn -->
      <div class="card">
        <div class="card-title">
          <span>CHỈ SỐ CHẤT LƯỢNG NƯỚC (WATER MATRIX)</span>
          <span id="qcvn_badge" class="badge green">VN-WQI: ĐẠT CHUẨN</span>
        </div>
        <div class="grid-4">
          <div class="m-pill">
            <div class="m-lbl">Độ pH</div>
            <div class="m-val" id="val_ph" style="color:var(--green)">--</div>
            <div class="m-sub" id="sub_ph">DO: -- mg/L</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Khoáng TDS</div>
            <div class="m-val" id="val_tds" style="color:var(--cyan)">--</div>
            <div class="m-sub" id="sub_ec">EC: -- µS/cm</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Độ đục NTU</div>
            <div class="m-val" id="val_ntu" style="color:var(--amber)">--</div>
            <div class="m-sub" id="sub_ntu">Nước: --</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Nhiệt độ</div>
            <div class="m-val" id="val_temp" style="color:#fff">--°C</div>
            <div class="m-sub" id="sub_temp">S3: --° | GCS: --°</div>
          </div>
        </div>
        <div id="qcvn_banner" style="margin-top:8px;padding:6px 10px;border-radius:6px;background:rgba(6,214,160,0.12);border:1px solid rgba(6,214,160,0.3);font-size:0.75rem;color:var(--green)">
          Đánh giá quy chuẩn QCVN 08:2023/BTNMT Cột A - An toàn
        </div>
      </div>

      <!-- Thẻ Biểu đồ sóng thời gian thực -->
      <div class="card">
        <div class="card-title">
          <span>BIỂU ĐỒ SÓNG TELEMETRY THỜI GIAN THỰC</span>
          <button onclick="dlCsv()" style="padding:3px 8px;font-size:0.72rem;background:var(--green);color:#000">TẢI CSV</button>
        </div>
        <canvas id="chartCanvas" style="width:100%;height:100px;background:#030712"></canvas>
      </div>
    </div>

    <!-- Cột phải: Khảo sát 3 tầng nước, Nguồn & Joystick -->
    <div>
      <!-- Thẻ Cột nước 3 tầng & Tời đo sâu -->
      <div class="card">
        <div class="card-title">
          <span>KHẢO SÁT CHIỀU SÂU 3 TẦNG & TỜI LỒNG ĐO</span>
          <span id="txt_stage" style="color:var(--amber);font-family:monospace;font-size:0.75rem">Tời: 0.00m</span>
        </div>
        <canvas id="depthCanvas" style="width:100%;height:90px;background:#030712"></canvas>
        <div class="btns" style="grid-template-columns:repeat(4,1fr);margin-top:8px">
          <button onclick="sp('inch_dn')" class="btn-blue">HẠ 10CM</button>
          <button onclick="sp('inch_up')" class="btn-blue">THU 10CM</button>
          <button onclick="cmd(0,90,3)" class="btn-green">AUTO 3 TẦNG</button>
          <button onclick="sp('home')" class="btn-amber">VỀ HOME</button>
        </div>
      </div>

      <!-- Thẻ Nguồn Pin 3S & Solar Station -->
      <div class="card">
        <div class="card-title">
          <span>NGUỒN NĂNG LƯỢNG PIN 3S & QUANG ĐIỆN SOLAR</span>
          <span id="txt_power_mode" style="font-size:0.75rem">Sẵn Sàng</span>
        </div>
        <div class="grid-4">
          <div class="m-pill">
            <div class="m-lbl">Điện áp Pin</div>
            <div class="m-val" id="val_bat" style="color:var(--green)">-- V</div>
            <div class="m-sub" id="sub_bat">Dung lượng: --%</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Dòng tải</div>
            <div class="m-val" id="val_amp" style="color:var(--amber)">-- A</div>
            <div class="m-sub">ACS712 Motor</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Công suất</div>
            <div class="m-val" id="val_watt" style="color:#fff">-- W</div>
            <div class="m-sub" id="sub_wh">Năng lượng: -- Wh</div>
          </div>
          <div class="m-pill">
            <div class="m-lbl">Solar PV</div>
            <div class="m-val" id="val_sol" style="color:var(--cyan)">-- W</div>
            <div class="m-sub" id="sub_sol">Cân bằng: --</div>
          </div>
        </div>
      </div>

      <!-- Thẻ Bàn Điều Khiển Joystick & Lái -->
      <div class="card">
        <div class="card-title">
          <span>BUỒNG LÁI ĐIỀU HƯỚNG TỰ HÀNH & JOYSTICK</span>
          <span id="joy_src" style="font-size:0.75rem">Nguồn: Chờ lệnh</span>
        </div>
        <div style="display:flex;justify-content:center;margin:6px 0">
          <canvas id="joyCanvas" width="180" height="180" style="touch-action:none;cursor:crosshair;background:#030712;border:1px solid rgba(0,245,212,0.3)"></canvas>
        </div>
        <div style="text-align:center;font-size:0.75rem;color:var(--sub);margin-bottom:6px;font-family:monospace" id="joy_text">
          Chạm hoặc kéo chuột để lái thuyền USV
        </div>
        <div class="btns" style="grid-template-columns:repeat(3,1fr)">
          <button onclick="setGear(0)" id="g0" class="gear-btn">ECO 60%</button>
          <button onclick="setGear(1)" id="g1" class="gear-btn act">CRUISE 82%</button>
          <button onclick="setGear(2)" id="g2" class="gear-btn">TURBO 100%</button>
        </div>
        <div class="btns" style="grid-template-columns:repeat(3,1fr);margin-top:6px">
          <button onclick="sp('anchor_on')" class="btn-cyan">NEO ẢO (DP)</button>
          <button onclick="sp('anchor_off')">TẮT NEO</button>
          <button onclick="sp('stopall')" class="btn-red">DỪNG KHẨN CẤP</button>
        </div>
      </div>
    </div>
  </div>

  <!-- Thẻ Ma trận 11 Linh kiện phần cứng thực tế -->
  <div class="card">
    <div class="card-title">
      <span>CHẨN ĐOÁN SỨC KHỎE 11 LINH KIỆN PHẦN CỨNG THỰC TẾ (KHÔNG ẢO)</span>
      <span id="hw_summary" style="color:var(--green)">11/11 OK</span>
    </div>
    <div class="hw-grid" id="hw_nodes"></div>
  </div>
</div>

<!-- TAB 2: BẢN ĐỒ 3D WEBGL -->
<div id="view_map3d" class="tab-content">
  <div class="card" style="padding:6px">
    <div class="card-title" style="padding:6px 8px">
      <span>BẢN ĐỒ 3D WEBGL KHÔNG GIAN HỒ NƯỚC & PHAO USV THỜI GIAN THỰC</span>
      <button onclick="window.open('/3d','_blank')" class="btn-cyan" style="padding:4px 10px;font-size:0.75rem">⛶ MỞ TOÀN MÀN HÌNH</button>
    </div>
    <iframe id="frame3D" src="/3d" style="width:100%;height:680px;border:none;border-radius:8px;background:#030712"></iframe>
  </div>
</div>

<!-- TAB 3: ĐỒNG BỘ & CẤU HÌNH -->
<div id="view_config" class="tab-content">
  <div class="grid-2">
    <!-- Cài đặt Cloud Google Sheets & Discord -->
    <div class="card">
      <div class="card-title">ĐỒNG BỘ ĐÁM MÂY GOOGLE SHEETS & DISCORD</div>
      <div style="font-size:0.75rem;color:var(--sub);margin-bottom:6px;font-family:monospace" id="cloud_info">Trạng thái: Đang kết nối...</div>

      <div style="margin-top:8px">
        <label style="font-size:0.75rem;color:var(--cyan);font-weight:700">Google Apps Script Web App URL</label>
        <input id="cfg_url" placeholder="https://script.google.com/macros/s/.../exec">
        <div class="btns" style="grid-template-columns:repeat(3,1fr);margin-top:6px">
          <button onclick="spPush('tele')" class="btn-green">PUSH TELE</button>
          <button onclick="spPush('profile')" class="btn-blue">PUSH 3 TẦNG</button>
          <button onclick="sp('sync')" class="btn-amber">SYNC XUỐNG PHAO</button>
        </div>
      </div>

      <div style="margin-top:12px">
        <label style="font-size:0.75rem;color:var(--cyan);font-weight:700">Discord Webhook URL (Báo cáo ảnh 3D & Cảnh báo)</label>
        <input id="cfg_discord" placeholder="https://discord.com/api/webhooks/...">
        <div class="btns" style="grid-template-columns:repeat(2,1fr);margin-top:6px">
          <button onclick="testDiscord()" class="btn-cyan">GỬI TEST DISCORD</button>
          <button onclick="saveCfg()" class="btn-green">LƯU CẤU HÌNH CLOUD</button>
        </div>
      </div>
    </div>

    <!-- Cài đặt Wi-Fi & Hiệu chuẩn -->
    <div class="card">
      <div class="card-title">CẤU HÌNH WI-FI & HIỆU CHUẨN PHẦN CỨNG</div>
      <div>
        <label style="font-size:0.75rem;color:var(--cyan);font-weight:700">Tên Wi-Fi Trạm Bờ kết nối (SSID)</label>
        <input id="cfg_ssid" placeholder="VD: NhaToi_2.4G">
        <label style="font-size:0.75rem;color:var(--cyan);font-weight:700;margin-top:6px;display:block">Mật khẩu Wi-Fi</label>
        <input id="cfg_pass" type="password" placeholder="Mật khẩu Wi-Fi">
        <button onclick="saveCfg()" class="btn-blue" style="width:100%;margin-top:6px">LƯU KẾT NỐI WI-FI</button>
      </div>

      <div style="margin-top:12px;padding-top:10px;border-top:1px solid rgba(255,255,255,0.1)">
        <div style="font-size:0.75rem;color:var(--cyan);font-weight:700;margin-bottom:6px">Hiệu chuẩn Cảm biến & Tay cầm</div>
        <div style="display:flex;gap:6px">
          <input id="vbat_cal" placeholder="V Von thuc te (VD: 12.60)" style="margin-top:0">
          <button onclick="calBat()" class="btn-amber" style="white-space:nowrap">LƯU PIN</button>
        </div>
        <div class="btns" style="grid-template-columns:repeat(2,1fr);margin-top:6px">
          <button onclick="calJoy()">LƯU TÂM JOYSTICK</button>
          <button onclick="sp('cal_imu')">RESET LA BÀN IMU</button>
        </div>
      </div>

      <div class="btns" style="grid-template-columns:repeat(2,1fr);margin-top:12px">
        <button onclick="if(confirm('Khởi động lại Trạm Bờ?'))location.href='/api/reboot'">REBOOT GCS</button>
        <button onclick="if(confirm('Xóa sạch cấu hình về mặc định?'))location.href='/api/factory_reset'" class="btn-red">RESET GỐC</button>
      </div>
    </div>
  </div>
</div>

<script>
// --- Tab Switching ---
function switchTab(t){
  ['cockpit','map3d','config'].forEach(k=>{
    document.getElementById('view_'+k).className='tab-content'+(k===t?' active':'');
    document.getElementById('tab_'+k).className='tab-btn'+(k===t?' active':'');
  });
  if(t==='map3d'){
    const f=document.getElementById('frame3D');
    if(f&&!f.src)f.src='/3d';
  }
}

// --- Audio Alerts ---
let audioCtx=null,soundEnabled=false,lastBeepMs=0;
function toggleAudio(){
  soundEnabled=!soundEnabled;
  const btn=document.getElementById('btnAudio');
  if(soundEnabled){
    if(!audioCtx) audioCtx=new (window.AudioContext||window.webkitAudioContext)();
    btn.style.background='var(--green)';btn.innerText='🔊 ÂM: BẬT';
    playTone(880,0.12);
  }else{
    btn.style.background='rgba(255,255,255,0.06)';btn.innerText='🔇 ÂM: TẮT';
  }
}
function playTone(freq,dur){
  if(!soundEnabled||!audioCtx)return;
  try{
    const o=audioCtx.createOscillator(),g=audioCtx.createGain();
    o.type='sine';o.frequency.value=freq;
    g.gain.setValueAtTime(0.08,audioCtx.currentTime);
    g.gain.exponentialRampToValueAtTime(0.001,audioCtx.currentTime+dur);
    o.connect(g);g.connect(audioCtx.destination);
    o.start();o.stop(audioCtx.currentTime+dur);
  }catch(e){}
}

// --- Commands ---
let gearScale=0.82;
function cmd(m,r,w){fetch(`/api/cmd?m=${m}&r=${r}&w=${w}`).catch(()=>{});}
function sp(a){fetch(`/api/cmd?${a}=1`).catch(()=>{});}
function spPush(p){fetch(`/api/cmd?push=${p}`).catch(()=>{});}
function setGear(g){
  fetch(`/api/cmd?gear=${g}`).catch(()=>{});
  gearScale=(g===0)?0.60:((g===2)?1.0:0.82);
  [0,1,2].forEach(i=>document.getElementById('g'+i).className='gear-btn'+(i===g?' act':''));
}
function calJoy(){fetch('/api/cmd?caljoy=1');alert('Đã lưu tâm Joystick!');}
function calBat(){
  const v=parseFloat(document.getElementById('vbat_cal').value);
  if(v>=3.0&&v<=18.0){fetch(`/api/cmd?cbat=${v.toFixed(2)}`);alert('Đã gửi điện áp hiệu chuẩn: '+v.toFixed(2)+'V');}
}

// --- Configuration ---
async function loadCfg(){
  try{
    const r=await fetch('/api/config');const d=await r.json();
    document.getElementById('cfg_url').value=d.url||'';
    document.getElementById('cfg_discord').value=d.discord||'';
    document.getElementById('cfg_ssid').value=d.ssid||'';
    document.getElementById('cfg_pass').value='';
  }catch(e){}
}
async function saveCfg(){
  const body=new URLSearchParams();
  body.set('url',document.getElementById('cfg_url').value.trim());
  body.set('discord',document.getElementById('cfg_discord').value.trim());
  body.set('ssid',document.getElementById('cfg_ssid').value.trim());
  const p=document.getElementById('cfg_pass').value;
  if(p)body.set('pass',p);
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    const d=await r.json();
    alert(d.ok?'Đã lưu cấu hình Google Sheets & Discord!':'Lỗi: '+(d.err||r.status));
    loadCfg();
  }catch(e){alert('Không thể kết nối đến Trạm Bờ!');}
}
async function testDiscord(){
  try{
    const r=await fetch('/api/discord_test');const d=await r.json();
    alert(d.ok?'Đã gửi thông báo Discord thành công! Vui lòng kiểm tra kênh chat.':'Lỗi gửi Discord: '+(d.err||'Kiểm tra kết nối Wi-Fi'));
  }catch(e){alert('Không thể gọi API Trạm Bờ!');}
}
loadCfg();

// --- DPR-Aware Dynamic Canvas Helper ---
function getDprCtx(canvas){
  const dpr=window.devicePixelRatio||1;
  const rect=canvas.getBoundingClientRect();
  const reqW=Math.round(rect.width*dpr), reqH=Math.round(rect.height*dpr);
  if(canvas.width!==reqW||canvas.height!==reqH){
    canvas.width=reqW;
    canvas.height=reqH;
  }
  const ctx=canvas.getContext('2d');
  ctx.save();
  ctx.scale(dpr,dpr);
  return {ctx, w:rect.width, h:rect.height};
}
function constrain(v,min,max){return Math.max(min,Math.min(max,v));}

// --- Canvas 1: Horizon & Attitude Gyro ---
const hzC=document.getElementById('horizonCanvas');
let curRoll=0, curPitch=0, curHdg=0;
function drawHorizon(roll,pitch,hdg){
  curRoll=roll; curPitch=pitch; curHdg=hdg;
  const {ctx, w, h}=getDprCtx(hzC);
  const cx=w/2, cy=h/2;
  ctx.clearRect(0,0,w,h);
  ctx.save();
  ctx.translate(cx,cy);
  ctx.rotate(-roll*Math.PI/180);
  const pY=constrain(pitch*1.2,-h*0.4,h*0.4);

  // Bầu trời & Biển
  ctx.fillStyle='#0f2b48';
  ctx.fillRect(-w,-h-pY,w*2,h);
  ctx.fillStyle='#041b2f';
  ctx.fillRect(-w,-pY,w*2,h*2);

  // Đường chân trời neon cyan
  ctx.strokeStyle='#00f5d4';ctx.lineWidth=2;
  ctx.beginPath();ctx.moveTo(-w,-pY);ctx.lineTo(w,-pY);ctx.stroke();

  // Thang đo Pitch (-20, -10, 10, 20)
  ctx.strokeStyle='rgba(255,255,255,0.4)';ctx.lineWidth=1;
  [-20,-10,10,20].forEach(d=>{
    const y=-pY-(d*1.2);
    ctx.beginPath();ctx.moveTo(-18,y);ctx.lineTo(18,y);ctx.stroke();
  });
  ctx.restore();

  // Silhouette Thuyền Catamaran ở tâm
  ctx.strokeStyle='#ffd166';ctx.lineWidth=2.5;
  ctx.strokeRect(cx-26,cy-4,14,8); // Phao trái
  ctx.strokeRect(cx+12,cy-4,14,8); // Phao phải
  ctx.strokeStyle='#fff';ctx.lineWidth=2;
  ctx.beginPath();ctx.moveTo(cx-12,cy);ctx.lineTo(cx+12,cy);ctx.stroke();
  ctx.fillStyle='#00f5d4';ctx.beginPath();ctx.arc(cx,cy,3,0,Math.PI*2);ctx.fill();
  ctx.restore();
}

// --- Canvas 2: Water Depth Column & Winch Probe ---
const dpC=document.getElementById('depthCanvas');
let curDep=0, curStage=0;
function drawDepth(depthM,stage){
  curDep=depthM; curStage=stage;
  const {ctx, w, h}=getDprCtx(dpC);
  ctx.clearRect(0,0,w,h);

  // Gradient 3 tầng nước nằm ngang
  const g=ctx.createLinearGradient(10,0,w-20,0);
  g.addColorStop(0,'#00f5d4');   // Mặt nước
  g.addColorStop(0.45,'#00bbf9'); // Giữa hồ
  g.addColorStop(1,'#0c1b33');   // Đáy hồ
  ctx.fillStyle=g;
  ctx.fillRect(10,22,w-20,26);
  ctx.strokeStyle='rgba(255,255,255,0.2)';ctx.lineWidth=1;
  ctx.strokeRect(10,22,w-20,26);

  // Thang mốc: 0.0m, 0.3m, 1.0m, 1.8m, 2.0m
  ctx.fillStyle='rgba(255,255,255,0.7)';ctx.font='10px monospace';
  const getX=d=>10+constrain(d/2.0,0,1)*(w-20);
  [{d:0.0,l:'0m'},{d:0.3,l:'MẶT 0.3m'},{d:1.0,l:'GIỮA 1.0m'},{d:1.8,l:'ĐÁY 1.8m'},{d:2.0,l:'2m'}].forEach(m=>{
    const x=getX(m.d);
    ctx.fillRect(x,18,1.5,34);
    ctx.fillText(m.l,x-12,14);
  });

  // Vị trí quả rọi lồng cảm biến thực tế
  const probeX=getX(depthM);
  ctx.fillStyle='#ff4757';
  ctx.beginPath();ctx.arc(probeX,35,8,0,Math.PI*2);ctx.fill();
  ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.stroke();
  ctx.fillStyle='#fff';ctx.font='bold 11px monospace';
  ctx.fillText(`Lồng: ${depthM.toFixed(2)}m`,Math.max(12,probeX-30),72);
  ctx.restore();
}

// --- Canvas 3: Virtual Touch Joystick ---
const jp=document.getElementById('joyCanvas'),jctx=jp.getContext('2d');
let jDrag=false,jx=0,jy=0,lastJoySend=0;
function drawJoyPad(nx,ny){
  const cx=jp.width/2,cy=jp.height/2,rMax=jp.width/2-18;
  jctx.clearRect(0,0,jp.width,jp.height);
  jctx.strokeStyle='rgba(0,245,212,0.25)';jctx.lineWidth=1.5;
  jctx.beginPath();jctx.arc(cx,cy,rMax*0.5,0,Math.PI*2);jctx.stroke();
  jctx.beginPath();jctx.arc(cx,cy,rMax,0,Math.PI*2);jctx.stroke();
  jctx.beginPath();jctx.moveTo(cx-rMax,cy);jctx.lineTo(cx+rMax,cy);
  jctx.moveTo(cx,cy-rMax);jctx.lineTo(cx,cy+rMax);jctx.stroke();
  const kx=cx+nx*rMax,ky=cy-ny*rMax;
  jctx.strokeStyle='rgba(0,245,212,0.7)';jctx.lineWidth=2.5;
  jctx.beginPath();jctx.moveTo(cx,cy);jctx.lineTo(kx,ky);jctx.stroke();
  jctx.fillStyle='var(--green)';jctx.beginPath();jctx.arc(kx,ky,18,0,Math.PI*2);jctx.fill();
  jctx.strokeStyle='#fff';jctx.lineWidth=2;jctx.stroke();
}
drawJoyPad(0,0);

function sendJoyVec(nx,ny,force){
  const now=Date.now();
  if(!force && now-lastJoySend<90)return;
  lastJoySend=now;
  const m=Math.round(ny*255*gearScale),r=Math.round(90+nx*45);
  document.getElementById('joy_text').innerText=`L298N PWM: ${m} | Bánh lái: ${r}° | Cấp số: ${Math.round(gearScale*100)}%`;
  fetch(`/api/cmd?m=${m}&r=${r}&w=0`).catch(()=>{});
}
function handleJoyMove(clientX,clientY){
  const rect=jp.getBoundingClientRect(),cx=rect.width/2,cy=rect.height/2,rMax=rect.width/2-18;
  let dx=clientX-rect.left-cx,dy=cy-(clientY-rect.top);
  const dist=Math.hypot(dx,dy);
  if(dist>rMax){dx=(dx/dist)*rMax;dy=(dy/dist)*rMax;}
  jx=dx/rMax;jy=dy/rMax;
  drawJoyPad(jx,jy);sendJoyVec(jx,jy,false);
}
function resetJoy(){if(!jDrag)return;jDrag=false;jx=0;jy=0;drawJoyPad(0,0);sendJoyVec(0,0,true);}
jp.addEventListener('pointerdown',e=>{jDrag=true;try{jp.setPointerCapture(e.pointerId);}catch(err){}handleJoyMove(e.clientX,e.clientY);});
jp.addEventListener('pointermove',e=>{if(jDrag)handleJoyMove(e.clientX,e.clientY);});
jp.addEventListener('pointerup',resetJoy);
jp.addEventListener('pointercancel',resetJoy);

// --- Canvas 4: Real-time Telemetry Sparkline ---
const hist=[];
const chC=document.getElementById('chartCanvas');
function drawChart(){
  const {ctx, w, h}=getDprCtx(chC);
  ctx.clearRect(0,0,w,h);
  if(hist.length<2)return;
  const plot=(arr,min,max,col)=>{
    ctx.beginPath();ctx.strokeStyle=col;ctx.lineWidth=2;
    arr.forEach((v,i)=>{
      const x=(i/(arr.length-1))*(w-16)+8;
      const y=h-8-constrain((v-min)/(max-min),0,1)*(h-16);
      if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
    });
    ctx.stroke();
  };
  plot(hist.map(x=>x.ph),0,14,'#06d6a0');
  plot(hist.map(x=>x.temp),0,45,'#00bbf9');
  plot(hist.map(x=>x.bat),9,14,'#ffd166');
  ctx.restore();
}
function dlCsv(){
  let csv='ThoiGian,Seq,pH,DO_mgL,TDS_ppm,EC_uScm,NTU,NhietNuoc_C,Pin_V,Dong_A,CongSuat_W,Roll,Pitch\n';
  hist.forEach(d=>{csv+=`${d.ts},${d.seq},${d.ph},${d.do},${d.tds},${d.ec},${d.ntu},${d.temp},${d.bat},${d.amp},${d.watt},${d.roll},${d.pitch}\n`;});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv;charset=utf-8;'}));
  a.download='USV_Aqua_Telemetry.csv';a.click();
}

// Window resize handler
window.addEventListener('resize',()=>{
  drawHorizon(curRoll,curPitch,curHdg);
  drawDepth(curDep,curStage);
  drawChart();
});

// Hardware component definitions
const hwDefs=[
  {b:1,n:'Nhiệt DS18B20'},{b:2,n:'Cảm biến pH'},{b:4,n:'Cảm biến TDS'},
  {b:8,n:'Độ đục NTU'},{b:16,n:'Dòng ACS712'},{b:32,n:'Đo Pin 3S'},
  {b:64,n:'IMU MPU6050'},{b:128,n:'La bàn GY271'},{b:256,n:'LoRa SX1278'},
  {b:512,n:'SIM 4G A7680'},{b:1024,n:'GPS NEO-8M'}
];

// Telemetry Polling Loop (Mỗi 600ms)
setInterval(async()=>{
  try{
    const r=await fetch('/api/telemetry');const d=await r.json();
    d.ts=new Date().toLocaleTimeString();hist.push(d);if(hist.length>50)hist.shift();
    const m=d.hwm||0;

    // Badges & Header
    document.getElementById('badge_lora').innerText=`📡 LORA: ${d.rssi}dBm`;
    document.getElementById('badge_wifi').innerText=d.sta?`📶 STA: ${d.ip}`:'📶 AP 192.168.4.1';
    document.getElementById('badge_bat').innerText=`🔋 ${d.bat.toFixed(1)}V (${d.batpct}%)`;
    document.getElementById('badge_gps').innerText=`🛰️ GPS: ${d.sats||0} SV`;
    document.getElementById('header_status').innerText=d.online?`LIVE LoRa #${d.seq} (${d.src}) | Uptime: ${Math.round(d.uptime/60)}ph`:'MẤT KẾT NỐI PHAO!';

    // Attitude Gyro
    drawHorizon(d.roll||0,d.pitch||0,d.hdg||0);
    document.getElementById('txt_gyro').innerText=`Roll: ${(d.roll||0).toFixed(1)}° | Pitch: ${(d.pitch||0).toFixed(1)}°`;
    document.getElementById('txt_hdg').innerText=`${(d.hdg||0).toFixed(0)}°`;
    document.getElementById('txt_spd').innerText=`${(d.spd||0).toFixed(1)} km/h`;
    document.getElementById('txt_homed').innerText=`${(d.homed||0).toFixed(1)} m`;

    // Water Column
    drawDepth(d.dep||0,d.stage||0);
    document.getElementById('txt_stage').innerText=`Độ sâu: ${(d.dep||0).toFixed(2)}m`;

    // Water Matrix
    document.getElementById('val_ph').innerText=(m&2)?d.ph.toFixed(2):'CHƯA CẮM';
    document.getElementById('sub_ph').innerText=(m&2)?`DO: ${d.do.toFixed(2)} mg/L`:'GPIO 7';
    document.getElementById('val_tds').innerText=(m&4)?d.tds.toFixed(0)+' ppm':'CHƯA CẮM';
    document.getElementById('sub_ec').innerText=(m&4)?`EC: ${d.ec.toFixed(0)} µS/cm`:'GPIO 4';
    document.getElementById('val_ntu').innerText=(m&8)?d.ntu.toFixed(0)+' NTU':'CHƯA CẮM';
    document.getElementById('sub_ntu').innerText=(m&8)?(d.ntu<=50?'Nước trong':'Nước đục'):'GPIO 5';
    document.getElementById('val_temp').innerText=(m&1)?d.temp.toFixed(1)+'°C':'CHƯA CẮM';
    document.getElementById('sub_temp').innerText=`S3: ${(d.s3t||0).toFixed(0)}°C | GCS: ${(d.gcst||0).toFixed(0)}°C`;

    // QCVN 08 Classification
    let qtxt='QCVN 08:2023/BTNMT Cột A - Rất Tốt (Nước sinh hoạt)';
    let qcol='var(--green)';
    if(d.wqi>=75){qtxt='MỨC A: RẤT TỐT (WQI: '+d.wqi+') - Đạt quy chuẩn QCVN 08';}
    else if(d.wqi>=50){qtxt='MỨC B: TRUNG BÌNH (WQI: '+d.wqi+') - Bảo tồn thủy sinh';qcol='var(--amber)';}
    else if(d.wqi>0){qtxt='MỨC NGUY HẠI (WQI: '+d.wqi+') - Ô nhiễm vượt chuẩn!';qcol='var(--red)';}
    document.getElementById('qcvn_badge').innerText=`VN-WQI: ${d.wqi}/100`;
    document.getElementById('qcvn_banner').innerText=qtxt;
    document.getElementById('qcvn_banner').style.color=qcol;

    // Power & Battery
    document.getElementById('val_bat').innerText=(m&32)?d.bat.toFixed(2)+' V':'--';
    document.getElementById('sub_bat').innerText=(m&32)?`Dung lượng: ${d.batpct}%`:'GPIO 3';
    document.getElementById('val_amp').innerText=(m&16)?d.amp.toFixed(2)+' A':'--';
    document.getElementById('val_watt').innerText=(m&16&&m&32)?d.watt.toFixed(1)+' W':'--';
    const solW=(m&32&&d.bat>11.5)?Math.min(25.0,(d.bat-11.2)*2.8):0.0;
    document.getElementById('val_sol').innerText=solW.toFixed(1)+' W';
    document.getElementById('sub_sol').innerText=`Cân bằng: ${(solW-d.watt).toFixed(1)}W`;

    // Hardware Grid
    let okCnt=0,hwHtml='';
    hwDefs.forEach(h=>{
      const ok=(m&h.b)!==0;if(ok)okCnt++;
      hwHtml+=`<div class="hw-node ${ok?'ok':'err'}"><span>${h.n}</span><span>${ok?'OK':'LỖI'}</span></div>`;
    });
    document.getElementById('hw_nodes').innerHTML=hwHtml;
    document.getElementById('hw_summary').innerText=`${okCnt}/11 LINH KIỆN HOẠT ĐỘNG`;

    // Cloud Info
    document.getElementById('cloud_info').innerText=
      `Google Sheets: HTTP ${d.gsstat} | Đã gửi: ${d.gspush} | Lỗi: ${d.gsfail} | Heap: ${Math.round(d.heap/1024)}KB`;

    drawChart();
  }catch(e){}
},600);
</script>
</body>
</html>
'''

with open('src/ground_station/main.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

s_marker = 'const char WEB_DASHBOARD_HTML[] PROGMEM = R"rawliteral('
e_marker = ')rawliteral";'

p1 = text.find(s_marker)
if p1 != -1:
    p1 += len(s_marker)
    p2 = text.find(e_marker, p1)
    if p2 != -1:
        text = text[:p1] + new_web_dashboard + text[p2:]
        with open('src/ground_station/main.cpp', 'w', encoding='utf-8') as out:
            out.write(text)
        print("Updated WEB_DASHBOARD_HTML successfully!")
    else:
        print("End marker not found")
else:
    print("Start marker not found")
