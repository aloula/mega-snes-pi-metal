#
# Makefile for Standalone SNES baremetal emulator
#

CIRCLEHOME = ../circle

OBJS	= main.o kernel.o osd.o emu_orchestrator.o clib_stubs.o \
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
	  ../snes9x/filter/snes_ntsc.o

SNESPI_VERSION = $(shell cat ../VERSION 2>/dev/null || echo 1.0)

DEFS     = -DSNESPI_VERSION="\"$(SNESPI_VERSION)\"" \
           -DPIXEL_FORMAT=RGB565 -DALLOW_CPU_OVERCLOCK=1 -DHAVE_STDINT_H -DHAVE_STRINGS_H -D_GNU_SOURCE

INCS     = -I. -I../shared -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu \
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

../snes9x/%.o: INCS = -I../shared -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
../snes9x/%.o: CFLAGS += -I../shared -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu -w
../snes9x/%.o: CPPFLAGS += -I../shared -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu -w

emu_orchestrator.o: INCS = -I../shared -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon -I/usr/include/newlib/c++/13.2.1 -I/usr/include/newlib/c++/13.2.1/arm-none-eabi
emu_orchestrator.o: CFLAGS += -I../shared -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu
emu_orchestrator.o: CPPFLAGS += -I../shared -I../snes9x -I../snes9x/apu -I../snes9x/apu/bapu

main.o: ../shared/main.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

clib_stubs.o: ../shared/clib_stubs.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

EXTRALIBS += $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libm.a) \
             $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libc.a) \
             $(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libnosys.a)

-include $(DEPS)
