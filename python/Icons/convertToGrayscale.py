import os
from PIL import Image

# --- SETTINGS ---
IN_DIR = "." 
OUT_DIR = "grayscale" 
LEVELS = 16 

if not os.path.exists(OUT_DIR):
    os.makedirs(OUT_DIR)

# --- CREATE LUT ---
step_size = 256 // LEVELS
lut = []
for i in range(256):
    level = i // step_size
    # Map 0-15 back to 0-255
    # Special check: Preserve pure white if you want
    val = level * (255 // (LEVELS - 1))
    lut.append(val)

files = [f for f in os.listdir(IN_DIR) if f.lower().endswith('.png')]

for filename in files:
    try:
        path = os.path.join(IN_DIR, filename)
        
        # 1. Open as RGBA (Preserve Transparency)
        img = Image.open(path).convert("RGBA")
        
        # 2. Create a solid WHITE background
        bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
        
        # 3. Composite the icon onto the white background
        # This turns transparent pixels into White (255,255,255) instead of Black
        img_with_bg = Image.alpha_composite(bg, img)
        
        # 4. Convert to Grayscale
        img_gray = img_with_bg.convert("L")

        # 5. Apply Quantization
        img_16 = img_gray.point(lut)

        # 6. Save
        out_path = os.path.join(OUT_DIR, filename)
        img_16.save(out_path)
        print(f"Converted: {filename}")
        
    except Exception as e:
        print(f"Error {filename}: {e}")

print("Done! Transparent areas are now White.")
