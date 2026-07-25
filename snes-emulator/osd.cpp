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
    int system;
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
      m_SystemCount(0),
      m_FilteredCount(0),
      m_ActiveTab(0),
      m_TabSplitK1(6),
      m_TabSplitK2(12),
      m_TabSplitK3(18),
      m_TabSplitK4(21),
      m_TabSplitK5(24)
{
    for (int i = 0; i < MAX_ROMS; i++) {
        m_RomFiles[i][0] = '\0';
        m_RomSizes[i] = 0;
        m_RomFavorites[i] = FALSE;
        m_RomSystems[i] = RomSystem_SNES;
        m_SystemIndices[i] = -1;
        m_FilteredIndices[i] = -1;
    }
    for (int t = 0; t < 8; t++) {
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
                    } else if (system == RomSystem_NES) {
                        if (my_strcasecmp(pDot, "nes") == 0) {
                            matches = TRUE;
                        }
                    } else if (system == RomSystem_PCE) {
                        if (my_strcasecmp(pDot, "pce") == 0 ||
                            my_strcasecmp(pDot, "cue") == 0 ||
                            my_strcasecmp(pDot, "chd") == 0) {
                            matches = TRUE;
                        }
                    } else if (system == RomSystem_SMS) {
                        if (my_strcasecmp(pDot, "sms") == 0 ||
                            my_strcasecmp(pDot, "gg") == 0 ||
                            my_strcasecmp(pDot, "sg") == 0 ||
                            my_strcasecmp(pDot, "bin") == 0) {
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
        if (res != FR_OK) {
            CLogger::Get()->Write("OSD", LogWarning, "f_findnext failed on %s: %d (scanned %d ROMs)", dirPath, res, m_RomCount);
        }
        f_closedir(&dir);
    };

    scan_dir("SD:/roms/snes", "snes/", RomSystem_SNES);
    scan_dir("SD:/roms/megadrive", "megadrive/", RomSystem_MD);
    scan_dir("SD:/roms/megacd", "megacd/", RomSystem_MCD);
    scan_dir("SD:/roms/nes", "nes/", RomSystem_NES);
    scan_dir("SD:/roms/pce", "pce/", RomSystem_PCE);
    scan_dir("SD:/roms/mastersystem", "mastersystem/", RomSystem_SMS);

    // Sort ROMs alphabetically on the base filename (excluding prefix)
    if (m_RomCount > 1) {
        RomEntry *entries = new RomEntry[m_RomCount];
        for (int i = 0; i < m_RomCount; i++) {
            strcpy(entries[i].file, m_RomFiles[i]);
            entries[i].size = m_RomSizes[i];
            entries[i].system = (int)m_RomSystems[i];
        }
        SortRomEntries(entries, 0, m_RomCount - 1);

        for (int i = 0; i < m_RomCount; i++) {
            strcpy(m_RomFiles[i], entries[i].file);
            m_RomSizes[i] = entries[i].size;
            m_RomSystems[i] = (RomSystem)entries[i].system;
        }

        delete[] entries;
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
    } else if (g_SharedState.active_emu_mode == EmuMode_MD) {
        for (int i = 0; i < m_RomCount; i++) {
            if (m_RomSystems[i] == RomSystem_MD || m_RomSystems[i] == RomSystem_MCD) {
                m_SystemIndices[m_SystemCount++] = i;
            }
        }
    } else if (g_SharedState.active_emu_mode == EmuMode_NES) {
        for (int i = 0; i < m_RomCount; i++) {
            if (m_RomSystems[i] == RomSystem_NES) {
                m_SystemIndices[m_SystemCount++] = i;
            }
        }
    } else if (g_SharedState.active_emu_mode == EmuMode_PCE) {
        for (int i = 0; i < m_RomCount; i++) {
            if (m_RomSystems[i] == RomSystem_PCE) {
                m_SystemIndices[m_SystemCount++] = i;
            }
        }
    } else if (g_SharedState.active_emu_mode == EmuMode_SMS) {
        for (int i = 0; i < m_RomCount; i++) {
            if (m_RomSystems[i] == RomSystem_SMS) {
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

    // Count non-CD games per letter
    int letter_counts[27] = {0};
    int total_non_cd = 0;
    for (int i = 0; i < m_SystemCount; i++) {
        if (!IsMCD(i) && !IsPCECD(i)) {
            char c = GetChar(i);
            int idx = GetLetterIdx(c);
            if (idx >= 0 && idx < 27) {
                letter_counts[idx]++;
                total_non_cd++;
            }
        }
    }

    int prefix_counts[27];
    int running = 0;
    for (int i = 0; i < 27; i++) {
        running += letter_counts[i];
        prefix_counts[i] = running;
    }

    auto computeBalancedSplits = [&](int segments, int *splits_out) {
        int prev = -1;
        for (int s = 1; s < segments; s++) {
            int target = (total_non_cd * s + segments / 2) / segments;
            int min_idx = prev + 1;
            int max_idx = 26 - ((segments - 1) - s);
            if (max_idx < min_idx) max_idx = min_idx;

            int best_idx = min_idx;
            int best_diff = 0x7FFFFFFF;
            for (int idx = min_idx; idx <= max_idx; idx++) {
                int diff = prefix_counts[idx] - target;
                if (diff < 0) diff = -diff;
                if (diff < best_diff) {
                    best_diff = diff;
                    best_idx = idx;
                }
            }
            splits_out[s - 1] = best_idx;
            prev = best_idx;
        }
    };

    auto setRangeLabel = [this](int tab_idx, int lower_bound_exclusive, int upper_bound_inclusive,
                                 bool skip_cd, const char *fallback) {
        int start = -1;
        int end = -1;
        for (int i = 0; i < m_SystemCount; i++) {
            if (skip_cd && (IsMCD(i) || IsPCECD(i))) {
                continue;
            }
            int idx = GetLetterIdx(GetChar(i));
            if (idx > lower_bound_exclusive && idx <= upper_bound_inclusive) {
                if (start == -1) start = i;
                end = i;
            }
        }

        if (start != -1 && end != -1) {
            char c_start = GetChar(start);
            char c_end = GetChar(end);
            if (c_start == c_end) {
                snprintf(m_TabLabels[tab_idx], sizeof(m_TabLabels[tab_idx]), "%c", c_start);
            } else {
                snprintf(m_TabLabels[tab_idx], sizeof(m_TabLabels[tab_idx]), "%c-%c", c_start, c_end);
            }
        } else {
            strcpy(m_TabLabels[tab_idx], fallback);
        }
    };

    if (g_SharedState.active_emu_mode == EmuMode_MD || g_SharedState.active_emu_mode == EmuMode_PCE) {
        // MD or PCE: 5 sorted tabs (2..6) + CD (7)
        if (g_SharedState.active_emu_mode == EmuMode_MD) {
            strcpy(m_TabLabels[7], "MCD");
        } else {
            strcpy(m_TabLabels[7], "PCD");
        }

        m_TabSplitK1 = 5;
        m_TabSplitK2 = 10;
        m_TabSplitK3 = 15;
        m_TabSplitK4 = 20;
        if (total_non_cd > 0) {
            int splits[4] = {m_TabSplitK1, m_TabSplitK2, m_TabSplitK3, m_TabSplitK4};
            computeBalancedSplits(5, splits);
            m_TabSplitK1 = splits[0];
            m_TabSplitK2 = splits[1];
            m_TabSplitK3 = splits[2];
            m_TabSplitK4 = splits[3];
        }

        setRangeLabel(2, -1, m_TabSplitK1, TRUE, "A-E");
        setRangeLabel(3, m_TabSplitK1, m_TabSplitK2, TRUE, "F-J");
        setRangeLabel(4, m_TabSplitK2, m_TabSplitK3, TRUE, "K-O");
        setRangeLabel(5, m_TabSplitK3, m_TabSplitK4, TRUE, "P-T");
        setRangeLabel(6, m_TabSplitK4, 26, TRUE, "U-Z");
    } else {
        // SNES/NES: 6 sorted tabs (2..7)
        m_TabSplitK1 = 4;
        m_TabSplitK2 = 8;
        m_TabSplitK3 = 12;
        m_TabSplitK4 = 16;
        m_TabSplitK5 = 20;
        if (total_non_cd > 0) {
            int splits[5] = {m_TabSplitK1, m_TabSplitK2, m_TabSplitK3, m_TabSplitK4, m_TabSplitK5};
            computeBalancedSplits(6, splits);
            m_TabSplitK1 = splits[0];
            m_TabSplitK2 = splits[1];
            m_TabSplitK3 = splits[2];
            m_TabSplitK4 = splits[3];
            m_TabSplitK5 = splits[4];
        }

        setRangeLabel(2, -1, m_TabSplitK1, FALSE, "A-D");
        setRangeLabel(3, m_TabSplitK1, m_TabSplitK2, FALSE, "E-H");
        setRangeLabel(4, m_TabSplitK2, m_TabSplitK3, FALSE, "I-L");
        setRangeLabel(5, m_TabSplitK3, m_TabSplitK4, FALSE, "M-P");
        setRangeLabel(6, m_TabSplitK4, m_TabSplitK5, FALSE, "Q-T");
        setRangeLabel(7, m_TabSplitK5, 26, FALSE, "U-Z");
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
    else if (g_SharedState.active_emu_mode == EmuMode_MD || g_SharedState.active_emu_mode == EmuMode_PCE) {
        bool is_md = (g_SharedState.active_emu_mode == EmuMode_MD);
        if (m_ActiveTab >= 2 && m_ActiveTab <= 6) {
            // Alphabetical splits (Tabs 2 to 6)
            int part = m_ActiveTab - 2;
            for (int i = 0; i < m_SystemCount; i++) {
                bool skip = is_md ? IsMCD(i) : IsPCECD(i);
                if (!skip) {
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
                        if (idx > m_TabSplitK2 && idx <= m_TabSplitK3) {
                            m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                        }
                    } else if (part == 3) {
                        if (idx > m_TabSplitK3 && idx <= m_TabSplitK4) {
                            m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                        }
                    } else if (part == 4) {
                        if (idx > m_TabSplitK4) {
                            m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                        }
                    }
                }
            }
        }
        else if (m_ActiveTab == 7) {
            // CD tab: include only CD games of the current system
            for (int i = 0; i < m_SystemCount; i++) {
                bool is_cd = is_md ? IsMCD(i) : IsPCECD(i);
                if (is_cd) {
                    m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                }
            }
        }
    }
    else {
        // SNES or NES: 6 alphabetical splits (Tabs 2 to 7)
        if (m_ActiveTab >= 2 && m_ActiveTab <= 7) {
            int part = m_ActiveTab - 2;
            for (int i = 0; i < m_SystemCount; i++) {
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
                    if (idx > m_TabSplitK2 && idx <= m_TabSplitK3) {
                        m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                    }
                } else if (part == 3) {
                    if (idx > m_TabSplitK3 && idx <= m_TabSplitK4) {
                        m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                    }
                } else if (part == 4) {
                    if (idx > m_TabSplitK4 && idx <= m_TabSplitK5) {
                        m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                    }
                } else if (part == 5) {
                    if (idx > m_TabSplitK5) {
                        m_FilteredIndices[m_FilteredCount++] = m_SystemIndices[i];
                    }
                }
            }
        }
    }
}

void COSDMenu::Update() {
    g_SharedState.menu_num_lines = m_FilteredCount;
    g_SharedState.menu_selected_idx = m_SelectedIndex;
    g_SharedState.menu_active_tab = m_ActiveTab;

    // Copy tab titles to shared state
    for (int t = 0; t < 8; t++) {
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
    int num_tabs = 8;
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
    int num_tabs = 8;
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
    if (strncmp(m_RomFiles[orig_idx], "megacd/", 7) == 0) {
        return TRUE;
    }
    return m_RomSystems[orig_idx] == RomSystem_MCD;
}

boolean COSDMenu::IsPCECD(int sys_idx) const {
    if (sys_idx < 0 || sys_idx >= m_SystemCount) return FALSE;
    int orig_idx = m_SystemIndices[sys_idx];
    if (m_RomSystems[orig_idx] != RomSystem_PCE) return FALSE;
    const char *name = m_RomFiles[orig_idx];
    if (name == nullptr) return FALSE;
    size_t len = strlen(name);
    if (len > 4 && (strcasecmp(name + len - 4, ".cue") == 0 || strcasecmp(name + len - 4, ".chd") == 0)) {
        return TRUE;
    }
    return FALSE;
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
