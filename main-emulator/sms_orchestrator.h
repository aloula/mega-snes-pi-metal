#ifndef SMS_ORCHESTRATOR_H
#define SMS_ORCHESTRATOR_H

#include <circle/types.h>
#include <ff.h>

class CSMSOrchestrator {
public:
    CSMSOrchestrator(FATFS *pFileSystem);
    ~CSMSOrchestrator();

    boolean Initialize();
    boolean LoadROM(const char *pRomName, unsigned nRomSize);
    void Unload();
    void RunFrame();

    void SaveState(int slot = 0);
    void LoadState(int slot = 0);
    boolean IsPAL() const;

    void CaptureRewindState();
    void RewindState();
    boolean IsAudioMuted() const { return m_nMuteFrames > 0; }

private:
    FATFS *m_pFileSystem;
    u8 *m_pRomBuffer;
    boolean m_bRomLoaded;
    char m_CurrentRomName[128];

#define SMS_REWIND_BUFFER_SIZE (512 * 1024)

    // 5 seconds state buffer (6 slots, captured once per second)
    u8 *m_pRewindBuffers[6];
    size_t m_nRewindStateSizes[6];
    int m_nRewindWriteIdx;
    int m_nRewindCount;
    u32 m_nRewindFrameCounter;
    u32 m_nMuteFrames;
};

#endif
