#ifndef PCE_ORCHESTRATOR_H
#define PCE_ORCHESTRATOR_H

#include <circle/types.h>
#include <ff.h>

class CPCEOrchestrator {
public:
    CPCEOrchestrator(FATFS *pFileSystem);
    ~CPCEOrchestrator();

    boolean Initialize();
    boolean LoadROM(const char *pRomName, unsigned nRomSize);
    void RunFrame();

    void SaveState(int slot = 0);
    void LoadState(int slot = 0);
    boolean IsPAL() const;

    void CaptureRewindState();
    void RewindState();

private:
    void ResetAudioAfterStateChange();

private:
    FATFS *m_pFileSystem;
    boolean m_bRomLoaded;
    char m_CurrentRomName[128];
    u16 m_LastPad1;
    u16 m_LastPad2;

    // 5 seconds state buffer (6 slots, captured once per second)
    u8 *m_pRewindBuffers[6];
    size_t m_nRewindStateSizes[6];
    int m_nRewindWriteIdx;
    int m_nRewindCount;
    u32 m_nRewindFrameCounter;
    size_t m_nStateSize;
};

#endif
