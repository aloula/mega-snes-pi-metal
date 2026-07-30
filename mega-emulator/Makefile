#
# Makefile
#

CIRCLEHOME = ../deps/circle

OBJS	= main.o kernel.o osd.o emu_orchestrator.o clib_stubs.o \
	  ../deps/picodrive/pico/pico.o \
	  ../deps/picodrive/pico/cart.o \
	  ../deps/picodrive/pico/memory.o \
	  ../deps/picodrive/pico/state.o \
	  ../deps/picodrive/pico/sek.o \
	  ../deps/picodrive/pico/z80if.o \
	  ../deps/picodrive/pico/videoport.o \
	  ../deps/picodrive/pico/draw2.o \
	  ../deps/picodrive/pico/draw.o \
	  ../deps/picodrive/pico/mode4.o \
	  ../deps/picodrive/pico/sms.o \
	  ../deps/picodrive/pico/misc.o \
	  ../deps/picodrive/pico/eeprom.o \
	  ../deps/picodrive/pico/patch.o \
	  ../deps/picodrive/pico/debug.o \
	  ../deps/picodrive/pico/media.o \
	  ../deps/picodrive/pico/carthw/carthw.o \
	  ../deps/picodrive/pico/carthw/eeprom_spi.o \
	  ../deps/picodrive/pico/carthw/svp/svp.o \
	  ../deps/picodrive/pico/carthw/svp/memory.o \
	  ../deps/picodrive/pico/carthw/svp/ssp16.o \
	  ../deps/picodrive/pico/sound/sound.o \
	  ../deps/picodrive/pico/sound/resampler.o \
	  ../deps/picodrive/pico/sound/sn76496.o \
	  ../deps/picodrive/pico/sound/ym2612.o \
	  ../deps/picodrive/pico/sound/ym2413.o \
	  ../deps/picodrive/pico/sound/vgm.o \
	  ../deps/picodrive/pico/m68kif_cyclone.o \
	  ../deps/picodrive/cpu/cyclone/Cyclone.o \
	  ../deps/picodrive/cpu/cyclone/tools/idle.o \
	  ../deps/picodrive/cpu/DrZ80/drz80.o \
	  ../deps/picodrive/cpu/drc/cmn.o \
	  ../deps/picodrive/pico/draw_arm.o \
	  ../deps/picodrive/pico/draw2_arm.o \
	  ../deps/picodrive/pico/memory_arm.o \
	  ../deps/picodrive/pico/sound/ym2612_arm.o \
	  ../deps/picodrive/pico/sound/mix_arm.o \
	  ../deps/picodrive/pico/misc_arm.o \
	  ../deps/picodrive/pico/cd/memory_arm.o \
	  ../deps/picodrive/pico/cd/misc_arm.o \
	  ../deps/picodrive/pico/cd/mcd.o \
	  ../deps/picodrive/pico/cd/memory.o \
	  ../deps/picodrive/pico/cd/sek.o \
	  ../deps/picodrive/pico/cd/cdc.o \
	  ../deps/picodrive/pico/cd/cdd.o \
	  ../deps/picodrive/pico/cd/cd_image.o \
	  ../deps/picodrive/pico/cd/cd_parse.o \
	  ../deps/picodrive/pico/cd/gfx.o \
	  ../deps/picodrive/pico/cd/gfx_dma.o \
	  ../deps/picodrive/pico/cd/misc.o \
	  ../deps/picodrive/pico/cd/pcm.o \
	  ../deps/picodrive/pico/cd/megasd.o \
	  ../deps/picodrive/pico/pico/pico.o \
	  ../deps/picodrive/pico/pico/memory.o \
	  ../deps/picodrive/pico/pico/xpcm.o \
	  ../deps/picodrive/zlib/adler32.o \
	  ../deps/picodrive/zlib/compress.o \
	  ../deps/picodrive/zlib/crc32.o \
	  ../deps/picodrive/zlib/deflate.o \
	  ../deps/picodrive/zlib/gzio.o \
	  ../deps/picodrive/zlib/infback.o \
	  ../deps/picodrive/zlib/inffast.o \
	  ../deps/picodrive/zlib/inflate.o \
	  ../deps/picodrive/zlib/inftrees.o \
	  ../deps/picodrive/zlib/trees.o \
	  ../deps/picodrive/zlib/uncompr.o \
	  ../deps/picodrive/zlib/zutil.o \
	  ../deps/picodrive/pico/cd/libchdr/src/libchdr_chd.o \
	  ../deps/picodrive/pico/cd/libchdr/src/libchdr_cdrom.o \
	  ../deps/picodrive/pico/cd/libchdr/src/libchdr_flac.o \
	  ../deps/picodrive/pico/cd/libchdr/src/libchdr_bitstream.o \
	  ../deps/picodrive/pico/cd/libchdr/src/libchdr_huffman.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/src/CpuArch.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/src/Alloc.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/src/LzmaEnc.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/src/Sort.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/src/LzmaDec.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/src/LzFind.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/src/Delta.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/entropy_common.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/error_private.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/fse_decompress.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/xxhash.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/zstd_common.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/huf_decompress.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/zstd_ddict.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/zstd_decompress_block.o \
	  ../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/zstd_decompress.o

# Get version from root VERSION file, fallback to 1.0 if not found
MEGAPI_VERSION = $(shell cat ../VERSION 2>/dev/null || echo 1.0)

# Additional C compiler flags for Picodrive core
DEFS     = -D_USE_DRZ80 -DEMU_C68K -D_ASM_DRAW_C -D_ASM_MEMORY_C -D_ASM_YM2612_C -D_ASM_MISC_C -D_ASM_CD_MEMORY_C -DNO_32X -DUSE_LIBCHDR -DZ7_ST -DEL_LOGMASK=0xc0000000 -DMEGAPI_VERSION="\"$(MEGAPI_VERSION)\""
INCS     = -I. -I../shared -I../deps/picodrive -I../deps/picodrive/pico -I../deps/picodrive/cpu -I../deps/picodrive/pico/sound -I../deps/picodrive/zlib -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon \
           -I../deps/picodrive/pico/cd/libchdr/include \
           -I../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/include \
           -I../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib \
           -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi

OPTIMIZE = -O3 -ffast-math -fomit-frame-pointer -ftree-vectorize -funroll-loops -fno-strict-aliasing
ARCH_FLAGS = -mcpu=cortex-a76 -mfpu=neon-fp-armv8 -mfloat-abi=hard

CFLAGS   += $(OPTIMIZE) $(ARCH_FLAGS) $(INCS) $(DEFS) -Wno-unused-result -Wno-write-strings -falign-functions=2
CPPFLAGS += $(OPTIMIZE) $(ARCH_FLAGS) $(INCS) $(DEFS) -falign-functions=2 -std=gnu++14 -fno-rtti -fno-exceptions
AFLAGS   += $(ARCH_FLAGS) $(INCS) $(DEFS)

LIBS	= $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	  $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	  $(CIRCLEHOME)/lib/sound/libsound.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/sched/libsched.a \
	  $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

../deps/picodrive/%.o: CFLAGS += -w
../deps/picodrive/%.o: CPPFLAGS += -w

# Custom assembler rule for lowercase .s files (Cyclone, etc.)
%.o: %.s
	@echo "  AS    $@"
	@$(AS) $(AFLAGS) -c -o $@ $<

# Generate ILP32 offset header for ASM files dynamically
../deps/picodrive/pico/pico_int_offs.h:
	cd ../deps/picodrive/tools && CC="arm-none-eabi-gcc" CFLAGS="$(DEFS) -I../pico/sound" sh ./mkoffsets.sh ../pico

# Make all objects depend on generated offset header
$(OBJS): ../deps/picodrive/pico/pico_int_offs.h

# Rebuild kernel.o when VERSION changes
# kernel.o: ../VERSION

main.o: ../shared/main.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

clib_stubs.o: ../shared/clib_stubs.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

EXTRALIBS += $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libm.a) \
             $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libc.a) \
             $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libnosys.a)

-include $(DEPS)
