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
        sys.exit(1)

    try:
        img = Image.open(input_path)
    except Exception as e:
        print(f"Error: Failed to open image: {e}", file=sys.stderr)
        sys.exit(1)

    # Convert to RGB if not already
    if img.mode != 'RGB':
        img = img.convert('RGB')

    width, height = img.size
    print(f"Source Image: {input_path} ({width}x{height} {img.mode})")

    # If the image is larger than the 640x480 boundary, scale it down to fit while keeping aspect ratio.
    # Otherwise, if it is smaller, we NEVER enlarge it; we keep its original size.
    if width > 640 or height > 480:
        scale = min(640.0 / width, 480.0 / height)
        new_width = int(width * scale)
        new_height = int(height * scale)
        print(f"Resizing/shrinking image to fit within 640x480 (aspect ratio scaling: {width}x{height} -> {new_width}x{new_height})")
        img = img.resize((new_width, new_height), Image.Resampling.LANCZOS)
        width, height = img.size
    else:
        print(f"Keeping original image dimensions: {width}x{height} (no upscaling/enlarging)")

    # The baremetal kernel expects a fixed horizontal stride of 640 pixels in the file.
    # We paste our image centered on a 640-pixel wide black canvas of the same height.
    if width != 640:
        print(f"Centering horizontally on a 640px wide black canvas (height: {height}px)")
        canvas = Image.new('RGB', (640, height), (0, 0, 0))
        offset_x = (640 - width) // 2
        canvas.paste(img, (offset_x, 0))
        img = canvas
        width, height = img.size

    print(f"Target Dimensions: {width}x{height} (Perfect fit for dynamic centering)")

    # Convert to RGB565 Little-Endian
    print(f"Converting and writing to '{output_path}'...")
    try:
        with open(output_path, 'wb') as f:
            for y in range(height):
                for x in range(width):
                    r, g, b = img.getpixel((x, y))
                    
                    # Convert 8-bit channels to 5-6-5 bits
                    r5 = (r >> 3) & 0x1F
                    g6 = (g >> 2) & 0x3F
                    b5 = (b >> 3) & 0x1F
                    
                    # Pack into a 16-bit word
                    rgb565 = (r5 << 11) | (g6 << 5) | b5
                    
                    # Write as little-endian unsigned short
                    f.write(struct.pack('<H', rgb565))
        
        file_size = os.path.getsize(output_path)
        print(f"Success! Created '{output_path}' ({file_size} bytes).")
        print("Move this file to the root of your SD card as 'Splash_Screen.raw16'.")
    except Exception as e:
        print(f"Error: Failed to write output file: {e}", file=sys.stderr)
        sys.exit(1)

def main():
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        print("MEGA-SNES Pi Metal Bootsplash Converter")
        print("Usage: python3 convert_splash.py <input_image> [output_file]")
        print("Example: python3 convert_splash.py my_logo.png Splash_Screen.raw16")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) == 3 else "Splash_Screen.raw16"

    convert_image(input_path, output_path)

if __name__ == "__main__":
    main()
