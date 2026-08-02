#
# Makefile for 5-in-1 Multi-Console baremetal emulator
#

CIRCLEHOME = ../deps/circle

OBJS	= main.o kernel.o osd.o snes_orchestrator.o md_orchestrator.o clib_stubs.o \
	  ../deps/snes9x/apu/apu.o \
	  ../deps/snes9x/apu/bapu/dsp/sdsp.o \
	  ../deps/snes9x/apu/bapu/smp/smp.o \
	  ../deps/snes9x/apu/bapu/smp/smp_state.o \
	  ../deps/snes9x/bsx.o \
	  ../deps/snes9x/c4.o \
	  ../deps/snes9x/c4emu.o \
	  ../deps/snes9x/cheats.o \
	  ../deps/snes9x/cheats2.o \
	  ../deps/snes9x/clip.o \
	  ../deps/snes9x/conffile.o \
	  ../deps/snes9x/controls.o \
	  ../deps/snes9x/cpu.o \
	  ../deps/snes9x/cpuexec.o \
	  ../deps/snes9x/cpuops.o \
	  ../deps/snes9x/crosshairs.o \
	  ../deps/snes9x/dma.o \
	  ../deps/snes9x/dsp.o \
	  ../deps/snes9x/dsp1.o \
	  ../deps/snes9x/dsp2.o \
	  ../deps/snes9x/dsp3.o \
	  ../deps/snes9x/dsp4.o \
	  ../deps/snes9x/fxinst.o \
	  ../deps/snes9x/fxemu.o \
	  ../deps/snes9x/gfx.o \
	  ../deps/snes9x/globals.o \
	  ../deps/snes9x/memmap.o \
	  ../deps/snes9x/obc1.o \
	  ../deps/snes9x/msu1.o \
	  ../deps/snes9x/ppu.o \
	  ../deps/snes9x/stream.o \
	  ../deps/snes9x/sa1.o \
	  ../deps/snes9x/sa1cpu.o \
	  ../deps/snes9x/screenshot.o \
	  ../deps/snes9x/sdd1.o \
	  ../deps/snes9x/sdd1emu.o \
	  ../deps/snes9x/seta.o \
	  ../deps/snes9x/seta010.o \
	  ../deps/snes9x/seta011.o \
	  ../deps/snes9x/seta018.o \
	  ../deps/snes9x/snapshot.o \
	  ../deps/snes9x/snes9x.o \
	  ../deps/snes9x/spc7110.o \
	  ../deps/snes9x/srtc.o \
	  ../deps/snes9x/tile.o \
	  ../deps/snes9x/tileimpl-n1x1.o \
	  ../deps/snes9x/tileimpl-n2x1.o \
	  ../deps/snes9x/tileimpl-h2x1.o \
	  ../deps/snes9x/sha256.o \
	  ../deps/snes9x/bml.o \
	  ../deps/snes9x/movie.o \
	  ../deps/snes9x/fscompat.o \
	  ../deps/snes9x/filter/snes_ntsc.o \
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

# Nestopia files
NESTOPIA_DIR = ../deps/nestopia/source
NESTOPIA_SRCS = $(wildcard $(NESTOPIA_DIR)/core/*.cpp) \
                $(wildcard $(NESTOPIA_DIR)/core/api/*.cpp) \
                $(wildcard $(NESTOPIA_DIR)/core/board/*.cpp) \
                $(wildcard $(NESTOPIA_DIR)/core/input/*.cpp) \
                $(wildcard $(NESTOPIA_DIR)/core/vssystem/*.cpp)

NESTOPIA_C_SRCS = $(NESTOPIA_DIR)/core/NstVideoFilterNtscCfg.c

NESTOPIA_OBJS = $(NESTOPIA_SRCS:.cpp=.o)
NESTOPIA_C_OBJS = $(NESTOPIA_C_SRCS:.c=.o)

PCE_DIR = ../deps/beetle-pce-fast-libretro
PCE_SRCS = $(PCE_DIR)/libretro.c \
           $(PCE_DIR)/mednafen/general.c \
           $(PCE_DIR)/mednafen/cdstream.c \
           $(PCE_DIR)/mednafen/mempatcher.c \
           $(PCE_DIR)/mednafen/okiadpcm.c \
           $(PCE_DIR)/mednafen/file.c \
           $(PCE_DIR)/mednafen/settings.c \
           $(PCE_DIR)/mednafen/state.c \
           $(PCE_DIR)/mednafen/mednafen-endian.c \
           $(PCE_DIR)/mednafen/sound/Blip_Buffer.c \
           $(wildcard $(PCE_DIR)/mednafen/pce_fast/*.c) \
           $(wildcard $(PCE_DIR)/mednafen/cdrom/*.c) \
           $(wildcard $(PCE_DIR)/mednafen/hw_misc/arcade_card/*.c) \
           $(filter-out %ivorbisfile_example.c, $(wildcard $(PCE_DIR)/mednafen/tremor/*.c))

PCE_OBJS = $(PCE_SRCS:.c=.o)

OBJS += $(NESTOPIA_OBJS) $(NESTOPIA_C_OBJS) nes_orchestrator.o $(PCE_OBJS) pce_orchestrator.o sms_orchestrator.o

SNESPI_VERSION = $(shell cat ../VERSION 2>/dev/null || echo 1.0)
MEGAPI_VERSION = $(shell cat ../VERSION 2>/dev/null || echo 1.0)

# Combined compiler definitions
DEFS     = -DSNESPI_VERSION="\"$(SNESPI_VERSION)\"" -DMEGAPI_VERSION="\"$(MEGAPI_VERSION)\"" \
           -DPIXEL_FORMAT=RGB565 -DALLOW_CPU_OVERCLOCK=1 -DHAVE_STDINT_H -DHAVE_STRINGS_H -D_GNU_SOURCE \
           -D_USE_DRZ80 -DEMU_C68K -D_ASM_DRAW_C -D_ASM_MEMORY_C -D_ASM_YM2612_C -D_ASM_MISC_C -D_ASM_CD_MEMORY_C -DNO_32X -DUSE_LIBCHDR -DZ7_ST -DEL_LOGMASK=0xc0000000

# Base include path order default (Shared first)
INCS     = -I. -I../shared -I../deps/snes9x2010/libretro -I../deps/snes9x2010/src -I../deps/snes9x2010/libretro/libretro-common/include \
           -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu \
           -I../deps/picodrive -I../deps/picodrive/pico -I../deps/picodrive/cpu -I../deps/picodrive/pico/sound -I../deps/picodrive/zlib \
           -I../deps/picodrive/pico/cd/libchdr/include \
           -I../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/include \
           -I../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib \
           -I../deps/nestopia/source -I../deps/nestopia/source/core -I../deps/nestopia/source/core/api \
           -I../deps/beetle-pce-fast-libretro -I../deps/beetle-pce-fast-libretro/mednafen -I../deps/beetle-pce-fast-libretro/mednafen/include -I../deps/beetle-pce-fast-libretro/mednafen/hw_sound -I../deps/beetle-pce-fast-libretro/mednafen/hw_cpu -I../deps/beetle-pce-fast-libretro/mednafen/hw_misc -I../deps/beetle-pce-fast-libretro/libretro-common/include \
           -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon \
           -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi

OPTIMIZE = -O3 -ffast-math -fomit-frame-pointer -ftree-vectorize -funroll-loops -fno-strict-aliasing
ARCH_FLAGS = -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard

CFLAGS   += $(OPTIMIZE) $(ARCH_FLAGS) $(INCS) $(DEFS) -Wno-unused-result -Wno-write-strings -falign-functions=2
CPPFLAGS += $(OPTIMIZE) $(ARCH_FLAGS) $(INCS) $(DEFS) -falign-functions=2 -std=gnu++14 -fno-rtti -fno-exceptions
LDFLAGS  += --allow-multiple-definition --defsym=end=_end
AFLAGS   += $(ARCH_FLAGS) $(INCS) $(DEFS)

LIBS	= $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	  $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	  $(CIRCLEHOME)/lib/sound/libsound.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/sched/libsched.a \
	  $(CIRCLEHOME)/lib/libcircle.a

STDLIB_SUPPORT = 3

../deps/picodrive/%.o: INCS = -I../shared -I../deps/picodrive -I../deps/picodrive/pico -I../deps/picodrive/cpu -I../deps/picodrive/pico/sound -I../deps/picodrive/zlib -I../deps/picodrive/pico/cd/libchdr/include -I../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/include -I../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon
../deps/picodrive/%.o: CFLAGS += -I../shared -I../deps/picodrive -I../deps/picodrive/pico -I../deps/picodrive/cpu -I../deps/picodrive/pico/sound -I../deps/picodrive/zlib -w
../deps/picodrive/%.o: CPPFLAGS += -I../shared -I../deps/picodrive -I../deps/picodrive/pico -I../deps/picodrive/cpu -I../deps/picodrive/pico/sound -I../deps/picodrive/zlib -w

../deps/snes9x/%.o: INCS = -I../shared -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
../deps/snes9x/%.o: CFLAGS += -I../shared -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu -w
../deps/snes9x/%.o: CPPFLAGS += -I../shared -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu -w

LR_SNES2010_DIR = ../deps/snes9x2010
LR_SNES2010_COMMON = $(LR_SNES2010_DIR)/libretro/libretro-common
LR_SNES2010_SRCS = $(filter-out $(LR_SNES2010_DIR)/src/hdpack.c,$(wildcard $(LR_SNES2010_DIR)/src/*.c)) \
	$(LR_SNES2010_DIR)/filter/snes_ntsc.c \
	$(LR_SNES2010_DIR)/libretro/libretro.c \
	$(LR_SNES2010_COMMON)/streams/memory_stream.c \
	$(LR_SNES2010_COMMON)/memmap/memalign.c
LR_SNES2010_OBJS = $(LR_SNES2010_SRCS:.c=.o)
LR_SNES2010_INCS = -I../shared -I$(LR_SNES2010_DIR)/libretro -I$(LR_SNES2010_DIR)/src -I$(LR_SNES2010_COMMON)/include
LR_SNES2010_DEFS = -D__LIBRETRO__ -DINLINE=inline -DHAVE_NO_LANGEXTRA -D__int64_t_defined=1 -U_POSIX_C_SOURCE \
	-Dretro_api_version=lr_snes2010_retro_api_version -Dretro_get_memory_data=lr_snes2010_retro_get_memory_data \
	-Dretro_get_memory_size=lr_snes2010_retro_get_memory_size -Dretro_set_video_refresh=lr_snes2010_retro_set_video_refresh \
	-Dretro_set_audio_sample=lr_snes2010_retro_set_audio_sample -Dretro_set_audio_sample_batch=lr_snes2010_retro_set_audio_sample_batch \
	-Dretro_set_input_poll=lr_snes2010_retro_set_input_poll -Dretro_set_input_state=lr_snes2010_retro_set_input_state \
	-Dretro_set_environment=lr_snes2010_retro_set_environment -Dretro_get_system_info=lr_snes2010_retro_get_system_info \
	-Dretro_set_controller_port_device=lr_snes2010_retro_set_controller_port_device \
	-Dretro_get_system_av_info=lr_snes2010_retro_get_system_av_info -Dretro_init=lr_snes2010_retro_init \
	-Dretro_deinit=lr_snes2010_retro_deinit -Dretro_reset=lr_snes2010_retro_reset -Dretro_run=lr_snes2010_retro_run \
	-Dretro_serialize_size=lr_snes2010_retro_serialize_size -Dretro_serialize=lr_snes2010_retro_serialize \
	-Dretro_unserialize=lr_snes2010_retro_unserialize -Dretro_cheat_reset=lr_snes2010_retro_cheat_reset \
	-Dretro_cheat_set=lr_snes2010_retro_cheat_set -Dretro_load_game=lr_snes2010_retro_load_game \
	-Dretro_load_game_special=lr_snes2010_retro_load_game_special -Dretro_unload_game=lr_snes2010_retro_unload_game \
	-Dretro_get_region=lr_snes2010_retro_get_region

OBJS := $(filter-out ../deps/snes9x/%, $(OBJS))
OBJS += $(LR_SNES2010_OBJS) ../shared/lr_snes2010_orchestrator.o ../shared/lr_snes2010_optional_stubs.o

$(LR_SNES2010_OBJS): CFLAGS += $(LR_SNES2010_INCS) $(LR_SNES2010_DEFS) -w
../shared/lr_snes2010_orchestrator.o: ../shared/lr_snes2010_orchestrator.cpp
	$(CPP) $(CPPFLAGS) $(LR_SNES2010_INCS) $(LR_SNES2010_DEFS) -c -o $@ $<

../shared/lr_snes2010_optional_stubs.o: ../shared/lr_snes2010_optional_stubs.c
	$(CC) $(CFLAGS) $(LR_SNES2010_INCS) $(LR_SNES2010_DEFS) -c -o $@ $<

snes_orchestrator.o: CPPFLAGS += $(LR_SNES2010_INCS) $(LR_SNES2010_DEFS)

md_orchestrator.o: INCS = -I../shared -I../deps/picodrive -I../deps/picodrive/pico -I../deps/picodrive/cpu -I../deps/picodrive/pico/sound -I../deps/picodrive/zlib -I../deps/picodrive/pico/cd/libchdr/include -I../deps/picodrive/pico/cd/libchdr/deps/lzma-24.05/include -I../deps/picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon
md_orchestrator.o: CFLAGS += -I../shared -I../deps/picodrive -I../deps/picodrive/pico -I../deps/picodrive/cpu -I../deps/picodrive/pico/sound -I../deps/picodrive/zlib
md_orchestrator.o: CPPFLAGS += -I../shared -I../deps/picodrive -I../deps/picodrive/pico -I../deps/picodrive/cpu -I../deps/picodrive/pico/sound -I../deps/picodrive/zlib

../deps/nestopia/%.o: INCS = -I../shared -I../deps/nestopia/source -I../deps/nestopia/source/core -I../deps/nestopia/source/core/api -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
../deps/nestopia/%.o: CFLAGS += -I../shared -I../deps/nestopia/source -I../deps/nestopia/source/core -I../deps/nestopia/source/core/api -DNST_NO_ZLIB -DNST_NO_HQ2X -DNST_NO_SCALEX -DNST_NO_2XSAI -w
../deps/nestopia/%.o: CPPFLAGS += -I../shared -I../deps/nestopia/source -I../deps/nestopia/source/core -I../deps/nestopia/source/core/api -DNST_NO_ZLIB -DNST_NO_HQ2X -DNST_NO_SCALEX -DNST_NO_2XSAI -fexceptions -fpermissive -w

nes_orchestrator.o: INCS = -I../shared -I../deps/nestopia/source -I../deps/nestopia/source/core -I../deps/nestopia/source/core/api -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
nes_orchestrator.o: CFLAGS += -I../shared -I../deps/nestopia/source -I../deps/nestopia/source/core -I../deps/nestopia/source/core/api -DNST_NO_ZLIB -DNST_NO_HQ2X -DNST_NO_SCALEX -DNST_NO_2XSAI
nes_orchestrator.o: CPPFLAGS += -I../shared -I../deps/nestopia/source -I../deps/nestopia/source/core -I../deps/nestopia/source/core/api -DNST_NO_ZLIB -DNST_NO_HQ2X -DNST_NO_SCALEX -DNST_NO_2XSAI -fexceptions -fpermissive -Wno-unused-local-typedefs -Wno-attributes -Wno-switch -Wno-narrowing

../deps/beetle-pce-fast-libretro/%.o: INCS = -I../shared -I../deps/beetle-pce-fast-libretro -I../deps/beetle-pce-fast-libretro/mednafen -I../deps/beetle-pce-fast-libretro/mednafen/include -I../deps/beetle-pce-fast-libretro/mednafen/hw_sound -I../deps/beetle-pce-fast-libretro/mednafen/hw_cpu -I../deps/beetle-pce-fast-libretro/mednafen/hw_misc -I../deps/beetle-pce-fast-libretro/libretro-common/include -I../deps/picodrive/pico/cd/libchdr/include -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
../deps/beetle-pce-fast-libretro/%.o: CFLAGS += -DNEED_CD -DNEED_TREMOR -DFRONTEND_SUPPORTS_RGB565 -DSTATIC_LINKING=1 -DNST_NO_ZLIB -DINLINE="inline" -DMEDNAFEN_VERSION_NUMERIC=931 -DMEDNAFEN_VERSION=\"0.9.31\" -D__LIBRETRO__ -DSTDC_HEADERS -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__int64_t_defined=1 -DHAVE_CHD -D_7ZIP_ST -DZSTD_DISABLE_ASM -w
../deps/beetle-pce-fast-libretro/%.o: CPPFLAGS += -DNEED_CD -DNEED_TREMOR -DFRONTEND_SUPPORTS_RGB565 -DSTATIC_LINKING=1 -DNST_NO_ZLIB -DINLINE="inline" -DMEDNAFEN_VERSION_NUMERIC=931 -DMEDNAFEN_VERSION=\"0.9.31\" -D__LIBRETRO__ -DSTDC_HEADERS -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__int64_t_defined=1 -DHAVE_CHD -D_7ZIP_ST -DZSTD_DISABLE_ASM -w

pce_orchestrator.o: INCS = -I../shared -I../deps/beetle-pce-fast-libretro -I../deps/beetle-pce-fast-libretro/mednafen -I../deps/beetle-pce-fast-libretro/mednafen/include -I../deps/beetle-pce-fast-libretro/mednafen/hw_sound -I../deps/beetle-pce-fast-libretro/mednafen/hw_cpu -I../deps/beetle-pce-fast-libretro/mednafen/hw_misc -I../deps/beetle-pce-fast-libretro/libretro-common/include -I../deps/picodrive/pico/cd/libchdr/include -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
pce_orchestrator.o: CFLAGS += -DNEED_CD -DNEED_TREMOR -DFRONTEND_SUPPORTS_RGB565 -DSTATIC_LINKING=1 -DINLINE="inline" -DMEDNAFEN_VERSION_NUMERIC=931 -DMEDNAFEN_VERSION=\"0.9.31\" -D__LIBRETRO__ -DSTDC_HEADERS -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__int64_t_defined=1 -DHAVE_CHD -D_7ZIP_ST -DZSTD_DISABLE_ASM
pce_orchestrator.o: CPPFLAGS += -DNEED_CD -DNEED_TREMOR -DFRONTEND_SUPPORTS_RGB565 -DSTATIC_LINKING=1 -DINLINE="inline" -DMEDNAFEN_VERSION_NUMERIC=931 -DMEDNAFEN_VERSION=\"0.9.31\" -D__LIBRETRO__ -DSTDC_HEADERS -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__int64_t_defined=1 -DHAVE_CHD -D_7ZIP_ST -DZSTD_DISABLE_ASM

# Custom assembler rule for lowercase .s files (Cyclone, etc.)
%.o: %.s
	@echo "  AS    $@"
	@$(AS) $(AFLAGS) -c -o $@ $<

# Generate ILP32 offset header for ASM files dynamically
../deps/picodrive/pico/pico_int_offs.h:
	cd ../deps/picodrive/tools && CC="arm-none-eabi-gcc" CFLAGS="$(DEFS) -I../pico/sound" sh ./mkoffsets.sh ../pico

# Local symlink so draw_arm.o can find it via local include
pico_int_offs.h: ../deps/picodrive/pico/pico_int_offs.h
	ln -sf ../deps/picodrive/pico/pico_int_offs.h .

pico:
	ln -sf ../deps/picodrive/pico .

pico/%: pico
	@:

pico/pico_int_offs.h: pico

$(OBJS): ../deps/picodrive/pico/pico_int_offs.h pico_int_offs.h pico

main.o: ../shared/main.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

clib_stubs.o: ../shared/clib_stubs.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

EXTRALIBS += $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libm.a) \
             $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libc.a) \
             $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libnosys.a)

include $(CIRCLEHOME)/Rules.mk
.DEFAULT_GOAL := $(TARGET).img

-include $(DEPS)
