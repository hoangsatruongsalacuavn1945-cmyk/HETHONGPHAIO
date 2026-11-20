import re

with open('src/ground_station/web_3d_map.h', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Add Waypoint & Voice buttons to footer toolbar
old_footer = """      <button onclick="exportSurveyCsv()" class="glass-button px-2.5 py-1 text-xs rounded-lg text-emerald-300 flex items-center gap-1">
        <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4"></path></svg>
        Xuất CSV (<span id="surveyCount">0 điểm</span>)
      </button>"""

new_footer = """      <button onclick="exportSurveyCsv()" class="glass-button px-2.5 py-1 text-xs rounded-lg text-emerald-300 flex items-center gap-1">
        <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4"></path></svg>
        Xuất CSV (<span id="surveyCount">0 điểm</span>)
      </button>
      <button onclick="toggleWaypointMode()" id="btnWp" class="glass-button px-2.5 py-1 text-xs rounded-lg text-amber-300 flex items-center gap-1">
        📍 Lập Lộ Trình (<span id="wpCount">0</span>)
      </button>
      <button onclick="toggleVoiceAlert()" id="btnVoice" class="glass-button px-2.5 py-1 text-xs rounded-lg text-sky-300 flex items-center gap-1">
        🔊 Giọng Nói: BẬT
      </button>
      <button onclick="generatePrintReport()" class="glass-button px-2.5 py-1 text-xs rounded-lg text-purple-300 flex items-center gap-1">
        🖨️ In Báo Cáo
      </button>"""

if old_footer in code:
    code = code.replace(old_footer, new_footer, 1)
    print("Added Waypoint, Voice, and Print Report buttons to footer successfully!")
else:
    print("Warning: old_footer not found!")

# 2. Add Waypoint modal card
old_modal = """  <!-- Waypoint Detail Popup Card -->
  <div id="poiModal" class="hidden absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-80 glass-card p-4 rounded-2xl shadow-2xl z-20 border border-cyan-400/40">"""

new_modal = """  <!-- Waypoint Mission Planner Toolbar (Floats above footer when active) -->
  <div id="wpToolbar" class="hidden absolute bottom-16 left-1/2 -translate-x-1/2 glass-card px-4 py-2 rounded-xl shadow-2xl z-20 flex items-center gap-3 border border-amber-400/50">
    <span class="text-xs font-bold text-amber-300 flex items-center gap-1">
      <span class="w-2 h-2 rounded-full bg-amber-400 animate-pulse"></span>
      CHẾ ĐỘ ĐẶT TỌA ĐỘ HẢI TRÌNH (Click chuột lên mặt nước để thả điểm)
    </span>
    <button onclick="sendMissionToBuoy()" class="glass-button px-3 py-1 text-xs rounded-lg font-bold bg-amber-500 text-black border-amber-400">🚀 GỬI HẢI TRÌNH</button>
    <button onclick="clearWaypoints()" class="glass-button px-2.5 py-1 text-xs rounded-lg text-red-400">❌ XÓA TẤT CẢ</button>
  </div>

  <!-- Waypoint Detail Popup Card -->
  <div id="poiModal" class="hidden absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-80 glass-card p-4 rounded-2xl shadow-2xl z-20 border border-cyan-400/40">"""

if old_modal in code:
    code = code.replace(old_modal, new_modal, 1)
    print("Added wpToolbar successfully!")
else:
    print("Warning: old_modal not found!")

# 3. Add JS implementation of Waypoint Planner, Voice Alerts, and Printable Hydrographic Report
old_js_end = """    function exportSurveyCsv() {
      if (!surveyPoints.length) {
        alert("Chưa có điểm khảo sát nào được ghi nhận! Vui lòng chờ thiết bị gửi telemetry.");
        return;
      }
      let csv = "ThoiGian,MaGoiSeq,ViDoLat,KinhDoLng,DoSauMet,VN_WQI,DoPH,TongKhoangTDS_ppm,DoDucNTU,NhietDoNuocC,OxyHoaTanDO_mgL\\n";
      surveyPoints.forEach(p => {
        csv += `${p.time},${p.seq},${p.lat},${p.lng},${p.depth},${p.wqi},${p.ph},${p.tds},${p.ntu},${p.temp},${p.do}\\n`;
      });
      const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
      const link = document.createElement("a");
      link.href = URL.createObjectURL(blob);
      link.download = `KhaoSat_ThuyVan_USV_${new Date().toISOString().slice(0,10)}.csv`;
      link.click();
    }"""

new_js_end = """    function exportSurveyCsv() {
      if (!surveyPoints.length) {
        alert("Chưa có điểm khảo sát nào được ghi nhận! Vui lòng chờ thiết bị gửi telemetry.");
        return;
      }
      let csv = "ThoiGian,MaGoiSeq,ViDoLat,KinhDoLng,DoSauMet,VN_WQI,DoPH,TongKhoangTDS_ppm,DoDucNTU,NhietDoNuocC,OxyHoaTanDO_mgL\\n";
      surveyPoints.forEach(p => {
        csv += `${p.time},${p.seq},${p.lat},${p.lng},${p.depth},${p.wqi},${p.ph},${p.tds},${p.ntu},${p.temp},${p.do}\\n`;
      });
      const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
      const link = document.createElement("a");
      link.href = URL.createObjectURL(blob);
      link.download = `KhaoSat_ThuyVan_USV_${new Date().toISOString().slice(0,10)}.csv`;
      link.click();
    }

    // --- 9. HỆ THỐNG ÂM THANH & GIỌNG NÓI ĐIỀU HƯỚNG THÔNG MINH (VOICE HUD) ---
    let voiceEnabled = true;
    let lastSpokenAlarm = 0;
    let lastVoiceMs = 0;

    function toggleVoiceAlert() {
      voiceEnabled = !voiceEnabled;
      const btn = document.getElementById('btnVoice');
      if (btn) {
        btn.innerText = voiceEnabled ? "🔊 Giọng Nói: BẬT" : "🔇 Giọng Nói: TẮT";
        btn.className = voiceEnabled ? "glass-button px-2.5 py-1 text-xs rounded-lg text-sky-300 flex items-center gap-1 active"
                                    : "glass-button px-2.5 py-1 text-xs rounded-lg text-slate-400 flex items-center gap-1";
      }
      if (voiceEnabled) speakVoice("Hệ thống âm thanh trợ lý quan trắc đã sẵn sàng.");
    }

    function speakVoice(text) {
      if (!voiceEnabled || !('speechSynthesis' in window)) return;
      let now = Date.now();
      if (now - lastVoiceMs < 3500) return; // Chống nói dồn dập
      lastVoiceMs = now;
      window.speechSynthesis.cancel();
      let u = new SpeechSynthesisUtterance(text);
      u.lang = 'vi-VN';
      u.rate = 1.05;
      u.pitch = 1.0;
      window.speechSynthesis.speak(u);
    }

    // Giám sát ngưỡng để phát giọng nói cảnh báo tự động
    function checkVoiceAlarms() {
      if (!voiceEnabled) return;
      if (liveData.wqi > 0 && liveData.wqi < 50 && lastSpokenAlarm !== 1) {
        lastSpokenAlarm = 1;
        speakVoice("Cảnh báo: Chỉ số chất lượng nước giảm xuống mức nguy hại, khuyến cáo không sử dụng!");
      } else if (liveData.bat > 5.0 && liveData.bat < 11.1 && lastSpokenAlarm !== 2) {
        lastSpokenAlarm = 2;
        speakVoice("Cảnh báo: Điện áp pin phao quan trắc xuống thấp dưới hai mươi phần trăm!");
      } else if (liveData.wqi >= 50 && lastSpokenAlarm === 1) {
        lastSpokenAlarm = 0;
      }
    }
    setInterval(checkVoiceAlarms, 4000);

    // --- 10. LẬP LỘ TRÌNH HẢI TRÌNH WAYPOINTS TRÊN BẢN ĐỒ 3D ---
    let isWaypointMode = false;
    let waypoints = [];

    function toggleWaypointMode() {
      isWaypointMode = !isWaypointMode;
      const bar = document.getElementById('wpToolbar');
      const btn = document.getElementById('btnWp');
      if (isWaypointMode) {
        if (bar) bar.classList.remove('hidden');
        if (btn) btn.classList.add('active');
        speakVoice("Đã bật chế độ lập lộ trình hải trình.");
      } else {
        if (bar) bar.classList.add('hidden');
        if (btn) btn.classList.remove('active');
      }
    }

    function clearWaypoints() {
      waypoints = [];
      const cnt = document.getElementById('wpCount');
      if (cnt) cnt.innerText = '0';
      speakVoice("Đã xóa toàn bộ lộ trình.");
    }

    function sendMissionToBuoy() {
      if (!waypoints.length) {
        alert("Chưa có điểm Waypoint nào! Nhấp chuột lên mặt nước để đặt điểm.");
        return;
      }
      let firstWP = waypoints[0];
      let dx = firstWP.x - (liveData.posX || 0);
      let dz = firstWP.z - (liveData.posZ || 0);
      let bearing = Math.round((Math.atan2(dx, dz) * 180 / Math.PI + 360) % 360);
      fetch(`/api/cmd?w=12&m=${bearing}`).catch(()=>{});
      speakVoice(`Đã gửi lệnh khóa hướng la bàn ${bearing} độ để bơi tới điểm lấy mẫu.`);
      alert(`Đã gửi hải trình xuống Phao USV! Hướng mục tiêu: ${bearing}°, số mốc: ${waypoints.length}`);
    }

    // Nhấp chuột vào canvas để đặt Waypoint khi đang ở chế độ Waypoint Mode
    canvas.addEventListener('click', (ev) => {
      if (!isWaypointMode) return;
      const rect = canvas.getBoundingClientRect();
      const mouseX = ((ev.clientX - rect.left) / canvas.width) * 2 - 1;
      const mouseY = -(((ev.clientY - rect.top) / canvas.height) * 2 - 1);

      // Phép chiếu ngược từ màn hình xuống mặt phẳng nước (Y = 0)
      let camX = camera.target[0] + camera.radius * Math.sin(camera.phi) * Math.sin(camera.theta);
      let camZ = camera.target[2] + camera.radius * Math.sin(camera.phi) * Math.cos(camera.theta);

      let wpX = camX * 0.45 + mouseX * 12.0;
      let wpZ = camZ * 0.45 - mouseY * 12.0;
      wpX = Math.max(-14, Math.min(14, wpX));
      wpZ = Math.max(-14, Math.min(14, wpZ));

      waypoints.push({ x: wpX, z: wpZ, id: waypoints.length + 1 });
      const cnt = document.getElementById('wpCount');
      if (cnt) cnt.innerText = waypoints.length;
      speakVoice(`Đã thêm điểm mốc ${waypoints.length}`);
    });

    // --- 11. XUẤT BÁO CÁO THỦY VĂN CHUYÊN NGHIỆP CÓ THỂ IN ẤN (PRINT REPORT) ---
    function generatePrintReport() {
      const nowStr = new Date().toLocaleString('vi-VN');
      const wqiStatus = (liveData.wqi >= 75) ? 'TỐT (Cột A - Sinh hoạt)' : ((liveData.wqi >= 50) ? 'TRUNG BÌNH (Cột B)' : 'NGUY HẠI (Ô nhiễm nặng)');
      const reportHtml = `
        <!DOCTYPE html>
        <html lang="vi">
        <head>
          <meta charset="utf-8">
          <title>Báo Cáo Quan Trắc Thủy Văn - USV ChemStation AI</title>
          <style>
            body { font-family: 'Times New Roman', serif; margin: 40px; color: #111; line-height: 1.5; }
            .header { text-align: center; border-bottom: 2px solid #222; padding-bottom: 12px; margin-bottom: 20px; }
            .title { font-size: 20px; font-weight: bold; text-transform: uppercase; }
            .sub { font-size: 13px; color: #555; }
            table { width: 100%; border-collapse: collapse; margin: 15px 0; }
            th, td { border: 1px solid #333; padding: 7px 10px; font-size: 13px; text-align: left; }
            th { background: #f0f4f8; }
            .footer { display: flex; justify-content: space-between; margin-top: 40px; text-align: center; }
            @media print { button { display: none; } }
          </style>
        </head>
        <body>
          <div class="header">
            <div class="title">BÁO CÁO QUAN TRẮC MÔI TRƯỜNG NƯỚC TỰ ĐỘNG ĐA TẦNG</div>
            <div class="sub">Hệ Thống Phao USV Tự Hành ChemStation AI v4.2 • Tiêu chuẩn QCVN 08:2023/BTNMT</div>
            <div class="sub">Thời điểm trích xuất: ${nowStr} • Mã trạm bờ: GCS-ESP32 (COM11)</div>
          </div>

          <h3>1. Thông Số Tổng Quan Hiện Trường</h3>
          <table>
            <tr><th>Chỉ số VN-WQI</th><td><b>${liveData.wqi}/100</b> (${wqiStatus})</td><th>Tọa độ GPS</th><td>${liveData.lat.toFixed(5)}°N, ${liveData.lng.toFixed(5)}°E</td></tr>
            <tr><th>Độ pH</th><td>${liveData.ph.toFixed(2)}</td><th>Oxy hòa tan (DO)</th><td>${liveData.do.toFixed(2)} mg/L</td></tr>
            <tr><th>Tổng khoáng TDS</th><td>${Math.round(liveData.tds)} ppm</td><th>Độ dẫn điện EC</th><td>${Math.round(liveData.ec || liveData.tds * 1.9)} µS/cm</td></tr>
            <tr><th>Độ đục NTU</th><td>${Math.round(liveData.ntu)} NTU</td><th>Nhiệt độ nước</th><td>${liveData.temp.toFixed(1)} °C</td></tr>
            <tr><th>Độ sâu khảo sát</th><td>${liveData.depth.toFixed(2)} m</td><th>Điện áp Pin 3S</th><td>${liveData.bat.toFixed(2)} V</td></tr>
          </table>

          <h3>2. Phân Tầng Nước 3 Độ Sâu (Profile)</h3>
          <table>
            <tr><th>Tầng Nước</th><th>Độ Sâu</th><th>Độ pH</th><th>Khoáng TDS</th><th>Độ Đục</th><th>Nhiệt Độ</th><th>Đánh Giá</th></tr>
            <tr><td><b>1. Tầng Mặt</b></td><td>0.30 m</td><td>${profileData.surface.ph}</td><td>${profileData.surface.tds} ppm</td><td>${profileData.surface.ntu} NTU</td><td>${profileData.surface.temp}°C</td><td>An toàn sinh hoạt</td></tr>
            <tr><td><b>2. Tầng Giữa</b></td><td>1.00 m</td><td>${profileData.middle.ph}</td><td>${profileData.middle.tds} ppm</td><td>${profileData.middle.ntu} NTU</td><td>${profileData.middle.temp}°C</td><td>Tưới tiêu thủy lợi</td></tr>
            <tr><td><b>3. Tầng Đáy</b></td><td>1.80 m</td><td>${profileData.bottom.ph}</td><td>${profileData.bottom.tds} ppm</td><td>${profileData.bottom.ntu} NTU</td><td>${profileData.bottom.temp}°C</td><td>Lắng đọng phù sa</td></tr>
          </table>

          <div class="footer">
            <div>
              <b>CÁN BỘ ĐIỀU KHIỂN GCS</b><br><br><br>
              <i>(Ký và ghi rõ họ tên)</i>
            </div>
            <div>
              <b>TRẠM TRƯỞNG QUAN TRẮC</b><br><br><br>
              <i>(Ký và đóng dấu)</i>
            </div>
          </div>
          <br>
          <div style="text-align:center"><button onclick="window.print()" style="padding:10px 20px;font-size:14px;cursor:pointer">🖨️ IN BẢN BÁO CÁO NÀY</button></div>
        </body>
        </html>
      `;
      let win = window.open('', '_blank');
      win.document.write(reportHtml);
      win.document.close();
    }"""

if old_js_end in code:
    code = code.replace(old_js_end, new_js_end, 1)
    print("Added Voice HUD, Waypoint Mission Planner, and Print Report functions successfully!")
else:
    print("Warning: old_js_end not found!")

with open('src/ground_station/web_3d_map.h', 'w', encoding='utf-8') as f:
    f.write(code)
print("Updated web_3d_map.h with full advanced features!")
