#include "snes_orchestrator.h"
#include "shared_state.h"
#include <circle/alloc.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/synchronize.h>

#define __time_t_defined
#define _TIME_T_DECLARED
#undef BIT


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Snes9x headers
#include <snes9x.h>
#include <memmap.h>
#include <apu/apu.h>
#include <gfx.h>
#include <controls.h>
#include <cheats.h>
#include <snapshot.h>
#include <conffile.h>
#include <apu/bapu/snes/snes.hpp>

static const char FromOrchestrator[] = "orchestrator";

// Snes9x platform hook implementations

bool8 S9xInitUpdate(void) {
    return TRUE;
}

bool8 S9xContinueUpdate(int width, int height) {
    return TRUE;
}



bool8 S9xDeinitUpdate(int width, int height) {
    static u32 frame_count = 0;
    frame_count++;

    int idx = g_SharedState.emu_write_idx;
    g_SharedState.game_w[idx] = width;
    g_SharedState.game_h[idx] = height;
    g_SharedState.start_line[idx] = 0;

    u16 *src = GFX.Screen;
    u16 *dest = g_SharedState.emu_frame_buffer[idx];
    int src_stride = GFX.Pitch / sizeof(u16);
    
    for (int y = 0; y < height; y++) {
        memcpy(dest + y * width, src + y * src_stride, width * sizeof(u16));
    }

    g_SharedState.emu_read_idx = idx;
    g_SharedState.emu_write_idx = 1 - idx;

    DataMemBarrier();
    g_SharedState.video_frame_ready = TRUE;

    return TRUE;
}

void S9xExit(void) {
    CLogger::Get()->Write("snes9x", LogNotice, "S9xExit called!");
    while(1);
}

void S9xMessage(int type, int number, const char *message) {
    CLogger::Get()->Write("snes9x", LogNotice, "[Snes9x Msg] Type=%d Num=%d: %s", type, number, message);
}

bool8 S9xOpenSoundDevice(void) {
    return TRUE;
}

bool S9xPollButton(uint32 id, bool *pressed) {
    return false;
}

bool S9xPollPointer(uint32 id, int16 *x, int16 *y) {
    return false;
}

bool S9xPollAxis(uint32 id, int16 *value) {
    return false;
}

void S9xToggleSoundChannel(int) {}
void S9xParsePortConfig(ConfigFile&, int) {}
const char* S9xStringInput(const char* in) { return in; }
void S9xInitInputDevices() {}
void S9xHandlePortCommand(s9xcommand_t, short, short) {}
void S9xExtraUsage(void) {}
void S9xParseArg(char **argv, int &index, int argc) {}

std::string S9xGetDirectory(enum s9x_getdirtype type) {
    switch (type) {
        case SRAM_DIR:
            return "SD:/sram";
        case SNAPSHOT_DIR:
            return "SD:/saves";
        case ROMFILENAME_DIR:
        case ROM_DIR:
            return "SD:/roms";
        default:
            return "SD:/";
    }
}

std::string S9xGetFilenameInc(std::string extension, enum s9x_getdirtype type) {
    return S9xGetDirectory(type) + "/save" + extension;
}

bool8 S9xOpenSnapshotFile(const char *filepath, bool8 read_only, STREAM *file) {
    if (read_only) {
        if ((*file = OPEN_STREAM(filepath, "rb")) != 0) {
            return TRUE;
        }
    } else {
        if ((*file = OPEN_STREAM(filepath, "wb")) != 0) {
            return TRUE;
        }
    }
    return FALSE;
}

void S9xCloseSnapshotFile(STREAM file) {
    CLOSE_STREAM(file);
}

void S9xAutoSaveSRAM(void) {
}

void S9xSyncSpeed(void) {
}

CSNESOrchestrator::CSNESOrchestrator(FATFS *pFileSystem)
    : m_pFileSystem(pFileSystem),
      m_pRomBuffer(nullptr),
      m_bRomLoaded(FALSE)
{
}

CSNESOrchestrator::~CSNESOrchestrator() {
    S9xGraphicsDeinit();
}

boolean CSNESOrchestrator::Initialize() {
    // 1. Initialize Snes9x settings
    memset(&Settings, 0, sizeof(Settings));
    
    Settings.MouseMaster = TRUE;
    Settings.SuperScopeMaster = TRUE;
    Settings.JustifierMaster = TRUE;
    Settings.MultiPlayer5Master = TRUE;
    Settings.MacsRifleMaster = TRUE;
    Settings.FrameTimePAL = 20000;
    Settings.FrameTimeNTSC = 16667;
    Settings.SixteenBitSound = TRUE;
    Settings.Stereo = TRUE;
    Settings.SoundPlaybackRate = 44100;
    Settings.SoundInputRate = 31950;
    Settings.SoundSync = TRUE;
    Settings.InterpolationMethod = DSP_INTERPOLATION_GAUSSIAN; // Enable hardware-authentic Gaussian audio interpolation
    Settings.Transparency = TRUE;
    Settings.AutoDisplayMessages = TRUE;
    Settings.InitialInfoStringTimeout = 120;
    Settings.HDMATimingHack = 100;
    Settings.BlockInvalidVRAMAccessMaster = TRUE;
    Settings.SeparateEchoBuffer = FALSE;
    Settings.CartAName[0] = 0;
    Settings.CartBName[0] = 0;
    Settings.AutoSaveDelay = 1;
    Settings.DontSaveOopsSnapshot = TRUE;
    Settings.MaxSpriteTilesPerLine = 34;

    Settings.OneClockCycle = 6;
    Settings.OneSlowClockCycle = 8;
    Settings.TwoClockCycles = 12;
    Settings.DisableGraphicWindows = FALSE;

    // Initialize Snes9x Graphics
    if (!S9xGraphicsInit()) {
        CLogger::Get()->Write(FromOrchestrator, LogPanic, "Failed to initialize Snes9x Graphics");
        return FALSE;
    }

    // 2. Initialize Snes9x CPU and APU
    if (!Memory.Init() || !S9xInitAPU()) {
        CLogger::Get()->Write(FromOrchestrator, LogPanic, "Failed to initialize Snes9x memory or APU");
        return FALSE;
    }

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "smp.apuram address: %p", SNES::smp.apuram);

    // 3. Initialize Sound
    S9xInitSound(128); // 128ms buffer capacity to prevent overflow dropouts
    S9xSetSoundMute(FALSE);

    // 4. Initialize Input mappings
    S9xInitInputDevices();
    S9xUnmapAllControls();
    
    // Map Pad 1 (IDs 0-11)
    S9xMapButton(0, S9xGetCommandT("Joypad1 Up"), false);
    S9xMapButton(1, S9xGetCommandT("Joypad1 Down"), false);
    S9xMapButton(2, S9xGetCommandT("Joypad1 Left"), false);
    S9xMapButton(3, S9xGetCommandT("Joypad1 Right"), false);
    S9xMapButton(4, S9xGetCommandT("Joypad1 A"), false);
    S9xMapButton(5, S9xGetCommandT("Joypad1 B"), false);
    S9xMapButton(6, S9xGetCommandT("Joypad1 X"), false);
    S9xMapButton(7, S9xGetCommandT("Joypad1 Y"), false);
    S9xMapButton(8, S9xGetCommandT("Joypad1 L"), false);
    S9xMapButton(9, S9xGetCommandT("Joypad1 R"), false);
    S9xMapButton(10, S9xGetCommandT("Joypad1 Start"), false);
    S9xMapButton(11, S9xGetCommandT("Joypad1 Select"), false);

    // Map Pad 2 (IDs 12-23)
    S9xMapButton(12, S9xGetCommandT("Joypad2 Up"), false);
    S9xMapButton(13, S9xGetCommandT("Joypad2 Down"), false);
    S9xMapButton(14, S9xGetCommandT("Joypad2 Left"), false);
    S9xMapButton(15, S9xGetCommandT("Joypad2 Right"), false);
    S9xMapButton(16, S9xGetCommandT("Joypad2 A"), false);
    S9xMapButton(17, S9xGetCommandT("Joypad2 B"), false);
    S9xMapButton(18, S9xGetCommandT("Joypad2 X"), false);
    S9xMapButton(19, S9xGetCommandT("Joypad2 Y"), false);
    S9xMapButton(20, S9xGetCommandT("Joypad2 L"), false);
    S9xMapButton(21, S9xGetCommandT("Joypad2 R"), false);
    S9xMapButton(22, S9xGetCommandT("Joypad2 Start"), false);
    S9xMapButton(23, S9xGetCommandT("Joypad2 Select"), false);

    for (int i = 0; i < 2; i++) {
        S9xSetController(i, CTL_JOYPAD, i, 0, 0, 0);
    }

    m_bRomLoaded = FALSE;
    return TRUE;
}

boolean CSNESOrchestrator::LoadROM(const char *pRomName, unsigned nRomSize) {
    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Loading ROM: %s (size %u)", pRomName, nRomSize);

    // Snes9x can load ROM directly using Memory.LoadROM
    if (!Memory.LoadROM(pRomName)) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Snes9x failed to load ROM: %s", pRomName);
        return FALSE;
    }

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "ROM Loaded Successfully!");

    strncpy(m_CurrentRomName, pRomName, sizeof(m_CurrentRomName) - 1);
    m_CurrentRomName[sizeof(m_CurrentRomName) - 1] = '\0';

    // Soft reset controllers and CPU
    S9xSoftReset();
    S9xControlsSoftReset();

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "LoadROM successfully completed!");

    m_bRomLoaded = TRUE;
    return TRUE;
}

void CSNESOrchestrator::RunFrame() {
    if (!m_bRomLoaded) return;

    // 1. Report controller state from g_SharedState to Snes9x
    u16 pad1 = g_SharedState.pad1;
    if ((pad1 & (1 << 10)) && (pad1 & (1 << 11))) {
        pad1 = 0; // Mask inputs when START + SELECT is held
    }

    S9xReportButton(0, (pad1 & (1 << 0)) != 0); // Up
    S9xReportButton(1, (pad1 & (1 << 1)) != 0); // Down
    S9xReportButton(2, (pad1 & (1 << 2)) != 0); // Left
    S9xReportButton(3, (pad1 & (1 << 3)) != 0); // Right
    S9xReportButton(4, (pad1 & (1 << 4)) != 0); // A
    S9xReportButton(5, (pad1 & (1 << 5)) != 0); // B
    S9xReportButton(6, (pad1 & (1 << 6)) != 0); // X
    S9xReportButton(7, (pad1 & (1 << 7)) != 0); // Y
    S9xReportButton(8, (pad1 & (1 << 8)) != 0); // L
    S9xReportButton(9, (pad1 & (1 << 9)) != 0); // R
    S9xReportButton(10, (pad1 & (1 << 10)) != 0); // Start
    S9xReportButton(11, (pad1 & (1 << 11)) != 0); // Select

    u16 pad2 = g_SharedState.pad2;
    if ((pad2 & (1 << 10)) && (pad2 & (1 << 11))) {
        pad2 = 0;
    }

    S9xReportButton(12, (pad2 & (1 << 0)) != 0); // Up
    S9xReportButton(13, (pad2 & (1 << 1)) != 0); // Down
    S9xReportButton(14, (pad2 & (1 << 2)) != 0); // Left
    S9xReportButton(15, (pad2 & (1 << 3)) != 0); // Right
    S9xReportButton(16, (pad2 & (1 << 4)) != 0); // A
    S9xReportButton(17, (pad2 & (1 << 5)) != 0); // B
    S9xReportButton(18, (pad2 & (1 << 6)) != 0); // X
    S9xReportButton(19, (pad2 & (1 << 7)) != 0); // Y
    S9xReportButton(20, (pad2 & (1 << 8)) != 0); // L
    S9xReportButton(21, (pad2 & (1 << 9)) != 0); // R
    S9xReportButton(22, (pad2 & (1 << 10)) != 0); // Start
    S9xReportButton(23, (pad2 & (1 << 11)) != 0); // Select

    // 2. Wait until video frame buffer is consumed by Core 1 (Video domain)
    while (g_SharedState.video_frame_ready) {
        CTimer::SimpleusDelay(100);
    }

    // 3. Emulate 1 frame
    S9xMainLoop();

    // 4. Mix and retrieve audio samples from Snes9x to our audio ring buffer
    int avail = S9xGetSampleCount();
    avail &= ~1; // Ensure even number of samples (stereo pairs) to prevent channel misalignment
    while (avail > 0) {
        int chunk = avail;
        if (chunk > 2048) chunk = 2048;
        static s16 local_audio_buf[2048];
        S9xMixSamples((uint8 *)local_audio_buf, chunk);
        g_SharedState.audio_ring_buffer.Write(local_audio_buf, chunk >> 1);
        avail -= chunk;
    }
}

void CSNESOrchestrator::SaveState(int slot) {
    if (!m_bRomLoaded) return;

    char stateName[160];
    strncpy(stateName, m_CurrentRomName, sizeof(stateName) - 8);
    stateName[sizeof(stateName) - 8] = '\0';
    char *dot = strrchr(stateName, '.');
    if (dot) {
        *dot = '\0';
    }
    sprintf(stateName + strlen(stateName), ".s%d", slot);

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Saving state to: %s", stateName);
    bool8 ret = S9xFreezeGame(stateName);
    if (ret) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "State saved successfully!");
    } else {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to save state!");
    }
}

void CSNESOrchestrator::LoadState(int slot) {
    if (!m_bRomLoaded) return;

    char stateName[160];
    strncpy(stateName, m_CurrentRomName, sizeof(stateName) - 8);
    stateName[sizeof(stateName) - 8] = '\0';
    char *dot = strrchr(stateName, '.');
    if (dot) {
        *dot = '\0';
    }
    sprintf(stateName + strlen(stateName), ".s%d", slot);

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Loading state from: %s", stateName);
    bool8 ret = S9xUnfreezeGame(stateName);
    if (ret) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "State loaded successfully!");
    } else {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to load state!");
    }
}

boolean CSNESOrchestrator::IsPAL() const {
    return Settings.PAL ? TRUE : FALSE;
}
