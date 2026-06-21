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
    
    const char *GetSelectedRom();
    unsigned GetSelectedRomSize();
    void FavoriteCurrent();
    void UnfavoriteCurrent();
    void OnEmuModeChanged();

private:
    void ScanRoms();
    void FilterSystemRoms();
    void BuildFilteredList();
    void CalculateTabLabels();
    void LoadFavorites();
    void SaveFavorites();

private:
    enum RomSystem {
        RomSystem_SNES,
        RomSystem_MD,
        RomSystem_MCD
    };

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

    int m_ActiveTab; // 0 = ALL, 1 = FAV, 2 = Split 1, 3 = Split 2, 4 = Split 3, 5 = Split 4
    char m_TabLabels[6][16];
    int m_TabSplitK1;
    int m_TabSplitK2;
    int m_TabSplitK3;
};

#endif
