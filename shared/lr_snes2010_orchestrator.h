#ifndef LR_SNES2010_ORCHESTRATOR_H
#define LR_SNES2010_ORCHESTRATOR_H

#include <circle/types.h>
#include <ff.h>

class CLRSnes2010Orchestrator {
public:
    CLRSnes2010Orchestrator(FATFS *pFileSystem);
    ~CLRSnes2010Orchestrator();

    boolean Initialize();
    boolean LoadROM(const char *pRomName, unsigned nRomSize);
    void RunFrame();

    void SaveState(int slot = 0);
    void LoadState(int slot = 0);
    boolean IsPAL() const;
    void SaveSRAM();

    void CaptureRewindState();
    void RewindState();

private:
    FATFS *m_pFileSystem;
    u8 *m_pRomBuffer;
    u8 *m_pStateBuffer;
    boolean m_bCoreInitialized;
    boolean m_bRomLoaded;
    boolean m_bSA1;
    int m_nPendingLoadSlot;
    char m_CurrentRomName[128];

    u8 *m_pRewindBuffers[6];
    int m_nRewindWriteIdx;
    int m_nRewindCount;
    u32 m_nRewindFrameCounter;
    size_t m_nStateSize;
    size_t m_nManualStateSize;

    u32 m_nAudioMuteFrames;
    u32 m_nAudioRate;
    u32 m_nAudioResampleAccumulator;

    static CLRSnes2010Orchestrator *s_pActive;

    static bool Environment(unsigned cmd, void *data);
    static void VideoRefresh(const void *data, unsigned width, unsigned height, size_t pitch);
    static size_t AudioBatch(const int16_t *data, size_t frames);
    static void InputPoll();
    static int16_t InputState(unsigned port, unsigned device, unsigned index, unsigned id);

    void ResetAudioAfterStateChange();
    void LoadSRAM();
    boolean LoadStateNow(int slot);
    void PublishVideoFrame(const void *data, unsigned width, unsigned height, size_t pitch);
    size_t PushAudio(const int16_t *data, size_t frames);
    int16_t ReadInput(unsigned port, unsigned device, unsigned index, unsigned id) const;
    void BuildStateName(char *stateName, size_t stateNameSize, int slot) const;
    void BuildSRAMName(char *sramName, size_t sramNameSize) const;
};

#endif
