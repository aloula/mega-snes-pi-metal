#ifndef MD_ORCHESTRATOR_H
#define MD_ORCHESTRATOR_H

#include <circle/types.h>
#include <ff.h>

#define NUM_REWIND_SLOTS 10

class CMDOrchestrator {
public:
    CMDOrchestrator(FATFS *pFileSystem);
    ~CMDOrchestrator();

    boolean Initialize();
    boolean LoadROM(const char *pRomName, unsigned nRomSize);
    void Unload();
    void RunFrame();

    // Returns FALSE if the save was deferred for VDP-busy safety (see
    // md_orchestrator.cpp) and should be retried on a later frame.
    boolean SaveState(int slot = 0);
    void LoadState(int slot = 0);
    boolean IsPAL() const;

    void CaptureRewindState();
    void RewindState();

private:
    FATFS *m_pFileSystem;
    u8 *m_pRomBuffer;
    boolean m_bRomLoaded;
    char m_CurrentRomName[128];

    // 5 seconds state buffer (6 slots, captured once per second)
    u8 *m_pRewindBuffers[6];
    size_t m_nRewindStateSizes[6];
    int m_nRewindWriteIdx;
    int m_nRewindCount;
    u32 m_nRewindFrameCounter;
    size_t m_nStateSize;
};

#endif
