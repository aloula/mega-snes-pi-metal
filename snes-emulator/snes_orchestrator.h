#ifndef SNES_ORCHESTRATOR_H
#define SNES_ORCHESTRATOR_H

#include <circle/types.h>
#include <ff.h>

class CSNESOrchestrator {
public:
    CSNESOrchestrator(FATFS *pFileSystem);
    ~CSNESOrchestrator();

    boolean Initialize();
    boolean LoadROM(const char *pRomName, unsigned nRomSize);
    void RunFrame();

    void SaveState(int slot = 0);
    void LoadState(int slot = 0);
    boolean IsPAL() const;

private:
    FATFS *m_pFileSystem;
    u8 *m_pRomBuffer;
    boolean m_bRomLoaded;
    char m_CurrentRomName[128];
    u16 m_LastPad1;
    u16 m_LastPad2;
};

#endif
