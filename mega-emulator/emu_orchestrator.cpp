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

static size_t state_skip(void *p, size_t size, size_t nmemb, void *file)
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

// Audio temp buffer for Picodrive output
static s16 g_AudioTempBuf[44100 / 50 * 2];

// Sound callback
static void EmuSoundCallback(int len) {
    // len is in bytes. Interleaved stereo 16-bit PCM (4 bytes per sample)
    unsigned num_stereo_samples = len / 4;
    g_SharedState.audio_ring_buffer.Write(g_AudioTempBuf, num_stereo_samples);
}

// lprintf implementation
extern "C" void lprintf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    // Strip trailing newlines
    int len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
        buf[len-1] = '\0';
        len--;
    }
    CLogger::Get()->Write(FromOrchestrator, LogNotice, "%s", buf);
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

    PicoIn.opt = POPT_EN_FM | POPT_EN_PSG | POPT_EN_Z80 | POPT_EN_STEREO | POPT_FM_YM2612 |
                 POPT_EN_MCD_PCM | POPT_EN_MCD_CDDA | POPT_EN_MCD_GFX | POPT_EN_MCD_RAMCART |
                 POPT_ACC_SPRITES;
    PicoIn.sndRate = 44100;
    PicoIn.sndOut = g_AudioTempBuf;
    PicoIn.writeSound = EmuSoundCallback;
    PicoIn.autoRgnOrder = 0x184; // Prefer USA (NTSC 60Hz), then EUR (PAL 50Hz), then JAP (NTSC 60Hz)

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
        snprintf(bios_path, sizeof(bios_path), "SD:/bios/bios_CD_U.bin");
    } else if (reg == 8) {
        snprintf(bios_path, sizeof(bios_path), "SD:/bios/bios_CD_E.bin");
    } else {
        snprintf(bios_path, sizeof(bios_path), "SD:/bios/bios_CD_J.bin");
    }
    
    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Sega CD BIOS requested for region %d, returning path: %s", reg, bios_path);
    return bios_path;
}

boolean CEmuOrchestrator::LoadROM(const char *pRomName, unsigned nRomSize) {
    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Loading ROM: %s (size %u)", pRomName, nRomSize);

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
    
    // Set draw format
    PicoDrawSetOutFormat(PDF_RGB555, 0);

    // Configure input ports as 6-button gamepads
    PicoSetInputDevice(0, PICO_INPUT_PAD_6BTN);
    PicoSetInputDevice(1, PICO_INPUT_PAD_6BTN);

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
    if (size_ret == 0 && temp_state.pos > 0) {
        m_nStateSize = temp_state.pos;
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "PicoDrive state size detected: %u bytes", m_nStateSize);
    } else {
        m_nStateSize = 512 * 1024; // Fallback to 512KB
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
    if (!m_bRomLoaded || m_nStateSize == 0) return;

    m_nRewindFrameCounter++;
    u32 framesPerSec = IsPAL() ? 50 : 60;
    if (m_nRewindFrameCounter >= framesPerSec) {
        m_nRewindFrameCounter = 0;

        if (m_pRewindBuffers[m_nRewindWriteIdx] != nullptr) {
            struct savestate_state state = { 0 };
            state.save_buf = (char *)m_pRewindBuffers[m_nRewindWriteIdx];
            state.load_buf = nullptr;
            state.size = m_nStateSize;
            state.pos = 0;

            int ret = PicoStateFP(&state, 1, nullptr, state_write, nullptr, state_fseek);
            if (ret == 0) {
                m_nRewindWriteIdx = (m_nRewindWriteIdx + 1) % 6;
                if (m_nRewindCount < 6) {
                    m_nRewindCount++;
                }
            } else {
                CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to capture rewind state! error=%d", ret);
            }
        }
    }
}

void CEmuOrchestrator::RewindState() {
    if (!m_bRomLoaded || m_nRewindCount == 0) return;

    // The oldest state is index 0 if not full, or m_nRewindWriteIdx if full (exactly 5s ago)
    int loadIdx = (m_nRewindCount == 6) ? m_nRewindWriteIdx : 0;

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Rewinding MD state... loading index %d", loadIdx);
    if (m_pRewindBuffers[loadIdx] != nullptr) {
        struct savestate_state state = { 0 };
        state.load_buf = (const char *)m_pRewindBuffers[loadIdx];
        state.save_buf = nullptr;
        state.size = m_nStateSize;
        state.pos = 0;

        int ret = PicoStateFP(&state, 0, state_read, nullptr, state_eof, state_fseek);
        if (ret == 0) {
            CLogger::Get()->Write(FromOrchestrator, LogNotice, "Rewind state loaded successfully!");
            // Reset rewind buffers to clean slate with current loaded state
            m_nRewindWriteIdx = 0;
            memcpy(m_pRewindBuffers[0], m_pRewindBuffers[loadIdx], m_nStateSize);
            m_nRewindWriteIdx = 1;
            m_nRewindCount = 1;
            m_nRewindFrameCounter = 0;
        } else {
            CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to load rewind state! error=%d", ret);
        }
    }
}
