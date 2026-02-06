import os
from PIL import Image

# --- SETTINGS ---
IN_DIR = "."             # Input folder
OUT_DIR = "resized_padded" # Output folder
CANVAS_SIZE = (256, 256)
PADDING = 20             # Padding on each side

if not os.path.exists(OUT_DIR):
    os.makedirs(OUT_DIR)

files = [f for f in os.listdir(IN_DIR) if f.lower().endswith('.png')]

for filename in files:
    try:
        path = os.path.join(IN_DIR, filename)
        
        # 1. Open Original
        img = Image.open(path).convert("RGBA")
        
        # 2. Calculate New Inner Size
        # 256 - (15*2) = 226x226
        new_w = CANVAS_SIZE[0] - (PADDING * 2)
        new_h = CANVAS_SIZE[1] - (PADDING * 2)
        
        # 3. Resize the icon down to fit inside the padding
        # Use LANCZOS for high quality downscaling
        img_resized = img.resize((new_w, new_h), Image.Resampling.LANCZOS)
        
        # 4. Create new White Canvas (256x256)
        # We fill with White (255,255,255,255) so the padding isn't transparent black
        canvas = Image.new("RGBA", CANVAS_SIZE, (255, 255, 255, 255))
        
        # 5. Paste resized icon in the center (at x=15, y=15)
        # We use the resized image itself as the mask to handle transparency correctly
        canvas.paste(img_resized, (PADDING, PADDING), img_resized)
        
        # 6. Save
        out_path = os.path.join(OUT_DIR, filename)
        canvas.save(out_path)
        print(f"Resized & Padded: {filename}")
        
    except Exception as e:
        print(f"Error {filename}: {e}")

print("Done! Check 'resized_padded' folder.")
