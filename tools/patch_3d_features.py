import sys

with open('src/ground_station/web_3d_map.h', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Update buildBuoyMesh to add orientation rotation and sonar cone
old_buoy_func = """    // D. Mô hình Phao USV (Phao đôi Catamaran + Tấm pin mặt trời + Lồng tời thả dây)
    let buoyVertices = [], buoyNormals = [], buoyColors = [], buoyIndices = [];
    function buildBuoyMesh(depth) {
      buoyVertices = []; buoyNormals = []; buoyColors = []; buoyIndices = [];

      // Hai thân phao Catamaran (Trắng/Đỏ)
      const addBox = (cx, cy, cz, w, h, l, r, g, b) => {
        let x1 = cx - w/2, x2 = cx + w/2;
        let y1 = cy - h/2, y2 = cy + h/2;
        let z1 = cz - l/2, z2 = cz + l/2;
        let start = buoyVertices.length / 3;

        // 8 Đỉnh
        buoyVertices.push(
          x1,y1,z1,  x2,y1,z1,  x2,y2,z1,  x1,y2,z1,
          x1,y1,z2,  x2,y1,z2,  x2,y2,z2,  x1,y2,z2
        );
        for(let k=0; k<8; k++) {
          buoyNormals.push(0, 1, 0);
          buoyColors.push(r, g, b, 1.0);
        }
        // 6 Mặt
        buoyIndices.push(
          start, start+1, start+2,  start, start+2, start+3, // Front
          start+4, start+6, start+5,  start+4, start+7, start+6, // Back
          start+3, start+2, start+6,  start+3, start+6, start+7, // Top
          start, start+5, start+1,  start, start+4, start+5, // Bottom
          start+1, start+5, start+6,  start+1, start+6, start+2, // Right
          start, start+3, start+7,  start, start+7, start+4  // Left
        );
      };

      // Thân trái & phải
      addBox(-0.7, 0.05, 0.0, 0.35, 0.25, 2.2, 0.95, 0.3, 0.2); // Đỏ USV
      addBox( 0.7, 0.05, 0.0, 0.35, 0.25, 2.2, 0.95, 0.3, 0.2);
      // Sàn trung tâm
      addBox(0.0, 0.15, 0.0, 1.2, 0.08, 1.5, 0.2, 0.25, 0.3);
      // Tấm pin Năng lượng Mặt trời trên mui
      addBox(0.0, 0.25, 0.0, 1.0, 0.04, 1.1, 0.08, 0.15, 0.35); // Lam đậm Solar
      // Cột ăng-ten LoRa 433MHz
      addBox(0.4, 0.6, -0.4, 0.04, 0.8, 0.04, 0.8, 0.8, 0.8);

      // Cáp tời thả xuống độ sâu Z (đường kẻ cáp)
      let targetZ = -Math.max(0.1, depth);
      addBox(0.0, targetZ / 2, 0.0, 0.03, Math.abs(targetZ), 0.03, 0.8, 0.8, 0.3);

      // Lồng cảm biến đáy (Sensor Cage hình trụ/khối phát sáng theo WQI)
      let cageCol = getColorForValue(currentParam, liveData[currentParam] || 50);
      addBox(0.0, targetZ - 0.15, 0.0, 0.35, 0.3, 0.35, cageCol[0], cageCol[1], cageCol[2]);

      return createMeshBuffers(buoyVertices, buoyNormals, buoyColors, buoyIndices);
    }"""

new_buoy_func = """    // D. Mô hình Phao USV (Phao đôi Catamaran + Tấm pin mặt trời + Lồng tời thả dây + Sonar Cone)
    let buoyVertices = [], buoyNormals = [], buoyColors = [], buoyIndices = [];
    function buildBuoyMesh(depth) {
      buoyVertices = []; buoyNormals = []; buoyColors = []; buoyIndices = [];

      // Góc nghiêng phần cứng thật (Roll & Pitch từ MPU6050, Heading từ La bàn GY-271)
      const rollRad  = (liveData.roll  || 0.0) * Math.PI / 180.0;
      const pitchRad = (liveData.pitch || 0.0) * Math.PI / 180.0;
      const yawRad   = ((liveData.hdg  || 0.0) - 90.0) * Math.PI / 180.0;

      const cR = Math.cos(rollRad),  sR = Math.sin(rollRad);
      const cP = Math.cos(pitchRad), sP = Math.sin(pitchRad);
      const cY = Math.cos(yawRad),   sY = Math.sin(yawRad);

      // Hàm biến đổi tọa độ đỉnh từ khung thân tàu sang không gian hồ nước
      const transformBody = (lx, ly, lz) => {
        // Roll (Z)
        let x1 = lx * cR - ly * sR;
        let y1 = lx * sR + ly * cR;
        let z1 = lz;
        // Pitch (X)
        let x2 = x1;
        let y2 = y1 * cP - z1 * sP;
        let z2 = y1 * sP + z1 * cP;
        // Yaw (Y)
        let x3 = x2 * cY + z2 * sY;
        let y3 = y2;
        let z3 = -x2 * sY + z2 * cY;
        return [x3, y3, z3];
      };

      // Thêm khối hộp có góc xoay 3D
      const addBox = (cx, cy, cz, w, h, l, r, g, b, isBody = true) => {
        let x1 = cx - w/2, x2 = cx + w/2;
        let y1 = cy - h/2, y2 = cy + h/2;
        let z1 = cz - l/2, z2 = cz + l/2;
        let rawVerts = [
          x1,y1,z1,  x2,y1,z1,  x2,y2,z1,  x1,y2,z1,
          x1,y1,z2,  x2,y1,z2,  x2,y2,z2,  x1,y2,z2
        ];
        let start = buoyVertices.length / 3;

        for (let k = 0; k < 8; k++) {
          let rx = rawVerts[k*3], ry = rawVerts[k*3+1], rz = rawVerts[k*3+2];
          if (isBody) {
            let tv = transformBody(rx, ry, rz);
            buoyVertices.push(tv[0], tv[1], tv[2]);
          } else {
            buoyVertices.push(rx, ry, rz);
          }
          buoyNormals.push(0, 1, 0);
          buoyColors.push(r, g, b, 1.0);
        }

        buoyIndices.push(
          start, start+1, start+2,  start, start+2, start+3, // Front
          start+4, start+6, start+5,  start+4, start+7, start+6, // Back
          start+3, start+2, start+6,  start+3, start+6, start+7, // Top
          start, start+5, start+1,  start, start+4, start+5, // Bottom
          start+1, start+5, start+6,  start+1, start+6, start+2, // Right
          start, start+3, start+7,  start, start+7, start+4  // Left
        );
      };

      // 1. Hai thân phao Catamaran khí động học (Đỏ Signal Red / Cam)
      addBox(-0.7, 0.05, 0.0, 0.35, 0.25, 2.2, 0.95, 0.26, 0.18, true);
      addBox( 0.7, 0.05, 0.0, 0.35, 0.25, 2.2, 0.95, 0.26, 0.18, true);
      // Mũi nhọn vát nước hai thân
      addBox(-0.7, 0.10,  1.15, 0.28, 0.20, 0.40, 1.0, 0.85, 0.0, true);
      addBox( 0.7, 0.10,  1.15, 0.28, 0.20, 0.40, 1.0, 0.85, 0.0, true);
      // Sàn trung tâm gắn thiết bị (Xám Nano Carbon)
      addBox(0.0, 0.15, 0.0, 1.25, 0.08, 1.5, 0.15, 0.20, 0.28, true);
      // Tấm pin Năng lượng Mặt trời PV Mono
      addBox(0.0, 0.24, 0.0, 1.05, 0.04, 1.15, 0.05, 0.18, 0.42, true);
      // Cột ăng-ten LoRa 433MHz
      addBox(0.42, 0.65, -0.42, 0.04, 0.9, 0.04, 0.85, 0.85, 0.9, true);

      // Điểm treo tời cáp trên boong thuyền (World coords)
      let spoolWorld = transformBody(0.0, 0.15, 0.0);
      let probeDepthZ = -Math.max(0.12, depth);

      // 2. Cáp tời thẳng đứng theo phương trọng lực (Vàng phản quang)
      let startCable = buoyVertices.length / 3;
      buoyVertices.push(
        spoolWorld[0]-0.02, spoolWorld[1], spoolWorld[2],
        spoolWorld[0]+0.02, spoolWorld[1], spoolWorld[2],
        spoolWorld[0]+0.02, probeDepthZ,   spoolWorld[2],
        spoolWorld[0]-0.02, probeDepthZ,   spoolWorld[2]
      );
      for(let k=0; k<4; k++) {
        buoyNormals.push(0, 1, 0);
        buoyColors.push(0.9, 0.9, 0.2, 1.0);
      }
      buoyIndices.push(startCable, startCable+1, startCable+2, startCable, startCable+2, startCable+3);

      // 3. Lồng cảm biến đa tầng (Sensor Cage phát sáng theo WQI)
      let cageCol = getColorForValue(currentParam, liveData[currentParam] || 50);
      addBox(spoolWorld[0], probeDepthZ - 0.16, spoolWorld[2], 0.38, 0.32, 0.38, cageCol[0], cageCol[1], cageCol[2], false);

      // 4. Chùm sóng siêu âm Sonar 3D Acoustic Cone (Tia đo độ sâu lòng hồ)
      const coneR = Math.max(0.5, Math.abs(probeDepthZ) * 0.45);
      const coneSegments = 12;
      let startCone = buoyVertices.length / 3;
      // Đỉnh nón tại đáy thuyền
      buoyVertices.push(spoolWorld[0], 0.0, spoolWorld[2]);
      buoyNormals.push(0, 1, 0);
      buoyColors.push(0.0, 0.95, 0.9, 0.25);

      for (let s = 0; s < coneSegments; s++) {
        let ang = (s / coneSegments) * Math.PI * 2;
        let px = spoolWorld[0] + coneR * Math.cos(ang);
        let pz = spoolWorld[2] + coneR * Math.sin(ang);
        buoyVertices.push(px, probeDepthZ - 0.3, pz);
        buoyNormals.push(0, 1, 0);
        buoyColors.push(0.0, 0.8, 1.0, 0.18);
      }
      for (let s = 0; s < coneSegments; s++) {
        let nextS = (s + 1) % coneSegments;
        buoyIndices.push(startCone, startCone + 1 + s, startCone + 1 + nextS);
      }

      return createMeshBuffers(buoyVertices, buoyNormals, buoyColors, buoyIndices);
    }"""

if old_buoy_func in content:
    content = content.replace(old_buoy_func, new_buoy_func)
    print("Replaced buildBuoyMesh successfully!")
else:
    print("Warning: old_buoy_func not found!")

# 2. Add Survey Recording and CSV export function
old_hud_func = """    function updateHUD() {
      document.getElementById('hudSeq').innerText = '#' + liveData.seq;
      document.getElementById('hudPH').innerText  = liveData.ph.toFixed(2);
      document.getElementById('hudTDS').innerText = Math.round(liveData.tds);
      document.getElementById('hudEC').innerText  = Math.round(liveData.ec || liveData.tds * 1.9);
      document.getElementById('hudNTU').innerText = Math.round(liveData.ntu);
      document.getElementById('hudTemp').innerText= liveData.temp.toFixed(1);
      document.getElementById('hudDO').innerText  = liveData.do.toFixed(2);
      document.getElementById('hudBat').innerText = liveData.bat.toFixed(2) + 'V (' + Math.round((liveData.bat - 9.0)/3.6 * 100) + '%)';
      document.getElementById('hudAmp').innerText = liveData.amp.toFixed(2) + 'A • ' + (liveData.bat * liveData.amp).toFixed(1) + 'W';
      document.getElementById('hudDepth').innerText = liveData.depth.toFixed(2) + 'm' + (liveData.depth < 0.05 ? ' (Home)' : '');
      document.getElementById('hudImu').innerText = `R: ${liveData.roll.toFixed(1)}° • P: ${liveData.pitch.toFixed(1)}°`;
      document.getElementById('hudGps').innerText = `${liveData.lat.toFixed(5)}°N, ${liveData.lng.toFixed(5)}°E`;
      document.getElementById('hudLora').innerText= `${liveData.rssi} dBm (SNR ${liveData.snr}dB)`;

      // Cập nhật giá trị chính trên HUD theo Metric đang chọn
      if (currentParam === 'wqi') {
        document.getElementById('mainParamVal').innerText = liveData.wqi;
      }

      // Xây dựng lại mô hình tời phao theo độ sâu thực tế
      buoyMesh = buildBuoyMesh(liveData.depth);
    }"""

new_hud_func = """    // Lưu trữ điểm khảo sát thủy văn thời gian thực
    let surveyPoints = [];

    function updateHUD() {
      document.getElementById('hudSeq').innerText = '#' + liveData.seq;
      document.getElementById('hudPH').innerText  = liveData.ph.toFixed(2);
      document.getElementById('hudTDS').innerText = Math.round(liveData.tds);
      document.getElementById('hudEC').innerText  = Math.round(liveData.ec || liveData.tds * 1.9);
      document.getElementById('hudNTU').innerText = Math.round(liveData.ntu);
      document.getElementById('hudTemp').innerText= liveData.temp.toFixed(1);
      document.getElementById('hudDO').innerText  = liveData.do.toFixed(2);
      document.getElementById('hudBat').innerText = liveData.bat.toFixed(2) + 'V (' + Math.round((liveData.bat - 9.0)/3.6 * 100) + '%)';
      document.getElementById('hudAmp').innerText = liveData.amp.toFixed(2) + 'A • ' + (liveData.bat * liveData.amp).toFixed(1) + 'W';
      document.getElementById('hudDepth').innerText = liveData.depth.toFixed(2) + 'm' + (liveData.depth < 0.05 ? ' (Home)' : '');
      document.getElementById('hudImu').innerText = `R: ${liveData.roll.toFixed(1)}° • P: ${liveData.pitch.toFixed(1)}° • H: ${liveData.hdg.toFixed(0)}°`;
      document.getElementById('hudGps').innerText = `${liveData.lat.toFixed(5)}°N, ${liveData.lng.toFixed(5)}°E`;
      document.getElementById('hudLora').innerText= `${liveData.rssi} dBm (SNR ${liveData.snr}dB)`;

      // Cập nhật giá trị chính trên HUD theo Metric đang chọn
      if (currentParam === 'wqi') {
        document.getElementById('mainParamVal').innerText = liveData.wqi;
      }

      // Tự động ghi nhận điểm khảo sát thủy văn khi có số liệu mới
      if (liveData.seq && (!surveyPoints.length || surveyPoints[surveyPoints.length - 1].seq !== liveData.seq)) {
        surveyPoints.push({
          time: new Date().toLocaleTimeString(),
          seq: liveData.seq,
          lat: liveData.lat.toFixed(6),
          lng: liveData.lng.toFixed(6),
          depth: liveData.depth.toFixed(2),
          wqi: liveData.wqi,
          ph: liveData.ph.toFixed(2),
          tds: Math.round(liveData.tds),
          ntu: Math.round(liveData.ntu),
          temp: liveData.temp.toFixed(1),
          do: liveData.do.toFixed(2)
        });
        if (surveyPoints.length > 500) surveyPoints.shift();
        const cntEl = document.getElementById('surveyCount');
        if (cntEl) cntEl.innerText = surveyPoints.length + ' điểm';
      }

      // Xây dựng lại mô hình tời phao theo độ sâu & góc nghiêng thực tế
      buoyMesh = buildBuoyMesh(liveData.depth);
    }

    function exportSurveyCsv() {
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

if old_hud_func in content:
    content = content.replace(old_hud_func, new_hud_func)
    print("Replaced updateHUD successfully!")
else:
    print("Warning: old_hud_func not found!")

# 3. Add Export CSV button to footer
old_footer_btns = """      <button onclick="exportSnapshot()" class="glass-button px-2.5 py-1 text-xs rounded-lg text-cyan-300 flex items-center gap-1">
        <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M3 9a2 2 0 012-2h.93a2 2 0 001.664-.89l.812-1.22A2 2 0 0110.07 4h3.86a2 2 0 011.664.89l.812 1.22A2 2 0 0018.07 7H19a2 2 0 012 2v9a2 2 0 01-2 2H5a2 2 0 01-2-2V9z"></path><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 13a3 3 0 11-6 0 3 3 0 016 0z"></path></svg>
        Lưu ảnh 3D
      </button>"""

new_footer_btns = """      <button onclick="exportSnapshot()" class="glass-button px-2.5 py-1 text-xs rounded-lg text-cyan-300 flex items-center gap-1">
        <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M3 9a2 2 0 012-2h.93a2 2 0 001.664-.89l.812-1.22A2 2 0 0110.07 4h3.86a2 2 0 011.664.89l.812 1.22A2 2 0 0018.07 7H19a2 2 0 012 2v9a2 2 0 01-2 2H5a2 2 0 01-2-2V9z"></path><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 13a3 3 0 11-6 0 3 3 0 016 0z"></path></svg>
        Lưu ảnh 3D
      </button>
      <button onclick="exportSurveyCsv()" class="glass-button px-2.5 py-1 text-xs rounded-lg text-emerald-300 flex items-center gap-1">
        <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4"></path></svg>
        Xuất CSV (<span id="surveyCount">0 điểm</span>)
      </button>"""

if old_footer_btns in content:
    content = content.replace(old_footer_btns, new_footer_btns)
    print("Added exportSurveyCsv button to footer successfully!")
else:
    print("Warning: old_footer_btns not found!")

with open('src/ground_station/web_3d_map.h', 'w', encoding='utf-8') as f:
    f.write(content)
print("Updated web_3d_map.h successfully!")
