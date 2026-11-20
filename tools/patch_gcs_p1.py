import sys
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

with open('src/ground_station/main.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    code = f.read()

# 1. Add Sprite declarations right after TFT_eSPI tft = TFT_eSPI();
old_tft_decl = "TFT_eSPI tft = TFT_eSPI();"
new_tft_decl = """TFT_eSPI tft = TFT_eSPI();

// Sprites bộ nhớ RAM hỗ trợ vẽ 2 lớp siêu mượt, loại bỏ 100% nhấp nháy (Zero-Flicker TFT_eSprite)
TFT_eSprite sprHorizon = TFT_eSprite(&tft);
TFT_eSprite sprDepth   = TFT_eSprite(&tft);
TFT_eSprite sprCanvas  = TFT_eSprite(&tft); // Dùng chung cho Trang 3 (3D Bathymetry) & Trang 4 (Radar 360)
bool sprHorizonCreated = false;
bool sprDepthCreated   = false;
bool sprCanvasCreated  = false;"""

if old_tft_decl in code and "sprHorizonCreated" not in code:
    code = code.replace(old_tft_decl, new_tft_decl, 1)
    print("Added TFT_eSprite declarations")
else:
    print("Sprite declarations already present or target not found")

# 2. Add Sprite initialization in setup() right after tft.setRotation(1);
old_tft_setup = """tft.init();
  tft.setRotation(1);
  Serial.printf("[GCS-TFT] Resolution: %dx%d (Rotation 1)\\n", tft.width(), tft.height());"""

new_tft_setup = """tft.init();
  tft.setRotation(1);
  Serial.printf("[GCS-TFT] Resolution: %dx%d (Rotation 1)\\n", tft.width(), tft.height());

  // Khởi tạo các sprite đệm bộ nhớ RAM (Off-Screen Double Buffering)
  sprHorizonCreated = (sprHorizon.createSprite(74, 54) != nullptr);
  sprDepthCreated   = (sprDepth.createSprite(14, 76) != nullptr);
  sprCanvasCreated  = (sprCanvas.createSprite(216, 146) != nullptr);
  Serial.printf("[GCS-SPRITES] Horizon:%d Depth:%d Canvas:%d | Heap:%u\\n",
                sprHorizonCreated, sprDepthCreated, sprCanvasCreated, (unsigned int)ESP.getFreeHeap());"""

if old_tft_setup in code:
    code = code.replace(old_tft_setup, new_tft_setup, 1)
    print("Added Sprite initialization in setup()")
else:
    print("Could not find old_tft_setup exactly")

with open('src/ground_station/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("Step 2A applied successfully!")
