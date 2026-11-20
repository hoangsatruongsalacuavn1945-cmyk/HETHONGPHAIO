
    // --- 1. BIẾN TRẠNG THÁI TOÀN CỤC (U-01: Khởi tạo sạch 100%, không nạp sẵn dữ liệu giả) ---
    let currentParam = 'wqi';
    let isLiveMode   = true;
    let isHistoryReplay = false;
    let historyRecords = [];
    let autoRotate   = false;
    let followBuoyMode = false;
    let visibleMeshes = { surface: true, columns: true, volume: true, bed: true, buoy: true, grid: true };
    let sliceDepthFilter = 0;

    // Dữ liệu đo 3 tầng nước chuẩn QCVN 08:2023/BTNMT (chỉ hiển thị khi nhận được dữ liệu thật từ Phao)
    let profileData = {
      surface: { depth: 0.3, ph: 0.0, tds: 0, ntu: 0, temp: 0.0, do: 0.0, wqi: null, valid: false },
      middle:  { depth: 1.0, ph: 0.0, tds: 0, ntu: 0, temp: 0.0, do: 0.0, wqi: null, valid: false },
      bottom:  { depth: 1.8, ph: 0.0, tds: 0, ntu: 0, temp: 0.0, do: 0.0, wqi: null, valid: false }
    };

    // Dữ liệu đo tức thời (Live Telemetry) — Khởi tạo hasTelemetry = false cho tới khi nhận gói thật
    let hasTelemetry = false;
    let liveData = {
      seq: 0, ph: 0.0, tds: 0, ec: 0, ntu: 0, temp: 0.0, do: 0.0, wqi: null,
      bat: 0.0, amp: 0.0, depth: 0.0, roll: 0.0, pitch: 0.0, hdg: 0,
      lat: 0.0, lng: 0.0, rssi: 0, snr: 0.0, hwm: 0, adf: 0,
      posX: 0.0, posZ: 0.0, gpsFix: false
    };

    // Mạng lưới các Trụ Cột Quan Trắc 3 Tầng (3D Water Column Prisms) trong lòng hồ
    const stationColumns = [
      { id: "ST01", x: -7.5, z: -6.5, name: "Trạm Thượng Lưu A",
        s: { wqi: 91, ph: 7.45, tds: 190, ntu: 10, temp: 28.8, do: 7.2 },
        m: { wqi: 84, ph: 7.30, tds: 230, ntu: 22, temp: 28.2, do: 6.5 },
        b: { wqi: 72, ph: 7.05, tds: 310, ntu: 55, temp: 27.5, do: 5.4 } },
      { id: "ST02", x:  0.0, z: -6.5, name: "Trạm Giữa Hồ B",
        s: { wqi: 86, ph: 7.38, tds: 240, ntu: 16, temp: 29.0, do: 6.8 },
        m: { wqi: 76, ph: 7.15, tds: 295, ntu: 35, temp: 28.4, do: 5.9 },
        b: { wqi: 58, ph: 6.90, tds: 390, ntu: 95, temp: 27.6, do: 4.9 } },
      { id: "ST03", x:  7.0, z: -4.5, name: "Khu Vực Cửa Xả C (Cảnh báo KL nặng)",
        s: { wqi: 64, ph: 6.65, tds: 420, ntu: 65, temp: 29.8, do: 5.1 },
        m: { wqi: 42, ph: 6.15, tds: 680, ntu: 160, temp: 29.2, do: 3.8 },
        b: { wqi: 24, ph: 5.40, tds: 950, ntu: 290, temp: 28.5, do: 2.4 } },
      { id: "ST04", x: -6.5, z:  5.0, name: "Bến Phao Tây D",
        s: { wqi: 89, ph: 7.40, tds: 210, ntu: 12, temp: 28.9, do: 7.0 },
        m: { wqi: 81, ph: 7.25, tds: 260, ntu: 28, temp: 28.3, do: 6.2 },
        b: { wqi: 68, ph: 7.00, tds: 340, ntu: 68, temp: 27.7, do: 5.1 } },
      { id: "ST05", x:  5.5, z:  6.0, name: "Vùng Bùn Đáy Sâu E",
        s: { wqi: 78, ph: 7.20, tds: 290, ntu: 25, temp: 29.3, do: 6.1 },
        m: { wqi: 62, ph: 6.85, tds: 410, ntu: 75, temp: 28.6, do: 5.0 },
        b: { wqi: 34, ph: 6.10, tds: 720, ntu: 220, temp: 27.9, do: 3.2 } }
    ];

    const numOr = (v, fallback) => (typeof v === 'number' && isFinite(v)) ? v : fallback;

    // --- 2. HỆ THỐNG TOÁN HỌC 3D (VECTOR, MATRIX, CAMERA) ---
    const M4 = {
      create: () => new Float32Array([1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]),
      perspective: (out, fovy, aspect, near, far) => {
        const f = 1.0 / Math.tan(fovy / 2);
        const nf = 1 / (near - far);
        out[0] = f / aspect; out[1] = 0; out[2] = 0; out[3] = 0;
        out[4] = 0; out[5] = f; out[6] = 0; out[7] = 0;
        out[8] = 0; out[9] = 0; out[10] = (far + near) * nf; out[11] = -1;
        out[12] = 0; out[13] = 0; out[14] = (2 * far * near) * nf; out[15] = 0;
        return out;
      },
      lookAt: (out, eye, center, up) => {
        let x0, x1, x2, y0, y1, y2, z0, z1, z2, len;
        let eyex = eye[0], eyey = eye[1], eyez = eye[2];
        let upx = up[0], upy = up[1], upz = up[2];
        let centerx = center[0], centery = center[1], centerz = center[2];

        z0 = eyex - centerx; z1 = eyey - centery; z2 = eyez - centerz;
        len = Math.hypot(z0, z1, z2);
        if (len < 1e-5) { z2 = 1.0; len = 1.0; } else { len = 1.0 / len; z0 *= len; z1 *= len; z2 *= len; }

        x0 = upy * z2 - upz * z1;
        x1 = upz * z0 - upx * z2;
        x2 = upx * z1 - upy * z0;
        len = Math.hypot(x0, x1, x2);
        if (len < 1e-5) { x0 = 1.0; len = 1.0; } else { len = 1.0 / len; x0 *= len; x1 *= len; x2 *= len; }

        y0 = z1 * x2 - z2 * x1;
        y1 = z2 * x0 - z0 * x2;
        y2 = z0 * x1 - z1 * x0;
        len = Math.hypot(y0, y1, y2);
        if (len > 1e-5) { len = 1.0 / len; y0 *= len; y1 *= len; y2 *= len; }

        out[0] = x0; out[1] = y0; out[2] = z0; out[3] = 0;
        out[4] = x1; out[5] = y1; out[6] = z1; out[7] = 0;
        out[8] = x2; out[9] = y2; out[10] = z2; out[11] = 0;
        out[12] = -(x0 * eyex + x1 * eyey + x2 * eyez);
        out[13] = -(y0 * eyex + y1 * eyey + y2 * eyez);
        out[14] = -(z0 * eyex + z1 * eyey + z2 * eyez);
        out[15] = 1;
        return out;
      },
      multiply: (out, a, b) => {
        let a00 = a[0], a01 = a[1], a02 = a[2], a03 = a[3];
        let a10 = a[4], a11 = a[5], a12 = a[6], a13 = a[7];
        let a20 = a[8], a21 = a[9], a22 = a[10], a23 = a[11];
        let a30 = a[12], a31 = a[13], a32 = a[14], a33 = a[15];

        let b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3];
        out[0] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
        out[1] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
        out[2] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
        out[3] = b0*a03 + b1*a13 + b2*a23 + b3*a33;

        b0 = b[4]; b1 = b[5]; b2 = b[6]; b3 = b[7];
        out[4] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
        out[5] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
        out[6] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
        out[7] = b0*a03 + b1*a13 + b2*a23 + b3*a33;

        b0 = b[8]; b1 = b[9]; b2 = b[10]; b3 = b[11];
        out[8] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
        out[9] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
        out[10] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
        out[11] = b0*a03 + b1*a13 + b2*a23 + b3*a33;

        b0 = b[12]; b1 = b[13]; b2 = b[14]; b3 = b[15];
        out[12] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
        out[13] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
        out[14] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
        out[15] = b0*a03 + b1*a13 + b2*a23 + b3*a33;
        return out;
      }
    };

    // Camera Orbit State
    let camera = {
      radius: 25.0,
      theta: 0.72,
      phi: 1.02,
      target: [0, -0.7, 0]
    };

    // --- 3. KHỞI TẠO WEBGL CONTEXT & SHADERS (CHUẨN GLSL ES 1.00 KHÔNG LỖI ANGLE) ---
    const canvas = document.getElementById('glCanvas');
    const gl = canvas.getContext('webgl', { antialias: true, alpha: false, preserveDrawingBuffer: true }) ||
               canvas.getContext('experimental-webgl', { antialias: true, alpha: false, preserveDrawingBuffer: true });

    if (!gl) {
      alert("Trình duyệt không hỗ trợ WebGL. Vui lòng bật tăng tốc phần cứng!");
    }

    // Chú ý quan trọng: Khai báo đồng nhất precision mediump float & uniform float uIsWater
    // ở cả Vertex Shader và Fragment Shader để tuyệt đối không bị lỗi Link Program trên Windows ANGLE!
    const vsSource = `
      precision mediump float;
      precision mediump int;

      attribute vec3 aPosition;
      attribute vec3 aNormal;
      attribute vec4 aColor;

      uniform mat4 uMatrix;
      uniform float uTime;
      uniform float uIsWater;

      varying vec3 vNormal;
      varying vec4 vColor;
      varying vec3 vPosition;

      void main() {
        vec3 pos = aPosition;
        if (uIsWater > 0.5) {
          float wave1 = sin(pos.x * 0.7 + uTime * 1.8) * cos(pos.z * 0.7 + uTime * 1.3) * 0.10;
          float wave2 = sin(pos.x * 1.4 - uTime * 2.2) * 0.04;
          pos.y += wave1 + wave2;
        }
        vPosition = pos;
        vNormal = aNormal;
        vColor = aColor;
        gl_Position = uMatrix * vec4(pos, 1.0);
        gl_PointSize = 10.0;
      }
    `;

    const fsSource = `
      precision mediump float;
      precision mediump int;

      varying vec3 vNormal;
      varying vec4 vColor;
      varying vec3 vPosition;

      uniform vec3 uLightDir;
      uniform float uIsWater;

      void main() {
        vec3 norm = normalize(vNormal);
        float diff = max(dot(norm, normalize(uLightDir)), 0.42);

        vec4 col = vColor;
        if (uIsWater > 0.5) {
          vec3 viewDir = normalize(vec3(0.0, 12.0, 22.0) - vPosition);
          vec3 halfDir = normalize(normalize(uLightDir) + viewDir);
          float spec = pow(max(dot(norm, halfDir), 0.0), 28.0);
          col.rgb = mix(col.rgb * diff + vec3(spec * 0.5), vec3(0.12, 0.72, 0.95), 0.35);
          col.a = vColor.a;
        } else {
          col.rgb = col.rgb * (0.35 + 0.65 * diff);
        }
        gl_FragColor = col;
      }
    `;

    function createShader(glCtx, type, source) {
      const shader = glCtx.createShader(type);
      glCtx.shaderSource(shader, source);
      glCtx.compileShader(shader);
      if (!glCtx.getShaderParameter(shader, glCtx.COMPILE_STATUS)) {
        console.error("Shader compile error:", glCtx.getShaderInfoLog(shader));
        glCtx.deleteShader(shader);
        return null;
      }
      return shader;
    }

    const program = gl.createProgram();
    gl.attachShader(program, createShader(gl, gl.VERTEX_SHADER, vsSource));
    gl.attachShader(program, createShader(gl, gl.FRAGMENT_SHADER, fsSource));
    gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      console.error("WebGL Program Link Error:", gl.getProgramInfoLog(program));
    }
    gl.useProgram(program);

    const loc_aPosition = gl.getAttribLocation(program, 'aPosition');
    const loc_aNormal   = gl.getAttribLocation(program, 'aNormal');
    const loc_aColor    = gl.getAttribLocation(program, 'aColor');
    const loc_uMatrix   = gl.getUniformLocation(program, 'uMatrix');
    const loc_uTime     = gl.getUniformLocation(program, 'uTime');
    const loc_uIsWater  = gl.getUniformLocation(program, 'uIsWater');
    const loc_uLightDir = gl.getUniformLocation(program, 'uLightDir');

    // --- 4. BẢNG PHÂN LOẠI MÀU SẮC CHUẨN QCVN 08:2023/BTNMT ---
    function getColorForValue(param, val) {
      // Nếu cảm biến chưa cắm (val <= 0), hiển thị màu xanh ngọc trung tính
      if (!isFinite(val) || val <= 0.01) {
        return [0.18, 0.80, 0.44, 1.0]; // #2ECC71 Emerald Safe
      }
      let r = 0.18, g = 0.80, b = 0.44;
      if (param === 'wqi') {
        if (val < 30)      { r = 0.60; g = 0.11; b = 0.11; } // #991B1B Đỏ sẫm (Nguy hại / KL nặng)
        else if (val < 50) { r = 0.91; g = 0.30; b = 0.24; } // #E74C3C Đỏ cam (Ô nhiễm nặng)
        else if (val < 75) { r = 0.95; g = 0.77; b = 0.06; } // #F1C40F Vàng hổ phách (Cảnh báo B1)
        else               { r = 0.18; g = 0.80; b = 0.44; } // #2ECC71 Xanh lục bảo (An toàn A1/A2)
      } else if (param === 'ph') {
        if (val < 5.5 || val > 9.0)      { r = 0.60; g = 0.11; b = 0.11; }
        else if (val < 6.0 || val > 8.5) { r = 0.91; g = 0.30; b = 0.24; }
        else if (val < 6.5 || val > 8.2) { r = 0.95; g = 0.77; b = 0.06; }
        else                             { r = 0.18; g = 0.80; b = 0.44; }
      } else if (param === 'tds') {
        if (val > 800)      { r = 0.60; g = 0.11; b = 0.11; }
        else if (val > 500) { r = 0.91; g = 0.30; b = 0.24; }
        else if (val > 300) { r = 0.95; g = 0.77; b = 0.06; }
        else                { r = 0.18; g = 0.80; b = 0.44; }
      } else if (param === 'ntu') {
        if (val > 200)     { r = 0.60; g = 0.11; b = 0.11; }
        else if (val > 90) { r = 0.91; g = 0.30; b = 0.24; }
        else if (val > 30) { r = 0.95; g = 0.77; b = 0.06; }
        else               { r = 0.18; g = 0.80; b = 0.44; }
      } else if (param === 'temp') {
        let t = Math.max(0, Math.min(1, (val - 22) / 12));
        r = 0.15 + 0.8 * t; g = 0.75 * (1 - Math.abs(t - 0.45)); b = 0.95 * (1 - t);
      } else { // DO
        if (val < 3.0)      { r = 0.60; g = 0.11; b = 0.11; }
        else if (val < 4.5) { r = 0.91; g = 0.30; b = 0.24; }
        else if (val < 6.0) { r = 0.95; g = 0.77; b = 0.06; }
        else                { r = 0.18; g = 0.80; b = 0.44; }
      }
      return [r, g, b, 1.0];
    }

    // Quản lý Buffer WebGL thông minh (Tái sử dụng Buffer, chống rò rỉ VRAM)
    function updateOrCreateMesh(existingMesh, verts, norms, cols, inds) {
      const m = existingMesh || {
        vbo: gl.createBuffer(),
        nbo: gl.createBuffer(),
        cbo: gl.createBuffer(),
        ibo: gl.createBuffer(),
        count: 0
      };
      gl.bindBuffer(gl.ARRAY_BUFFER, m.vbo);
      gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(verts), gl.DYNAMIC_DRAW);

      gl.bindBuffer(gl.ARRAY_BUFFER, m.nbo);
      gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(norms), gl.DYNAMIC_DRAW);

      gl.bindBuffer(gl.ARRAY_BUFFER, m.cbo);
      gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(cols), gl.DYNAMIC_DRAW);

      gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, m.ibo);
      gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(inds), gl.DYNAMIC_DRAW);

      m.count = inds.length;
      return m;
    }

    const GRID_SIZE = 24, SPAN = 22;

    // A. Lòng hồ / Địa hình đáy (Bathymetry Mesh) + Lưới địa hình đáy rõ nét
    let bedVertices = [], bedNormals = [], bedColors = [], bedIndices = [];
    for (let i = 0; i <= GRID_SIZE; i++) {
      for (let j = 0; j <= GRID_SIZE; j++) {
        let x = (i / GRID_SIZE - 0.5) * SPAN;
        let z = (j / GRID_SIZE - 0.5) * SPAN;
        let dist = Math.hypot(x, z);
        let depth = -2.25 + Math.min(1.65, Math.pow(dist / 10.5, 2.0)) - Math.sin(x * 0.45) * 0.12;

        bedVertices.push(x, depth, z);
        bedNormals.push(0, 1, 0);
        let c = Math.max(0.15, Math.min(0.65, (depth + 2.4) * 0.32));
        bedColors.push(0.14 + c * 0.25, 0.24 + c * 0.35, 0.34 + c * 0.40, 1.0);
      }
    }
    for (let i = 0; i < GRID_SIZE; i++) {
      for (let j = 0; j < GRID_SIZE; j++) {
        let a = i * (GRID_SIZE + 1) + j;
        let b = a + 1;
        let c = (i + 1) * (GRID_SIZE + 1) + j;
        let d = c + 1;
        bedIndices.push(a, b, c, b, d, c);
      }
    }
    let bedMesh = updateOrCreateMesh(null, bedVertices, bedNormals, bedColors, bedIndices);

    // B. Mặt nước động 3D (Water Surface Mesh)
    let surfMesh = null;
    function buildSurfMesh() {
      let surfVertices = [], surfNormals = [], surfColors = [], surfIndices = [];
      let activeVal = (liveData[currentParam] !== null && liveData[currentParam] > 0) ? liveData[currentParam] : (profileData.surface[currentParam] || 0);
      let baseCol = getColorForValue(currentParam, activeVal);
      let r = 0.04 * 0.45 + baseCol[0] * 0.55;
      let g = 0.55 * 0.45 + baseCol[1] * 0.55;
      let b = 0.85 * 0.45 + baseCol[2] * 0.55;
      for (let i = 0; i <= GRID_SIZE; i++) {
        for (let j = 0; j <= GRID_SIZE; j++) {
          let x = (i / GRID_SIZE - 0.5) * SPAN;
          let z = (j / GRID_SIZE - 0.5) * SPAN;
          surfVertices.push(x, 0.0, z);
          surfNormals.push(0, 1, 0);
          surfColors.push(r, g, b, 0.62);
        }
      }
      for (let i = 0; i < GRID_SIZE; i++) {
        for (let j = 0; j < GRID_SIZE; j++) {
          let a = i * (GRID_SIZE + 1) + j;
          let b = a + 1;
          let c = (i + 1) * (GRID_SIZE + 1) + j;
          let d = c + 1;
          surfIndices.push(a, b, c, b, d, c);
        }
      }
      surfMesh = updateOrCreateMesh(surfMesh, surfVertices, surfNormals, surfColors, surfIndices);
    }
    buildSurfMesh();

    // C. 3 Tầng nước Mặt cắt Thể tích Không gian (IDW Spatial Heatmap Slices)
    let sliceMesh = null;
    function buildSlicePlanes() {
      let sliceVertices = [], sliceNormals = [], sliceColors = [], sliceIndices = [];
      const layers = [
        { d: -0.35, key: 's', data: profileData.surface },
        { d: -1.05, key: 'm', data: profileData.middle },
        { d: -1.80, key: 'b', data: profileData.bottom }
      ];

      const SLICE_GRID = 12;
      let vOffset = 0;
      layers.forEach((l, idx) => {
        if (sliceDepthFilter > 0 && Math.abs(-l.d - sliceDepthFilter) > 0.4) return;
        let spanL = 18.5 - idx * 2.2;

        for (let i = 0; i <= SLICE_GRID; i++) {
          for (let j = 0; j <= SLICE_GRID; j++) {
            let x = (i / SLICE_GRID - 0.5) * spanL;
            let z = (j / SLICE_GRID - 0.5) * spanL;

            // Nội suy không gian IDW (Inverse Distance Weighting)
            let num = 0, den = 0;
            let dBuoy = Math.hypot(x - (liveData.posX || 0), z - (liveData.posZ || 0)) + 0.5;
            let wBuoy = 1.0 / (dBuoy * dBuoy);
            let buoyVal = (l.data[currentParam] !== null && l.data[currentParam] > 0) ? l.data[currentParam] : (liveData[currentParam] || 0);
            if (buoyVal > 0) {
              num += buoyVal * wBuoy;
              den += wBuoy;
            }

            if (!isLiveMode) {
              for (let st of stationColumns) {
                let dst = Math.hypot(x - st.x, z - st.z) + 0.5;
                let wst = 1.0 / (dst * dst);
                let stVal = st[l.key][currentParam] || 75;
                num += stVal * wst;
                den += wst;
              }
            }
            let interpVal = den > 0 ? (num / den) : buoyVal;
            let col = getColorForValue(currentParam, interpVal);

            sliceVertices.push(x, l.d, z);
            sliceNormals.push(0, 1, 0);
            sliceColors.push(col[0], col[1], col[2], 0.52);
          }
        }
        for (let i = 0; i < SLICE_GRID; i++) {
          for (let j = 0; j < SLICE_GRID; j++) {
            let a = vOffset + i * (SLICE_GRID + 1) + j;
            let b = a + 1;
            let c = vOffset + (i + 1) * (SLICE_GRID + 1) + j;
            let d = c + 1;
            sliceIndices.push(a, b, c, b, d, c);
          }
        }
        vOffset += (SLICE_GRID + 1) * (SLICE_GRID + 1);
      });
      sliceMesh = updateOrCreateMesh(sliceMesh, sliceVertices, sliceNormals, sliceColors, sliceIndices);
    }
    buildSlicePlanes();

    // D. Trụ Cột Quan Trắc Đa Tầng 3D (3D Multi-Layer Water Column Prisms)
    let columnsMesh = null;
    function buildWaterColumnsMesh() {
      let verts = [], norms = [], cols = [], inds = [];

      const addCylinderSegment = (cx, cz, yTop, yBot, radius, r, g, b, alpha) => {
        const segs = 10;
        let start = verts.length / 3;
        for (let s = 0; s <= segs; s++) {
          let ang = (s / segs) * Math.PI * 2;
          let nx = Math.cos(ang), nz = Math.sin(ang);
          let px = cx + radius * nx, pz = cz + radius * nz;
          // Top vertex
          verts.push(px, yTop, pz);
          norms.push(nx, 0.2, nz);
          cols.push(r, g, b, alpha);
          // Bottom vertex
          verts.push(px, yBot, pz);
          norms.push(nx, 0.2, nz);
          cols.push(r * 0.85, g * 0.85, b * 0.85, alpha);
        }
        for (let s = 0; s < segs; s++) {
          let i0 = start + s * 2;
          let i1 = i0 + 1;
          let i2 = i0 + 2;
          let i3 = i0 + 3;
          inds.push(i0, i1, i2, i1, i3, i2);
        }
      };

      // U-02: Ở chế độ Phao Thật (isLiveMode), chỉ vẽ trụ cột tại vị trí Phao thật; ở chế độ Mô Phỏng mới vẽ thêm 5 trạm mẫu
      const allCols = isLiveMode ? [
        {
          id: "BUOY_COL",
          x: liveData.posX || 0,
          z: liveData.posZ || 0,
          s: profileData.surface,
          m: profileData.middle,
          b: profileData.bottom,
          isBuoy: true
        }
      ] : [
        ...stationColumns,
        {
          id: "BUOY_COL",
          x: liveData.posX || 0,
          z: liveData.posZ || 0,
          s: profileData.surface,
          m: profileData.middle,
          b: profileData.bottom,
          isBuoy: true
        }
      ];

      allCols.forEach(st => {
        let rad = st.isBuoy ? 0.55 : 0.45;
        let cS = getColorForValue(currentParam, st.s[currentParam]);
        let cM = getColorForValue(currentParam, st.m[currentParam]);
        let cB = getColorForValue(currentParam, st.b[currentParam]);

        // Tầng 1: Mặt (0.0m -> -0.6m)
        addCylinderSegment(st.x, st.z, -0.02, -0.60, rad, cS[0], cS[1], cS[2], 0.88);
        // Tầng 2: Giữa (-0.62m -> -1.25m)
        addCylinderSegment(st.x, st.z, -0.62, -1.25, rad, cM[0], cM[1], cM[2], 0.88);
        // Tầng 3: Đáy (-1.27m -> -1.90m)
        addCylinderSegment(st.x, st.z, -1.27, -1.90, rad, cB[0], cB[1], cB[2], 0.88);
      });

      columnsMesh = updateOrCreateMesh(columnsMesh, verts, norms, cols, inds);
    }
    buildWaterColumnsMesh();

    // E. Mô hình Phao USV (Catamaran + Tấm pin mặt trời + Lồng tời thả dây + Chùm Sonar 3D)
    let buoyMesh = null;
    function buildBuoyMesh(depth) {
      let buoyVertices = [], buoyNormals = [], buoyColors = [], buoyIndices = [];

      const bx = liveData.posX || 0.0;
      const bz = liveData.posZ || 0.0;
      const rollRad  = (liveData.roll  || 0.0) * Math.PI / 180.0;
      const pitchRad = (liveData.pitch || 0.0) * Math.PI / 180.0;
      const yawRad   = ((liveData.hdg  || 0.0) - 90.0) * Math.PI / 180.0;

      const cR = Math.cos(rollRad),  sR = Math.sin(rollRad);
      const cP = Math.cos(pitchRad), sP = Math.sin(pitchRad);
      const cY = Math.cos(yawRad),   sY = Math.sin(yawRad);

      const transformBody = (lx, ly, lz) => {
        let x1 = lx * cR - ly * sR;
        let y1 = lx * sR + ly * cR;
        let z1 = lz;
        let x2 = x1;
        let y2 = y1 * cP - z1 * sP;
        let z2 = y1 * sP + z1 * cP;
        let x3 = x2 * cY + z2 * sY;
        let y3 = y2;
        let z3 = -x2 * sY + z2 * cY;
        return [x3 + bx, y3, z3 + bz];
      };

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
          start, start+1, start+2,  start, start+2, start+3,
          start+4, start+6, start+5,  start+4, start+7, start+6,
          start+3, start+2, start+6,  start+3, start+6, start+7,
          start, start+5, start+1,  start, start+4, start+5,
          start+1, start+5, start+6,  start+1, start+6, start+2,
          start, start+3, start+7,  start, start+7, start+4
        );
      };

      // 1. Hai thân phao Catamaran khí động học (Đỏ cam nổi bật)
      addBox(-0.75, 0.08, 0.0, 0.38, 0.28, 2.4, 0.96, 0.28, 0.18, true);
      addBox( 0.75, 0.08, 0.0, 0.38, 0.28, 2.4, 0.96, 0.28, 0.18, true);
      // Mũi nhọn vát nước hai thân (Vàng phản quang)
      addBox(-0.75, 0.12, 1.25, 0.30, 0.22, 0.45, 1.0, 0.85, 0.05, true);
      addBox( 0.75, 0.12, 1.25, 0.30, 0.22, 0.45, 1.0, 0.85, 0.05, true);
      // Sàn trung tâm gắn thiết bị
      addBox(0.0, 0.18, 0.0, 1.35, 0.10, 1.6, 0.22, 0.28, 0.36, true);
      // Tấm pin Năng lượng Mặt trời PV Mono
      addBox(0.0, 0.28, 0.0, 1.15, 0.05, 1.25, 0.08, 0.32, 0.68, true);
      // Cột ăng-ten LoRa 433MHz + Đèn hiệu đỉnh
      addBox(0.45, 0.75, -0.45, 0.05, 1.0, 0.05, 0.90, 0.92, 0.95, true);
      addBox(0.45, 1.28, -0.45, 0.12, 0.12, 0.12, 0.0, 1.0, 0.65, true);

      // 2. Cáp tời thẳng đứng theo phương trọng lực
      let spoolWorld = transformBody(0.0, 0.15, 0.0);
      let probeDepthZ = -Math.max(0.35, depth);
      addBox(spoolWorld[0], probeDepthZ / 2, spoolWorld[2], 0.04, Math.abs(probeDepthZ), 0.04, 1.0, 0.92, 0.2, false);

      // 3. Lồng cảm biến đa tầng (Sensor Cage phát sáng theo chất lượng nước)
      let cageCol = getColorForValue(currentParam, liveData[currentParam] || 86);
      addBox(spoolWorld[0], probeDepthZ - 0.18, spoolWorld[2], 0.44, 0.34, 0.44, cageCol[0], cageCol[1], cageCol[2], false);

      // 4. Chùm sóng siêu âm Sonar 3D Acoustic Cone
      const coneR = 0.85;
      const coneSegments = 12;
      let startCone = buoyVertices.length / 3;
      buoyVertices.push(spoolWorld[0], 0.0, spoolWorld[2]);
      buoyNormals.push(0, 1, 0);
      buoyColors.push(0.0, 0.95, 0.95, 0.32);

      for (let s = 0; s < coneSegments; s++) {
        let ang = (s / coneSegments) * Math.PI * 2;
        let px = spoolWorld[0] + coneR * Math.cos(ang);
        let pz = spoolWorld[2] + coneR * Math.sin(ang);
        buoyVertices.push(px, -1.95, pz);
        buoyNormals.push(0, 1, 0);
        buoyColors.push(0.0, 0.75, 1.0, 0.16);
      }
      for (let s = 0; s < coneSegments; s++) {
        let nextS = (s + 1) % coneSegments;
        buoyIndices.push(startCone, startCone + 1 + s, startCone + 1 + nextS);
      }

      buoyMesh = updateOrCreateMesh(buoyMesh, buoyVertices, buoyNormals, buoyColors, buoyIndices);
    }
    buildBuoyMesh(liveData.depth);

    // F. Lưới toạ độ GPS 3D Grid + Vòng tròn bán kính Neo Ảo 1.5m (Dynamic Positioning Ring)
    let gridMesh = null;
    function buildGridAndAnchorMesh() {
      let gridVertices = [], gridNormals = [], gridColors = [], gridIndices = [];
      for (let x = -SPAN/2; x <= SPAN/2; x += 2.75) {
        gridVertices.push(x, 0.02, -SPAN/2,  x, 0.02, SPAN/2);
        gridNormals.push(0,1,0, 0,1,0);
        gridColors.push(0.22, 0.55, 0.82, 0.42,  0.22, 0.55, 0.82, 0.42);
        gridIndices.push(gridVertices.length/3 - 2, gridVertices.length/3 - 1);
      }
      for (let z = -SPAN/2; z <= SPAN/2; z += 2.75) {
        gridVertices.push(-SPAN/2, 0.02, z,  SPAN/2, 0.02, z);
        gridNormals.push(0,1,0, 0,1,0);
        gridColors.push(0.22, 0.55, 0.82, 0.42,  0.22, 0.55, 0.82, 0.42);
        gridIndices.push(gridVertices.length/3 - 2, gridVertices.length/3 - 1);
      }

      // Vòng tròn Neo Ảo 1.5m (xanh ngọc sáng) xung quanh Phao
      const ringSegs = 32;
      const ringR = 1.8;
      const bx = liveData.posX || 0.0;
      const bz = liveData.posZ || 0.0;
      for (let s = 0; s < ringSegs; s++) {
        let a1 = (s / ringSegs) * Math.PI * 2;
        let a2 = ((s + 1) / ringSegs) * Math.PI * 2;
        gridVertices.push(bx + ringR * Math.cos(a1), 0.05, bz + ringR * Math.sin(a1));
        gridVertices.push(bx + ringR * Math.cos(a2), 0.05, bz + ringR * Math.sin(a2));
        gridNormals.push(0,1,0, 0,1,0);
        gridColors.push(0.0, 0.96, 0.83, 0.90,  0.0, 0.96, 0.83, 0.90);
        gridIndices.push(gridVertices.length/3 - 2, gridVertices.length/3 - 1);
      }

      gridMesh = updateOrCreateMesh(gridMesh, gridVertices, gridNormals, gridColors, gridIndices);
    }
    buildGridAndAnchorMesh();

    // --- 5. RENDER LOOP CHÍNH ---
    let startTime = performance.now();
    gl.enable(gl.DEPTH_TEST);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

    function drawMesh(mesh, isWater, drawMode = gl.TRIANGLES) {
      if (!mesh || !mesh.count) return;
      gl.uniform1f(loc_uIsWater, isWater ? 1.0 : 0.0);

      gl.bindBuffer(gl.ARRAY_BUFFER, mesh.vbo);
      gl.vertexAttribPointer(loc_aPosition, 3, gl.FLOAT, false, 0, 0);
      gl.enableVertexAttribArray(loc_aPosition);

      gl.bindBuffer(gl.ARRAY_BUFFER, mesh.nbo);
      gl.vertexAttribPointer(loc_aNormal, 3, gl.FLOAT, false, 0, 0);
      gl.enableVertexAttribArray(loc_aNormal);

      gl.bindBuffer(gl.ARRAY_BUFFER, mesh.cbo);
      gl.vertexAttribPointer(loc_aColor, 4, gl.FLOAT, false, 0, 0);
      gl.enableVertexAttribArray(loc_aColor);

      gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, mesh.ibo);
      gl.drawElements(drawMode, mesh.count, gl.UNSIGNED_SHORT, 0);
    }

    function render(nowMs) {
      try {
        let timeSec = (nowMs - startTime) * 0.001;

        if (autoRotate) {
          camera.theta += 0.004;
        }
        if (followBuoyMode) {
          camera.target[0] = numOr(liveData.posX, 0.0);
          camera.target[1] = -0.45;
          camera.target[2] = numOr(liveData.posZ, 0.0);
        }

        const displayW = Math.max(64, Math.floor(canvas.clientWidth || window.innerWidth || 1024));
        const displayH = Math.max(64, Math.floor(canvas.clientHeight || window.innerHeight || 768));
        if (canvas.width !== displayW || canvas.height !== displayH) {
          canvas.width = displayW;
          canvas.height = displayH;
          gl.viewport(0, 0, displayW, displayH);
        }

        gl.clearColor(0.015, 0.042, 0.095, 1.0);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

        const aspect = (canvas.height > 0) ? (canvas.width / canvas.height) : 1.33;
        const pMatrix = M4.create();
        M4.perspective(pMatrix, Math.PI / 4, aspect, 0.5, 140.0);

        const vMatrix = M4.create();
        let eyeX = camera.target[0] + camera.radius * Math.sin(camera.phi) * Math.sin(camera.theta);
        let eyeY = camera.target[1] + camera.radius * Math.cos(camera.phi);
        let eyeZ = camera.target[2] + camera.radius * Math.sin(camera.phi) * Math.cos(camera.theta);
        M4.lookAt(vMatrix, [eyeX, eyeY, eyeZ], camera.target, [0, 1, 0]);

        const vpMatrix = M4.create();
        M4.multiply(vpMatrix, pMatrix, vMatrix);

        gl.useProgram(program);
        gl.uniformMatrix4fv(loc_uMatrix, false, vpMatrix);
        gl.uniform1f(loc_uTime, timeSec);
        gl.uniform3f(loc_uLightDir, 0.55, 1.0, 0.45);

        // 1. Vẽ Lòng hồ (Bathymetry)
        if (visibleMeshes.bed) {
          drawMesh(bedMesh, false);
        }

        // 2. Vẽ Lưới toạ độ GPS & Vòng Neo Ảo (Grid & DP Ring)
        if (visibleMeshes.grid) {
          drawMesh(gridMesh, false, gl.LINES);
        }

        // 3. Vẽ Trụ cột quan trắc đa tầng 3D (Water Column Prisms)
        if (visibleMeshes.columns) {
          drawMesh(columnsMesh, false);
        }

        // 4. Vẽ Mô hình Phao USV & Lồng tời cảm biến
        if (visibleMeshes.buoy) {
          drawMesh(buoyMesh, false);
        }

        // 5. Vẽ Các Mặt Cắt Tầng Nước (Volumetric Slices - Bán trong suốt)
        if (visibleMeshes.volume) {
          gl.depthMask(false);
          drawMesh(sliceMesh, false);
          gl.depthMask(true);
        }

        // 6. Vẽ Mặt nước động 3D (Water Surface - Bán trong suốt)
        if (visibleMeshes.surface) {
          gl.depthMask(false);
          drawMesh(surfMesh, true);
          gl.depthMask(true);
        }
      } catch (err) {
        console.warn("WebGL render error:", err);
      }
      requestAnimationFrame(render);
    }
    requestAnimationFrame(render);

    // --- 6. XỬ LÝ CHUỘT / CẢM ỨNG XOAY & PHÓNG TO CAMERA 3D ---
    let isDragging = false;
    let isPanning = false;
    let lastMouseX = 0, lastMouseY = 0;

    canvas.addEventListener('mousedown', (e) => {
      if (e.button === 2) isPanning = true;
      else isDragging = true;
      lastMouseX = e.clientX;
      lastMouseY = e.clientY;
    });

    window.addEventListener('mouseup', () => {
      isDragging = false;
      isPanning = false;
    });

    canvas.addEventListener('contextmenu', e => e.preventDefault());

    window.addEventListener('mousemove', (e) => {
      let dx = e.clientX - lastMouseX;
      let dy = e.clientY - lastMouseY;
      lastMouseX = e.clientX;
      lastMouseY = e.clientY;

      if (isDragging) {
        camera.theta -= dx * 0.006;
        camera.phi   = Math.max(0.12, Math.min(Math.PI / 2 - 0.04, camera.phi - dy * 0.006));
      } else if (isPanning) {
        followBuoyMode = false;
        camera.target[0] -= dx * 0.03;
        camera.target[2] -= dy * 0.03;
      }
    });

    canvas.addEventListener('wheel', (e) => {
      camera.radius = Math.max(4.5, Math.min(65.0, camera.radius + e.deltaY * 0.025));
      e.preventDefault();
    }, { passive: false });

    let touchDist = 0;
    canvas.addEventListener('touchstart', (e) => {
      if (e.touches.length === 1) {
        isDragging = true;
        lastMouseX = e.touches[0].clientX;
        lastMouseY = e.touches[0].clientY;
      } else if (e.touches.length === 2) {
        touchDist = Math.hypot(e.touches[0].clientX - e.touches[1].clientX, e.touches[0].clientY - e.touches[1].clientY);
      }
    });
    canvas.addEventListener('touchmove', (e) => {
      if (e.touches.length === 1 && isDragging) {
        let dx = e.touches[0].clientX - lastMouseX;
        let dy = e.touches[0].clientY - lastMouseY;
        lastMouseX = e.touches[0].clientX;
        lastMouseY = e.touches[0].clientY;
        camera.theta -= dx * 0.007;
        camera.phi   = Math.max(0.12, Math.min(Math.PI / 2 - 0.04, camera.phi - dy * 0.007));
      } else if (e.touches.length === 2) {
        let dist = Math.hypot(e.touches[0].clientX - e.touches[1].clientX, e.touches[0].clientY - e.touches[1].clientY);
        camera.radius = Math.max(4.5, Math.min(65.0, camera.radius - (dist - touchDist) * 0.05));
        touchDist = dist;
      }
    });
    window.addEventListener('touchend', () => isDragging = false);

    // --- 7. TƯƠNG TÁC GIAO DIỆN & CHỌN THÔNG SỐ (UI CONTROLS) ---
    function rebuildAllDynamicMeshes() {
      buildSurfMesh();
      buildSlicePlanes();
      buildWaterColumnsMesh();
      buildBuoyMesh(liveData.depth);
      buildGridAndAnchorMesh();
    }

    function setParam(param) {
      currentParam = param;
      document.querySelectorAll('header button[id^="btn-"]').forEach(b => b.classList.remove('active'));
      const activeBtn = document.getElementById('btn-' + param);
      if (activeBtn) activeBtn.classList.add('active');

      const legTitle = document.getElementById('legParamTitle');
      const legMin = document.getElementById('legMinTxt');
      const legMax = document.getElementById('legMaxTxt');
      const mainLbl = document.getElementById('mainParamLabel');
      const mainVal = document.getElementById('mainParamVal');

      if (param === 'wqi') {
        legTitle.innerText = "VN-WQI (QCVN 08)"; legMin.innerText = "0 (Nguy hại)"; legMax.innerText = "100 (Tốt)";
        mainLbl.innerText = "CHẤT LƯỢNG NƯỚC VN-WQI";
      } else if (param === 'ph') {
        legTitle.innerText = "Độ pH"; legMin.innerText = "4.0 (Axit)"; legMax.innerText = "9.0 (Kiềm)";
        mainLbl.innerText = "ĐỘ pH HIỆN TẠI";
      } else if (param === 'do') {
        legTitle.innerText = "Oxy hòa tan DO (mg/L)"; legMin.innerText = "0 mg/L"; legMax.innerText = "10 mg/L";
        mainLbl.innerText = "OXY HÒA TAN DO";
      } else if (param === 'tds') {
        legTitle.innerText = "Tổng khoáng TDS (ppm)"; legMin.innerText = "0 ppm"; legMax.innerText = "1000 ppm";
        mainLbl.innerText = "TỔNG KHOÁNG TDS";
      } else if (param === 'ntu') {
        legTitle.innerText = "Độ đục (NTU)"; legMin.innerText = "0 NTU"; legMax.innerText = "300 NTU";
        mainLbl.innerText = "ĐỘ ĐỤC NTU";
      } else if (param === 'temp') {
        legTitle.innerText = "Nhiệt độ nước (°C)"; legMin.innerText = "20°C"; legMax.innerText = "35°C";
        mainLbl.innerText = "NHIỆT ĐỘ NƯỚC";
      }
      mainVal.innerText = mainValueText(param);
      rebuildAllDynamicMeshes();
    }

    function selectLayer(idx) {
      const cards = ['card-l1', 'card-l2', 'card-l3'];
      cards.forEach((c, i) => {
        let el = document.getElementById(c);
        if (i === idx) {
          el.classList.add('border-cyan-400', 'bg-cyan-950/40');
        } else {
          el.classList.remove('border-cyan-400', 'bg-cyan-950/40');
        }
      });
      const depths = [0.3, 1.0, 1.8];
      document.getElementById('depthSlider').value = depths[idx];
      onDepthSlide(depths[idx]);
    }

    function onDepthSlide(val) {
      val = parseFloat(val);
      sliceDepthFilter = val;
      const txt = document.getElementById('sliderValTxt');
      const depthTxt = document.getElementById('curDepthTxt');
      if (val === 0) {
        txt.innerText = "Tất cả tầng";
        depthTxt.innerText = "0.00m";
      } else {
        txt.innerText = val.toFixed(2) + "m";
        depthTxt.innerText = val.toFixed(2) + "m";
      }
      buildSlicePlanes();
    }

    function toggleMesh(type) {
      visibleMeshes[type] = !visibleMeshes[type];
    }

    function setCameraView(preset) {
      document.querySelectorAll('footer button[id^="cam-"]').forEach(b => b.classList.remove('active'));
      const activeBtn = document.getElementById('cam-' + preset);
      if (activeBtn) activeBtn.classList.add('active');

      followBuoyMode = false;
      if (preset === 'iso') {
        camera.radius = 25.0; camera.theta = 0.72; camera.phi = 1.02; camera.target = [0, -0.7, 0];
      } else if (preset === 'top') {
        camera.radius = 24.0; camera.theta = 0.0;  camera.phi = 0.14; camera.target = [0, 0, 0];
      } else if (preset === 'side') {
        camera.radius = 22.0; camera.theta = 1.57; camera.phi = 1.42; camera.target = [0, -0.9, 0];
      } else if (preset === 'buoy') {
        followBuoyMode = true;
        camera.radius = 11.5; camera.theta = 0.65; camera.phi = 1.08;
        camera.target = [numOr(liveData.posX, 0.0), -0.45, numOr(liveData.posZ, 0.0)];
      }
    }

    function toggleAutoRotate() {
      autoRotate = !autoRotate;
      document.getElementById('btnRotate').innerText = autoRotate ? "Xoay 360°: BẬT" : "Xoay 360°: Tắt";
    }

    function resetCamera() {
      setCameraView('iso');
    }

    function exportSnapshot() {
      const a = document.createElement('a');
      a.download = `3D-Water-Map-${new Date().toISOString().slice(0,19)}.png`;
      a.href = canvas.toDataURL('image/png');
      a.click();
    }

    function toggleSource() {
      isLiveMode = !isLiveMode;
      const badge = document.getElementById('connBadge');
      const btn = document.getElementById('btnSource');
      const demoBanner = document.getElementById('demoBanner');
      if (isLiveMode) {
        badge.className = "text-xs px-2 py-0.5 rounded-full bg-emerald-950 text-emerald-400 border border-emerald-700/60 font-mono";
        badge.innerText = hasTelemetry ? "LIVE LoRa GCS" : "CHỜ DỮ LIỆU...";
        btn.innerText = "Nguồn: Phao Thật";
        if (demoBanner) demoBanner.classList.add('hidden');
      } else {
        badge.className = "text-xs px-2 py-0.5 rounded-full bg-amber-950 text-amber-400 border border-amber-700/60 font-mono";
        badge.innerText = "MÔ PHỎNG HỒ 3D (DEMO)";
        btn.innerText = "Nguồn: Mô Phỏng";
        if (demoBanner) demoBanner.classList.remove('hidden');
      }
      rebuildAllDynamicMeshes();
    }

    // U-01: Trả về null khi chưa có cảm biến nào hợp lệ (tuyệt đối không trả về 85 ảo!)
    function calcWqiJs(ph, tds, ntu, doVal) {
      if (ph <= 0.1 && tds <= 0.1 && ntu <= 0.1) return null;
      let wPh = 100;
      if (ph > 0.1) {
        if (ph < 5.5 || ph > 9.0) wPh = 10;
        else if (ph < 6.5) wPh = 50 + ((ph - 5.5) / 1.0) * 50;
        else if (ph > 8.5) wPh = 50 + ((9.0 - ph) / 0.5) * 50;
      }
      let wDo = 100;
      if (doVal > 0.1) {
        if (doVal < 2.0) wDo = 10;
        else if (doVal < 4.0) wDo = 30;
        else if (doVal < 6.0) wDo = 65;
        else wDo = 100;
      }
      let wNtu = 100;
      if (ntu > 0.1) {
        if (ntu <= 5) wNtu = 100;
        else if (ntu <= 25) wNtu = 100 - (ntu - 5);
        else if (ntu <= 75) wNtu = 80 - (ntu - 25) * 0.6;
        else if (ntu <= 200) wNtu = 50 - (ntu - 75) * 0.2;
        else wNtu = 15;
      }
      let wTds = 100;
      if (tds > 0.1) {
        if (tds <= 300) wTds = 100;
        else if (tds <= 600) wTds = 100 - (tds - 300) * (20 / 300);
        else if (tds <= 1200) wTds = 80 - (tds - 600) * (35 / 600);
        else wTds = 30;
      }
      let sum = 0, weights = 0;
      if (ph > 0.1) { sum += wPh * 0.25; weights += 0.25; }
      if (doVal > 0.1) { sum += wDo * 0.30; weights += 0.30; }
      if (ntu > 0.1) { sum += wNtu * 0.25; weights += 0.25; }
      if (tds > 0.1) { sum += wTds * 0.20; weights += 0.20; }
      return weights > 0 ? Math.round(sum / weights) : null;
    }

    // --- 8. ĐỒNG BỘ TELEMETRY ĐA NGUỒN (LOCALHOST:8080 & ESP32 AP 192.168.4.1) ---
    async function fetchTelemetryWithFallback() {
      const urls = [];
      if (location.protocol === 'http:' || location.protocol === 'https:') {
        urls.push('/api/telemetry');
      }
      urls.push('http://localhost:8080/api/telemetry');
      urls.push('http://192.168.4.1/api/telemetry');

      for (const u of urls) {
        try {
          const ctrl = new AbortController();
          const timer = setTimeout(() => ctrl.abort(), 850);
          const res = await fetch(u, { signal: ctrl.signal });
          clearTimeout(timer);
          if (res.ok) {
            return await res.json();
          }
        } catch (e) {}
      }
      return null;
    }

    const updateCard = (prefix, data) => {
      const elPh = document.getElementById(prefix + '-ph');
      const elTds = document.getElementById(prefix + '-tds');
      const elNtu = document.getElementById(prefix + '-ntu');
      const elT = document.getElementById(prefix + '-temp');
      const elB = document.getElementById(prefix + '-badge');
      if (!data || !data.valid) {
        if (elPh) elPh.innerText = '--';
        if (elTds) elTds.innerText = '-- ppm';
        if (elNtu) elNtu.innerText = '--';
        if (elT) elT.innerText = '--°C';
        if (elB) elB.innerText = 'WQI: --';
        return;
      }
      if (elPh) elPh.innerText = data.ph > 0.1 ? data.ph.toFixed(2) : '0.00';
      if (elTds) elTds.innerText = Math.round(data.tds) + ' ppm';
      if (elNtu) elNtu.innerText = Math.round(data.ntu);
      if (elT) elT.innerText = data.temp.toFixed(1) + '°C';
      if (elB) elB.innerText = 'WQI: ' + ((data.wqi !== null && data.wqi > 0) ? data.wqi : '--');
    };

    let teleInFlight = false;
    async function syncGcsTelemetry() {
      if (isHistoryReplay) return;
      if (teleInFlight) return;
      teleInFlight = true;
      try {
        if (!isLiveMode) {
          // U-02: Chế độ mô phỏng tự hành quanh lòng hồ (Không ghi vào surveyPoints)
          const t = performance.now() * 0.0004;
          liveData.posX = Math.sin(t) * 4.2;
          liveData.posZ = Math.cos(t * 0.8) * 3.5;
          liveData.hdg  = ((Math.atan2(Math.cos(t), -Math.sin(t * 0.8)) * 180 / Math.PI) + 360) % 360;
          liveData.depth = 0.3 + (Math.sin(t * 2.5) * 0.5 + 0.5) * 1.5;
          updateHUD();
          return;
        }

        const d = await fetchTelemetryWithFallback();
        if (d && (numOr(d.seq, 0) > 0 || numOr(d.bat, 0) > 0.5 || numOr(d.temp, 0) > 0.5)) {
          hasTelemetry = true;
          const badge = document.getElementById('connBadge');
          if (badge && isLiveMode) {
            badge.innerText = d.online ? "LIVE LoRa GCS" : "CẦU NỐI SERIAL";
          }
          liveData.seq   = numOr(d.seq,   liveData.seq);
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
          liveData.hdg   = numOr(d.hdg,   liveData.hdg);
          liveData.lat   = numOr(d.lat,   0.0);
          liveData.lng   = numOr(d.lng,   0.0);
          liveData.gpsFix = Boolean(d.gps_fix || (Math.abs(liveData.lat) > 0.001 && Math.abs(liveData.lng) > 0.001));
          liveData.rssi  = numOr(d.rssi,  liveData.rssi);
          liveData.snr   = numOr(d.snr,   liveData.snr);

          const wifiBadge = document.getElementById('wifiBadge');
          if (wifiBadge) {
            wifiBadge.innerText = 'WiFi: Mang bao tri' + (d.ip && d.ip !== '127.0.0.1' && d.ip !== '192.168.4.1' ? ' (' + d.ip + ')' : '');
          }

          // Cập nhật liên tục thời gian thực vào tầng nước hiện tại theo độ sâu thực tế
          if (liveData.depth < 0.65) {
            profileData.surface = { depth: 0.3, ph: liveData.ph, tds: liveData.tds, ntu: liveData.ntu, temp: liveData.temp, do: liveData.do, wqi: liveData.wqi, valid: true };
            updateCard('l1', profileData.surface);
          } else if (liveData.depth < 1.40) {
            profileData.middle = { depth: 1.0, ph: liveData.ph, tds: liveData.tds, ntu: liveData.ntu, temp: liveData.temp, do: liveData.do, wqi: liveData.wqi, valid: true };
            updateCard('l2', profileData.middle);
          } else {
            profileData.bottom = { depth: 1.8, ph: liveData.ph, tds: liveData.tds, ntu: liveData.ntu, temp: liveData.temp, do: liveData.do, wqi: liveData.wqi, valid: true };
            updateCard('l3', profileData.bottom);
          }

          if (d.prf) {
            if (d.prf.s && d.prf.s.v) {
              profileData.surface = { depth: 0.3, ph: d.prf.s.ph, tds: d.prf.s.tds, ntu: d.prf.s.ntu, temp: d.prf.s.t, do: d.prf.s.do, wqi: calcWqiJs(d.prf.s.ph, d.prf.s.tds, d.prf.s.ntu, d.prf.s.do), valid: true };
              updateCard('l1', profileData.surface);
            }
            if (d.prf.m && d.prf.m.v) {
              profileData.middle = { depth: 1.0, ph: d.prf.m.ph, tds: d.prf.m.tds, ntu: d.prf.m.ntu, temp: d.prf.m.t, do: d.prf.m.do, wqi: calcWqiJs(d.prf.m.ph, d.prf.m.tds, d.prf.m.ntu, d.prf.m.do), valid: true };
              updateCard('l2', profileData.middle);
            }
            if (d.prf.b && d.prf.b.v) {
              profileData.bottom = { depth: 1.8, ph: d.prf.b.ph, tds: d.prf.b.tds, ntu: d.prf.b.ntu, temp: d.prf.b.t, do: d.prf.b.do, wqi: calcWqiJs(d.prf.b.ph, d.prf.b.tds, d.prf.b.ntu, d.prf.b.do), valid: true };
              updateCard('l3', profileData.bottom);
            }
          }
        }
        updateHUD();
      } finally {
        teleInFlight = false;
      }
    }

    // U-03: Vòng lặp đồng bộ Telemetry có khoá chống chồng lệnh (Recursive setTimeout 800ms)
    function scheduleNextTelemetrySync() {
      syncGcsTelemetry().finally(() => {
        setTimeout(scheduleNextTelemetrySync, 800);
      });
    }
    scheduleNextTelemetrySync();

    // U-01: Khởi tạo mảng khảo sát RỖNG 100% (chỉ ghi nhận khi có gói tin phần cứng thật!)
    let surveyPoints = [];

    function mainValueText(param) {
      if (!hasTelemetry && isLiveMode) return '--';
      switch (param) {
        case 'wqi':  return (liveData.wqi !== null && liveData.wqi > 0) ? String(liveData.wqi) : '--';
        case 'ph':   return (liveData.ph > 0.1) ? liveData.ph.toFixed(2) : '0.00';
        case 'do':   return (liveData.do > 0) ? liveData.do.toFixed(2) + ' mg/L' : '--';
        case 'tds':  return Math.round(liveData.tds) + ' ppm';
        case 'ntu':  return Math.round(liveData.ntu) + ' NTU';
        case 'temp': return (liveData.temp > 0) ? liveData.temp.toFixed(1) + ' °C' : '--';
      }
      return '--';
    }

    function updateHUD() {
      if (!hasTelemetry && isLiveMode) {
        document.getElementById('mainParamVal').innerText = '--';
        rebuildAllDynamicMeshes();
        return;
      }
      const phConnected = liveData.ph > 0.1;
      const clampedBat  = Math.max(0.0, liveData.bat || 0.0);
      const batPct      = Math.min(100, Math.max(0, Math.round((clampedBat - 9.0) / 3.6 * 100)));

      document.getElementById('hudSeq').innerText   = '#' + (liveData.seq || 0);
      document.getElementById('hudPH').innerText    = phConnected ? liveData.ph.toFixed(2) : '0.00 (Chưa cắm)';
      document.getElementById('hudTDS').innerText   = Math.round(liveData.tds);
      document.getElementById('hudEC').innerText    = Math.round(liveData.ec > 0 ? liveData.ec : liveData.tds * 2.0);
      document.getElementById('hudNTU').innerText   = Math.round(liveData.ntu);
      document.getElementById('hudTemp').innerText  = liveData.temp.toFixed(1);
      document.getElementById('hudDO').innerText    = liveData.do.toFixed(2);
      document.getElementById('hudBat').innerText   = clampedBat.toFixed(2) + 'V (' + batPct + '%)';
      document.getElementById('hudAmp').innerText   = liveData.amp.toFixed(2) + 'A • ' + (clampedBat * liveData.amp).toFixed(1) + 'W';
      document.getElementById('hudDepth').innerText = liveData.depth.toFixed(2) + 'm' + (liveData.depth < 0.05 ? ' (Home)' : '');
      document.getElementById('hudImu').innerText   = `R: ${liveData.roll.toFixed(1)}° • P: ${liveData.pitch.toFixed(1)}° • H: ${liveData.hdg.toFixed(0)}°`;
      document.getElementById('hudGps').innerText   = (Math.abs(liveData.lat) > 0.001) ? `${liveData.lat.toFixed(5)}°N, ${liveData.lng.toFixed(5)}°E` : `0.00000°N, 0.00000°E (Chờ Fix)`;
      document.getElementById('hudLora').innerText  = `${liveData.rssi} dBm (SNR ${liveData.snr}dB)`;

      const curDepthEl = document.getElementById('curDepthTxt');
      if (curDepthEl && sliceDepthFilter === 0) {
        curDepthEl.innerText = liveData.depth.toFixed(2) + 'm';
      }

      const phStatus = document.getElementById('hudPhStatus');
      if (phStatus) {
        if (!phConnected) {
          phStatus.innerText = 'Ngắt áp giả (GPIO 7 hở)';
          phStatus.className = 'text-[10px] text-amber-400';
        } else if (liveData.ph < 6.5) {
          phStatus.innerText = 'Axit nhẹ';
          phStatus.className = 'text-[10px] text-amber-400';
        } else if (liveData.ph <= 8.5) {
          phStatus.innerText = 'Trung tính / An toàn';
          phStatus.className = 'text-[10px] text-emerald-400';
        } else {
          phStatus.innerText = 'Kiềm';
          phStatus.className = 'text-[10px] text-amber-400';
        }
      }

      // H-10: Phân loại 4 bậc chuẩn QCVN 08:2023/BTNMT (A1 >= 90, A2 75-89, B1 50-74, B2 < 50)
      const wqiBadge = document.getElementById('wqiStatusBadge');
      const wqiDesc  = document.getElementById('wqiDesc');
      const wqiVal   = liveData.wqi;
      if (wqiBadge && wqiDesc) {
        if (wqiVal === null || wqiVal <= 0) {
          wqiBadge.innerText = 'CHỜ CẢM BIẾN';
          wqiBadge.className = 'text-xs font-bold px-2 py-0.5 rounded bg-slate-900/60 text-slate-400 border border-slate-700';
          wqiDesc.innerText  = 'Chưa đủ thông số tính WQI';
        } else if (wqiVal >= 90) {
          wqiBadge.innerText = 'RẤT TỐT (MỨC A1)';
          wqiBadge.className = 'text-xs font-bold px-2 py-0.5 rounded bg-emerald-950 text-emerald-300 border border-emerald-700';
          wqiDesc.innerText  = 'QCVN 08:2023 (Cấp nước sinh hoạt)';
        } else if (wqiVal >= 75) {
          wqiBadge.innerText = 'AN TOÀN (MỨC A2)';
          wqiBadge.className = 'text-xs font-bold px-2 py-0.5 rounded bg-emerald-950 text-emerald-300 border border-emerald-700';
          wqiDesc.innerText  = 'QCVN 08:2023 (Sinh hoạt sau xử lý)';
        } else if (wqiVal >= 50) {
          wqiBadge.innerText = 'CẢNH BÁO (MỨC B1)';
          wqiBadge.className = 'text-xs font-bold px-2 py-0.5 rounded bg-amber-950 text-amber-300 border border-amber-700';
          wqiDesc.innerText  = 'QCVN 08:2023 (Tưới tiêu / Giao thông)';
        } else {
          wqiBadge.innerText = 'Ô NHIỄM (MỨC B2)';
          wqiBadge.className = 'text-xs font-bold px-2 py-0.5 rounded bg-slate-900 text-red-400 border border-red-400';
          wqiDesc.innerText  = 'Vượt ngưỡng QCVN 08:2023/BTNMT';
        }
      }

      document.getElementById('mainParamVal').innerText = mainValueText(currentParam);

      // U-02: Chỉ ghi nhận điểm khảo sát vào CSV khi đang ở chế độ Phao Thật (isLiveMode) và có mã gói thật
      if (isLiveMode && hasTelemetry && liveData.seq > 0 && (!surveyPoints.length || surveyPoints[surveyPoints.length - 1].seq !== liveData.seq)) {
        surveyPoints.push({
          time: new Date().toLocaleTimeString(),
          seq: liveData.seq,
          lat: liveData.lat.toFixed(6),
          lng: liveData.lng.toFixed(6),
          depth: liveData.depth.toFixed(2),
          wqi: liveData.wqi || 0,
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

      rebuildAllDynamicMeshes();
    }
    updateHUD();

    // --- U-04: TẢI & TUA LỊCH SỬ KHẢO SÁT (TIME SLIDER REPLAY) ---
    async function loadHistoryReplay() {
      const urls = ['/api/history', 'http://localhost:8080/api/history'];
      let rows = null;
      for (const u of urls) {
        try {
          const res = await fetch(u);
          if (res.ok) {
            const j = await res.json();
            if (j && Array.isArray(j.rows) && j.rows.length > 0) {
              rows = j.rows;
              break;
            }
          }
        } catch (e) {}
      }
      if (!rows || !rows.length) {
        if (surveyPoints.length > 0) {
          rows = surveyPoints;
        } else {
          alert("Chưa có lịch sử khảo sát trong gsheets_offline.csv hoặc phiên hiện tại!");
          return;
        }
      }
      historyRecords = rows;
      const slider = document.getElementById('timeSlider');
      if (slider) {
        slider.min = 0;
        slider.max = historyRecords.length - 1;
        slider.value = historyRecords.length - 1;
      }
      onTimeSlide(historyRecords.length - 1);
    }

    function onTimeSlide(idx) {
      idx = parseInt(idx, 10);
      if (!historyRecords.length || isNaN(idx) || idx < 0 || idx >= historyRecords.length) return;
      isHistoryReplay = true;
      hasTelemetry = true;
      const r = historyRecords[idx];
      liveData.seq   = numOr(Number(r.seq), idx + 1);
      liveData.ph    = numOr(Number(r.ph), 0.0);
      liveData.tds   = numOr(Number(r.tds), 0.0);
      liveData.ntu   = numOr(Number(r.ntu), 0.0);
      liveData.temp  = numOr(Number(r.temp), 0.0);
      liveData.do    = numOr(Number(r.do), 0.0);
      liveData.wqi   = numOr(Number(r.wqi), calcWqiJs(liveData.ph, liveData.tds, liveData.ntu, liveData.do));
      liveData.bat   = numOr(Number(r.bat), liveData.bat);
      liveData.depth = numOr(Number(r.depth || r.dep), 0.0);
      liveData.lat   = numOr(Number(r.lat), 0.0);
      liveData.lng   = numOr(Number(r.lng), 0.0);
      const lbl = document.getElementById('timeSliderLbl');
      if (lbl) lbl.innerText = `${r.time || ('#' + liveData.seq)} (${idx + 1}/${historyRecords.length})`;
      updateHUD();
    }

    function exitHistoryReplay() {
      isHistoryReplay = false;
      const lbl = document.getElementById('timeSliderLbl');
      if (lbl) lbl.innerText = 'Trực tiếp (Live)';
      syncGcsTelemetry();
    }

    function exportSurveyCsv() {
      let csv = "ThoiGian,MaGoiSeq,ViDoLat,KinhDoLng,DoSauMet,VN_WQI,DoPH,TongKhoangTDS_ppm,DoDucNTU,NhietDoNuocC,OxyHoaTanDO_mgL\n";
      surveyPoints.forEach(p => {
        csv += `${p.time},${p.seq},${p.lat},${p.lng},${p.depth},${p.wqi},${p.ph},${p.tds},${p.ntu},${p.temp},${p.do}\n`;
      });
      const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
      const link = document.createElement("a");
      link.href = URL.createObjectURL(blob);
      link.download = `KhaoSat_ThuyVan_USV_${new Date().toISOString().slice(0,10)}.csv`;
      link.click();
    }

    // --- 9. HỆ THỐNG ÂM THANH & GIỌNG NÓI ĐIỀU HƯỚNG THÔNG MINH ---
    let voiceEnabled = true;
    let lastVoiceMs = 0;

    function toggleVoiceAlert() {
      voiceEnabled = !voiceEnabled;
      const btn = document.getElementById('btnVoice');
      if (btn) {
        btn.innerText = voiceEnabled ? "🔊 Giọng Nói: BẬT" : "🔇 Giọng Nói: TẮT";
      }
      if (voiceEnabled) speakVoice("Hệ thống âm thanh trợ lý quan trắc đã sẵn sàng.");
    }

    function speakVoice(text) {
      if (!voiceEnabled || !('speechSynthesis' in window)) return;
      let now = Date.now();
      if (now - lastVoiceMs < 3500) return;
      lastVoiceMs = now;
      window.speechSynthesis.cancel();
      let u = new SpeechSynthesisUtterance(text);
      u.lang = 'vi-VN';
      u.rate = 1.05;
      window.speechSynthesis.speak(u);
    }

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
      sendCmdWithFallback(`hdg=${bearing}`);
      speakVoice(`Đã gửi lệnh khóa hướng la bàn ${bearing} độ.`);
      alert(`Đã gửi hải trình xuống Phao USV! Hướng mục tiêu: ${bearing}°, số mốc: ${waypoints.length}`);
    }

    canvas.addEventListener('click', (ev) => {
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
    });

    // --- 11. XUẤT BÁO CÁO THỦY VĂN CHUYÊN NGHIỆP CÓ THỂ IN ẤN ---
    function generatePrintReport() {
      const nowStr = new Date().toLocaleString('vi-VN');
      const clampedBat = Math.max(0.0, liveData.bat || 0.0);
      const reportHtml = `<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="utf-8">
  <title>Báo Cáo Quan Trắc Thủy Văn - USV ChemStation AI</title>
  <style>
    body { font-family: 'Times New Roman', serif; margin: 40px; color: #111; line-height: 1.5; }
    .header { text-align: center; border-bottom: 2px solid #222; padding-bottom: 12px; margin-bottom: 20px; }
    .title { font-size: 20px; font-weight: bold; text-transform: uppercase; }
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
    <div>Hệ Thống Phao USV Tự Hành ChemStation AI v4.3-AUDIT-PRO • Tiêu chuẩn QCVN 08:2023/BTNMT</div>
    <div>Thời điểm trích xuất: ${nowStr} • Mã trạm bờ: GCS-ESP32 (COM12)</div>
  </div>
  <h3>1. Thông Số Tổng Quan Hiện Trường</h3>
  <table>
    <tr><th>Chỉ số VN-WQI</th><td><b>${liveData.wqi !== null ? liveData.wqi + '/100' : '--'}</b></td><th>Tọa độ GPS</th><td>${liveData.lat.toFixed(5)}°N, ${liveData.lng.toFixed(5)}°E</td></tr>
    <tr><th>Độ pH</th><td>${liveData.ph > 0.1 ? liveData.ph.toFixed(2) : 'Chưa cắm đầu dò'}</td><th>Oxy hòa tan (DO ước tính)</th><td>${liveData.do.toFixed(2)} mg/L</td></tr>
    <tr><th>Tổng khoáng TDS</th><td>${Math.round(liveData.tds)} ppm</td><th>Độ dẫn điện EC (quy đổi)</th><td>${Math.round(liveData.ec || liveData.tds * 2)} µS/cm</td></tr>
    <tr><th>Độ đục NTU</th><td>${Math.round(liveData.ntu)} NTU</td><th>Nhiệt độ nước</th><td>${liveData.temp.toFixed(1)} °C</td></tr>
    <tr><th>Độ sâu khảo sát</th><td>${liveData.depth.toFixed(2)} m</td><th>Điện áp Pin 3S</th><td>${clampedBat.toFixed(2)} V</td></tr>
  </table>
  <h3>2. Phân Tầng Nước 3 Độ Sâu (Profile)</h3>
  <table>
    <tr><th>Tầng Nước</th><th>Độ Sâu</th><th>Độ pH</th><th>Khoáng TDS</th><th>Độ Đục</th><th>Nhiệt Độ</th><th>VN-WQI</th></tr>
    <tr><td><b>1. Tầng Mặt</b></td><td>0.30 m</td><td>${profileData.surface.valid ? profileData.surface.ph : '--'}</td><td>${profileData.surface.valid ? profileData.surface.tds + ' ppm' : '--'}</td><td>${profileData.surface.valid ? profileData.surface.ntu + ' NTU' : '--'}</td><td>${profileData.surface.valid ? profileData.surface.temp + '°C' : '--'}</td><td>${profileData.surface.wqi !== null ? 'WQI ' + profileData.surface.wqi : '--'}</td></tr>
    <tr><td><b>2. Tầng Giữa</b></td><td>1.00 m</td><td>${profileData.middle.valid ? profileData.middle.ph : '--'}</td><td>${profileData.middle.valid ? profileData.middle.tds + ' ppm' : '--'}</td><td>${profileData.middle.valid ? profileData.middle.ntu + ' NTU' : '--'}</td><td>${profileData.middle.valid ? profileData.middle.temp + '°C' : '--'}</td><td>${profileData.middle.wqi !== null ? 'WQI ' + profileData.middle.wqi : '--'}</td></tr>
    <tr><td><b>3. Tầng Đáy</b></td><td>1.80 m</td><td>${profileData.bottom.valid ? profileData.bottom.ph : '--'}</td><td>${profileData.bottom.valid ? profileData.bottom.tds + ' ppm' : '--'}</td><td>${profileData.bottom.valid ? profileData.bottom.ntu + ' NTU' : '--'}</td><td>${profileData.bottom.valid ? profileData.bottom.temp + '°C' : '--'}</td><td>${profileData.bottom.wqi !== null ? 'WQI ' + profileData.bottom.wqi : '--'}</td></tr>
  </table>
  <div style="text-align:center"><button onclick="window.print()" style="padding:10px 20px;font-size:14px;cursor:pointer">🖨️ IN BẢN BÁO CÁO NÀY</button></div>
</body>
</html>`;
      let win = window.open('', '_blank');
      win.document.write(reportHtml);
      win.document.close();
    }

    function sendCmdWithFallback(queryStr) {
      const tk = localStorage.getItem('gcs_token') || 'chemstation2026';
      const hdr = { 'X-GCS-Token': tk };
      const q = queryStr + '&token=' + encodeURIComponent(tk);
      fetch('/api/cmd?' + q, { headers: hdr }).catch(() => {
        fetch('http://localhost:8080/api/cmd?' + q, { headers: hdr }).catch(() => {
          fetch('http://192.168.4.1/api/cmd?' + q, { headers: hdr }).catch(() => {});
        });
      });
    }

    function triggerBuoyAction(act) {
      if (act === 'auto') {
        sendCmdWithFallback('w=3');
        alert("Đã gửi lệnh xuống Phao: TỰ ĐỘNG THẢ TỜI ĐO 3 TẦNG NƯỚC (0.3m -> 1.0m -> 1.8m)!");
      } else if (act === 'anchor_toggle') {
        const btnAnc = document.getElementById('btnAnchorAct');
        const isCurrentlyOn = btnAnc && btnAnc.innerText.includes('BẬT');
        if (isCurrentlyOn) {
          sendCmdWithFallback('anchor_off=1');
          btnAnc.innerText = 'NEO ẢO: TẮT';
        } else {
          sendCmdWithFallback('anchor_on=1');
          btnAnc.innerText = 'NEO ẢO: BẬT';
        }
      } else if (act === 'winch_home' || act === 'home') {
        sendCmdWithFallback('w=2');
        alert("Đã gửi lệnh xuống Phao: THU TỜI VỀ GỐC HOME (0.0m)!");
      } else if (act === 'goto_home') {
        sendCmdWithFallback('goto=1');
        alert("Đã gửi lệnh xuống Phao: TỰ HÀNH QUAY VỀ HOME GPS!");
      } else if (act === 'clean') {
        sendCmdWithFallback('clean=1');
        alert("Đã gửi lệnh xuống Phao: KÍCH HOẠT CHU TRÌNH SỤC RỬA ĐẦU DÒ TỰ ĐỘNG (30 GIÂY)!");
      } else if (act === 'sample') {
        sendCmdWithFallback('sample=1');
        alert("Đã gửi lệnh xuống Phao: KÍCH HOẠT BƠM LẤY MẪU NƯỚC TỰ ĐỘNG (20 GIÂY)!");
      } else if (act === 'stop_all') {
        sendCmdWithFallback('stopall=1');
        alert("Đã gửi lệnh xuống Phao: DỪNG KHẨN CẤP TOÀN BỘ!");
      }
    }
  