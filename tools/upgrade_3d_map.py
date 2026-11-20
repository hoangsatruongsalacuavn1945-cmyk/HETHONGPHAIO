#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os
import sys

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

html_path = os.path.join(os.path.dirname(__file__), "..", "water_map_3d.html")
with open(html_path, "r", encoding="utf-8") as f:
    c = f.read()

# 1. Update liveData definition
old_livedata = """    let liveData = {
      seq: 0, ph: 0.0, tds: 0, ec: 0, ntu: 0, temp: 0.0, do: 0.0, wqi: null,
      bat: 0.0, amp: 0.0, depth: 0.0, roll: 0.0, pitch: 0.0, hdg: 0,
      lat: 0.0, lng: 0.0, rssi: 0, snr: 0.0, hwm: 0, adf: 0,
      posX: 0.0, posZ: 0.0, gpsFix: false
    };"""

new_livedata = """    let liveData = {
      seq: 0, ph: 0.0, tds: 0, ec: 0, ntu: 0, temp: 0.0, do: 0.0, wqi: null,
      bat: 0.0, batpct: 0, amp: 0.0, depth: 0.0, roll: 0.0, pitch: 0.0, hdg: 0,
      lat: 0.0, lng: 0.0, rssi: 0, snr: 0.0, hwm: 0, adf: 0, ct1: 0, ct2: 0,
      port: 'AUTO', posX: 0.0, posZ: 0.0, gpsFix: false
    };"""

if old_livedata in c:
    c = c.replace(old_livedata, new_livedata)
    print("[✓] Updated liveData object.")

# 2. Add Limit switch line and AI Co-Pilot / Hardware modal buttons in Right Aside
old_hud_end = """      <button onclick="triggerBuoyAction('stop_all')" class="glass-button text-xs py-1.5 rounded-lg font-semibold text-rose-400 col-span-2">DỪNG KHẨN CẤP TOÀN BỘ</button>
    </div>
  </aside>"""

new_hud_end = """      <button onclick="triggerBuoyAction('stop_all')" class="glass-button text-xs py-1.5 rounded-lg font-semibold text-rose-400 col-span-2">DỪNG KHẨN CẤP TOÀN BỘ</button>
    </div>

    <!-- Breakthrough Features: Chẩn đoán linh kiện thực & AI Co-Pilot Advisor -->
    <div class="pt-2 border-t border-slate-700/60 space-y-2">
      <button onclick="openHardwareModal()" class="w-full glass-button text-xs py-1.5 rounded-lg font-semibold text-cyan-300 flex items-center justify-center gap-1.5">
        🛠️ CHẨN ĐOÁN 11 LINH KIỆN & CÔNG TẮC
      </button>
      <div class="p-2.5 rounded-xl bg-cyan-950/40 border border-cyan-500/40 text-xs">
        <div class="flex items-center justify-between mb-1">
          <span class="font-bold text-cyan-300 flex items-center gap-1">
            <span>✨ AI CO-PILOT</span>
            <span class="text-[9px] px-1 py-0.2 rounded bg-cyan-900 text-cyan-200">QCVN 08</span>
          </span>
          <button onclick="consultAiAdvisor()" class="text-[10px] text-cyan-400 hover:text-white underline cursor-pointer">Tham vấn AI</button>
        </div>
        <div id="aiCoPilotRisk" class="text-[11px] font-bold text-emerald-400 mb-1">Đang giám sát thời gian thực...</div>
        <div id="aiCoPilotDiag" class="text-[10px] text-slate-300 leading-tight">Hệ thống AI đang đối soát các chỉ số với chuẩn QCVN 08:2023/BTNMT...</div>
      </div>
    </div>
  </aside>"""

if old_hud_end in c:
    c = c.replace(old_hud_end, new_hud_end)
    print("[✓] Added AI Co-Pilot & Hardware Diagnostic UI.")

# 3. Add limit switch in telemetry HUD list
old_limit_target = """      <div class="flex justify-between items-center">
        <span class="text-slate-400">Góc nghiêng (MPU):</span>
        <span class="text-slate-300" id="hudImu">R: --° • P: --° • H: --°</span>
      </div>"""

new_limit_target = """      <div class="flex justify-between items-center">
        <span class="text-slate-400">Công tắc hành trình:</span>
        <span class="font-mono text-xs" id="hudLimitSw">CT1 (Bùn): 0 • CT2 (Gốc): 0</span>
      </div>
      <div class="flex justify-between items-center">
        <span class="text-slate-400">Góc nghiêng (MPU):</span>
        <span class="text-slate-300" id="hudImu">R: --° • P: --° • H: --°</span>
      </div>"""

if old_limit_target in c:
    c = c.replace(old_limit_target, new_limit_target)
    print("[✓] Added Limit Switch HUD display.")

# 4. Add Toast Container and Hardware Modal before </body>
modal_and_toast = """
  <!-- Toast Notification Container -->
  <div id="toastContainer" style="position:fixed;top:20px;right:20px;z-index:9999;display:flex;flex-direction:column;gap:10px;pointer-events:none;max-width:380px;"></div>

  <!-- Hardware Health Modal (Bảng Chẩn Đoán Chi Tiết 11 Linh Kiện) -->
  <div id="hwModal" class="hidden fixed inset-0 z-50 flex items-center justify-center bg-black/75 backdrop-blur-sm p-4">
    <div class="glass-card max-w-lg w-full p-5 rounded-2xl border border-cyan-400/50 shadow-2xl space-y-4">
      <div class="flex justify-between items-center border-b border-slate-700/80 pb-2.5">
        <h3 class="text-sm font-bold text-cyan-300 flex items-center gap-2">
          <span>🛠️ BẢNG CHẨN ĐOÁN SỨC KHỎE LINH KIỆN THỰC (ZERO-FAKE)</span>
        </h3>
        <button onclick="closeHardwareModal()" class="text-slate-400 hover:text-white text-lg font-bold">✕</button>
      </div>
      <div class="grid grid-cols-2 gap-2 text-xs font-mono" id="hwModalGrid">
        <!-- 11 items rendered dynamically -->
      </div>
      <div class="p-3 rounded-xl bg-slate-900/70 border border-slate-800 text-xs space-y-1.5 font-mono">
        <div class="flex justify-between"><span>CT1 (Chống chùng bùn đáy):</span><span id="hwCt1Status" class="font-bold text-emerald-400">AN TOÀN (CHƯA CHẠM)</span></div>
        <div class="flex justify-between"><span>CT2 (Gốc thu tời Home 0.0m):</span><span id="hwCt2Status" class="font-bold text-slate-300">HỞ</span></div>
        <div class="flex justify-between"><span>Bão hòa ADC (adcFault):</span><span id="hwAdcStatus" class="font-bold text-emerald-400">BÌNH THƯỜNG (0)</span></div>
      </div>
      <div class="flex justify-between items-center pt-1 border-t border-slate-800">
        <span class="text-[11px] text-slate-400">Dữ liệu bitmask 11 linh kiện chuẩn trực tiếp từ Phao USV</span>
        <button onclick="closeHardwareModal()" class="glass-button px-4 py-1.5 rounded-lg text-xs font-semibold text-cyan-300">Đóng</button>
      </div>
    </div>
  </div>
</body>"""

if "</body>" in c and "id=\"hwModal\"" not in c:
    c = c.replace("</body>", modal_and_toast)
    print("[✓] Added Toast Container and Hardware Modal HTML.")

# 5. Add Toast function, Hardware Modal scripts, AI Co-Pilot scripts, and replace alerts
script_additions = """
    // --- HỆ THỐNG THÔNG BÁO TOAST KHÔNG CHẶN (ZERO-BLOCKING TOAST) ---
    function showToast(message, type = 'info', duration = 3500) {
      const container = document.getElementById('toastContainer');
      if (!container) return;
      const toast = document.createElement('div');
      let borderCol = 'rgba(56,189,248,0.7)';
      let icon = 'ℹ️';
      if (type === 'success') { borderCol = 'rgba(52,211,153,0.85)'; icon = '✅'; }
      else if (type === 'warning') { borderCol = 'rgba(251,191,36,0.85)'; icon = '⚠️'; }
      else if (type === 'error') { borderCol = 'rgba(239,68,68,0.85)'; icon = '🛑'; }

      toast.style.cssText = `pointer-events:auto;background:rgba(15,23,42,0.94);backdrop-filter:blur(16px);border:1px solid ${borderCol};border-left:4px solid ${borderCol};padding:12px 16px;border-radius:12px;color:#f1f5f9;font-size:13px;display:flex;align-items:center;gap:10px;box-shadow:0 10px 25px rgba(0,0,0,0.6);transition:all 0.3s cubic-bezier(0.16,1,0.3,1);transform:translateX(100%);opacity:0;`;
      toast.innerHTML = `<span style="font-size:18px;">${icon}</span><div style="flex:1;line-height:1.4;">${message}</div>`;
      container.appendChild(toast);

      requestAnimationFrame(() => {
        toast.style.transform = 'translateX(0)';
        toast.style.opacity = '1';
      });

      setTimeout(() => {
        toast.style.transform = 'translateX(100%)';
        toast.style.opacity = '0';
        setTimeout(() => toast.remove(), 350);
      }, duration);
    }

    // --- BẢNG CHẨN ĐOÁN LINH KIỆN 11 MÓN ---
    const HW_NAMES_LIST = ["DS18B20", "pH", "TDS", "NTU", "ACS712", "Pin3S", "MPU6050", "GY271", "LoRa", "SIM4G", "GPS"];
    function openHardwareModal() {
      const m = document.getElementById('hwModal');
      if (m) {
        m.classList.remove('hidden');
        updateHardwareModalLive();
      }
    }
    function closeHardwareModal() {
      const m = document.getElementById('hwModal');
      if (m) m.classList.add('hidden');
    }

    function updateHardwareModalLive() {
      const grid = document.getElementById('hwModalGrid');
      if (!grid) return;
      let html = '';
      for (let i = 0; i < HW_NAMES_LIST.length; i++) {
        const isOk = (liveData.hwm & (1 << i)) !== 0;
        const color = isOk ? 'text-emerald-400 border-emerald-700/60 bg-emerald-950/40' : 'text-rose-400 border-red-700/60 bg-red-950/40';
        const status = isOk ? 'KẾT NỐI (OK)' : 'CHƯA CẮM';
        html += `<div class="p-2 rounded-lg border ${color} flex justify-between items-center">
          <span>${HW_NAMES_LIST[i]}</span>
          <span class="font-bold text-[10px]">${status}</span>
        </div>`;
      }
      grid.innerHTML = html;

      const ct1Status = document.getElementById('hwCt1Status');
      if (ct1Status) {
        ct1Status.innerText = liveData.ct1 ? 'CẢNH BÁO: CHẠM BÙN ĐÁY!' : 'AN TOÀN (CHƯA CHẠM)';
        ct1Status.className = liveData.ct1 ? 'font-bold text-rose-400 animate-pulse' : 'font-bold text-emerald-400';
      }
      const ct2Status = document.getElementById('hwCt2Status');
      if (ct2Status) {
        ct2Status.innerText = liveData.ct2 ? 'ĐÃ VỀ GỐC HOME (0.0m)' : 'LỒNG ĐANG THẢ DƯỚI NƯỚC';
        ct2Status.className = liveData.ct2 ? 'font-bold text-amber-300' : 'font-bold text-slate-300';
      }
      const adcStatus = document.getElementById('hwAdcStatus');
      if (adcStatus) {
        adcStatus.innerText = liveData.adf ? `BÃO HÒA ADC (Mã lỗi ${liveData.adf})` : 'BÌNH THƯỜNG (0)';
        adcStatus.className = liveData.adf ? 'font-bold text-rose-400' : 'font-bold text-emerald-400';
      }
    }

    // --- CHEMSTATION AI CO-PILOT ADVISOR ---
    async function consultAiAdvisor() {
      showToast("Đang kết nối Trợ lý AI Phân tích Thủy văn...", "info");
      try {
        const res = await fetch('/api/ai_consult');
        if (res.ok) {
          const aiData = await res.json();
          renderAiAdvice(aiData);
          showToast("AI đã cập nhật chẩn đoán thủy văn mới nhất!", "success");
          if (aiData.diagnostics && aiData.diagnostics.length > 0) {
            speakVoice(aiData.diagnostics[0]);
          }
          return;
        }
      } catch (e) {}
      renderLocalAiAdvice();
    }

    function renderAiAdvice(aiData) {
      const riskEl = document.getElementById('aiCoPilotRisk');
      const diagEl = document.getElementById('aiCoPilotDiag');
      if (riskEl) {
        riskEl.innerText = aiData.risk_level || 'AN TOÀN (A1)';
        riskEl.className = `text-[11px] font-bold mb-1 ${aiData.badge_color === 'red' ? 'text-rose-400 animate-pulse' : (aiData.badge_color === 'amber' ? 'text-amber-300' : 'text-emerald-400')}`;
      }
      if (diagEl) {
        const text = (aiData.diagnostics && aiData.diagnostics.length > 0) ? aiData.diagnostics[0] : (aiData.diagnosis || 'Thông số đạt chuẩn.');
        diagEl.innerText = text;
      }
    }

    function renderLocalAiAdvice() {
      const ph = liveData.ph || 0.0;
      const tds = liveData.tds || 0;
      const ntu = liveData.ntu || 0;
      let risk = "AN TOÀN (MỨC A1)";
      let color = "emerald";
      let diag = "Chất lượng nước đạt tiêu chuẩn QCVN 08:2023/BTNMT cấp nước sinh hoạt.";

      if (liveData.ct1) {
        risk = "CẢNH BÁO BÙN ĐÁY!"; color = "red"; diag = "Lồng tời đã chạm đáy bùn hồ (CT1 kích hoạt). Hãy thu tời về Home!";
      } else if (ph > 0.1 && (ph < 5.5 || ph > 9.0)) {
        risk = "Ô NHIỄM NGUY HIỂM (B2)"; color = "red"; diag = `Độ pH bất thường (${ph.toFixed(2)}): Vượt ngưỡng an toàn sinh thái.`;
      } else if (tds > 1000) {
        risk = "CẢNH BÁO KHOÁNG CAO (B1)"; color = "amber"; diag = `Tổng khoáng TDS cao (${tds} ppm): Nguy cơ xâm nhập mặn.`;
      } else if (ntu > 100) {
        risk = "CẢNH BÁO ĐỘ ĐỤC (B1)"; color = "amber"; diag = `Độ đục cao (${ntu} NTU): Phù sa hoặc dòng xả đáy lơ lửng.`;
      }

      renderAiAdvice({ risk_level: risk, badge_color: color, diagnostics: [diag] });
      showToast("Đã phân tích nhanh theo bộ quy chuẩn QCVN 08:2023", "info");
    }

    // --- RAY-PLANE INTERSECTION FOR EXACT WAYPOINT PLACEMENT ---
    function screenToWaterPlane(clientX, clientY) {
      const rect = canvas.getBoundingClientRect();
      const x = ((clientX - rect.left) / canvas.width) * 2 - 1;
      const y = -(((clientY - rect.top) / canvas.height) * 2 - 1);

      const eyeX = camera.target[0] + camera.radius * Math.sin(camera.phi) * Math.sin(camera.theta);
      const eyeY = camera.target[1] + camera.radius * Math.cos(camera.phi);
      const eyeZ = camera.target[2] + camera.radius * Math.sin(camera.phi) * Math.cos(camera.theta);

      let fX = camera.target[0] - eyeX, fY = camera.target[1] - eyeY, fZ = camera.target[2] - eyeZ;
      let fLen = Math.hypot(fX, fY, fZ) || 1.0;
      fX /= fLen; fY /= fLen; fZ /= fLen;

      let rX = fZ, rY = 0, rZ = -fX;
      let rLen = Math.hypot(rX, rZ) || 1.0;
      rX /= rLen; rZ /= rLen;

      let uX = rY * fZ - rZ * fY, uY = rZ * fX - rX * fZ, uZ = rX * fY - rY * fX;

      const aspect = canvas.width / canvas.height;
      const tanHalfFov = Math.tan((45 * Math.PI / 180) / 2);

      let dirX = fX + rX * (x * aspect * tanHalfFov) + uX * (y * tanHalfFov);
      let dirY = fY + rY * (x * aspect * tanHalfFov) + uY * (y * tanHalfFov);
      let dirZ = fZ + rZ * (x * aspect * tanHalfFov) + uZ * (y * tanHalfFov);
      let dLen = Math.hypot(dirX, dirY, dirZ) || 1.0;
      dirX /= dLen; dirY /= dLen; dirZ /= dLen;

      if (Math.abs(dirY) > 1e-4) {
        let t = -eyeY / dirY;
        if (t > 0) {
          let hitX = eyeX + t * dirX;
          let hitZ = eyeZ + t * dirZ;
          return { x: Math.max(-10, Math.min(10, hitX)), z: Math.max(-10, Math.min(10, hitZ)) };
        }
      }
      return { x: Math.max(-10, Math.min(10, x * 10.0)), z: Math.max(-10, Math.min(10, -y * 10.0)) };
    }
"""

# Insert scripts before </script>
if "</script>" in c and "showToast" not in c:
    c = c.replace("</script>", script_additions + "\n  </script>")
    print("[✓] Added helper scripts before </script>.")

# 6. Update telemetry reception in syncGcsTelemetry()
old_rx_block = """          liveData.seq   = numOr(d.seq,   liveData.seq);
          liveData.ph    = numOr(d.ph,    liveData.ph);
          liveData.tds   = numOr(d.tds,   liveData.tds);
          liveData.ec    = numOr(d.ec,    liveData.ec);
          liveData.ntu   = numOr(d.ntu,   liveData.ntu);
          liveData.temp  = numOr(d.temp,  liveData.temp);
          liveData.do    = numOr(d.do,    liveData.do);
          liveData.wqi   = (typeof d.wqi === 'number' && d.wqi > 0) ? d.wqi : calcWqiJs(liveData.ph, liveData.tds, liveData.ntu, liveData.do);
          liveData.bat   = numOr(d.bat,   liveData.bat);
          liveData.amp   = numOr(d.amp,   liveData.amp);
          liveData.depth = numOr(d.dep,   liveData.depth);
          liveData.roll  = numOr(d.roll,  liveData.roll);
          liveData.pitch = numOr(d.pitch, liveData.pitch);
          liveData.hdg   = numOr(d.hdg,   liveData.hdg);"""

new_rx_block = """          liveData.seq   = numOr(d.seq,   liveData.seq);
          liveData.ph    = numOr(d.ph,    liveData.ph);
          liveData.tds   = numOr(d.tds,   liveData.tds);
          liveData.ec    = numOr(d.ec,    liveData.ec);
          liveData.ntu   = numOr(d.ntu,   liveData.ntu);
          liveData.temp  = numOr(d.temp,  liveData.temp);
          liveData.do    = numOr(d.do,    liveData.do);
          liveData.wqi   = (typeof d.wqi === 'number' && d.wqi > 0) ? d.wqi : calcWqiJs(liveData.ph, liveData.tds, liveData.ntu, liveData.do);
          liveData.bat   = numOr(d.bat,   liveData.bat);
          liveData.batpct= (typeof d.batpct === 'number' && d.batpct >= 0) ? d.batpct : liveData.batpct;
          liveData.amp   = numOr(d.amp,   liveData.amp);
          liveData.depth = numOr(d.dep,   liveData.depth);
          liveData.roll  = numOr(d.roll,  liveData.roll);
          liveData.pitch = numOr(d.pitch, liveData.pitch);
          liveData.hdg   = numOr(d.hdg,   liveData.hdg);
          liveData.port  = d.port || liveData.port || 'AUTO';
          liveData.ct1   = numOr(d.ct1,   0);
          liveData.ct2   = numOr(d.ct2,   0);
          liveData.hwm   = numOr(d.hwm,   0);
          liveData.adf   = numOr(d.adf,   0);"""

if old_rx_block in c:
    c = c.replace(old_rx_block, new_rx_block)
    print("[✓] Updated syncGcsTelemetry liveData fields.")

# 7. Update updateHUD() with Limit Switch & AI Co-Pilot triggers
old_hud_bat = """      const clampedBat  = Math.max(0.0, liveData.bat || 0.0);
      const batPct      = Math.min(100, Math.max(0, Math.round((clampedBat - 9.0) / 3.6 * 100)));"""

new_hud_bat = """      const clampedBat  = Math.max(0.0, liveData.bat || 0.0);
      const batPct      = (typeof liveData.batpct === 'number' && liveData.batpct > 0) ? liveData.batpct : Math.min(100, Math.max(0, Math.round((clampedBat - 9.0) / 3.6 * 100)));"""

if old_hud_bat in c:
    c = c.replace(old_hud_bat, new_hud_bat)
    print("[✓] Updated updateHUD battery formula.")

old_hud_bottom = """      document.getElementById('hudLora').innerText  = `${liveData.rssi} dBm (SNR ${liveData.snr}dB)`;"""
new_hud_bottom = """      document.getElementById('hudLora').innerText  = `${liveData.rssi} dBm (SNR ${liveData.snr}dB)`;
      const limitSwEl = document.getElementById('hudLimitSw');
      if (limitSwEl) {
        limitSwEl.innerHTML = `<span class="${liveData.ct1 ? 'text-rose-400 font-bold animate-pulse' : 'text-slate-400'}">CT1(Bùn): ${liveData.ct1 ? 'CHẠM' : '0'}</span> • <span class="${liveData.ct2 ? 'text-amber-300 font-bold' : 'text-slate-400'}">CT2(Gốc): ${liveData.ct2 ? 'HOME' : '0'}</span>`;
      }
      if (typeof updateHardwareModalLive === 'function') updateHardwareModalLive();"""

if old_hud_bottom in c:
    c = c.replace(old_hud_bottom, new_hud_bottom)
    print("[✓] Updated updateHUD limit switch & hardware modal.")

# 8. Replace blocking alerts with showToast in triggerBuoyAction & sendMission
c = c.replace(
    'alert("Đã gửi lệnh xuống Phao: TỰ ĐỘNG THẢ TỜI ĐO 3 TẦNG NƯỚC (0.3m -> 1.0m -> 1.8m)!");',
    'showToast("Đã gửi lệnh: TỰ ĐỘNG THẢ TỜI ĐO 3 TẦNG NƯỚC (0.3m -> 1.0m -> 1.8m)!", "success");'
)
c = c.replace(
    'alert("Đã gửi lệnh xuống Phao: THU TỜI VỀ GỐC HOME (0.0m)!");',
    'showToast("Đã gửi lệnh: THU TỜI VỀ GỐC HOME (0.0m)!", "warning");'
)
c = c.replace(
    'alert("Đã gửi lệnh xuống Phao: TỰ HÀNH QUAY VỀ HOME GPS!");',
    'showToast("Đã gửi lệnh: TỰ HÀNH QUAY VỀ HOME GPS!", "success");'
)
c = c.replace(
    'alert("Đã gửi lệnh xuống Phao: KÍCH HOẠT CHU TRÌNH SỤC RỬA ĐẦU DÒ TỰ ĐỘNG (30 GIÂY)!");',
    'showToast("Đã gửi lệnh: KÍCH HOẠT CHU TRÌNH SỤC RỬA ĐẦU DÒ (30s)!", "info");'
)
c = c.replace(
    'alert("Đã gửi lệnh xuống Phao: KÍCH HOẠT BƠM LẤY MẪU NƯỚC TỰ ĐỘNG (20 GIÂY)!");',
    'showToast("Đã gửi lệnh: KÍCH HOẠT BƠM LẤY MẪU NƯỚC (20s)!", "success");'
)
c = c.replace(
    'alert("Đã gửi lệnh xuống Phao: DỪNG KHẨN CẤP TOÀN BỘ!");',
    'showToast("ĐÃ GỬI LỆNH DỪNG KHẨN CẤP TOÀN BỘ!", "error");'
)
c = c.replace(
    'alert("Chưa có điểm Waypoint nào! Nhấp chuột lên mặt nước để đặt điểm.");',
    'showToast("Chưa có điểm Waypoint nào! Nhấp chuột lên mặt nước để đặt điểm.", "warning");'
)
c = c.replace(
    'alert(`Đã gửi hải trình xuống Phao USV! Hướng mục tiêu: ${bearing}°, số mốc: ${waypoints.length}`);',
    'showToast(`Đã gửi hải trình xuống Phao USV! Hướng mục tiêu: ${bearing}°, số mốc: ${waypoints.length}`, "success");'
)
c = c.replace(
    'alert("Chưa có lịch sử khảo sát trong gsheets_offline.csv hoặc phiên hiện tại!");',
    'showToast("Chưa có lịch sử khảo sát trong gsheets_offline.csv hoặc phiên hiện tại!", "warning");'
)

# 9. Update Waypoint Click to use screenToWaterPlane()
old_wp_click = """    canvas.addEventListener('click', (ev) => {
      if (!isWaypointMode) return;
      const rect = canvas.getBoundingClientRect();
      const mouseX = ((ev.clientX - rect.left) / canvas.width) * 2 - 1;
      const mouseY = -(((ev.clientY - rect.top) / canvas.height) * 2 - 1);
      let wpX = Math.max(-10, Math.min(10, mouseX * 10.0));
      let wpZ = Math.max(-10, Math.min(10, -mouseY * 10.0));
      waypoints.push({ x: wpX, z: wpZ, id: waypoints.length + 1 });
      const cnt = document.getElementById('wpCount');
      if (cnt) cnt.innerText = waypoints.length;
      speakVoice(`Đã thêm điểm mốc ${waypoints.length}`);
    });"""

new_wp_click = """    canvas.addEventListener('click', (ev) => {
      if (!isWaypointMode) return;
      const pt = screenToWaterPlane(ev.clientX, ev.clientY);
      waypoints.push({ x: pt.x, z: pt.z, id: waypoints.length + 1 });
      const cnt = document.getElementById('wpCount');
      if (cnt) cnt.innerText = waypoints.length;
      speakVoice(`Đã thêm điểm mốc ${waypoints.length}`);
      showToast(`Đã thêm điểm mốc #${waypoints.length} tại toạ độ (${pt.x.toFixed(1)}, ${pt.z.toFixed(1)})`, "info", 2000);
    });"""

if old_wp_click in c:
    c = c.replace(old_wp_click, new_wp_click)
    print("[✓] Updated Waypoint click with exact ray-plane intersection.")

# 10. Replace hardcoded COM12 in report
old_report_port = "Mã trạm bờ: GCS-ESP32 (COM12)"
new_report_port = "Mã trạm bờ: GCS-ESP32 (${liveData.port || 'Tự động'}) • Kiểm định CRC-8 phần cứng"
if old_report_port in c:
    c = c.replace(old_report_port, new_report_port)
    print("[✓] Replaced hardcoded COM12 in Printable Report.")

with open(html_path, "w", encoding="utf-8") as f:
    f.write(c)

# Synchronize test_3d.html and web_3d_map.h
test_3d_path = os.path.join(os.path.dirname(__file__), "..", "test_3d.html")
with open(test_3d_path, "w", encoding="utf-8") as f:
    f.write(c)
print("[✓] Synchronized test_3d.html.")

header_path = os.path.join(os.path.dirname(__file__), "..", "src", "ground_station", "web_3d_map.h")
if os.path.exists(header_path):
    with open(header_path, "w", encoding="utf-8") as f:
        f.write('#pragma once\n// Auto-generated 3D Web Map Header\nconst char WEB_3D_MAP_HTML[] PROGMEM = R"rawliteral(\n')
        f.write(c)
        f.write('\n)rawliteral";\n')
    print("[✓] Synchronized src/ground_station/web_3d_map.h.")

print("\nAll 3D Map upgrades applied successfully!")
