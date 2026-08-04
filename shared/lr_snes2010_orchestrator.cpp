#include "lr_snes2010_orchestrator.h"
#include "shared_state.h"

#include <circle/logger.h>
#include <circle/synchronize.h>
#include <circle/timer.h>

#ifndef __time_t_defined
#define __time_t_defined
#endif

#include <libretro.h>
#include <snes9x.h>

#include <stdio.h>
#include <string.h>

static const char FromOrchestrator[] = "orchestrator";
static const u32 AudioOutputRate = 44100;
static const UINT StateTransferChunkSize = 64 * 1024;
static u8 StateTransferBuffer[StateTransferChunkSize] __attribute__((aligned(64)));

CLRSnes2010Orchestrator *CLRSnes2010Orchestrator::s_pActive = nullptr;

CLRSnes2010Orchestrator::CLRSnes2010Orchestrator(FATFS *pFileSystem)
    : m_pFileSystem(pFileSystem),
      m_pRomBuffer(nullptr),
      m_pStateBuffer(nullptr),
      m_bCoreInitialized(FALSE),
      m_bRomLoaded(FALSE),
      m_bSA1(FALSE),
      m_nPendingLoadSlot(-1),
      m_nRewindWriteIdx(0),
      m_nRewindCount(0),
      m_nRewindFrameCounter(0),
      m_nStateSize(0),
      m_nManualStateSize(0),
      m_nAudioMuteFrames(0),
      m_nAudioRate(32040),
      m_nAudioResampleAccumulator(0) {
    m_CurrentRomName[0] = '\0';
    for (unsigned i = 0; i < 6; i++) {
        m_pRewindBuffers[i] = nullptr;
    }
}

CLRSnes2010Orchestrator::~CLRSnes2010Orchestrator() {
    if (m_bCoreInitialized) {
        if (m_bRomLoaded) {
            SaveSRAM();
            retro_unload_game();
        }
        retro_deinit();
    }
    delete[] m_pRomBuffer;
    delete[] m_pStateBuffer;
    for (unsigned i = 0; i < 6; i++) {
        delete[] m_pRewindBuffers[i];
    }
    if (s_pActive == this) {
        s_pActive = nullptr;
    }
}

boolean CLRSnes2010Orchestrator::Initialize() {
    s_pActive = this;
    retro_set_environment(Environment);
    retro_set_video_refresh(VideoRefresh);
    retro_set_audio_sample(nullptr);
    retro_set_audio_sample_batch(AudioBatch);
    retro_set_input_poll(InputPoll);
    retro_set_input_state(InputState);
    retro_init();
    m_bCoreInitialized = TRUE;
    return TRUE;
}

boolean CLRSnes2010Orchestrator::LoadROM(const char *pRomName, unsigned nRomSize) {
    if (!m_bCoreInitialized || nRomSize == 0) {
        return FALSE;
    }

    FIL romFile;
    UINT bytesRead = 0;
    if (f_open(&romFile, pRomName, FA_READ) != FR_OK) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Cannot open SNES ROM: %s", pRomName);
        return FALSE;
    }

    u8 *romBuffer = new u8[nRomSize];
    FRESULT readResult = f_read(&romFile, romBuffer, nRomSize, &bytesRead);
    f_close(&romFile);
    if (readResult != FR_OK || bytesRead != nRomSize) {
        delete[] romBuffer;
        CLogger::Get()->Write(FromOrchestrator, LogError, "Cannot read SNES ROM: %s", pRomName);
        return FALSE;
    }

    if (m_bRomLoaded) {
        SaveSRAM();
        retro_unload_game();
        m_bRomLoaded = FALSE;
    }
    delete[] m_pRomBuffer;
    m_pRomBuffer = romBuffer;
    delete[] m_pStateBuffer;
    m_pStateBuffer = nullptr;
    m_nManualStateSize = 0;

    retro_game_info game = {};
    game.path = pRomName;
    game.data = m_pRomBuffer;
    game.size = nRomSize;
    if (!retro_load_game(&game)) {
        delete[] m_pRomBuffer;
        m_pRomBuffer = nullptr;
        CLogger::Get()->Write(FromOrchestrator, LogError, "lr-snes9x2010 failed to load ROM: %s", pRomName);
        return FALSE;
    }
    retro_reset();

    strncpy(m_CurrentRomName, pRomName, sizeof(m_CurrentRomName) - 1);
    m_CurrentRomName[sizeof(m_CurrentRomName) - 1] = '\0';
    m_bSA1 = Settings.SA1;
    m_bRomLoaded = TRUE;
    m_nAudioRate = 32040;
    retro_system_av_info avInfo = {};
    retro_get_system_av_info(&avInfo);
    if (avInfo.timing.sample_rate > 0) {
        m_nAudioRate = (u32)(avInfo.timing.sample_rate + 0.5);
    }

    for (unsigned i = 0; i < 6; i++) {
        delete[] m_pRewindBuffers[i];
        m_pRewindBuffers[i] = nullptr;
    }
    m_nRewindWriteIdx = 0;
    m_nRewindCount = 0;
    m_nRewindFrameCounter = 0;
    m_nAudioMuteFrames = 0;
    m_nAudioResampleAccumulator = 0;
    m_nPendingLoadSlot = -1;

    m_nStateSize = retro_serialize_size();
    if (m_nStateSize > 0) {
        CLogger::Get()->Write(FromOrchestrator, LogNotice,
                               "Allocating lr-snes9x2010 rewind buffers (6 slots of %u bytes)",
                               (unsigned)m_nStateSize);
        for (unsigned i = 0; i < 6; i++) {
            m_pRewindBuffers[i] = new u8[m_nStateSize];
        }
    }

    m_nManualStateSize = m_nStateSize;
    if (m_nManualStateSize > 0) {
        m_pStateBuffer = new u8[m_nManualStateSize];
    }
    LoadSRAM();
    CLogger::Get()->Write(FromOrchestrator, LogNotice, "Loaded SNES ROM with lr-snes9x2010: %s", pRomName);
    return TRUE;
}

void CLRSnes2010Orchestrator::RunFrame() {
    if (!m_bRomLoaded) {
        return;
    }

    unsigned waitSpins = 0;
    while (g_SharedState.video_frame_ready && !g_SharedState.in_menu && !g_SharedState.escape_pressed) {
        if (waitSpins++ >= 200) {
            CTimer::SimpleusDelay(10);
        }
    }

    if (m_nPendingLoadSlot >= 0) {
        int slot = m_nPendingLoadSlot;
        m_nPendingLoadSlot = -1;
        LoadStateNow(slot);
        return;
    }

    retro_run();
    if (m_nAudioMuteFrames > 0) {
        m_nAudioMuteFrames--;
    }
    CaptureRewindState();
}

void CLRSnes2010Orchestrator::SaveState(int slot) {
    if (!m_bRomLoaded) {
        return;
    }

    if (m_pStateBuffer == nullptr || m_nManualStateSize == 0) {
        return;
    }
    boolean serialized = retro_serialize(m_pStateBuffer, m_nManualStateSize);
    if (serialized) {
        char stateName[160];
        BuildStateName(stateName, sizeof(stateName), slot);
        FIL file;
        FRESULT result = f_open(&file, stateName, FA_WRITE | FA_CREATE_ALWAYS);
        if (result == FR_OK) {
            size_t offset = 0;
            while (offset < m_nManualStateSize && result == FR_OK) {
                UINT bytesWritten = 0;
                UINT chunkSize = (UINT)((m_nManualStateSize - offset) > StateTransferChunkSize
                    ? StateTransferChunkSize : m_nManualStateSize - offset);
                memcpy(StateTransferBuffer, m_pStateBuffer + offset, chunkSize);
                result = f_write(&file, StateTransferBuffer, chunkSize, &bytesWritten);
                if (bytesWritten != chunkSize) {
                    result = FR_DISK_ERR;
                }
                offset += bytesWritten;
            }
            if (result == FR_OK && offset == m_nManualStateSize) {
                result = f_sync(&file);
            }
            f_close(&file);
        }
        if (result == FR_OK) {
            CLogger::Get()->Write(FromOrchestrator, LogNotice, "Saved lr-snes9x2010 state: %s", stateName);
        } else {
            CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to save state: %s", stateName);
        }
    }
}

void CLRSnes2010Orchestrator::LoadState(int slot) {
    if (!m_bRomLoaded) {
        return;
    }

    m_nPendingLoadSlot = slot;
}

boolean CLRSnes2010Orchestrator::LoadStateNow(int slot) {
    if (m_pStateBuffer == nullptr || m_nManualStateSize == 0) {
        return FALSE;
    }

    char stateName[160];
    BuildStateName(stateName, sizeof(stateName), slot);
    FIL file;
    if (f_open(&file, stateName, FA_READ) != FR_OK) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Cannot open state: %s", stateName);
        return FALSE;
    }

    FRESULT result = FR_OK;
    size_t offset = 0;
    while (offset < m_nManualStateSize && result == FR_OK) {
        UINT bytesRead = 0;
        UINT chunkSize = (UINT)((m_nManualStateSize - offset) > StateTransferChunkSize
            ? StateTransferChunkSize : m_nManualStateSize - offset);
        result = f_read(&file, StateTransferBuffer, chunkSize, &bytesRead);
        if (bytesRead != chunkSize) {
            result = FR_DISK_ERR;
        }
        if (bytesRead > 0) {
            memcpy(m_pStateBuffer + offset, StateTransferBuffer, bytesRead);
        }
        offset += bytesRead;
    }
    f_close(&file);
    if (result != FR_OK || offset != m_nManualStateSize) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to load state: %s", stateName);
        return FALSE;
    }

    if (retro_unserialize(m_pStateBuffer, m_nManualStateSize)) {
        ResetAudioAfterStateChange();
        CLogger::Get()->Write(FromOrchestrator, LogNotice, "Loaded lr-snes9x2010 state: %s", stateName);
        return TRUE;
    }

    CLogger::Get()->Write(FromOrchestrator, LogError, "Failed to load state: %s", stateName);
    return FALSE;
}

boolean CLRSnes2010Orchestrator::IsPAL() const {
    return retro_get_region() == RETRO_REGION_PAL;
}

void CLRSnes2010Orchestrator::LoadSRAM() {
    void *sram = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    size_t sramSize = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (sram == nullptr || sramSize == 0) {
        return;
    }

    memset(sram, 0, sramSize);
    char sramName[160];
    BuildSRAMName(sramName, sizeof(sramName));
    FILE *file = fopen(sramName, "rb");
    if (file != nullptr) {
        size_t bytesRead = fread(sram, 1, sramSize, file);
        fclose(file);
        CLogger::Get()->Write(FromOrchestrator, LogNotice,
                               "Loaded SNES SRAM: %s (%u bytes)",
                               sramName, (unsigned)bytesRead);
    }
}

void CLRSnes2010Orchestrator::SaveSRAM() {
    if (!m_bRomLoaded) {
        return;
    }

    void *sram = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    size_t sramSize = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (sram == nullptr || sramSize == 0) {
        return;
    }

    char sramName[160];
    BuildSRAMName(sramName, sizeof(sramName));
    FILE *file = fopen(sramName, "wb");
    if (file == nullptr) {
        CLogger::Get()->Write(FromOrchestrator, LogError,
                               "Cannot save SNES SRAM: %s", sramName);
        return;
    }

    size_t bytesWritten = fwrite(sram, 1, sramSize, file);
    fclose(file);
    if (bytesWritten != sramSize) {
        CLogger::Get()->Write(FromOrchestrator, LogError,
                               "Failed to save SNES SRAM: %s", sramName);
        return;
    }

    CLogger::Get()->Write(FromOrchestrator, LogNotice,
                           "Saved SNES SRAM: %s (%u bytes)",
                           sramName, (unsigned)bytesWritten);
}

void CLRSnes2010Orchestrator::CaptureRewindState() {
    if (!m_bRomLoaded || m_nStateSize == 0) {
        return;
    }

    u32 framesPerSecond = IsPAL() ? 50 : 60;
    if (++m_nRewindFrameCounter < framesPerSecond) {
        return;
    }
    m_nRewindFrameCounter = 0;

    if (retro_serialize(m_pRewindBuffers[m_nRewindWriteIdx], m_nStateSize)) {
        m_nRewindWriteIdx = (m_nRewindWriteIdx + 1) % 6;
        if (m_nRewindCount < 6) {
            m_nRewindCount++;
        }
    }
}

void CLRSnes2010Orchestrator::RewindState() {
    if (!m_bRomLoaded || m_nRewindCount == 0) {
        return;
    }

    int loadIndex = m_nRewindCount == 6 ? m_nRewindWriteIdx : 0;
    if (retro_unserialize(m_pRewindBuffers[loadIndex], m_nStateSize)) {
        memcpy(m_pRewindBuffers[0], m_pRewindBuffers[loadIndex], m_nStateSize);
        m_nRewindWriteIdx = 1;
        m_nRewindCount = 1;
        m_nRewindFrameCounter = 0;
        ResetAudioAfterStateChange();
    }
}

bool CLRSnes2010Orchestrator::Environment(unsigned cmd, void *data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return *(retro_pixel_format *)data == RETRO_PIXEL_FORMAT_RGB565;
    case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER: {
        retro_framebuffer *framebuffer = (retro_framebuffer *)data;
        if (s_pActive == nullptr || framebuffer->width > 512 || framebuffer->height > 480) {
            return false;
        }
        framebuffer->data = g_SharedState.emu_frame_buffer[g_SharedState.emu_write_idx];
        framebuffer->pitch = framebuffer->width * sizeof(u16);
        framebuffer->format = RETRO_PIXEL_FORMAT_RGB565;
        framebuffer->memory_flags = 0;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char **)data = "SD:/bios";
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = "SD:/saves";
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        retro_variable *variable = (retro_variable *)data;
        if (strcmp(variable->key, "snes9x_2010_audio_interpolation") == 0) {
            variable->value = Settings.SA1 ? "none" : "gaussian";
            return true;
        }
        if (strcmp(variable->key, "snes9x_2010_overclock_cycles") == 0) {
            variable->value = (Settings.SA1 || Settings.SuperFX) ? "compatible" : "disabled";
            return true;
        }
        return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *(bool *)data = false;
        return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        *(int *)data = RETRO_AV_ENABLE_AUDIO | RETRO_AV_ENABLE_VIDEO | RETRO_AV_ENABLE_FAST_SAVESTATES;
        return true;
    default:
        return false;
    }
}

void CLRSnes2010Orchestrator::VideoRefresh(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (s_pActive != nullptr) {
        s_pActive->PublishVideoFrame(data, width, height, pitch);
    }
}

size_t CLRSnes2010Orchestrator::AudioBatch(const int16_t *data, size_t frames) {
    return s_pActive == nullptr ? 0 : s_pActive->PushAudio(data, frames);
}

void CLRSnes2010Orchestrator::InputPoll() {
}

int16_t CLRSnes2010Orchestrator::InputState(unsigned port, unsigned device, unsigned index, unsigned id) {
    return s_pActive == nullptr ? 0 : s_pActive->ReadInput(port, device, index, id);
}

void CLRSnes2010Orchestrator::ResetAudioAfterStateChange() {
    g_SharedState.audio_ring_buffer.Init();
    m_nAudioMuteFrames = 5;
    m_nAudioResampleAccumulator = 0;
}

void CLRSnes2010Orchestrator::PublishVideoFrame(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (data == nullptr) {
        return;
    }
    if (width > 512 || height > 480 || pitch < width * sizeof(u16)) {
        CLogger::Get()->Write(FromOrchestrator, LogError, "Unsupported lr-snes9x2010 frame: %ux%u", width, height);
        return;
    }

    int index = g_SharedState.emu_write_idx;
    u16 *destination = g_SharedState.emu_frame_buffer[index];
    const u8 *source = (const u8 *)data;
    if (data != destination) {
        for (unsigned y = 0; y < height; y++) {
            memcpy(destination + y * width, source + y * pitch, width * sizeof(u16));
        }
    }
    g_SharedState.game_w[index] = width;
    g_SharedState.game_h[index] = height;
    g_SharedState.start_line[index] = 0;
    g_SharedState.emu_read_idx = index;
    g_SharedState.emu_write_idx = 1 - index;
    DataMemBarrier();
    g_SharedState.video_frame_ready = TRUE;
}

size_t CLRSnes2010Orchestrator::PushAudio(const int16_t *data, size_t frames) {
    if (m_nAudioMuteFrames > 0) {
        return frames;
    }

    s16 output[1536 * 2];
    size_t outputFrames = 0;
    for (size_t i = 0; i < frames; i++) {
        m_nAudioResampleAccumulator += AudioOutputRate;
        while (m_nAudioResampleAccumulator >= m_nAudioRate && outputFrames < 1536) {
            output[outputFrames * 2] = data[i * 2];
            output[outputFrames * 2 + 1] = data[i * 2 + 1];
            outputFrames++;
            m_nAudioResampleAccumulator -= m_nAudioRate;
        }
    }
    g_SharedState.audio_ring_buffer.Write(output, outputFrames);
    return frames;
}

int16_t CLRSnes2010Orchestrator::ReadInput(unsigned port, unsigned device, unsigned index, unsigned id) const {
    if (port > 1 || index != 0 || (device & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD) {
        return 0;
    }
    u16 pad = port == 0 ? g_SharedState.pad1 : g_SharedState.pad2;
    if ((pad & (1 << 10)) && (pad & (1 << 11))) {
        pad = 0;
    }
    static const u16 buttonBits[] = {
        1 << 5, 1 << 7, 1 << 11, 1 << 10,
        1 << 0, 1 << 1, 1 << 2, 1 << 3,
        1 << 4, 1 << 6, 1 << 8, 1 << 9,
    };
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
        u16 mask = 0;
        for (unsigned button = 0; button < 12; button++) {
            if (pad & buttonBits[button]) {
                mask |= 1 << button;
            }
        }
        return mask;
    }
    return id < 12 && (pad & buttonBits[id]) ? 1 : 0;
}

void CLRSnes2010Orchestrator::BuildStateName(char *stateName, size_t stateNameSize, int slot) const {
    strncpy(stateName, m_CurrentRomName, stateNameSize - 1);
    stateName[stateNameSize - 1] = '\0';
    char *dot = strrchr(stateName, '.');
    if (dot != nullptr) {
        *dot = '\0';
    }
    snprintf(stateName + strlen(stateName), stateNameSize - strlen(stateName), ".s%d", slot);
}

void CLRSnes2010Orchestrator::BuildSRAMName(char *sramName, size_t sramNameSize) const {
    strncpy(sramName, m_CurrentRomName, sramNameSize - 1);
    sramName[sramNameSize - 1] = '\0';
    char *dot = strrchr(sramName, '.');
    if (dot != nullptr) {
        *dot = '\0';
    }
    snprintf(sramName + strlen(sramName), sramNameSize - strlen(sramName), ".srm");
}
