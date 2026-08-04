#
# Makefile for Standalone SNES baremetal emulator
#

CIRCLEHOME = ../deps/circle

OBJS	= main.o kernel.o osd.o emu_orchestrator.o clib_stubs.o \
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
	  ../deps/snes9x/filter/snes_ntsc.o

SNESPI_VERSION = $(shell cat ../VERSION 2>/dev/null || echo 1.0)

DEFS     = -DSNESPI_VERSION="\"$(SNESPI_VERSION)\"" \
           -DPIXEL_FORMAT=RGB565 -DALLOW_CPU_OVERCLOCK=1 -DHAVE_STDINT_H -DHAVE_STRINGS_H -D_GNU_SOURCE

INCS     = -I. -I../shared -I../deps/snes9x2010/libretro -I../deps/snes9x2010/src -I../deps/snes9x2010/libretro/libretro-common/include \
           -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu \
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

emu_orchestrator.o: CPPFLAGS += $(LR_SNES2010_INCS) $(LR_SNES2010_DEFS)

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
