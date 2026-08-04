#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "osd.h"
#include <circle/string.h>
#include <circle/logger.h>
#include <string.h>
#include <stdio.h>

static int my_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

struct RomEntry {
    char file[128];
    unsigned size;
};

static int CompareRomEntries(const void *a, const void *b) {
    const RomEntry *ea = (const RomEntry *)a;
    const RomEntry *eb = (const RomEntry *)b;
    const char *name_a = strchr(ea->file, '/');
    name_a = name_a ? name_a + 1 : ea->file;
    const char *name_b = strchr(eb->file, '/');
    name_b = name_b ? name_b + 1 : eb->file;
    return my_strcasecmp(name_a, name_b);
}

static void SwapRomEntries(RomEntry &a, RomEntry &b) {
    RomEntry tmp = a;
    a = b;
    b = tmp;
}

static void SortRomEntries(RomEntry *entries, int left, int right) {
    int i = left;
    int j = right;
    RomEntry pivot = entries[(left + right) >> 1];

    while (i <= j) {
        while (CompareRomEntries(&entries[i], &pivot) < 0) i++;
        while (CompareRomEntries(&entries[j], &pivot) > 0) j--;
        if (i <= j) {
            SwapRomEntries(entries[i], entries[j]);
            i++;
            j--;
        }
    }
    if (left < j) SortRomEntries(entries, left, j);
    if (i < right) SortRomEntries(entries, i, right);
}

COSDMenu::COSDMenu(FATFS *pFileSystem)
    : m_pFileSystem(pFileSystem),
      m_RomCount(0),
      m_SelectedIndex(0),
      m_FilteredCount(0),
      m_ActiveTab(0),
      m_TabSplitK1(4),
      m_TabSplitK2(8),
      m_TabSplitK3(12),
      m_TabSplitK4(16),
      m_TabSplitK5(20)
{
    for (int i = 0; i < MAX_ROMS; i++) {
        m_RomFiles[i][0] = '\0';
        m_RomSizes[i] = 0;
        m_RomFavorites[i] = FALSE;
        m_FilteredIndices[i] = -1;
    }
    for (int t = 0; t < 8; t++) {
        m_TabLabels[t][0] = '\0';
    }
}

COSDMenu::~COSDMenu() {}

boolean COSDMenu::Initialize() {
    ScanRoms();
    CalculateTabLabels();
    BuildFilteredList();
    Update();
    return TRUE;
}

void COSDMenu::ScanRoms() {
    m_RomCount = 0;
    DIR dir;
    FILINFO fileInfo;
    FRESULT res = f_findfirst(&dir, &fileInfo, "SD:/roms/snes", "*");
    if (res != FR_OK) {
        CLogger::Get()->Write("OSD", LogWarning, "f_findfirst failed on SD:/roms/snes: %d", res);
        return;
    }

    while (res == FR_OK && fileInfo.fname[0] != '\0' && m_RomCount < MAX_ROMS) {
        if (!(fileInfo.fattrib & AM_DIR) && !(fileInfo.fattrib & (AM_HID | AM_SYS))) {
            const char *pDot = strrchr(fileInfo.fname, '.');
            if (pDot != nullptr) {
                pDot++;
                if (my_strcasecmp(pDot, "sfc") == 0 || my_strcasecmp(pDot, "smc") == 0) {
                    snprintf(m_RomFiles[m_RomCount], sizeof(m_RomFiles[m_RomCount]), "%s", fileInfo.fname);
                    m_RomSizes[m_RomCount] = fileInfo.fsize;
                    m_RomFavorites[m_RomCount] = FALSE;
                    m_RomCount++;
                }
            }
        }
        res = f_findnext(&dir, &fileInfo);
    }
    f_closedir(&dir);

    if (m_RomCount > 1) {
        RomEntry *entries = new RomEntry[m_RomCount];
        for (int i = 0; i < m_RomCount; i++) {
            strcpy(entries[i].file, m_RomFiles[i]);
            entries[i].size = m_RomSizes[i];
        }
        SortRomEntries(entries, 0, m_RomCount - 1);
        for (int i = 0; i < m_RomCount; i++) {
            strcpy(m_RomFiles[i], entries[i].file);
            m_RomSizes[i] = entries[i].size;
        }
        delete[] entries;
    }

    LoadFavorites();
}

void COSDMenu::CalculateTabLabels() {
    strcpy(m_TabLabels[0], "ALL");
    strcpy(m_TabLabels[1], "FAV");
    strcpy(m_TabLabels[2], "A-D");
    strcpy(m_TabLabels[3], "E-H");
    strcpy(m_TabLabels[4], "I-L");
    strcpy(m_TabLabels[5], "M-P");
    strcpy(m_TabLabels[6], "Q-T");
    strcpy(m_TabLabels[7], "U-Z");
}

static int GetLetterIdx(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
    return 0;
}

void COSDMenu::BuildFilteredList() {
    m_FilteredCount = 0;
    if (m_ActiveTab == 0) {
        for (int i = 0; i < m_RomCount; i++) {
            m_FilteredIndices[m_FilteredCount++] = i;
        }
    } else if (m_ActiveTab == 1) {
        for (int i = 0; i < m_RomCount; i++) {
            if (m_RomFavorites[i]) {
                m_FilteredIndices[m_FilteredCount++] = i;
            }
        }
    } else if (m_ActiveTab >= 2 && m_ActiveTab <= 7) {
        int part = m_ActiveTab - 2;
        for (int i = 0; i < m_RomCount; i++) {
            char c = m_RomFiles[i][0];
            int idx = GetLetterIdx(c);
            if (part == 0 && idx <= 4) m_FilteredIndices[m_FilteredCount++] = i;
            else if (part == 1 && idx > 4 && idx <= 8) m_FilteredIndices[m_FilteredCount++] = i;
            else if (part == 2 && idx > 8 && idx <= 12) m_FilteredIndices[m_FilteredCount++] = i;
            else if (part == 3 && idx > 12 && idx <= 16) m_FilteredIndices[m_FilteredCount++] = i;
            else if (part == 4 && idx > 16 && idx <= 20) m_FilteredIndices[m_FilteredCount++] = i;
            else if (part == 5 && idx > 20) m_FilteredIndices[m_FilteredCount++] = i;
        }
    }
}

void COSDMenu::Update() {
    g_SharedState.menu_num_lines = m_FilteredCount;
    g_SharedState.menu_selected_idx = m_SelectedIndex;
    g_SharedState.menu_active_tab = m_ActiveTab;
    g_SharedState.menu_num_tabs = 8;

    for (int t = 0; t < 8; t++) {
        snprintf(g_SharedState.menu_tab_names[t], sizeof(g_SharedState.menu_tab_names[t]), "%s", m_TabLabels[t]);
    }

    for (int i = 0; i < m_FilteredCount; i++) {
        int orig_idx = m_FilteredIndices[i];
        char temp[80];
        snprintf(temp, sizeof(temp), "%s", m_RomFiles[orig_idx]);
        char *pDot = strrchr(temp, '.');
        if (pDot != nullptr) *pDot = '\0';

        if (m_RomFavorites[orig_idx]) {
            snprintf(g_SharedState.menu_lines[i], sizeof(g_SharedState.menu_lines[i]), "* %s", temp);
        } else {
            snprintf(g_SharedState.menu_lines[i], sizeof(g_SharedState.menu_lines[i]), "  %s", temp);
        }
    }
}

void COSDMenu::MoveUp() {
    if (m_FilteredCount == 0) return;
    if (m_SelectedIndex > 0) m_SelectedIndex--;
    else m_SelectedIndex = m_FilteredCount - 1;
    Update();
}

void COSDMenu::MoveDown() {
    if (m_FilteredCount == 0) return;
    if (m_SelectedIndex < m_FilteredCount - 1) m_SelectedIndex++;
    else m_SelectedIndex = 0;
    Update();
}

void COSDMenu::MoveLeft() {
    if (m_ActiveTab > 0) m_ActiveTab--;
    else m_ActiveTab = 7;
    m_SelectedIndex = 0;
    BuildFilteredList();
    Update();
}

void COSDMenu::MoveRight() {
    if (m_ActiveTab < 7) m_ActiveTab++;
    else m_ActiveTab = 0;
    m_SelectedIndex = 0;
    BuildFilteredList();
    Update();
}

const char *COSDMenu::GetSelectedRom() {
    if (m_FilteredCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_FilteredCount) {
        return nullptr;
    }
    return m_RomFiles[m_FilteredIndices[m_SelectedIndex]];
}

unsigned COSDMenu::GetSelectedRomSize() {
    if (m_FilteredCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_FilteredCount) {
        return 0;
    }
    return m_RomSizes[m_FilteredIndices[m_SelectedIndex]];
}

void COSDMenu::LoadFavorites() {
    for (int i = 0; i < MAX_ROMS; i++) m_RomFavorites[i] = FALSE;

    FIL file;
    if (f_open(&file, "SD:/roms/favorites.txt", FA_READ) != FR_OK) return;

    char line[128];
    while (f_gets(line, sizeof(line), &file) != nullptr) {
        int len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[len - 1] = '\0';
            len--;
        }
        if (len == 0) continue;
        for (int i = 0; i < m_RomCount; i++) {
            if (strcmp(m_RomFiles[i], line) == 0) {
                m_RomFavorites[i] = TRUE;
                break;
            }
        }
    }
    f_close(&file);
}

void COSDMenu::SaveFavorites() {
    FIL file;
    if (f_open(&file, "SD:/roms/favorites.txt", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return;
    for (int i = 0; i < m_RomCount; i++) {
        if (m_RomFavorites[i]) {
            char line[160];
            snprintf(line, sizeof(line), "%s\n", m_RomFiles[i]);
            UINT written = 0;
            f_write(&file, line, strlen(line), &written);
        }
    }
    f_close(&file);
}

void COSDMenu::FavoriteCurrent() {
    if (m_FilteredCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_FilteredCount) return;
    int orig_idx = m_FilteredIndices[m_SelectedIndex];
    m_RomFavorites[orig_idx] = TRUE;
    SaveFavorites();
    if (m_ActiveTab == 1) BuildFilteredList();
    Update();
}

void COSDMenu::UnfavoriteCurrent() {
    if (m_FilteredCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_FilteredCount) return;
    int orig_idx = m_FilteredIndices[m_SelectedIndex];
    m_RomFavorites[orig_idx] = FALSE;
    SaveFavorites();
    if (m_ActiveTab == 1) {
        BuildFilteredList();
        if (m_SelectedIndex >= m_FilteredCount && m_FilteredCount > 0) {
            m_SelectedIndex = m_FilteredCount - 1;
        }
    }
    Update();
}
