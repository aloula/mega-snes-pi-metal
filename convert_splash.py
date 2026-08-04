#!/usr/bin/env python3
import os
import sys
import struct

try:
    from PIL import Image
except ImportError:
    print("Error: The 'Pillow' library is required to run this script.", file=sys.stderr)
    print("Please install it using your system package manager or pip:", file=sys.stderr)
    print("\n    On Debian/Ubuntu/Raspbian:")
    print("        sudo apt install python3-pil")
    print("\n    On Arch Linux:")
    print("        sudo pacman -S python-pillow")
    print("\n    Via pip:")
    print("        pip install Pillow\n")
    sys.exit(1)

def convert_image(input_path, output_path):
    if not os.path.exists(input_path):
        print(f"Error: Input file '{input_path}' not found.", file=sys.stderr)
        return False

    try:
        img = Image.open(input_path)
    except Exception as e:
        print(f"Error: Failed to open image '{input_path}': {e}", file=sys.stderr)
        return False

    # Convert to RGB if not already
    if img.mode != 'RGB':
        img = img.convert('RGB')

    width, height = img.size
    print(f"Converting: {input_path} ({width}x{height}) -> {output_path}")

    # If the image is larger than the 640x480 boundary, scale it down to fit while keeping aspect ratio.
    if width > 640 or height > 480:
        scale = min(640.0 / width, 480.0 / height)
        new_width = int(width * scale)
        new_height = int(height * scale)
        img = img.resize((new_width, new_height), Image.Resampling.LANCZOS)
        width, height = img.size

    # The baremetal kernel expects a fixed horizontal stride of 640 pixels in the file.
    if width != 640:
        canvas = Image.new('RGB', (640, height), (0, 0, 0))
        offset_x = (640 - width) // 2
        canvas.paste(img, (offset_x, 0))
        img = canvas
        width, height = img.size

    # Convert to RGB565 Little-Endian
    try:
        output_dir = os.path.dirname(output_path)
        if output_dir:
            os.makedirs(output_dir, exist_ok=True)
        with open(output_path, 'wb') as f:
            for y in range(height):
                for x in range(width):
                    r, g, b = img.getpixel((x, y))
                    r5 = (r >> 3) & 0x1F
                    g6 = (g >> 2) & 0x3F
                    b5 = (b >> 3) & 0x1F
                    rgb565 = (r5 << 11) | (g6 << 5) | b5
                    f.write(struct.pack('<H', rgb565))
        
        file_size = os.path.getsize(output_path)
        print(f"  ✓ Success: '{output_path}' ({file_size} bytes)")
        return True
    except Exception as e:
        print(f"Error writing '{output_path}': {e}", file=sys.stderr)
        return False

def convert_directory(input_dir, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    valid_exts = ('.png', '.jpg', '.jpeg', '.bmp', '.tga')
    files = [f for f in os.listdir(input_dir) if f.lower().endswith(valid_exts)]
    
    if not files:
        print(f"No image files found in '{input_dir}'")
        return 0

    print(f"Found {len(files)} image(s) in '{input_dir}'. Batch converting to '{output_dir}'...")
    count = 0
    for filename in sorted(files):
        in_file = os.path.join(input_dir, filename)
        base_name = os.path.splitext(filename)[0]
        out_file = os.path.join(output_dir, f"{base_name}.raw16")
        if convert_image(in_file, out_file):
            count += 1
    print(f"Batch conversion complete: {count}/{len(files)} converted.")
    return count

def main():
    if len(sys.argv) < 2:
        print("MEGA-SNES Pi Metal Bootsplash Converter")
        print("Usage:")
        print("  python3 convert_splash.py <input_image_or_dir> [output_file_or_dir]")
        print("\nExamples:")
        print("  python3 convert_splash.py res/5-in-1_Baremetal_Emulator_2.png boot/Splash_Screen.raw16")
        print("  python3 convert_splash.py res/ boot/splash/")
        sys.exit(1)

    input_path = sys.argv[1]
    
    if os.path.isdir(input_path):
        output_dir = sys.argv[2] if len(sys.argv) >= 3 else "boot/splash"
        convert_directory(input_path, output_dir)
    else:
        output_path = sys.argv[2] if len(sys.argv) >= 3 else "Splash_Screen.raw16"
        convert_image(input_path, output_path)

if __name__ == "__main__":
    main()
