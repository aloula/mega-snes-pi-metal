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

INCS     = -I. -I../shared -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu \
           -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon \
           -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi

OPTIMIZE = -O3 -ffast-math -fomit-frame-pointer -ftree-vectorize -funroll-loops -fno-strict-aliasing
ARCH_FLAGS = -mcpu=cortex-a76 -mfpu=neon-fp-armv8 -mfloat-abi=hard

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

include $(CIRCLEHOME)/Rules.mk

../deps/snes9x/%.o: INCS = -I../shared -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
../deps/snes9x/%.o: CFLAGS += -I../shared -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu -w
../deps/snes9x/%.o: CPPFLAGS += -I../shared -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu -w

emu_orchestrator.o: INCS = -I../shared -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
emu_orchestrator.o: CFLAGS += -I../shared -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu
emu_orchestrator.o: CPPFLAGS += -I../shared -I../deps/snes9x -I../deps/snes9x/apu -I../deps/snes9x/apu/bapu

main.o: ../shared/main.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

clib_stubs.o: ../shared/clib_stubs.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

EXTRALIBS += $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libm.a) \
             $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libc.a) \
             $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libnosys.a)

-include $(DEPS)
