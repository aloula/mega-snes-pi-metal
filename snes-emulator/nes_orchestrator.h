#ifndef NES_ORCHESTRATOR_H
#define NES_ORCHESTRATOR_H

#include <circle/types.h>
#include <ff.h>

#include <api/NstApiEmulator.hpp>
#include <api/NstApiMachine.hpp>
#include <api/NstApiVideo.hpp>
#include <api/NstApiSound.hpp>
#include <api/NstApiInput.hpp>
#include <api/NstApiUser.hpp>

class CNESOrchestrator {
public:
    CNESOrchestrator(FATFS *pFileSystem);
    ~CNESOrchestrator();

    boolean Initialize();
    boolean LoadROM(const char *pRomName, unsigned nRomSize);
    void RunFrame();

    void SaveState(int slot = 0);
    void LoadState(int slot = 0);
    boolean IsPAL() const;

    void CaptureRewindState();
    void RewindState();

private:
    static bool VideoLockCallback(void* userData, Nes::Core::Video::Output& output);
    static void VideoUnlockCallback(void* userData, Nes::Core::Video::Output& output);
    static bool SoundLockCallback(void* userData, Nes::Core::Sound::Output& output);
    static void SoundUnlockCallback(void* userData, Nes::Core::Sound::Output& output);
    static bool InputPollCallback(void* userData, Nes::Core::Input::Controllers::Pad& pad, unsigned port);

private:
    FATFS *m_pFileSystem;
    u8 *m_pRomBuffer;
    boolean m_bRomLoaded;
    char m_CurrentRomName[128];
    u16 m_LastPad1;
    u16 m_LastPad2;

    Nes::Api::Emulator* m_pEmulator;

    // 5 seconds state buffer (6 slots, captured once per second)
    u8 *m_pRewindBuffers[6];
    int m_nRewindWriteIdx;
    int m_nRewindCount;
    u32 m_nRewindFrameCounter;
    size_t m_nStateSize;

    // Sound buffer for the current frame
    s16 m_SoundBuffer[2048 * 2];
};

#endif
