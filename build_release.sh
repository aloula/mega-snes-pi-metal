#!/bin/bash
set -e

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Starting Bare-Metal SD Card Release Build ===${NC}"

# 1. Check toolchain
if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo -e "${RED}Error: arm-none-eabi-gcc cross-compiler not found in PATH!${NC}"
    exit 1
fi

# 2. Check zip utility
if ! command -v zip &> /dev/null; then
    echo -e "${RED}Error: 'zip' utility is not installed! Please install it (e.g. sudo apt install zip).${NC}"
    exit 1
fi

# Generate Splash_Screen.raw16 if res/Splash_Screen.png exists
if [ -f "res/Splash_Screen.png" ]; then
    echo -e "${BLUE}Generating Splash_Screen.raw16 from res/Splash_Screen.png...${NC}"
    if command -v convert &> /dev/null; then
        convert res/Splash_Screen.png rgb:res/Splash_Screen.rgb
        python3 -c '
import struct
with open("res/Splash_Screen.rgb", "rb") as f:
    rgb = f.read()
out = bytearray()
for i in range(0, len(rgb), 3):
    r, g, b = rgb[i], rgb[i+1], rgb[i+2]
    val = (((r >> 3) & 0x1F) << 11) | (((g >> 2) & 0x3F) << 5) | ((b >> 3) & 0x1F)
    out.extend(struct.pack("<H", val))
with open("snes-emulator/boot/Splash_Screen.raw16", "wb") as f:
    f.write(out)
'
        rm -f res/Splash_Screen.rgb
        echo -e "${GREEN}Splash screen converted successfully!${NC}"
    else
        echo -e "${RED}Warning: 'convert' (ImageMagick) not found. Skipping splash screen generation.${NC}"
    fi
fi


# 3. Clean snes-emulator build files
echo -e "${BLUE}Cleaning previous build in snes-emulator...${NC}"
make -C snes-emulator clean

# 4. Build snes-emulator
echo -e "${BLUE}Compiling snes-emulator...${NC}"
make -C snes-emulator -j$(nproc)

# 5. Copy the compiled kernel image to the boot directory and verify it exists
cp snes-emulator/kernel8-32.img snes-emulator/boot/kernel8-32.img
IMAGE_FILE="snes-emulator/boot/kernel8-32.img"
if [ ! -f "$IMAGE_FILE" ]; then
    echo -e "${RED}Error: $IMAGE_FILE was not compiled successfully!${NC}"
    exit 1
fi
echo -e "${GREEN}Build succeeded! kernel8-32.img is ready.${NC}"

# 6. Create temporary staging area for SD card file structure
echo -e "${BLUE}Staging SD Card file structure...${NC}"
STAGING_DIR="tmp_release"
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"

# Copy all boot partition files from snes-emulator/boot/
cp -r snes-emulator/boot/* "$STAGING_DIR/"

# Create roms and bios directories
mkdir -p "$STAGING_DIR/roms/snes"
mkdir -p "$STAGING_DIR/roms/megadrive"
mkdir -p "$STAGING_DIR/roms/megacd"
mkdir -p "$STAGING_DIR/bios"

# Put a simple README placeholder in folders
echo "Place your Super Nintendo (.sfc, .smc) ROMs in this folder." > "$STAGING_DIR/roms/snes/place_snes_roms_here.txt"
echo "Place your Sega Mega Drive/Genesis (.bin, .md, .gen) ROMs in this folder." > "$STAGING_DIR/roms/megadrive/place_megadrive_roms_here.txt"
echo "Place your Sega CD / Mega CD (.iso, .cue, .chd) ROMs in this folder." > "$STAGING_DIR/roms/megacd/place_megacd_roms_here.txt"
echo "Place your Sega CD BIOS files (bios_CD_U.bin, bios_CD_E.bin, bios_CD_J.bin) in this folder." > "$STAGING_DIR/bios/place_sega_cd_bios_here.txt"

# 7. Create release directory and compress
echo -e "${BLUE}Compressing folder structure into release zip...${NC}"
RELEASE_DIR="release"
mkdir -p "$RELEASE_DIR"
ZIP_FILE="$RELEASE_DIR/sdcard_release.zip"
rm -f "$ZIP_FILE"

cd "$STAGING_DIR"
zip -r "../$ZIP_FILE" * > /dev/null
cd ..

# 8. Clean up staging folder
rm -rf "$STAGING_DIR"

echo -e "${GREEN}=== Release Pack Created Successfully! ===${NC}"
echo -e "Location: ${BLUE}$ZIP_FILE${NC}"
echo -e "Unzip the contents of this package directly onto the root of a FAT32-formatted SD Card."
