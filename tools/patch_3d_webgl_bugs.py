import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

with open('src/ground_station/web_3d_map.h', 'r', encoding='utf-8', errors='ignore') as f:
    code = f.read()

# 1. Fix grid creation & binding
old_grid_code = """    const gridVbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, gridVbo);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(gridVertices), gl.STATIC_DRAW);
    const gridCbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, gridCbo);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(gridColors), gl.STATIC_DRAW);
    const gridIbo = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, gridIbo);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(gridIndices), gl.STATIC_DRAW);"""

new_grid_code = """    let gridMesh = createMeshBuffers(gridVertices, gridNormals, gridColors, gridIndices);"""

if old_grid_code in code:
    code = code.replace(old_grid_code, new_grid_code, 1)
    print("Fixed gridMesh buffer creation")
else:
    print("old_grid_code not found")

# 2. Fix grid drawing in render()
old_grid_draw = """      // 2. Vẽ Lưới toạ độ GPS (Grid)
      if (visibleMeshes.grid) {
        gl.uniform1i(loc_uIsWater, 0);
        gl.bindBuffer(gl.ARRAY_BUFFER, gridVbo);
        gl.vertexAttribPointer(loc_aPosition, 3, gl.FLOAT, false, 0, 0);
        gl.bindBuffer(gl.ARRAY_BUFFER, gridCbo);
        gl.vertexAttribPointer(loc_aColor, 4, gl.FLOAT, false, 0, 0);
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, gridIbo);
        gl.drawElements(gl.LINES, gridIndices.length, gl.UNSIGNED_SHORT, 0);
      }"""

new_grid_draw = """      // 2. Vẽ Lưới toạ độ GPS (Grid)
      if (visibleMeshes.grid && gridMesh) {
        drawMesh(gridMesh, false, gl.LINES);
      }"""

if old_grid_draw in code:
    code = code.replace(old_grid_draw, new_grid_draw, 1)
    print("Fixed grid drawing in render()")
else:
    print("old_grid_draw not found")

# 3. Fix canvas resizing & aspect ratio NaN bug
old_resize = """      // Cập nhật kích thước khung nhìn
      if (canvas.width !== window.innerWidth || canvas.height !== window.innerHeight) {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
        gl.viewport(0, 0, canvas.width, canvas.height);
      }

      gl.clearColor(0.012, 0.035, 0.08, 1.0);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

      // Tính ma trận phối cảnh và góc nhìn Camera Orbit
      const pMatrix = M4.create();
      M4.perspective(pMatrix, Math.PI / 4, canvas.width / canvas.height, 0.5, 120.0);"""

new_resize = """      // Cập nhật kích thước khung nhìn an toàn (chống lỗi NaN khi ẩn/trong iframe)
      const displayW = Math.max(16, Math.floor(canvas.clientWidth || window.innerWidth || 800));
      const displayH = Math.max(16, Math.floor(canvas.clientHeight || window.innerHeight || 600));
      if (canvas.width !== displayW || canvas.height !== displayH) {
        canvas.width = displayW;
        canvas.height = displayH;
        gl.viewport(0, 0, displayW, displayH);
      }

      gl.clearColor(0.012, 0.035, 0.08, 1.0);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

      // Tính ma trận phối cảnh và góc nhìn Camera Orbit (an toàn không bao giờ NaN)
      const aspect = (canvas.height > 0) ? (canvas.width / canvas.height) : 1.33;
      const pMatrix = M4.create();
      M4.perspective(pMatrix, Math.PI / 4, aspect, 0.5, 120.0);"""

if old_resize in code:
    code = code.replace(old_resize, new_resize, 1)
    print("Fixed canvas resizing and aspect ratio NaN bug")
else:
    print("old_resize not found")

# 4. Wrap render() body in try-catch so requestAnimationFrame never dies
old_raf = """      requestAnimationFrame(render);
    }
    requestAnimationFrame(render);"""

new_raf = """      requestAnimationFrame(render);
    } catch(renderErr) {
      console.warn("Render loop warning:", renderErr);
      requestAnimationFrame(render);
    }
  }
  requestAnimationFrame(render);"""

# Let's inspect where render(nowMs) starts
render_start = "    function render(nowMs) {"
render_try = "    function render(nowMs) {\n    try {"
if render_start in code and render_try not in code:
    code = code.replace(render_start, render_try, 1)
    if old_raf in code:
        code = code.replace(old_raf, new_raf, 1)
        print("Wrapped render loop in try-catch guard")

with open('src/ground_station/web_3d_map.h', 'w', encoding='utf-8') as f:
    f.write(code)

print("Applied 3D WebGL fixes to web_3d_map.h successfully!")
