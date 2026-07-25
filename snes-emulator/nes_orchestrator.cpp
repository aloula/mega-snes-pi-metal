#include "nes_orchestrator.h"
#include "shared_state.h"
#include <circle/alloc.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/synchronize.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <streambuf>
#include <sstream>

#include <api/NstApiEmulator.hpp>
#include <api/NstApiMachine.hpp>
#include <api/NstApiVideo.hpp>
#include <api/NstApiSound.hpp>
#include <api/NstApiInput.hpp>
#include <api/NstApiUser.hpp>

static const size_t NES_REWIND_SLOT_CAPACITY = 256 * 1024;

// Custom stream buffer to read from a memory block with seeking support
struct membuf : std::streambuf {
    membuf(char* begin, char* end) {
        this->setg(begin, begin, end);
    }

protected:
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) override {
        char* target = nullptr;
        if (dir == std::ios_base::beg) {
            target = eback() + off;
        } else if (dir == std::ios_base::cur) {
            target = gptr() + off;
        } else if (dir == std::ios_base::end) {
            target = egptr() + off;
        }
        if (target < eback() || target > egptr()) {
            return pos_type(off_type(-1));
        }
        setg(eback(), target, egptr());
        return pos_type(target - eback());
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in) override {
        return seekoff(off_type(pos), std::ios_base::beg, which);
    }
};

// Custom stream buffer to write directly into preallocated memory buffer without heap allocation
struct omembuf : std::streambuf {
    omembuf(char* begin, size_t capacity) {
        this->setp(begin, begin + capacity);
    }

    size_t size() const {
        return pptr() - pbase();
    }

protected:
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::out) override {
        char* target = nullptr;
        if (dir == std::ios_base::beg) {
            target = pbase() + off;
        } else if (dir == std::ios_base::cur) {
            target = pptr() + off;
        } else if (dir == std::ios_base::end) {
            target = epptr() + off;
        }
        if (target < pbase() || target > epptr()) {
            return pos_type(off_type(-1));
        }
        pbump(target - pptr());
        return pos_type(target - pbase());
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::out) override {
        return seekoff(off_type(pos), std::ios_base::beg, which);
    }
};

CNESOrchestrator::CNESOrchestrator(FATFS *pFileSystem)
    : m_pFileSystem(pFileSystem),
      m_pRomBuffer(nullptr),
      m_bRomLoaded(FALSE),
      m_LastPad1(0xFFFF),
      m_LastPad2(0xFFFF),
      m_pEmulator(nullptr)
{
    m_nRewindWriteIdx = 0;
    m_nRewindCount = 0;
    m_nRewindFrameCounter = 0;
    m_nStateSize = 0;
    m_nAudioMuteFrames = 0;
    for (int i = 0; i < 6; i++) {
        m_pRewindBuffers[i] = nullptr;
        m_nRewindStateSizes[i] = 0;
    }
}

CNESOrchestrator::~CNESOrchestrator()
{
    if (m_pRomBuffer) {
        delete[] m_pRomBuffer;
    }
    if (m_pEmulator) {
        delete m_pEmulator;
    }
    for (int i = 0; i < 6; i++) {
        if (m_pRewindBuffers[i] != nullptr) {
            delete[] m_pRewindBuffers[i];
            m_pRewindBuffers[i] = nullptr;
        }
        m_nRewindStateSizes[i] = 0;
    }
}

boolean CNESOrchestrator::Initialize()
{
    m_pEmulator = new Nes::Api::Emulator();
    if (!m_pEmulator) {
        CLogger::Get()->Write("orchestrator", LogPanic, "Failed to instantiate Nestopia Emulator");
        return FALSE;
    }

    // Set static callbacks
    Nes::Core::Video::Output::lockCallback.Set(VideoLockCallback, this);
    Nes::Core::Video::Output::unlockCallback.Set(VideoUnlockCallback, this);
    Nes::Core::Sound::Output::lockCallback.Set(SoundLockCallback, this);
    Nes::Core::Sound::Output::unlockCallback.Set(SoundUnlockCallback, this);
    Nes::Core::Input::Controllers::Pad::callback.Set(InputPollCallback, this);

    m_bRomLoaded = FALSE;
    return TRUE;
}

boolean CNESOrchestrator::LoadROM(const char *pRomName, unsigned nRomSize)
{
    CLogger::Get()->Write("orchestrator", LogNotice, "Loading NES ROM: %s (size %u)", pRomName, nRomSize);

    FILE* f = fopen(pRomName, "rb");
    if (!f) {
        CLogger::Get()->Write("orchestrator", LogError, "Failed to open NES ROM file: %s", pRomName);
        return FALSE;
    }

    if (m_pRomBuffer != nullptr) {
        delete[] m_pRomBuffer;
        m_pRomBuffer = nullptr;
    }
    m_pRomBuffer = new u8[nRomSize];
    if (m_pRomBuffer == nullptr) {
        fclose(f);
        CLogger::Get()->Write("orchestrator", LogError, "Failed to allocate memory for NES ROM buffer");
        return FALSE;
    }

    size_t read = fread(m_pRomBuffer, 1, nRomSize, f);
    fclose(f);
    if (read < nRomSize) {
        delete[] m_pRomBuffer;
        m_pRomBuffer = nullptr;
        CLogger::Get()->Write("orchestrator", LogError, "Failed to read NES ROM file completely");
        return FALSE;
    }

    membuf sbuf((char*)m_pRomBuffer, (char*)m_pRomBuffer + nRomSize);
    std::istream stream(&sbuf);

    Nes::Api::Machine machine(*m_pEmulator);
    Nes::Result ret = machine.LoadCartridge(stream, Nes::Api::Machine::FAVORED_NES_NTSC);
    
    // Nestopia parses and copies all cartridge data internally, so the source stream buffer is no longer needed
    delete[] m_pRomBuffer;
    m_pRomBuffer = nullptr;

    if (ret != Nes::RESULT_OK && ret != Nes::RESULT_NOP) {
        CLogger::Get()->Write("orchestrator", LogError, "Nestopia failed to load cartridge: %d", (int)ret);
        return FALSE;
    }

    ret = machine.Power(true);
    if (ret != Nes::RESULT_OK && ret != Nes::RESULT_NOP) {
        CLogger::Get()->Write("orchestrator", LogError, "Failed to Power ON Nestopia: %d", (int)ret);
        return FALSE;
    }

    // Connect controllers
    Nes::Api::Input input(*m_pEmulator);
    input.ConnectController(Nes::Api::Input::PORT_1, Nes::Api::Input::PAD1);
    input.ConnectController(Nes::Api::Input::PORT_2, Nes::Api::Input::PAD2);

    // Set render state (RGB565)
    Nes::Api::Video::RenderState renderState;
    renderState.bits.count = 16;
    renderState.bits.mask.r = 0xf800;
    renderState.bits.mask.g = 0x07e0;
    renderState.bits.mask.b = 0x001f;
    renderState.width = 256;
    renderState.height = 240;
    renderState.filter = Nes::Api::Video::RenderState::FILTER_NONE;
    
    Nes::Api::Video video(*m_pEmulator);
    video.SetRenderState(renderState);

    // Check for custom palette file (.pal) on the SD card
    boolean loadedCustomPalette = FALSE;
    unsigned char customPalette[64][3];
    FILE* palFile = nullptr;

    // 1. Try ROM-specific palette: <romname>.pal
    char palPath[160];
    strncpy(palPath, pRomName, sizeof(palPath) - 8);
    palPath[sizeof(palPath) - 8] = '\0';
    char *dot = strrchr(palPath, '.');
    if (dot) {
        *dot = '\0';
    }
    strcat(palPath, ".pal");
    palFile = fopen(palPath, "rb");

    // 2. Try global/system overrides
    if (!palFile) {
        palFile = fopen("/roms/nes/custom.pal", "rb");
    }
    if (!palFile) {
        palFile = fopen("/roms/nes/nes.pal", "rb");
    }
    if (!palFile) {
        palFile = fopen("/bios/custom.pal", "rb");
    }
    if (!palFile) {
        palFile = fopen("/bios/nes.pal", "rb");
    }
    if (!palFile) {
        palFile = fopen("/roms/custom.pal", "rb");
    }

    if (palFile) {
        size_t bytesRead = fread(customPalette, 1, 192, palFile);
        fclose(palFile);
        if (bytesRead == 192) {
            video.GetPalette().SetCustom(customPalette, Nes::Api::Video::Palette::STD_PALETTE);
            video.GetPalette().SetMode(Nes::Api::Video::Palette::MODE_CUSTOM);
            loadedCustomPalette = TRUE;
            CLogger::Get()->Write("orchestrator", LogNotice, "Loaded custom NES palette from file");
        } else {
            CLogger::Get()->Write("orchestrator", LogError, "Custom palette file found but had invalid size (%u bytes, expected 192)", (unsigned)bytesRead);
        }
    }

    if (!loadedCustomPalette) {
        // Use Consumer YUV decoder preset (Sony CXA2095S/U based), which fixes red colors appearing as pink.
        Nes::Api::Video::Decoder decoder(Nes::Api::Video::DECODER_CONSUMER);
        video.SetDecoder(decoder);
        CLogger::Get()->Write("orchestrator", LogNotice, "Using DECODER_CONSUMER preset for accurate NES colors");
    }

    // Set sound state
    Nes::Api::Sound sound(*m_pEmulator);
    sound.SetSampleRate(44100);
    sound.SetSampleBits(16);
    sound.SetSpeaker(Nes::Api::Sound::SPEAKER_STEREO);

    CLogger::Get()->Write("orchestrator", LogNotice, "NES ROM Loaded Successfully!");

    strncpy(m_CurrentRomName, pRomName, sizeof(m_CurrentRomName) - 1);
    m_CurrentRomName[sizeof(m_CurrentRomName) - 1] = '\0';

    // Free existing rewind buffers if any
    for (int i = 0; i < 6; i++) {
        if (m_pRewindBuffers[i] != nullptr) {
            delete[] m_pRewindBuffers[i];
            m_pRewindBuffers[i] = nullptr;
        }
    }
    // Preallocate rewind buffers to a safe maximum size (256KB) to avoid runtime fragmentation
    for (int i = 0; i < 6; i++) {
        m_pRewindBuffers[i] = new u8[NES_REWIND_SLOT_CAPACITY];
        m_nRewindStateSizes[i] = 0;
    }
    m_nRewindWriteIdx = 0;
    m_nRewindCount = 0;
    m_nRewindFrameCounter = 0;
    m_nStateSize = 0;
    m_nAudioMuteFrames = 0;

    m_bRomLoaded = TRUE;
    m_LastPad1 = 0xFFFF;
    m_LastPad2 = 0xFFFF;
    return TRUE;
}

void CNESOrchestrator::RunFrame()
{
    if (!m_bRomLoaded) return;

    // Wait until video frame buffer is consumed by Core 1 (Video domain)
    unsigned wait_spins = 0;
    while (g_SharedState.video_frame_ready) {
        if (wait_spins < 200) {
            wait_spins++;
        } else {
            CTimer::SimpleusDelay(10);
        }
    }

    Nes::Core::Video::Output videoOut;
    Nes::Core::Sound::Output soundOut;
    Nes::Core::Input::Controllers controllers;

    m_pEmulator->Execute(&videoOut, &soundOut, &controllers);

    CaptureRewindState();
}

void CNESOrchestrator::SaveState(int slot)
{
    if (!m_bRomLoaded) return;

    char stateName[160];
    strncpy(stateName, m_CurrentRomName, sizeof(stateName) - 8);
    stateName[sizeof(stateName) - 8] = '\0';
    char *dot = strrchr(stateName, '.');
    if (dot) {
        *dot = '\0';
    }
    sprintf(stateName + strlen(stateName), ".s%d", slot);

    CLogger::Get()->Write("orchestrator", LogNotice, "Saving NES state to: %s", stateName);

    std::stringstream ss;
    Nes::Api::Machine machine(*m_pEmulator);
    Nes::Result ret = machine.SaveState(ss, Nes::Api::Machine::NO_COMPRESSION);
    if (ret == Nes::RESULT_OK || ret == Nes::RESULT_NOP) {
        std::string s = ss.str();
        FILE *f = fopen(stateName, "wb");
        if (f) {
            fwrite(s.data(), 1, s.size(), f);
            fclose(f);
            CLogger::Get()->Write("orchestrator", LogNotice, "NES State saved successfully! Size=%u bytes", (unsigned)s.size());
            ResetAudioAfterStateChange();
        } else {
            CLogger::Get()->Write("orchestrator", LogError, "Failed to open state file for writing: %s", stateName);
        }
    } else {
        CLogger::Get()->Write("orchestrator", LogError, "Failed to save state! error=%d", (int)ret);
    }
}

void CNESOrchestrator::LoadState(int slot)
{
    if (!m_bRomLoaded) return;

    char stateName[160];
    strncpy(stateName, m_CurrentRomName, sizeof(stateName) - 8);
    stateName[sizeof(stateName) - 8] = '\0';
    char *dot = strrchr(stateName, '.');
    if (dot) {
        *dot = '\0';
    }
    sprintf(stateName + strlen(stateName), ".s%d", slot);

    CLogger::Get()->Write("orchestrator", LogNotice, "Loading NES state from: %s", stateName);

    FILE *f = fopen(stateName, "rb");
    if (!f) {
        CLogger::Get()->Write("orchestrator", LogError, "Failed to open state file for reading: %s", stateName);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        CLogger::Get()->Write("orchestrator", LogError, "Invalid state file size: %ld", size);
        return;
    }

    char *buf = new char[size];
    if (!buf) {
        fclose(f);
        CLogger::Get()->Write("orchestrator", LogError, "Failed to allocate memory for loading state");
        return;
    }

    fread(buf, 1, size, f);
    fclose(f);

    membuf sbuf(buf, buf + size);
    std::istream ss(&sbuf);

    Nes::Api::Machine machine(*m_pEmulator);
    Nes::Result ret = machine.LoadState(ss);
    delete[] buf;

    if (ret == Nes::RESULT_OK || ret == Nes::RESULT_NOP) {
        CLogger::Get()->Write("orchestrator", LogNotice, "NES State loaded successfully!");
        ResetAudioAfterStateChange();
    } else {
        CLogger::Get()->Write("orchestrator", LogError, "Failed to load state! error=%d", (int)ret);
    }
}

boolean CNESOrchestrator::IsPAL() const
{
    if (!m_bRomLoaded) return FALSE;
    Nes::Api::Machine machine(*m_pEmulator);
    return machine.GetMode() == Nes::Api::Machine::PAL;
}

void CNESOrchestrator::CaptureRewindState()
{
    if (!m_bRomLoaded) return;

    m_nRewindFrameCounter++;
    u32 framesPerSec = IsPAL() ? 50 : 60;
    if (m_nRewindFrameCounter >= framesPerSec) {
        m_nRewindFrameCounter = 0;

        if (m_pRewindBuffers[m_nRewindWriteIdx] != nullptr) {
            omembuf sbuf((char*)m_pRewindBuffers[m_nRewindWriteIdx], NES_REWIND_SLOT_CAPACITY);
            std::ostream ss(&sbuf);
            Nes::Api::Machine machine(*m_pEmulator);
            Nes::Result ret = machine.SaveState(ss, Nes::Api::Machine::NO_COMPRESSION);
            if (ret == Nes::RESULT_OK || ret == Nes::RESULT_NOP) {
                size_t state_size = sbuf.size();
                if (state_size == 0 || state_size > NES_REWIND_SLOT_CAPACITY) {
                    CLogger::Get()->Write("orchestrator", LogError, "NES rewind capture size out of bounds: %u bytes", (unsigned)state_size);
                    return;
                }

                m_nRewindStateSizes[m_nRewindWriteIdx] = state_size;
                m_nStateSize = state_size;

                m_nRewindWriteIdx = (m_nRewindWriteIdx + 1) % 6;
                if (m_nRewindCount < 6) {
                    m_nRewindCount++;
                }
            } else {
                CLogger::Get()->Write("orchestrator", LogError, "Failed to capture NES rewind state! error=%d", (int)ret);
            }
        }
    }
}

void CNESOrchestrator::RewindState()
{
    if (!m_bRomLoaded || m_nRewindCount == 0) return;

    int loadIdx = (m_nRewindCount == 6) ? m_nRewindWriteIdx : 0;
    size_t loadSize = m_nRewindStateSizes[loadIdx];
    if (loadSize == 0 || loadSize > NES_REWIND_SLOT_CAPACITY) {
        CLogger::Get()->Write("orchestrator", LogError, "Invalid NES rewind slot size at index %d: %u", loadIdx, (unsigned)loadSize);
        return;
    }

    CLogger::Get()->Write("orchestrator", LogNotice, "Rewinding NES state... loading index %d", loadIdx);
    if (m_pRewindBuffers[loadIdx] != nullptr) {
        membuf sbuf((char*)m_pRewindBuffers[loadIdx], (char*)m_pRewindBuffers[loadIdx] + loadSize);
        std::istream ss(&sbuf);

        Nes::Api::Machine machine(*m_pEmulator);
        Nes::Result ret = machine.LoadState(ss);
        if (ret == Nes::RESULT_OK || ret == Nes::RESULT_NOP) {
            CLogger::Get()->Write("orchestrator", LogNotice, "Rewind NES state loaded successfully!");
            // Reset rewind buffers to clean slate with current loaded state
            m_nRewindWriteIdx = 0;
            // Copy loaded state to slot 0 (which is already preallocated)
            memcpy(m_pRewindBuffers[0], m_pRewindBuffers[loadIdx], loadSize);
            m_nRewindStateSizes[0] = loadSize;
            m_nStateSize = loadSize;
            for (int i = 1; i < 6; i++) {
                m_nRewindStateSizes[i] = 0;
            }
            m_nRewindWriteIdx = 1;
            m_nRewindCount = 1;
            m_nRewindFrameCounter = 0;
            ResetAudioAfterStateChange();
        } else {
            CLogger::Get()->Write("orchestrator", LogError, "Failed to load rewind NES state! error=%d", (int)ret);
        }
    }
}

void CNESOrchestrator::ResetAudioAfterStateChange()
{
    if (!m_bRomLoaded || !m_pEmulator) return;

    Nes::Api::Sound sound(*m_pEmulator);
    sound.EmptyBuffer();

    g_SharedState.audio_ring_buffer.Init();

    m_nAudioMuteFrames = 60; // Mute for 60 frames (1.0 second at 60 FPS)
}

bool CNESOrchestrator::VideoLockCallback(void* userData, Nes::Core::Video::Output& output)
{
    (void)userData;
    int idx = g_SharedState.emu_write_idx;
    output.pixels = g_SharedState.emu_frame_buffer[idx];
    output.pitch = 512 * sizeof(u16); // Nestopia writes to a buffer with pitch of 512 shorts (we support SNES up to 512x480)
    return true;
}

void CNESOrchestrator::VideoUnlockCallback(void* userData, Nes::Core::Video::Output& output)
{
    (void)userData;
    (void)output;
    int idx = g_SharedState.emu_write_idx;
    g_SharedState.game_w[idx] = 256;
    g_SharedState.game_h[idx] = 240;
    g_SharedState.start_line[idx] = 0;

    g_SharedState.emu_read_idx = idx;
    g_SharedState.emu_write_idx = 1 - idx;

    DataMemBarrier();
    g_SharedState.video_frame_ready = TRUE;
}

bool CNESOrchestrator::SoundLockCallback(void* userData, Nes::Core::Sound::Output& output)
{
    CNESOrchestrator* pSelf = (CNESOrchestrator*)userData;
    unsigned samples_to_render = pSelf->IsPAL() ? 882 : 735;
    output.samples[0] = pSelf->m_SoundBuffer;
    output.length[0] = samples_to_render;
    output.samples[1] = nullptr;
    output.length[1] = 0;
    return true;
}

void CNESOrchestrator::SoundUnlockCallback(void* userData, Nes::Core::Sound::Output& output)
{
    CNESOrchestrator* pSelf = (CNESOrchestrator*)userData;
    unsigned num_samples = output.length[0];
    if (num_samples > 0) {
        if (pSelf->m_nAudioMuteFrames > 0) {
            pSelf->m_nAudioMuteFrames--;
            memset(pSelf->m_SoundBuffer, 0, num_samples * 2 * sizeof(s16));
        }
        g_SharedState.audio_ring_buffer.Write(pSelf->m_SoundBuffer, num_samples);
    }
}

bool CNESOrchestrator::InputPollCallback(void* userData, Nes::Core::Input::Controllers::Pad& pad, unsigned port)
{
    (void)userData;
    u16 raw_pad = 0;
    if (port == 0) {
        raw_pad = g_SharedState.pad1;
    } else if (port == 1) {
        raw_pad = g_SharedState.pad2;
    }

    if ((raw_pad & (1 << 10)) && (raw_pad & (1 << 11))) {
        raw_pad = 0; // Mask inputs when START + SELECT is held
    }

    pad.buttons = 0;
    
    if (raw_pad & (1 << 0))  pad.buttons |= Nes::Core::Input::Controllers::Pad::UP;
    if (raw_pad & (1 << 1))  pad.buttons |= Nes::Core::Input::Controllers::Pad::DOWN;
    if (raw_pad & (1 << 2))  pad.buttons |= Nes::Core::Input::Controllers::Pad::LEFT;
    if (raw_pad & (1 << 3))  pad.buttons |= Nes::Core::Input::Controllers::Pad::RIGHT;
    
    // SNES A or X -> NES A
    if ((raw_pad & (1 << 4)) || (raw_pad & (1 << 6))) pad.buttons |= Nes::Core::Input::Controllers::Pad::A;
    // SNES B or Y -> NES B
    if ((raw_pad & (1 << 5)) || (raw_pad & (1 << 7))) pad.buttons |= Nes::Core::Input::Controllers::Pad::B;
    
    if (raw_pad & (1 << 10)) pad.buttons |= Nes::Core::Input::Controllers::Pad::START;
    if (raw_pad & (1 << 11)) pad.buttons |= Nes::Core::Input::Controllers::Pad::SELECT;

    return true;
}
