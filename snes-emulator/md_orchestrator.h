#ifndef MD_ORCHESTRATOR_H
#define MD_ORCHESTRATOR_H

#include <circle/types.h>
#include <ff.h>

class CMDOrchestrator {
public:
    CMDOrchestrator(FATFS *pFileSystem);
    ~CMDOrchestrator();

    boolean Initialize();
    boolean LoadROM(const char *pRomName, unsigned nRomSize);
    void RunFrame();

    void SaveState(int slot = 0);
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
    int m_nRewindWriteIdx;
    int m_nRewindCount;
    u32 m_nRewindFrameCounter;
    size_t m_nStateSize;
};

#endif
