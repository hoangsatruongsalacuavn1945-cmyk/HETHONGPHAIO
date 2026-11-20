with open('temp_3d_script.js', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

# Check for gridNbo
print("gridNbo in text:", "gridNbo" in text)
# Check for gridNormals
print("gridNormals in text:", "gridNormals" in text)
# Check all references to visibleMeshes
import re
for m in re.finditer(r'visibleMeshes\.(\w+)', text):
    print("Mesh toggle:", m.group(1))
