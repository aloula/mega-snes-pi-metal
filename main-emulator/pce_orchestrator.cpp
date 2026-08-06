#include "pce_orchestrator.h"
#include "shared_state.h"
#include "heap_bucket_rounding.h"
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/synchronize.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <libretro.h>
#ifdef __cplusplus
}
#endif

static const char FromOrchestrator[] = "orchestrator";
static CPCEOrchestrator* s_pInstance = nullptr;
static u32 s_nAudioMuteFrames = 0;
static s16 s_PceSingleAudioBuf[128 * 2];
static unsigned s_PceSingleAudioFrames = 0;

static void FlushPceSingleAudioBuffer() {
    if (s_PceSingleAudioFrames == 0) return;
    g_SharedState.audio_ring_buffer.Write(s_PceSingleAudioBuf, s_PceSingleAudioFrames);
    s_PceSingleAudioFrames = 0;
}

static void pce_log_printf(enum retro_log_level level, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    TLogSeverity severity = LogNotice;
    if (level == RETRO_LOG_WARN) severity = LogWarning;
    else if (level == RETRO_LOG_ERROR) severity = LogError;
    else if (level == RETRO_LOG_DEBUG) severity = LogDebug;

    CLogger::Get()->Write("beetle-pce", severity, "%s", buf);
}

static bool pce_environment_cb(unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
            static const char system_dir[] = "SD:/bios";
            *(const char **)data = system_dir;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
            *(bool *)data = true;
            return true;
        }
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            enum retro_pixel_format fmt = *(const enum retro_pixel_format *)data;
            if (fmt == RETRO_PIXEL_FORMAT_RGB565) {
                return true;
            }
            return false;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            struct retro_variable *var = (struct retro_variable *)data;
            if (strcmp(var->key, "pce_fast_cdimagecache") == 0 || strcmp(var->key, "pce_cdimagecache") == 0) {
                var->value = "disabled";
                return true;
            }
            if (strcmp(var->key, "pce_fast_nospritelimit") == 0 || strcmp(var->key, "pce_nospritelimit") == 0) {
                var->value = "disabled";
                return true;
            }
            if (strcmp(var->key, "pce_fast_ocmultiplier") == 0 || strcmp(var->key, "pce_ocmultiplier") == 0) {
                var->value = "1";
                return true;
            }
            if (strcmp(var->key, "pce_fast_initial_scanline") == 0 || strcmp(var->key, "pce_initial_scanline") == 0) {
                var->value = "3";
                return true;
            }
            if (strcmp(var->key, "pce_fast_last_scanline") == 0 || strcmp(var->key, "pce_last_scanline") == 0) {
                var->value = "242";
                return true;
            }
            return false;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
            *(bool *)data = false;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            struct retro_log_callback *log = (struct retro_log_callback *)data;
            log->log = pce_log_printf;
            return true;
        }
        default:
            return false;
    }
}

static void pce_video_cb(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (data == nullptr) return;

    int idx = g_SharedState.emu_write_idx;
    g_SharedState.game_w[idx] = width;
    g_SharedState.game_h[idx] = height;
    g_SharedState.start_line[idx] = 0;

    u16 *dest = g_SharedState.emu_frame_buffer[idx];
    const u16 *src = (const u16 *)data;
    unsigned src_pitch_pixels = pitch / sizeof(u16);

    if (src_pitch_pixels == width) {
        memcpy(dest, src, width * height * sizeof(u16));
    } else {
        for (unsigned y = 0; y < height; y++) {
            memcpy(dest + y * width, src + y * src_pitch_pixels, width * sizeof(u16));
        }
    }

    g_SharedState.emu_read_idx = idx;
    g_SharedState.emu_write_idx = 1 - idx;

    DataMemBarrier();
    g_SharedState.video_frame_ready = TRUE;
}

static void pce_audio_cb(int16_t left, int16_t right) {
    s16 samples[2] = {left, right};
    if (s_nAudioMuteFrames > 0) {
        s_nAudioMuteFrames--;
        samples[0] = 0;
        samples[1] = 0;
    }

    unsigned pos = s_PceSingleAudioFrames * 2;
    s_PceSingleAudioBuf[pos] = samples[0];
    s_PceSingleAudioBuf[pos + 1] = samples[1];
    s_PceSingleAudioFrames++;
    if (s_PceSingleAudioFrames >= 128) {
        FlushPceSingleAudioBuffer();
    }
}

static size_t pce_audio_batch_cb(const int16_t *data, size_t frames) {
    if (frames == 0 || data == nullptr) return 0;

    FlushPceSingleAudioBuffer();

    if (s_nAudioMuteFrames > 0) {
        if (s_nAudioMuteFrames >= frames) {
            s_nAudioMuteFrames -= frames;
        } else {
            s_nAudioMuteFrames = 0;
        }
        static s16 silence[2048 * 2] = {0};
        size_t written = 0;
        while (written < frames) {
            size_t chunk = frames - written;
            if (chunk > 2048) chunk = 2048;
            g_SharedState.audio_ring_buffer.Write(silence, chunk);
            written += chunk;
        }
    } else {
        g_SharedState.audio_ring_buffer.Write((const s16 *)data, frames);
    }
    return frames;
}

static void pce_input_poll_cb(void) {
    // Input is polled on Core 3
}

static int16_t pce_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (device != RETRO_DEVICE_JOYPAD) return 0;
    if (port >= 2) return 0;

    u16 pad = (port == 0) ? g_SharedState.pad1 : g_SharedState.pad2;
    if ((pad & (1 << 10)) && (pad & (1 << 11))) {
        return 0; // Mask out inputs on Escape
    }

    switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_UP:     return (pad & (1 << 0)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:   return (pad & (1 << 1)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:   return (pad & (1 << 2)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return (pad & (1 << 3)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_A:      return (pad & (1 << 4)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_B:      return (pad & (1 << 5)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_X:      return (pad & (1 << 6)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_Y:      return (pad & (1 << 7)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_L:      return (pad & (1 << 8)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_R:      return (pad & (1 << 9)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_START:  return (pad & (1 << 10)) ? 1 : 0;
        case RETRO_DEVICE_ID_JOYPAD_SELECT: return (pad & (1 << 11)) ? 1 : 0;
        default:                            return 0;
    }
}

CPCEOrchestrator::CPCEOrchestrator(FATFS *pFileSystem)
    : m_pFileSystem(pFileSystem),
      m_bRomLoaded(FALSE)
{
    m_CurrentRomName[0] = '\0';
    m_LastPad1 = 0xFFFF;
    m_LastPad2 = 0xFFFF;
    m_nRewindWriteIdx = 0;
    m_nRewindCount = 0;
    m_nRewindFrameCounter = 0;
    m_nStateSize = 0;
    s_nAudioMuteFrames = 0;
    for (int i = 0; i < 6; i++) {
        m_pRewindBuffers[i] = nullptr;
        m_nRewindStateSizes[i] = 0;
    }
    s_pInstance = this;
}

CPCEOrchestrator::~CPCEOrchestrator() {
    if (m_bRomLoaded) {
        SaveSRAM();
        retro_unload_game();
    }
    retro_deinit();
    for (int i = 0; i < 6; i++) {
        if (m_pRewindBuffers[i] != nullptr) {
            delete[] m_pRewindBuffers[i];
            m_pRewindBuffers[i] = nullptr;
        }
    }
    if (s_pInstance == this) {
        s_pInstance = nullptr;
    }
}

boolean CPCEOrchestrator::Initialize() {
    retro_set_environment(pce_environment_cb);
    retro_set_video_refresh(pce_video_cb);
    retro_set_audio_sample(pce_audio_cb);
    retro_set_audio_sample_batch(pce_audio_batch_cb);
    retro_set_input_poll(pce_input_poll_cb);
    retro_set_input_state(pce_input_state_cb);

    retro_init();

    m_bRomLoaded = FALSE;
    s_PceSingleAudioFrames = 0;
    return TRUE;
}

boolean CPCEOrchestrator::LoadROM(const char *pRomName, unsigned nRomSize) {
    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Loading PCE ROM: %s (size %u)", pRomName, nRomSize);

    if (m_bRomLoaded) {
        SaveSRAM();
        retro_unload_game();
        m_bRomLoaded = FALSE;
    }

    struct retro_game_info game_info;
    game_info.path = pRomName;
    game_info.data = nullptr;
    game_info.size = nRomSize;
    game_info.meta = nullptr;

    if (!retro_load_game(&game_info)) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to load PCE ROM: %s", pRomName);
        return FALSE;
    }

    strncpy(m_CurrentRomName, pRomName, sizeof(m_CurrentRomName) - 1);
    m_CurrentRomName[sizeof(m_CurrentRomName) - 1] = '\0';

    retro_reset();

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
    m_nStateSize = retro_serialize_size();
    s_nAudioMuteFrames = 0;
    s_PceSingleAudioFrames = 0;

    if (m_nStateSize > 0) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "Allocating PCE rewind buffer (6 slots of %u bytes)", m_nStateSize);
        for (int i = 0; i < 6; i++) {
            m_pRewindBuffers[i] = new u8[RoundUpToCanonicalBufferSize(m_nStateSize)];
        }
    }

    // Load native SRAM if available
    void* sram_ptr = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    size_t sram_size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (sram_ptr != nullptr && sram_size > 0) {
        char sramPath[160];
        strncpy(sramPath, m_CurrentRomName, sizeof(sramPath) - 8);
        sramPath[sizeof(sramPath) - 8] = '\0';
        char *dot = strrchr(sramPath, '.');
        if (dot) {
            *dot = '\0';
        }
        strcat(sramPath, ".srm");

        FILE *f = fopen(sramPath, "rb");
        if (f != nullptr) {
            size_t read_bytes = fread(sram_ptr, 1, sram_size, f);
            fclose(f);
            CLogger::Get()->Write(FromOrchestrator, LogNotice, "PCE SRAM loaded successfully: %s (%u bytes)", sramPath, (unsigned)read_bytes);
        } else {
            CLogger::Get()->Write(FromOrchestrator, LogNotice, "No PCE SRAM file found at: %s", sramPath);
        }
    }

    m_bRomLoaded = TRUE;
    m_LastPad1 = 0xFFFF;
    m_LastPad2 = 0xFFFF;
    return TRUE;
}

void CPCEOrchestrator::Unload() {
    if (!m_bRomLoaded) return;

    SaveSRAM();
    retro_unload_game();

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
    m_bRomLoaded = FALSE;
}

void CPCEOrchestrator::RunFrame() {
    if (!m_bRomLoaded) return;

    // Ensure the video thread has consumed the previous frame before writing.
    // Adaptive wait keeps latency low without burning a full core on long waits.
    unsigned wait_spins = 0;
    while (g_SharedState.video_frame_ready) {
        if (wait_spins < 200) {
            wait_spins++;
        } else {
            CTimer::SimpleusDelay(10);
        }
    }

    retro_run();
    FlushPceSingleAudioBuffer();
    CaptureRewindState();
}

void CPCEOrchestrator::SaveState(int slot) {
    if (!m_bRomLoaded) return;

    char stateName[160];
    strncpy(stateName, m_CurrentRomName, sizeof(stateName) - 8);
    stateName[sizeof(stateName) - 8] = '\0';
    char *dot = strrchr(stateName, '.');
    if (dot) {
        *dot = '\0';
    }
    sprintf(stateName + strlen(stateName), ".s%d", slot);

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Saving PCE state to: %s", stateName);

    size_t stateSize = retro_serialize_size();
    if (stateSize == 0) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "PCE serialize size is 0!");
        return;
    }

    u8 *buf = new u8[RoundUpToCanonicalBufferSize(stateSize)];
    if (!buf) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to allocate buffer for state save");
        return;
    }

    if (retro_serialize(buf, stateSize)) {
        FILE *f = fopen(stateName, "wb");
        if (f) {
            fwrite(buf, 1, stateSize, f);
            fclose(f);
            CLogger::Get()->Write(FromOrchestrator, LogNotice, "PCE State saved successfully! Size=%u bytes", (unsigned)stateSize);
            ResetAudioAfterStateChange();
        } else {
            CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to open state file for writing: %s", stateName);
        }
    } else {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to serialize PCE state!");
    }
    delete[] buf;
}

void CPCEOrchestrator::LoadState(int slot) {
    if (!m_bRomLoaded) return;

    char stateName[160];
    strncpy(stateName, m_CurrentRomName, sizeof(stateName) - 8);
    stateName[sizeof(stateName) - 8] = '\0';
    char *dot = strrchr(stateName, '.');
    if (dot) {
        *dot = '\0';
    }
    sprintf(stateName + strlen(stateName), ".s%d", slot);

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Loading PCE state from: %s", stateName);

    FILE *f = fopen(stateName, "rb");
    if (!f) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to open state file for reading: %s", stateName);
        return;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    size_t stateSize = retro_serialize_size();
    if (fileSize != (long)stateSize) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "PCE state file size mismatch: file=%ld expected=%u", fileSize, (unsigned)stateSize);
        fclose(f);
        return;
    }

    u8 *buf = new u8[RoundUpToCanonicalBufferSize(stateSize)];
    if (!buf) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to allocate buffer for state load");
        fclose(f);
        return;
    }

    size_t read = fread(buf, 1, stateSize, f);
    fclose(f);

    if (read == stateSize && retro_unserialize(buf, stateSize)) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "PCE State loaded successfully!");
        ResetAudioAfterStateChange();
    } else {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to unserialize PCE state!");
    }
    delete[] buf;
}

boolean CPCEOrchestrator::IsPAL() const {
    return FALSE;
}

void CPCEOrchestrator::CaptureRewindState() {
    if (!m_bRomLoaded || m_nStateSize == 0) return;

    m_nRewindFrameCounter++;
    u32 framesPerSec = 60;
    if (m_nRewindFrameCounter >= framesPerSec) {
        m_nRewindFrameCounter = 0;

        if (m_pRewindBuffers[m_nRewindWriteIdx] != nullptr) {
            if (retro_serialize(m_pRewindBuffers[m_nRewindWriteIdx], m_nStateSize)) {
                m_nRewindStateSizes[m_nRewindWriteIdx] = m_nStateSize;
                m_nRewindWriteIdx = (m_nRewindWriteIdx + 1) % 6;
                if (m_nRewindCount < 6) {
                    m_nRewindCount++;
                }
            } else {
                CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to capture PCE rewind state!");
            }
        }
    }
}

void CPCEOrchestrator::RewindState() {
    if (!m_bRomLoaded || m_nRewindCount == 0 || m_nStateSize == 0) return;

    int loadIdx = (m_nRewindCount == 6) ? m_nRewindWriteIdx : 0;
    size_t loadSize = m_nRewindStateSizes[loadIdx];
    if (loadSize == 0 || loadSize != m_nStateSize) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Invalid PCE rewind slot size at index %d: %u", loadIdx, (unsigned)loadSize);
        return;
    }

    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Rewinding PCE state... loading index %d", loadIdx);
    if (m_pRewindBuffers[loadIdx] != nullptr) {
        if (retro_unserialize(m_pRewindBuffers[loadIdx], loadSize)) {
            CLogger::Get()->Write(FromOrchestrator, LogNotice, "Rewind PCE state loaded successfully!");
            memcpy(m_pRewindBuffers[0], m_pRewindBuffers[loadIdx], loadSize);
            m_nRewindStateSizes[0] = loadSize;
            for (int i = 1; i < 6; i++) {
                m_nRewindStateSizes[i] = 0;
            }
            m_nRewindWriteIdx = 1;
            m_nRewindCount = 1;
            m_nRewindFrameCounter = 0;
            ResetAudioAfterStateChange();
        } else {
            CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to load rewind PCE state!");
        }
    }
}

void CPCEOrchestrator::ResetAudioAfterStateChange() {
    g_SharedState.audio_ring_buffer.Init();
    s_nAudioMuteFrames = 60;
    s_PceSingleAudioFrames = 0;
}

extern "C" {

struct RFILE {
    FILE *file;
};

RFILE* filestream_open(const char *path, unsigned mode, unsigned hints) {
    const char *mode_str = "rb";
    if ((mode & 3) == 3) { // RETRO_VFS_FILE_ACCESS_READ_WRITE
        mode_str = "r+b";
    } else if (mode & 2) { // RETRO_VFS_FILE_ACCESS_WRITE
        mode_str = "wb";
    } else if (mode & 1) { // RETRO_VFS_FILE_ACCESS_READ
        mode_str = "rb";
    }
    FILE *f = fopen(path, mode_str);
    if (!f) return nullptr;
    RFILE *rf = (RFILE*)malloc(sizeof(RFILE));
    rf->file = f;
    return rf;
}

int filestream_close(RFILE *stream) {
    if (!stream) return -1;
    fclose(stream->file);
    free(stream);
    return 0;
}

int64_t filestream_read(RFILE *stream, void *data, int64_t len) {
    if (!stream) return -1;
    return fread(data, 1, len, stream->file);
}

int64_t filestream_seek(RFILE *stream, int64_t offset, int seek_position) {
    if (!stream) return -1;
    int whence = SEEK_SET;
    if (seek_position == 1) whence = SEEK_CUR; // RETRO_VFS_SEEK_POSITION_CURRENT
    else if (seek_position == 2) whence = SEEK_END; // RETRO_VFS_SEEK_POSITION_END
    return fseek(stream->file, offset, whence);
}

int64_t filestream_tell(RFILE *stream) {
    if (!stream) return -1;
    return ftell(stream->file);
}

int64_t filestream_get_size(RFILE *stream) {
    if (!stream) return -1;
    long current = ftell(stream->file);
    fseek(stream->file, 0, SEEK_END);
    long size = ftell(stream->file);
    fseek(stream->file, current, SEEK_SET);
    return size;
}

void filestream_vfs_init(const void* vfs_info) {
    // No-op
}

char* filestream_gets(RFILE *stream, char *s, size_t len) {
    if (!stream) return nullptr;
    return fgets(s, len, stream->file);
}

int64_t filestream_read_file(const char *path, void **buf, int64_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *data = malloc(size + 1);
    if (!data) {
        fclose(f);
        return 0;
    }
    size_t read_bytes = fread(data, 1, size, f);
    fclose(f);
    ((char*)data)[read_bytes] = '\0';
    *buf = data;
    if (len) *len = read_bytes;
    return 1;
}

void string_trim_whitespace_right(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[len - 1] = '\0';
        len--;
    }
}

size_t strlcpy_retro__(char *dest, const char *src, size_t size) {
    if (size == 0) return strlen(src);
    size_t i;
    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return strlen(src);
}

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50abf6b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t encoding_crc32(uint32_t seed, const uint8_t *data, size_t len) {
    uint32_t crc = seed ^ 0xffffffff;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xff];
    }
    return crc ^ 0xffffffff;
}

void CPCEOrchestrator::SaveSRAM() {
    if (!m_bRomLoaded) return;

    void* sram_ptr = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    size_t sram_size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (sram_ptr != nullptr && sram_size > 0) {
        char sramPath[160];
        strncpy(sramPath, m_CurrentRomName, sizeof(sramPath) - 8);
        sramPath[sizeof(sramPath) - 8] = '\0';
        char *dot = strrchr(sramPath, '.');
        if (dot) {
            *dot = '\0';
        }
        strcat(sramPath, ".srm");

        FILE *f = fopen(sramPath, "wb");
        if (f != nullptr) {
            size_t written = fwrite(sram_ptr, 1, sram_size, f);
            fclose(f);
            CLogger::Get()->Write(FromOrchestrator, LogNotice, "PCE SRAM saved successfully: %s (%u bytes)", sramPath, (unsigned)written);
        } else {
            CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to save PCE SRAM to: %s", sramPath);
        }
    }
}

}
