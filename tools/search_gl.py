with open('temp_3d_script.js', 'r', encoding='utf-8') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if any(k in line for k in ['getContext', 'glCanvas', 'initGL', 'initWebGL', 'requestAnimationFrame', 'window.addEventListener', 'resizeCanvas', 'gl.createShader']):
        print(f"{i+1:3d}: {line.strip()[:85]}")
