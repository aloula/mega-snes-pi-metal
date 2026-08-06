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

# Generate splash screens in boot/splash/
mkdir -p boot/splash

if [ -d "res" ]; then
    echo -e "${BLUE}Converting all bootsplash images from res/ to boot/splash/...${NC}"
    python3 convert_splash.py res/ boot/splash/
fi

# 2b. Ensure Circle configuration and libraries exist (generated on fresh clones)
if [ ! -f "deps/circle/Config.mk" ]; then
    echo -e "${BLUE}Configuring Circle environment (KERNEL_MAX_SIZE=24MB)...${NC}"
    (cd deps/circle && ./configure -r 3 --prefix arm-none-eabi- --multicore --kernel-max-size 24 -f)
fi

# Config2.mk is loaded by Circle's Rules.mk but (unlike Config.mk) is never touched
# by `configure`, so regenerating it here is safe even if Config.mk already exists.
# It widens the heap allocator's bucket sizes up to 8MB - the stock default tops out
# at 512KB, and any single allocation above that can never be reclaimed once freed
# (see circle/sysconfig.h). This project routinely allocates well above 512KB
# (Sega CD save states alone are 2MB), so without this every large ROM/state buffer
# permanently leaks heap space instead of being reused by the next one.
echo -e "${BLUE}Widening Circle heap allocator buckets (required by this project)...${NC}"
cat > deps/circle/Config2.mk <<'EOF'
DEFINE += -DHEAP_BLOCK_BUCKET_SIZES=0x40,0x400,0x1000,0x4000,0x10000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000
EOF

if [ ! -f "deps/circle/lib/libcircle.a" ] || [ ! -f "deps/circle/addon/fatfs/libfatfs.a" ]; then
    echo -e "${BLUE}Building Circle core libraries and addons...${NC}"
    (cd deps/circle && ./makeall && cd addon/fatfs && make -j$(nproc) && cd ../SDCard && make -j$(nproc))
fi

# 2c. Fix potential clock skew from git clones or machine switches
find . -type f -exec touch -d "1 minute ago" {} + 2>/dev/null || true
find deps/ -name "*.d" -delete 2>/dev/null || true

# 3. Clean previous build files
echo -e "${BLUE}Cleaning previous builds...${NC}"
make -C master-emulator clean
make -C mega-emulator clean
make -C snes-emulator clean
make -C main-emulator clean

# 4. Build all emulator targets
echo -e "${BLUE}Compiling master-emulator...${NC}"
make -C master-emulator -j$(nproc)
echo -e "${BLUE}Compiling mega-emulator...${NC}"
make -C mega-emulator -j$(nproc)
echo -e "${BLUE}Compiling snes-emulator...${NC}"
make -C snes-emulator -j$(nproc)
echo -e "${BLUE}Compiling main-emulator (5-in-1 multi-console target)...${NC}"
make -C main-emulator -j$(nproc)

# 5. Verify the compiled kernel image exists
IMAGE_FILE="main-emulator/kernel8-32.img"
if [ ! -f "$IMAGE_FILE" ]; then
    echo -e "${RED}Error: $IMAGE_FILE was not compiled successfully!${NC}"
    exit 1
fi
echo -e "${GREEN}Build succeeded! main-emulator kernel8-32.img is ready.${NC}"

# 6. Create temporary staging area for SD card file structure
echo -e "${BLUE}Staging SD Card file structure...${NC}"
STAGING_DIR="tmp_release"
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"

# Copy all boot partition files from central boot/
cp -r boot/* "$STAGING_DIR/"
cp main-emulator/kernel8-32.img "$STAGING_DIR/kernel8-32.img"

if [ -f "system_order.txt" ]; then
    cp system_order.txt "$STAGING_DIR/"
fi
if [ -f "osd_theme.txt" ]; then
    cp osd_theme.txt "$STAGING_DIR/"
fi
if [ -f "osd_colors.txt" ]; then
    cp osd_colors.txt "$STAGING_DIR/"
fi

# Create roms and bios directories
mkdir -p "$STAGING_DIR/roms/snes"
mkdir -p "$STAGING_DIR/roms/megadrive"
mkdir -p "$STAGING_DIR/roms/megacd"
mkdir -p "$STAGING_DIR/roms/nes"
mkdir -p "$STAGING_DIR/roms/pce"
mkdir -p "$STAGING_DIR/roms/mastersystem"
mkdir -p "$STAGING_DIR/bios"

# Put a simple README placeholder in folders
echo "Place your Super Nintendo (.sfc, .smc) ROMs in this folder." > "$STAGING_DIR/roms/snes/place_snes_roms_here.txt"
echo "Place your Sega Mega Drive/Genesis (.bin, .md, .gen) ROMs in this folder." > "$STAGING_DIR/roms/megadrive/place_megadrive_roms_here.txt"
echo "Place your Sega CD / Mega CD (.iso, .cue, .chd) ROMs in this folder." > "$STAGING_DIR/roms/megacd/place_megacd_roms_here.txt"
echo "Place your Nintendo Entertainment System (.nes) ROMs in this folder." > "$STAGING_DIR/roms/nes/place_nes_roms_here.txt"
echo "Place your PC Engine / TurboGrafx-16 (.pce) ROMs in this folder." > "$STAGING_DIR/roms/pce/place_pce_roms_here.txt"
echo "Place your Sega Master System (.sms, .gg, .bin) ROMs in this folder." > "$STAGING_DIR/roms/mastersystem/place_mastersystem_roms_here.txt"
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
