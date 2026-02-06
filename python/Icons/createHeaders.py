import os
from PIL import Image

# --- SETTINGS ---
IN_DIR = "."      # Input folder
OUT_DIR = "headers"       # Output folder
MASTER_FILENAME = "icons_all.h"
# ---------------------

if not os.path.exists(OUT_DIR):
    os.makedirs(OUT_DIR)

def get_var_name(filename):
    """Converts filename to valid C variable name."""
    name_no_ext = os.path.splitext(filename)[0]
    return name_no_ext.replace("-", "_").replace(" ", "_").lower()

def convert_image_to_bytes(file_path):
    """Reads image and returns width, height, and packed 4bpp byte list."""
    img = Image.open(file_path).convert("L")
    width, height = img.size
    pixels = list(img.getdata())
    
    packed_bytes = []
    for i in range(0, len(pixels), 2):
        p1 = pixels[i] >> 4
        p2 = (pixels[i+1] >> 4) if (i + 1 < len(pixels)) else 0
        packed_bytes.append((p1 << 4) | p2)
        
    return width, height, packed_bytes

# --- MAIN PROCESS ---
files = sorted([f for f in os.listdir(IN_DIR) if f.lower().endswith('.png')])

if not files:
    print("No PNG files found!")
    exit()

all_vars = []  # To store (var_name, width, height) for the master file
master_content = []

master_content.append("// Master Header File - All Icons")
master_content.append("// Format: 4bpp (2 pixels per byte)")
master_content.append("#ifndef ICONS_ALL_H")
master_content.append("#define ICONS_ALL_H")
master_content.append("")
master_content.append("#include <stdint.h>")
master_content.append("")

for filename in files:
    try:
        file_path = os.path.join(IN_DIR, filename)
        var_name = get_var_name(filename)
        width, height, data = convert_image_to_bytes(file_path)
        
        all_vars.append((var_name, width, height))
        
        # Add array definition to master content
        master_content.append(f"// Icon: {filename} ({width}x{height})")
        master_content.append(f"#define {var_name.upper()}_WIDTH {width}")
        master_content.append(f"#define {var_name.upper()}_HEIGHT {height}")
        master_content.append(f"const uint8_t {var_name}_map[] = {{")
        
        # Hex formatting
        hex_lines = []
        current_line = "    "
        for idx, byte in enumerate(data):
            current_line += f"0x{byte:02X}, "
            if (idx + 1) % 16 == 0:
                hex_lines.append(current_line.rstrip())
                current_line = "    "
        if current_line.strip():
            hex_lines.append(current_line.rstrip())
            
        master_content.extend(hex_lines)
        master_content.append("};")
        master_content.append("")
        
        print(f"Processed: {filename}")

    except Exception as e:
        print(f"Error processing {filename}: {e}")

# --- Add Lookup Table ---
master_content.append("// Array of pointers to all icons for easy iteration")
master_content.append(f"const uint8_t* all_icons[{len(all_vars)}] = {{")
for var, _, _ in all_vars:
    master_content.append(f"    {var}_map,")
master_content.append("};")
master_content.append("")

# --- Add Enum for Clean Indexing ---
master_content.append("// Enum for easy indexing")
master_content.append("typedef enum {")
for var, _, _ in all_vars:
    master_content.append(f"    ICON_{var.upper()},")
master_content.append("    ICON_COUNT")
master_content.append("} IconID;")

master_content.append("")
master_content.append("#endif // ICONS_ALL_H")

# Write Master File
master_path = os.path.join(OUT_DIR, MASTER_FILENAME)
with open(master_path, "w") as f:
    f.write("\n".join(master_content))

print(f"\nSUCCESS! Master file generated: {master_path}")
