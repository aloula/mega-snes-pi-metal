#include "emu_orchestrator.h"
#include "shared_state.h"
#include <circle/alloc.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/synchronize.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
#define UTYPES_DEFINED 1
#include <pico/pico_int.h>
#include <pico/state.h>
#ifdef __cplusplus
}
#endif

// In-memory save state structure and callbacks for PicoDrive
struct savestate_state {
   const char *load_buf;
   char *save_buf;
   size_t size;
   size_t pos;
};

static size_t state_read(void *p, size_t size, size_t nmemb, void *file)
{
   struct savestate_state *state = (struct savestate_state *)file;
   size_t bsize = size * nmemb;

   if (state->pos + bsize > state->size) {
      bsize = state->size - state->pos;
      if ((int)bsize <= 0)
         return 0;
   }

   memcpy(p, state->load_buf + state->pos, bsize);
   state->pos += bsize;
   return bsize;
}

static size_t state_write(void *p, size_t size, size_t nmemb, void *file)
{
   struct savestate_state *state = (struct savestate_state *)file;
   size_t bsize = size * nmemb;

   if (state->pos + bsize > state->size) {
      bsize = state->size - state->pos;
      if ((int)bsize <= 0)
         return 0;
   }

   memcpy(state->save_buf + state->pos, p, bsize);
   state->pos += bsize;
   return bsize;
}

static __attribute__((unused)) size_t state_skip(void *p, size_t size, size_t nmemb, void *file)
{
   struct savestate_state *state = (struct savestate_state *)file;
   size_t bsize = size * nmemb;

   state->pos += bsize;
   return bsize;
}

static size_t state_eof(void *file)
{
   struct savestate_state *state = (struct savestate_state *)file;

   return state->pos >= state->size;
}

static int state_fseek(void *file, long offset, int whence)
{
   struct savestate_state *state = (struct savestate_state *)file;

   switch (whence) {
   case SEEK_SET:
      state->pos = offset;
      break;
   case SEEK_CUR:
      state->pos += offset;
      break;
   case SEEK_END:
      state->pos = state->size + offset;
      break;
   }
   return (int)state->pos;
}

#define ROM_BUFFER_SIZE (8 * 1024 * 1024)

extern "C" {
unsigned int p32x_event_times[1] = {0};
}

static const char FromOrchestrator[] = "orchestrator";

// Audio temp buffer for Picodrive output (64KB buffer)
static s16 g_AudioTempBuf[32768];
static void EmuSoundCallback(int len);

static void ApplyMDPicoConfig() {
    PicoIn.opt = POPT_EN_FM | POPT_EN_PSG | POPT_EN_Z80 | POPT_EN_STEREO | POPT_FM_YM2612 |
                 POPT_EN_MCD_PCM | POPT_EN_MCD_CDDA | POPT_EN_MCD_GFX |
                 POPT_ACC_SPRITES | POPT_DIS_IDLE_DET;
    PicoIn.skipFrame = 0;
    PicoIn.sndRate = 44100;
    PicoIn.sndOut = g_AudioTempBuf;
    PicoIn.writeSound = EmuSoundCallback;
    PicoIn.autoRgnOrder = 0x184; // Prefer USA (NTSC 60Hz), then EUR (PAL 50Hz), then JAP (NTSC 60Hz)
    // PicoLoadMedia() resets quirks to 0, so this must be re-applied after every load
    // (see the ApplyMDPicoConfig() call following PicoLoadMedia() in LoadROM()).
    // Without it, CaptureRewindState()'s VDP-busy guard never engages and a rewind
    // snapshot taken mid-DMA/FIFO-transfer can leave the 68000 microstate
    // unrecoverable, freezing the screen with looping audio when later rewound into.
    PicoIn.quirks |= PQUIRK_SAFE_REWIND;
}

// Sound callback
static void EmuSoundCallback(int len) {
    if (len <= 0) return;
    unsigned num_bytes = (unsigned)len;
    if (num_bytes > sizeof(g_AudioTempBuf)) {
        num_bytes = sizeof(g_AudioTempBuf);
    }
    unsigned num_stereo_samples = num_bytes / 4;
    if (num_stereo_samples == 0) return;

    g_SharedState.audio_ring_buffer.Write(g_AudioTempBuf, num_stereo_samples);
}



// plat_mmap stubs
extern "C" void *plat_mmap(unsigned long addr, size_t size, int need_exec, int is_fixed) {
    return malloc(size);
}
extern "C" void *plat_mremap(void *ptr, size_t oldsize, size_t newsize) {
    return realloc(ptr, newsize);
}
extern "C" void plat_munmap(void *ptr, size_t size) {
    free(ptr);
}
extern "C" void *plat_mem_get_for_drc(size_t size) {
    return nullptr;
}
extern "C" int plat_mem_set_exec(void *ptr, size_t size) {
    return 0;
}

CEmuOrchestrator::CEmuOrchestrator(FATFS *pFileSystem)
    : m_pFileSystem(pFileSystem),
      m_pRomBuffer(nullptr),
      m_bRomLoaded(FALSE)
{
    m_nRewindWriteIdx = 0;
    m_nRewindCount = 0;
    m_nRewindFrameCounter = 0;
    m_nStateSize = 0;
    for (int i = 0; i < 6; i++) {
        m_pRewindBuffers[i] = nullptr;
        m_nRewindStateSizes[i] = 0;
    }
}

CEmuOrchestrator::~CEmuOrchestrator() {
    if (m_pRomBuffer) {
        delete[] m_pRomBuffer;
    }
    for (int i = 0; i < 6; i++) {
        if (m_pRewindBuffers[i] != nullptr) {
            delete[] m_pRewindBuffers[i];
            m_pRewindBuffers[i] = nullptr;
        }
    }
}

boolean CEmuOrchestrator::Initialize() {
    m_pRomBuffer = new u8[ROM_BUFFER_SIZE];
    if (!m_pRomBuffer) {
        CLogger::Get()->Write(FromOrchestrator, LogPanic, "Failed to allocate ROM buffer");
        return FALSE;
    }

    ApplyMDPicoConfig();

    PicoInit();
    return TRUE;
}

static int my_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

static const char *FindExistingBiosPath(const char *primary, const char *alt1, const char *alt2, const char *alt3, const char *alt4) {
    FILINFO fno;
    if (f_stat(primary, &fno) == FR_OK) return primary;
    if (alt1 && f_stat(alt1, &fno) == FR_OK) return alt1;
    if (alt2 && f_stat(alt2, &fno) == FR_OK) return alt2;
    if (alt3 && f_stat(alt3, &fno) == FR_OK) return alt3;
    if (alt4 && f_stat(alt4, &fno) == FR_OK) return alt4;
    return primary;
}

static const char *GetBiosFilename(int *region, const char *cd_fname) {
    static char bios_path[256];
    int reg = *region;
    
    // If region is unknown (0), try to detect from the filename
    if (reg == 0 && cd_fname != nullptr) {
        char lower_name[256];
        size_t len = strlen(cd_fname);
        if (len >= sizeof(lower_name)) len = sizeof(lower_name) - 1;
        for (size_t i = 0; i < len; ++i) {
            char c = cd_fname[i];
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            lower_name[i] = c;
        }
        lower_name[len] = '\0';
        
        boolean is_us = FALSE;
        boolean is_eu = FALSE;
        if (strstr(lower_name, "(u)") != nullptr || 
            strstr(lower_name, "(usa)") != nullptr || 
            strstr(lower_name, "(us)") != nullptr ||
            strstr(lower_name, "usa") != nullptr) {
            is_us = TRUE;
        } else if (strstr(lower_name, "(e)") != nullptr || 
                   strstr(lower_name, "(eur)") != nullptr || 
                   strstr(lower_name, "(europe)") != nullptr ||
                   strstr(lower_name, "europe") != nullptr) {
            is_eu = TRUE;
        }
        
        if (is_us) {
            reg = 4;
            *region = 4;
        } else if (is_eu) {
            reg = 8;
            *region = 8;
        }
    }
    
    // Check region: 4 = US, 8 = EU, others (1, 2) = JP
    if (reg == 4) {
        snprintf(bios_path, sizeof(bios_path), "%s", FindExistingBiosPath(
            "SD:/bios/bios_CD_U.bin",
            "SD:/bios/us_scd2_9303.bin",
            "SD:/bios/us_scd1_9210.bin",
            "SD:/bios/mcd2_us.bin",
            "SD:/bios/bios_CD_U.iso"
        ));
    } else if (reg == 8) {
        snprintf(bios_path, sizeof(bios_path), "%s", FindExistingBiosPath(
            "SD:/bios/bios_CD_E.bin",
            "SD:/bios/eu_mcd2_9306.bin",
            "SD:/bios/eu_mcd1_9210.bin",
            "SD:/bios/mcd2_eu.bin",
            "SD:/bios/bios_CD_E.iso"
        ));
    } else {
        snprintf(bios_path, sizeof(bios_path), "%s", FindExistingBiosPath(
            "SD:/bios/bios_CD_J.bin",
            "SD:/bios/jp_mcd1_9112.bin",
            "SD:/bios/jp_mcd2_9212.bin",
            "SD:/bios/mcd2_jp.bin",
            "SD:/bios/bios_CD_J.iso"
        ));
    }
    
    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Sega CD BIOS requested for region %d, returning path: %s", reg, bios_path);
    return bios_path;
}

boolean CEmuOrchestrator::LoadROM(const char *pRomName, unsigned nRomSize) {
    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Loading ROM: %s (size %u)", pRomName, nRomSize);

    // Reset hardware architecture mode flags
    PicoIn.AHW = 0;
    // Restore MD/Mega CD config before media detection/load in case PicoIn was altered.
    ApplyMDPicoConfig();

    boolean is_cd = FALSE;
    const char *pDot = strrchr(pRomName, '.');
    if (pDot != nullptr) {
        pDot++;
        if (my_strcasecmp(pDot, "iso") == 0 ||
            my_strcasecmp(pDot, "cue") == 0 ||
            my_strcasecmp(pDot, "chd") == 0) {
            is_cd = TRUE;
        }
    }

    if (is_cd) {
        // For CD images, we do not load them into memory.
        // Picodrive reads CD tracks directly from the file system.
        enum media_type_e type = PicoLoadMedia(pRomName, nullptr, 0, nullptr, GetBiosFilename, nullptr, nullptr);
        if (type <= 0) {
            CLogger::Get()->Write(FromOrchestrator, LogError, "Picodrive failed to load CD image: %s (type %d)", pRomName, type);
            return FALSE;
        }
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "CD Image Loaded Successfully! Type: %d", type);
    } else {
        // Ensure ROM size is within bounds
        if (nRomSize > ROM_BUFFER_SIZE) {
            CLogger::Get()->Write(FromOrchestrator, LogError, "ROM size too big: %u > %u", nRomSize, ROM_BUFFER_SIZE);
            return FALSE;
        }

        FIL file;
        FRESULT res = f_open(&file, pRomName, FA_READ);
        if (res != FR_OK) {
            CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to open ROM file: %s (error %d)", pRomName, res);
            return FALSE;
        }

        UINT bytesRead = 0;
        res = f_read(&file, m_pRomBuffer, nRomSize, &bytesRead);
        f_close(&file);

        if (res != FR_OK || bytesRead != nRomSize) {
            CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to read whole ROM. Read %u of %u bytes (error %d)", bytesRead, nRomSize, res);
            return FALSE;
        }

        // Load media into Picodrive
        enum media_type_e type = PicoLoadMedia(pRomName, m_pRomBuffer, nRomSize, nullptr, GetBiosFilename, nullptr, nullptr);
        if (type <= 0) {
            CLogger::Get()->Write(FromOrchestrator, LogError, "Picodrive failed to load ROM: %s (type %d)", pRomName, type);
            return FALSE;
        }

        CLogger::Get()->Write(FromOrchestrator, LogNotice, "ROM Loaded Successfully! Type: %d", type);
    }

    strncpy(m_CurrentRomName, pRomName, sizeof(m_CurrentRomName) - 1);
    m_CurrentRomName[sizeof(m_CurrentRomName) - 1] = '\0';

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Configuring emulator draw formats, controls, and power...");
    
    // Re-apply Pico config after PicoLoadMedia to ensure quirks remain active
    ApplyMDPicoConfig();

    // Set draw format
    PicoDrawSetOutFormat(PDF_RGB555, 0);

    // Configure input ports as 3-button or 6-button gamepads based on game requirements
    extern boolean is6ButtonGame(const char *pRomName);
    enum input_device dev_type = is6ButtonGame(pRomName) ? PICO_INPUT_PAD_6BTN : PICO_INPUT_PAD_3BTN;
    PicoSetInputDevice(0, dev_type);
    PicoSetInputDevice(1, dev_type);

    // Power on and reset
    PicoPower();
    PsndRerate(0);
    PicoLoopPrepare();

    // Clear input registers
    for (int i = 0; i < 4; ++i) {
        PicoIn.pad[i] = 0;
        PicoIn.padInt[i] = 0;
    }

    // Reset gamepad handshake phase state
    Pico.m.padTHPhase[0] = 0;
    Pico.m.padTHPhase[1] = 0;
    Pico.m.padDelay[0] = 0;
    Pico.m.padDelay[1] = 0;
    Pico.m.frame_count = 0;

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Emulator power-on and reset completed!");

    // Free existing rewind buffers if any
    for (int i = 0; i < 6; i++) {
        if (m_pRewindBuffers[i] != nullptr) {
            delete[] m_pRewindBuffers[i];
            m_pRewindBuffers[i] = nullptr;
        }
        m_nRewindStateSizes[i] = 0;
    }
    m_nRewindWriteIdx = 0;
    m_nRewindCount = 0;
    m_nRewindFrameCounter = 0;
    m_nStateSize = 0;

    // Detect state size using dry run
    struct savestate_state temp_state = { 0 };
    temp_state.load_buf = nullptr;
    temp_state.save_buf = nullptr;
    temp_state.size = 0;
    temp_state.pos = 0;
    int size_ret = PicoStateFP(&temp_state, 1, nullptr, state_skip, nullptr, state_fseek);
    size_t min_size = is_cd ? (2048 * 1024) : (512 * 1024);
    if (size_ret == 0 && temp_state.pos > 0) {
        m_nStateSize = temp_state.pos + 262144; // Add 256KB safety margin for dynamic runtime state growth
        if (m_nStateSize < min_size) m_nStateSize = min_size;
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "PicoDrive state size detected: %u bytes", m_nStateSize);
    } else {
        m_nStateSize = min_size; // Fallback
        CLogger::Get()->Write(FromOrchestrator, LogWarning, "PicoDrive state size detection failed, using fallback: %u bytes", m_nStateSize);
    }

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Allocating rewind buffer (6 slots of %u bytes)", m_nStateSize);
    for (int i = 0; i < 6; i++) {
        m_pRewindBuffers[i] = new u8[m_nStateSize];
    }

    m_bRomLoaded = TRUE;
    return TRUE;
}

void CEmuOrchestrator::RunFrame() {
    if (!m_bRomLoaded) return;

    // Ensure the video thread has consumed the previous frame of this buffer before writing
    while (g_SharedState.video_frame_ready) {
        CTimer::SimpleusDelay(100);
    }

    static int frame_count = 0;
    if (frame_count < 10) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "RunFrame starting frame %d...", frame_count);
    }

    // Map global input pad state into PicoIn, masking inputs when START + SELECT combo is held
    u16 pad1 = g_SharedState.pad1;
    if ((pad1 & (1 << 7)) && (pad1 & (1 << 11))) {
        pad1 = 0;
    }
    PicoIn.pad[0] = pad1;

    u16 pad2 = g_SharedState.pad2;
    if ((pad2 & (1 << 7)) && (pad2 & (1 << 11))) {
        pad2 = 0;
    }
    PicoIn.pad[1] = pad2;

    // Set Picodrive draw output buffer with line stride (320 pixels * 2 bytes = 640 bytes pitch)
    int idx = g_SharedState.emu_write_idx;
    PicoDrawSetOutBuf(g_SharedState.emu_frame_buffer[idx], 320 * 2);
    PicoDraw2SetOutBuf(g_SharedState.emu_frame_buffer[idx], 320 * 2);

    // Run emulator frame
    PicoFrame();

    // Query active VDP width and height after frame execution
    int is_32col = (Pico.est.rendstatus & PDRAW_32_COLS) || !(Pico.video.reg[12] & 1);
    g_SharedState.frame_geom[idx].game_w = is_32col ? 256 : 320;
    g_SharedState.frame_geom[idx].game_h = (Pico.video.reg[1] & 8) ? 240 : 224;
    g_SharedState.frame_geom[idx].start_line = (Pico.video.reg[1] & 8) ? 0 : 8;

    // Capture rewind state if 1 second elapsed
    CaptureRewindState();

    if (frame_count < 10) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "RunFrame finished frame %d.", frame_count);
        frame_count++;
    }

    // Flip buffers
    g_SharedState.emu_read_idx = idx;
    g_SharedState.emu_write_idx = 1 - idx;

    // Signal Core 1 (Video) that frame is ready
    DataMemBarrier();
    g_SharedState.video_frame_ready = TRUE;
}

extern "C" int PicoState(const char *fname, int is_save);

void CEmuOrchestrator::SaveState(int slot) {
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
    int ret = PicoState(stateName, 1);
    if (ret == 0) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "State saved successfully!");
    } else {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to save state! error=%d", ret);
    }
}

void CEmuOrchestrator::LoadState(int slot) {
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
    int ret = PicoState(stateName, 0);
    if (ret == 0) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "State loaded successfully!");
    } else {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to load state! error=%d", ret);
    }
}

boolean CEmuOrchestrator::IsPAL() const {
    return Pico.m.pal ? TRUE : FALSE;
}

void CEmuOrchestrator::CaptureRewindState() {
    if (!m_bRomLoaded || m_nStateSize == 0 ||
        (PicoIn.quirks & PQUIRK_DISABLE_REWIND)) return;

    m_nRewindFrameCounter++;
    u32 framesPerSec = IsPAL() ? 50 : 60;
    if (m_nRewindFrameCounter >= framesPerSec) {
        // Titles that run tight VDP DMA/FIFO sequences can leave the 68000/Z80
        // microstate unrecoverable if snapshotted mid-transfer (screen freeze +
        // looping audio when later rewound into). Defer capture by a frame at a
        // time instead of forcing it at an unsafe boundary; frame counter is
        // left elevated so the check retries every subsequent frame.
        static u32 s_nRewindDeferStreak = 0;
        if ((PicoIn.quirks & PQUIRK_SAFE_REWIND) &&
            (Pico.video.pending || Pico.video.fifo_cnt || Pico.video.fifo_bgcnt ||
             (Pico.video.status & (PVS_CPUWR | PVS_CPURD | PVS_DMAFILL | PVS_DMABG | PVS_FIFORUN)))) {
            s_nRewindDeferStreak++;
            return;
        }
        if (s_nRewindDeferStreak > 0) {
            CLogger::Get()->Write(FromOrchestrator, LogNotice,
                "Rewind capture deferred %u frame(s) for VDP-busy safety, capturing now", s_nRewindDeferStreak);
            s_nRewindDeferStreak = 0;
        }

        m_nRewindFrameCounter = 0;

        if (m_pRewindBuffers[m_nRewindWriteIdx] != nullptr) {
            struct savestate_state state = { 0 };
            state.save_buf = (char *)m_pRewindBuffers[m_nRewindWriteIdx];
            state.load_buf = nullptr;
            state.size = m_nStateSize;
            state.pos = 0;

            int ret = PicoStateFP(&state, 1, nullptr, state_write, nullptr, state_fseek);
            if (ret == 0 && state.pos > 0) {
                m_nRewindStateSizes[m_nRewindWriteIdx] = state.pos;
                m_nRewindWriteIdx = (m_nRewindWriteIdx + 1) % 6;
                if (m_nRewindCount < 6) {
                    m_nRewindCount++;
                }
            } else {
                CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to capture MD/SegaCD rewind state! error=%d", ret);
            }
        }
    }
}

void CEmuOrchestrator::RewindState() {
    if (!m_bRomLoaded || m_nRewindCount == 0) return;

    // Pop the most recently saved rewind snapshot from the ring buffer (1 step back)
    int loadIdx = (m_nRewindWriteIdx + 6 - 1) % 6;
    size_t loadSize = m_nRewindStateSizes[loadIdx];

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Rewinding MD/SegaCD state... loading index %d (size %u)", loadIdx, (unsigned)loadSize);
    if (m_pRewindBuffers[loadIdx] != nullptr && loadSize > 0) {
        struct savestate_state state = { 0 };
        state.load_buf = (const char *)m_pRewindBuffers[loadIdx];
        state.save_buf = nullptr;
        state.size = loadSize;
        state.pos = 0;

        int ret = PicoStateFP(&state, 0, state_read, nullptr, state_eof, state_fseek);
        if (ret == 0) {
            CLogger::Get()->Write(FromOrchestrator, LogNotice, "Rewind state loaded successfully!");
            m_nRewindWriteIdx = loadIdx;
            m_nRewindCount--;
            m_nRewindFrameCounter = 0;
            g_SharedState.audio_ring_buffer.Init();
        } else {
            CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to load rewind state! error=%d", ret);
        }
    }
}
