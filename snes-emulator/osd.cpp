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

COSDMenu::COSDMenu(FATFS *pFileSystem)
    : m_pFileSystem(pFileSystem),
      m_RomCount(0),
      m_SelectedIndex(0),
      m_SystemCount(0),
      m_FilteredCount(0),
      m_ActiveTab(0),
      m_TabSplitK1(6),
      m_TabSplitK2(12),
      m_TabSplitK3(18)
{
    for (int i = 0; i < MAX_ROMS; i++) {
        m_RomFiles[i][0] = '\0';
        m_RomSizes[i] = 0;
        m_RomFavorites[i] = FALSE;
        m_RomSystems[i] = RomSystem_SNES;
        m_SystemIndices[i] = -1;
        m_FilteredIndices[i] = -1;
    }
    for (int t = 0; t < 6; t++) {
        m_TabLabels[t][0] = '\0';
    }
}

COSDMenu::~COSDMenu() {}

boolean COSDMenu::Initialize() {
    ScanRoms();
    FilterSystemRoms();
    CalculateTabLabels();
    BuildFilteredList();
    Update();
    return TRUE;
}

void COSDMenu::OnEmuModeChanged() {
    FilterSystemRoms();
    CalculateTabLabels();
    m_ActiveTab = 0; // Reset to ALL tab when switching systems
    m_SelectedIndex = 0;
    BuildFilteredList();
    Update();
}

void COSDMenu::ScanRoms() {
    m_RomCount = 0;

    auto scan_dir = [this](const char *dirPath, const char *prefix, RomSystem system) {
        DIR dir;
        FILINFO fileInfo;
        FRESULT res = f_findfirst(&dir, &fileInfo, dirPath, "*");
        if (res != FR_OK) {
            CLogger::Get()->Write("OSD", LogWarning, "f_findfirst failed on %s: %d", dirPath, res);
            return;
        }

        while (res == FR_OK && fileInfo.fname[0] != '\0' && m_RomCount < MAX_ROMS) {
            if (!(fileInfo.fattrib & AM_DIR) && !(fileInfo.fattrib & (AM_HID | AM_SYS))) {
                const char *pDot = strrchr(fileInfo.fname, '.');
                if (pDot != 0) {
                    pDot++;
                    boolean matches = FALSE;
                    if (system == RomSystem_SNES) {
                        if (my_strcasecmp(pDot, "sfc") == 0 ||
                            my_strcasecmp(pDot, "smc") == 0) {
                            matches = TRUE;
                        }
                    } else if (system == RomSystem_MD || system == RomSystem_MCD) {
                        if (my_strcasecmp(pDot, "bin") == 0 ||
                            my_strcasecmp(pDot, "md") == 0 ||
                            my_strcasecmp(pDot, "gen") == 0 ||
                            my_strcasecmp(pDot, "iso") == 0 ||
                            my_strcasecmp(pDot, "cue") == 0 ||
                            my_strcasecmp(pDot, "chd") == 0) {
                            matches = TRUE;
                        }
                    }

                    if (matches) {
                        snprintf(m_RomFiles[m_RomCount], sizeof(m_RomFiles[m_RomCount]), "%s%s", prefix, fileInfo.fname);
                        m_RomSizes[m_RomCount] = fileInfo.fsize;
                        m_RomSystems[m_RomCount] = system;
                        m_RomCount++;
                    }
                }
            }
            res = f_findnext(&dir, &fileInfo);
        }
        f_closedir(&dir);
    };

    scan_dir("SD:/roms/snes", "snes/", RomSystem_SNES);
    scan_dir("SD:/roms/megadrive", "megadrive/", RomSystem_MD);
    scan_dir("SD:/roms/megacd", "megacd/", RomSystem_MCD);

    // Sort ROMs alphabetically on the base filename (excluding prefix)
    for (int i = 0; i < m_RomCount - 1; i++) {
        for (int j = i + 1; j < m_RomCount; j++) {
            const char *name_i = m_RomFiles[i];
            const char *slash_i = strchr(name_i, '/');
            if (slash_i) name_i = slash_i + 1;

            const char *name_j = m_RomFiles[j];
            const char *slash_j = strchr(name_j, '/');
            if (slash_j) name_j = slash_j + 1;

            if (my_strcasecmp(name_i, name_j) > 0) {
                // Swap files
                char tempFile[128];
                strcpy(tempFile, m_RomFiles[i]);
                strcpy(m_RomFiles[i], m_RomFiles[j]);
                strcpy(m_RomFiles[j], tempFile);
                // Swap sizes
                unsigned tempSize = m_RomSizes[i];
                m_RomSizes[i] = m_RomSizes[j];
                m_RomSizes[j] = tempSize;
                // Swap systems
                RomSystem tempSystem = m_RomSystems[i];
                m_RomSystems[i] = m_RomSystems[j];
                m_RomSystems[j] = tempSystem;
            }
        }
    }

    LoadFavorites();
}

void COSDMenu::FilterSystemRoms() {
    m_SystemCount = 0;
    if (g_SharedState.active_emu_mode == EmuMode_SNES) {
        for (int i = 0; i < m_RomCount; i++) {
            if (m_RomSystems[i] == RomSystem_SNES) {
                m_SystemIndices[m_SystemCount++] = i;
            }
        }
    } else {
        for (int i = 0; i < m_RomCount; i++) {
            if (m_RomSystems[i] == RomSystem_MD || m_RomSystems[i] == RomSystem_MCD) {
                m_SystemIndices[m_SystemCount++] = i;
            }
        }
    }
}

void COSDMenu::CalculateTabLabels() {
    // Tab 0: "ALL"
    strcpy(m_TabLabels[0], "ALL");

    // Tab 1: "FAV"
    strcpy(m_TabLabels[1], "FAV");

    // Tab 5: "MCD"
    if (g_SharedState.active_emu_mode != EmuMode_SNES) {
        strcpy(m_TabLabels[5], "MCD");
    } else {
        m_TabLabels[5][0] = '\0';
    }

    // Count non-MCD games per letter
    int letter_counts[27] = {0};
    int non_mcd_count = 0;
    for (int i = 0; i < m_SystemCount; i++) {
        if (!IsMCD(i)) {
            char c = GetChar(i);
            int idx = GetLetterIdx(c);
            if (idx >= 0 && idx < 27) {
                letter_counts[idx]++;
                non_mcd_count++;
            }
        }
    }

    // Optimize k1 and k2 to divide non-MCD games as equally as possible across 3 tabs (Tabs 2 to 4)
    m_TabSplitK1 = 8;   // Default: #-H
    m_TabSplitK2 = 16;  // Default: I-P
    int min_diff = 1000000;
    
    if (non_mcd_count > 0) {
        for (int k1 = 0; k1 < 25; k1++) {
            for (int k2 = k1 + 1; k2 < 26; k2++) {
                int size0 = 0;
                for (int i = 0; i <= k1; i++) size0 += letter_counts[i];
                
                int size1 = 0;
                for (int i = k1 + 1; i <= k2; i++) size1 += letter_counts[i];
                
                int size2 = 0;
                for (int i = k2 + 1; i < 27; i++) size2 += letter_counts[i];
                
                int ideal = non_mcd_count / 3;
                int d0 = size0 - ideal; if (d0 < 0) d0 = -d0;
                int d1 = size1 - ideal; if (d1 < 0) d1 = -d1;
                int d2 = size2 - ideal; if (d2 < 0) d2 = -d2;
                int diff = d0 + d1 + d2;
                
                if (diff < min_diff) {
                    min_diff = diff;
                    m_TabSplitK1 = k1;
                    m_TabSplitK2 = k2;
                }
            }
        }
    }

    // Now generate labels based on the actual games present in each split
    int start0 = -1, end0 = -1;
    for (int i = 0; i < m_SystemCount; i++) {
        if (!IsMCD(i)) {
            char c = GetChar(i);
            int idx = GetLetterIdx(c);
            if (idx <= m_TabSplitK1) {
                if (start0 == -1) start0 = i;
                end0 = i;
            }
        }
    }
    if (start0 != -1 && end0 != -1) {
        char c_start = GetChar(start0);
        char c_end = GetChar(end0);
        if (c_start == c_end) {
            snprintf(m_TabLabels[2], sizeof(m_TabLabels[2]), "%c", c_start);
        } else {
            snprintf(m_TabLabels[2], sizeof(m_TabLabels[2]), "%c-%c", c_start, c_end);
        }
    } else {
        strcpy(m_TabLabels[2], "A-H");
    }

    int start1 = -1, end1 = -1;
    for (int i = 0; i < m_SystemCount; i++) {
        if (!IsMCD(i)) {
            char c = GetChar(i);
            int idx = GetLetterIdx(c);
            if (idx > m_TabSplitK1 && idx <= m_TabSplitK2) {
                if (start1 == -1) start1 = i;
                end1 = i;
            }
        }
    }
    if (start1 != -1 && end1 != -1) {
        char c_start = GetChar(start1);
        char c_end = GetChar(end1);
        if (c_start == c_end) {
            snprintf(m_TabLabels[3], sizeof(m_TabLabels[3]), "%c", c_start);
        } else {
            snprintf(m_TabLabels[3], sizeof(m_TabLabels[3]), "%c-%c", c_start, c_end);
        }
    } else {
        strcpy(m_TabLabels[3], "I-P");
    }

    int start2 = -1, end2 = -1;
    for (int i = 0; i < m_SystemCount; i++) {
        if (!IsMCD(i)) {
            char c = GetChar(i);
            int idx = GetLetterIdx(c);
            if (idx > m_TabSplitK2) {
                if (start2 == -1) start2 = i;
                end2 = i;
            }
        }
    }
    if (start2 != -1 && end2 != -1) {
        char c_start = GetChar(start2);
        char c_end = GetChar(end2);
        if (c_start == c_end) {
            snprintf(m_TabLabels[4], sizeof(m_TabLabels[4]), "%c", c_start);
        } else {
            snprintf(m_TabLabels[4], sizeof(m_TabLabels[4]), "%c-%c", c_start, c_end);
        }
    } else {
        strcpy(m_TabLabels[4], "Q-Z");
    }
}

void COSDMenu::BuildFilteredList() {
    m_FilteredCount = 0;

    if (m_ActiveTab == 0) {
        // ALL tab: include all scanned roms of the current system
        for (int i = 0; i < m_SystemCount; i++) {
            m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
        }
    }
    else if (m_ActiveTab == 1) {
        // FAV tab: include only favorited roms of the current system
        for (int i = 0; i < m_SystemCount; i++) {
            int orig_idx = m_SystemIndices[i];
            if (m_RomFavorites[orig_idx]) {
                m_FilteredIndices[m_FilteredCount++] = orig_idx;
            }
        }
    }
    else if (m_ActiveTab >= 2 && m_ActiveTab <= 4) {
        // Alphabetical splits (Tabs 2 to 4)
        if (m_SystemCount > 0) {
            int part = m_ActiveTab - 2;
            for (int i = 0; i < m_SystemCount; i++) {
                if (!IsMCD(i)) {
                    char c = GetChar(i);
                    int idx = GetLetterIdx(c);
                    
                    if (part == 0) {
                        if (idx <= m_TabSplitK1) {
                            m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                        }
                    } else if (part == 1) {
                        if (idx > m_TabSplitK1 && idx <= m_TabSplitK2) {
                            m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                        }
                    } else if (part == 2) {
                        if (idx > m_TabSplitK2) {
                            m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                        }
                    }
                }
            }
        }
    }
    else if (m_ActiveTab == 5) {
        // MCD tab: include only Mega CD games of the current system
        for (int i = 0; i < m_SystemCount; i++) {
            if (IsMCD(i)) {
                m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
            }
        }
    }
}

void COSDMenu::Update() {
    g_SharedState.menu_num_lines = m_FilteredCount;
    g_SharedState.menu_selected_idx = m_SelectedIndex;
    g_SharedState.menu_active_tab = m_ActiveTab;

    // Copy tab titles to shared state
    for (int t = 0; t < 6; t++) {
        strncpy(g_SharedState.menu_tab_names[t], m_TabLabels[t], sizeof(g_SharedState.menu_tab_names[t]) - 1);
        g_SharedState.menu_tab_names[t][sizeof(g_SharedState.menu_tab_names[t]) - 1] = '\0';
    }

    for (int i = 0; i < m_FilteredCount; i++) {
        int orig_idx = m_FilteredIndices[i];
        char temp[80];
        strncpy(temp, m_RomFiles[orig_idx], sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';

        char *pDot = strrchr(temp, '.');
        if (pDot != nullptr) {
            *pDot = '\0';
        }

        const char *displayName = temp;
        const char *slash = strchr(temp, '/');
        if (slash != nullptr) {
            displayName = slash + 1;
        }

        char cleanName[80];
        strncpy(cleanName, displayName, sizeof(cleanName) - 1);
        cleanName[sizeof(cleanName) - 1] = '\0';
        
        int max_len = 52;
        if ((int)strlen(cleanName) > max_len) {
            strcpy(cleanName + max_len - 3, "...");
        }

        unsigned size_kb = m_RomSizes[orig_idx] / 1024;
        const char *prefix = m_RomFavorites[orig_idx] ? "* " : "  ";
        if (size_kb >= 1024) {
            snprintf(g_SharedState.menu_lines[i], 80, "%s%s (%u MB)", prefix, cleanName, size_kb / 1024);
        } else {
            snprintf(g_SharedState.menu_lines[i], 80, "%s%s (%u KB)", prefix, cleanName, size_kb);
        }
    }

    DataMemBarrier();
    g_SharedState.menu_needs_redraw = TRUE;
}

void COSDMenu::MoveUp() {
    if (m_FilteredCount == 0) return;
    if (m_SelectedIndex > 0) {
        m_SelectedIndex--;
    } else {
        m_SelectedIndex = m_FilteredCount - 1; // wrap around
    }
    Update();
}

void COSDMenu::MoveDown() {
    if (m_FilteredCount == 0) return;
    if (m_SelectedIndex < m_FilteredCount - 1) {
        m_SelectedIndex++;
    } else {
        m_SelectedIndex = 0; // wrap around
    }
    Update();
}

void COSDMenu::MoveLeft() {
    int num_tabs = (g_SharedState.active_emu_mode == EmuMode_SNES) ? 5 : 6;
    if (m_ActiveTab > 0) {
        m_ActiveTab--;
    } else {
        m_ActiveTab = num_tabs - 1;
    }
    m_SelectedIndex = 0;
    BuildFilteredList();
    Update();
}

void COSDMenu::MoveRight() {
    int num_tabs = (g_SharedState.active_emu_mode == EmuMode_SNES) ? 5 : 6;
    if (m_ActiveTab < num_tabs - 1) {
        m_ActiveTab++;
    } else {
        m_ActiveTab = 0;
    }
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
    for (int i = 0; i < MAX_ROMS; i++) {
        m_RomFavorites[i] = FALSE;
    }

    FIL file;
    FRESULT res = f_open(&file, "SD:/roms/favorites.txt", FA_READ);
    if (res != FR_OK) {
        return;
    }

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
    FRESULT res = f_open(&file, "SD:/roms/favorites.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        CLogger::Get()->Write("OSD", LogError, "Failed to open favorites.txt for writing: %d", res);
        return;
    }

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
    if (m_FilteredCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_FilteredCount) {
        return;
    }
    int orig_idx = m_FilteredIndices[m_SelectedIndex];
    m_RomFavorites[orig_idx] = TRUE;
    SaveFavorites();
    Update();
}

void COSDMenu::UnfavoriteCurrent() {
    if (m_FilteredCount == 0 || m_SelectedIndex < 0 || m_SelectedIndex >= m_FilteredCount) {
        return;
    }
    int orig_idx = m_FilteredIndices[m_SelectedIndex];
    m_RomFavorites[orig_idx] = FALSE;
    SaveFavorites();
    Update();
}

boolean COSDMenu::IsMCD(int sys_idx) const {
    if (sys_idx < 0 || sys_idx >= m_SystemCount) return FALSE;
    int orig_idx = m_SystemIndices[sys_idx];
    return m_RomSystems[orig_idx] == RomSystem_MCD;
}

char COSDMenu::GetChar(int sys_idx) const {
    if (sys_idx < 0 || sys_idx >= m_SystemCount) return '?';
    const char *name = m_RomFiles[m_SystemIndices[sys_idx]];
    if (name == nullptr || *name == '\0') return '?';
    const char *slash = strchr(name, '/');
    if (slash != nullptr) {
        name = slash + 1;
    }
    char c = *name;
    if (c >= 'a' && c <= 'z') c -= 32;
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
    return '#';
}

int COSDMenu::GetLetterIdx(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
    return 0;
}
