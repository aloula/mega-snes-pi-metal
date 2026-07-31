#ifndef OSD_H
#define OSD_H

#include <ff.h>
#include "shared_state.h"

class COSDMenu {
public:
    COSDMenu(FATFS *pFileSystem);
    ~COSDMenu();

    boolean Initialize();
    void Update();

    // Input handlers (called from orchestrator)
    void MoveUp();
    void MoveDown();
    void MoveLeft();
    void MoveRight();

    enum RomSystem : int {
        RomSystem_SNES,
        RomSystem_MD,
        RomSystem_MCD,
        RomSystem_NES,
        RomSystem_PCE,
        RomSystem_SMS
    };

    const char *GetSelectedRom();
    unsigned GetSelectedRomSize();
    RomSystem GetSelectedRomSystem() const;
    void FavoriteCurrent();
    void UnfavoriteCurrent();
    void OnEmuModeChanged();
    void OnReturnFromGame();
    void RebuildLibrary();

private:
    boolean LoadLibraryCache();
    void SaveLibraryCache();
    void ScanRoms();
    void FilterSystemRoms();
    void BuildFilteredList();
    void CalculateTabLabels();
    void LoadFavorites();
    void SaveFavorites();
    boolean IsMCD(int sys_idx) const;
    boolean IsPCECD(int sys_idx) const;
    char GetChar(int sys_idx) const;
    static int GetLetterIdx(char c);

private:
    FATFS *m_pFileSystem;
    char m_RomFiles[MAX_ROMS][128];
    unsigned m_RomSizes[MAX_ROMS];
    boolean m_RomFavorites[MAX_ROMS];
    RomSystem m_RomSystems[MAX_ROMS];
    int m_RomCount;
    int m_SelectedIndex;

    // Filtered lists for tabs based on current system
    int m_SystemIndices[MAX_ROMS];
    int m_SystemCount;

    int m_FilteredIndices[MAX_ROMS];
    int m_FilteredCount;

    int m_ActiveTab; // 0 = ALL, 1 = FAV, 2..7 = custom splits (or 7 = MCD in MD mode)
    int m_NumActiveTabs;
    int m_FavTabSystems[8];
    char m_TabLabels[8][16];
    int m_TabSplitK1;
    int m_TabSplitK2;
    int m_TabSplitK3;
    int m_TabSplitK4;
    int m_TabSplitK5;
    int m_TabSplitK6;
};

#endif
