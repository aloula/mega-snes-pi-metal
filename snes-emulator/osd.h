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

private:
    void ScanRoms();
    void BuildFilteredList();
    void CalculateTabLabels();
    void LoadFavorites();
    void SaveFavorites();

private:
    FATFS *m_pFileSystem;
    char m_RomFiles[MAX_ROMS][128];
    unsigned m_RomSizes[MAX_ROMS];
    boolean m_RomFavorites[MAX_ROMS];
    int m_RomCount;
    int m_SelectedIndex;

    int m_FilteredIndices[MAX_ROMS];
    int m_FilteredCount;

    int m_ActiveTab; // 0 = ALL, 1 = FAV, 2..7 = A-D, E-H, I-L, M-P, Q-T, U-Z
    char m_TabLabels[8][16];
    int m_TabSplitK1;
    int m_TabSplitK2;
    int m_TabSplitK3;
    int m_TabSplitK4;
    int m_TabSplitK5;
};

#endif
