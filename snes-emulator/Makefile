#
# Makefile for Combined SNES-PI and MEGA-PI baremetal emulator
#

CIRCLEHOME = ../circle

OBJS	= main.o kernel.o osd.o snes_orchestrator.o md_orchestrator.o clib_stubs.o \
	  ../snes9x/apu/apu.o \
	  ../snes9x/apu/bapu/dsp/sdsp.o \
	  ../snes9x/apu/bapu/smp/smp.o \
	  ../snes9x/apu/bapu/smp/smp_state.o \
	  ../snes9x/bsx.o \
	  ../snes9x/c4.o \
	  ../snes9x/c4emu.o \
	  ../snes9x/cheats.o \
	  ../snes9x/cheats2.o \
	  ../snes9x/clip.o \
	  ../snes9x/conffile.o \
	  ../snes9x/controls.o \
	  ../snes9x/cpu.o \
	  ../snes9x/cpuexec.o \
	  ../snes9x/cpuops.o \
	  ../snes9x/crosshairs.o \
	  ../snes9x/dma.o \
	  ../snes9x/dsp.o \
	  ../snes9x/dsp1.o \
	  ../snes9x/dsp2.o \
	  ../snes9x/dsp3.o \
	  ../snes9x/dsp4.o \
	  ../snes9x/fxinst.o \
	  ../snes9x/fxemu.o \
	  ../snes9x/gfx.o \
	  ../snes9x/globals.o \
	  ../snes9x/memmap.o \
	  ../snes9x/obc1.o \
	  ../snes9x/msu1.o \
	  ../snes9x/ppu.o \
	  ../snes9x/stream.o \
	  ../snes9x/sa1.o \
	  ../snes9x/sa1cpu.o \
	  ../snes9x/screenshot.o \
	  ../snes9x/sdd1.o \
	  ../snes9x/sdd1emu.o \
	  ../snes9x/seta.o \
	  ../snes9x/seta010.o \
	  ../snes9x/seta011.o \
	  ../snes9x/seta018.o \
	  ../snes9x/snapshot.o \
	  ../snes9x/snes9x.o \
	  ../snes9x/spc7110.o \
	  ../snes9x/srtc.o \
	  ../snes9x/tile.o \
	  ../snes9x/tileimpl-n1x1.o \
	  ../snes9x/tileimpl-n2x1.o \
	  ../snes9x/tileimpl-h2x1.o \
	  ../snes9x/sha256.o \
	  ../snes9x/bml.o \
	  ../snes9x/movie.o \
	  ../snes9x/fscompat.o \
	  ../snes9x/filter/snes_ntsc.o \
	  ../picodrive/pico/pico.o \
	  ../picodrive/pico/cart.o \
	  ../picodrive/pico/memory.o \
	  ../picodrive/pico/state.o \
	  ../picodrive/pico/sek.o \
	  ../picodrive/pico/z80if.o \
	  ../picodrive/pico/videoport.o \
	  ../picodrive/pico/draw2.o \
	  ../picodrive/pico/draw.o \
	  ../picodrive/pico/mode4.o \
	  ../picodrive/pico/misc.o \
	  ../picodrive/pico/eeprom.o \
	  ../picodrive/pico/patch.o \
	  ../picodrive/pico/debug.o \
	  ../picodrive/pico/media.o \
	  ../picodrive/pico/carthw/carthw.o \
	  ../picodrive/pico/carthw/eeprom_spi.o \
	  ../picodrive/pico/carthw/svp/svp.o \
	  ../picodrive/pico/carthw/svp/memory.o \
	  ../picodrive/pico/carthw/svp/ssp16.o \
	  ../picodrive/pico/sound/sound.o \
	  ../picodrive/pico/sound/resampler.o \
	  ../picodrive/pico/sound/sn76496.o \
	  ../picodrive/pico/sound/ym2612.o \
	  ../picodrive/pico/sound/ym2413.o \
	  ../picodrive/pico/sound/vgm.o \
	  ../picodrive/pico/m68kif_cyclone.o \
	  ../picodrive/cpu/cyclone/Cyclone.o \
	  ../picodrive/cpu/cyclone/tools/idle.o \
	  ../picodrive/cpu/DrZ80/drz80.o \
	  ../picodrive/cpu/drc/cmn.o \
	  ../picodrive/pico/draw_arm.o \
	  ../picodrive/pico/draw2_arm.o \
	  ../picodrive/pico/memory_arm.o \
	  ../picodrive/pico/sound/ym2612_arm.o \
	  ../picodrive/pico/sound/mix_arm.o \
	  ../picodrive/pico/misc_arm.o \
	  ../picodrive/pico/cd/memory_arm.o \
	  ../picodrive/pico/cd/misc_arm.o \
	  ../picodrive/pico/cd/mcd.o \
	  ../picodrive/pico/cd/memory.o \
	  ../picodrive/pico/cd/sek.o \
	  ../picodrive/pico/cd/cdc.o \
	  ../picodrive/pico/cd/cdd.o \
	  ../picodrive/pico/cd/cd_image.o \
	  ../picodrive/pico/cd/cd_parse.o \
	  ../picodrive/pico/cd/gfx.o \
	  ../picodrive/pico/cd/gfx_dma.o \
	  ../picodrive/pico/cd/misc.o \
	  ../picodrive/pico/cd/pcm.o \
	  ../picodrive/pico/cd/megasd.o \
	  ../picodrive/pico/pico/pico.o \
	  ../picodrive/pico/pico/memory.o \
	  ../picodrive/pico/pico/xpcm.o \
	  ../picodrive/zlib/adler32.o \
	  ../picodrive/zlib/compress.o \
	  ../picodrive/zlib/crc32.o \
	  ../picodrive/zlib/deflate.o \
	  ../picodrive/zlib/gzio.o \
	  ../picodrive/zlib/infback.o \
	  ../picodrive/zlib/inffast.o \
	  ../picodrive/zlib/inflate.o \
	  ../picodrive/zlib/inftrees.o \
	  ../picodrive/zlib/trees.o \
	  ../picodrive/zlib/uncompr.o \
	  ../picodrive/zlib/zutil.o \
	  ../picodrive/pico/cd/libchdr/src/libchdr_chd.o \
	  ../picodrive/pico/cd/libchdr/src/libchdr_cdrom.o \
	  ../picodrive/pico/cd/libchdr/src/libchdr_flac.o \
	  ../picodrive/pico/cd/libchdr/src/libchdr_bitstream.o \
	  ../picodrive/pico/cd/libchdr/src/libchdr_huffman.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/CpuArch.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/Alloc.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/LzmaEnc.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/Sort.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/LzmaDec.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/LzFind.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/Delta.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/entropy_common.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/error_private.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/fse_decompress.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/xxhash.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/zstd_common.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/huf_decompress.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/zstd_ddict.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/zstd_decompress_block.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/zstd_decompress.o

SNESPI_VERSION = $(shell cat ../VERSION 2>/dev/null || echo 1.0)
MEGAPI_VERSION = $(shell cat ../VERSION 2>/dev/null || echo 1.0)

# Combined compiler definitions
DEFS     = -DSNESPI_VERSION="\"$(SNESPI_VERSION)\"" -DMEGAPI_VERSION="\"$(MEGAPI_VERSION)\"" \
           -DPIXEL_FORMAT=RGB565 -DALLOW_CPU_OVERCLOCK=1 -DHAVE_STDINT_H -DHAVE_STRINGS_H -D_GNU_SOURCE \
           -D_USE_DRZ80 -DEMU_C68K -D_ASM_DRAW_C -D_ASM_MEMORY_C -D_ASM_YM2612_C -D_ASM_MISC_C -D_ASM_CD_MEMORY_C -DNO_32X -DNO_SMS -DUSE_LIBCHDR -DZ7_ST -DEL_LOGMASK=0xc0000000

# Base include path order default (Snes9x first)
INCS     = -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu \
           -I../picodrive -I../picodrive/pico -I../picodrive/cpu -I../picodrive/pico/sound -I../picodrive/zlib \
           -I../picodrive/pico/cd/libchdr/include \
           -I../picodrive/pico/cd/libchdr/deps/lzma-24.05/include \
           -I../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib \
           -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon \
           -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi

OPTIMIZE = -O3 -ffast-math -fomit-frame-pointer -ftree-vectorize -funroll-loops -fno-strict-aliasing

CFLAGS   += $(INCS) $(DEFS) -Wno-unused-result -Wno-write-strings -falign-functions=2
CPPFLAGS += $(INCS) $(DEFS) -falign-functions=2 -std=gnu++14 -fno-rtti -fno-exceptions
LDFLAGS  += --allow-multiple-definition --defsym=end=_end
AFLAGS   += $(INCS) $(DEFS)

LIBS	= $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	  $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	  $(CIRCLEHOME)/lib/sound/libsound.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/sched/libsched.a \
	  $(CIRCLEHOME)/lib/libcircle.a

STDLIB_SUPPORT = 3

include $(CIRCLEHOME)/Rules.mk

# Target-specific include path modifications to resolve header shadowing collisions:
# (1) Picodrive files must search their own unzip first.
# (2) Snes9x files must search their own memory.h / port headers first.
../picodrive/%.o: INCS = -I../picodrive -I../picodrive/pico -I../picodrive/cpu -I../picodrive/pico/sound -I../picodrive/zlib -I../picodrive/pico/cd/libchdr/include -I../picodrive/pico/cd/libchdr/deps/lzma-24.05/include -I../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon
../picodrive/%.o: CFLAGS += -I../picodrive -I../picodrive/pico -I../picodrive/cpu -I../picodrive/pico/sound -I../picodrive/zlib
../picodrive/%.o: CPPFLAGS += -I../picodrive -I../picodrive/pico -I../picodrive/cpu -I../picodrive/pico/sound -I../picodrive/zlib

../snes9x/%.o: INCS = -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
../snes9x/%.o: CFLAGS += -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu
../snes9x/%.o: CPPFLAGS += -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu

snes_orchestrator.o: INCS = -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
snes_orchestrator.o: CFLAGS += -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu
snes_orchestrator.o: CPPFLAGS += -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu

md_orchestrator.o: INCS = -I../picodrive -I../picodrive/pico -I../picodrive/cpu -I../picodrive/pico/sound -I../picodrive/zlib -I../picodrive/pico/cd/libchdr/include -I../picodrive/pico/cd/libchdr/deps/lzma-24.05/include -I../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon
md_orchestrator.o: CFLAGS += -I../picodrive -I../picodrive/pico -I../picodrive/cpu -I../picodrive/pico/sound -I../picodrive/zlib
md_orchestrator.o: CPPFLAGS += -I../picodrive -I../picodrive/pico -I../picodrive/cpu -I../picodrive/pico/sound -I../picodrive/zlib

# Custom assembler rule for lowercase .s files (Cyclone, etc.)
%.o: %.s
	@echo "  AS    $@"
	@$(AS) $(AFLAGS) -c -o $@ $<

# Generate ILP32 offset header for ASM files dynamically
../picodrive/pico/pico_int_offs.h:
	cd ../picodrive/tools && CC="arm-none-eabi-gcc" CFLAGS="$(DEFS) -I../pico/sound" sh ./mkoffsets.sh ../pico

# Local symlink so draw_arm.o can find it via local include
pico_int_offs.h: ../picodrive/pico/pico_int_offs.h
	ln -sf ../picodrive/pico/pico_int_offs.h .

# Local symlink so cd/memory_arm.o can find pico/pico_int_offs.h
pico:
	ln -sf ../picodrive/pico .

pico/pico_int_offs.h: pico

# Make all objects depend on generated offset header
$(OBJS): ../picodrive/pico/pico_int_offs.h pico_int_offs.h pico

EXTRALIBS += $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libm.a) \
             $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libc.a) \
             $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libnosys.a)

-include $(DEPS)
