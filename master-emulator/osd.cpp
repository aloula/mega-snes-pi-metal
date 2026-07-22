#include "osd.h"
#include <circle/string.h>
#include <circle/logger.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>

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
      m_FilteredCount(0),
      m_ActiveTab(0),
      m_TabSplitK1(8),
      m_TabSplitK2(16),
      m_TabSplitK3(22)
{
    for (int i = 0; i < MAX_ROMS; i++) {
        m_RomFiles[i][0] = '\0';
        m_RomSizes[i] = 0;
        m_RomFavorites[i] = FALSE;
        m_FilteredIndices[i] = -1;
    }
    for (int t = 0; t < 6; t++) {
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

    auto scan_dir = [this](const char *dirPath, const char *prefix) {
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
                    if (my_strcasecmp(pDot, "sms") == 0 ||
                        my_strcasecmp(pDot, "gg") == 0 ||
                        my_strcasecmp(pDot, "sg") == 0 ||
                        my_strcasecmp(pDot, "bin") == 0) {
                        snprintf(m_RomFiles[m_RomCount], sizeof(m_RomFiles[m_RomCount]), "%s%s", prefix, fileInfo.fname);
                        m_RomSizes[m_RomCount] = fileInfo.fsize;
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

    scan_dir("SD:/roms/mastersystem", "mastersystem/");
    scan_dir("SD:/roms/sms", "sms/");

    // Sort ROMs alphabetically on the base filename (excluding prefix) using fast O(N log N) std::sort
    if (m_RomCount > 1) {
        struct RomEntry {
            char file[128];
            unsigned size;
        };

        RomEntry *entries = new RomEntry[m_RomCount];
        for (int i = 0; i < m_RomCount; i++) {
            strcpy(entries[i].file, m_RomFiles[i]);
            entries[i].size = m_RomSizes[i];
        }

        std::sort(entries, entries + m_RomCount, [](const RomEntry &a, const RomEntry &b) {
            const char *name_a = strchr(a.file, '/');
            name_a = name_a ? name_a + 1 : a.file;

            const char *name_b = strchr(b.file, '/');
            name_b = name_b ? name_b + 1 : b.file;

            return my_strcasecmp(name_a, name_b) < 0;
        });

        for (int i = 0; i < m_RomCount; i++) {
            strcpy(m_RomFiles[i], entries[i].file);
            m_RomSizes[i] = entries[i].size;
        }

        delete[] entries;
    }
    LoadFavorites();
}

void COSDMenu::CalculateTabLabels() {
    // Tab 0: "ALL"
    strcpy(m_TabLabels[0], "ALL");

    // Tab 1: "FAV"
    strcpy(m_TabLabels[1], "FAV");

    // Helper to get uppercase starting character or '#' for numbers/symbols
    auto get_char = [this](int rom_idx) -> char {
        if (rom_idx < 0 || rom_idx >= m_RomCount) return '?';
        const char *name = m_RomFiles[rom_idx];
        if (name == nullptr || *name == '\0') return '?';
        const char *slash = strchr(name, '/');
        if (slash != nullptr) {
            name = slash + 1;
        }
        char c = *name;
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
        return '#';
    };

    // Helper to get letter index (0 to 26)
    auto get_letter_idx = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A' + 1; // 1 to 26
        return 0; // '#' or others
    };

    // Count games per letter
    int letter_counts[27] = {0};
    for (int i = 0; i < m_RomCount; i++) {
        char c = get_char(i);
        int idx = get_letter_idx(c);
        if (idx >= 0 && idx < 27) {
            letter_counts[idx]++;
        }
    }

    // Optimize split points k1, k2, k3
    m_TabSplitK1 = 6;  // A-F
    m_TabSplitK2 = 13; // G-M
    m_TabSplitK3 = 19; // N-S
    int min_diff = 1000000;
    
    if (m_RomCount > 0) {
        for (int k1 = 0; k1 < 24; k1++) {
            for (int k2 = k1 + 1; k2 < 25; k2++) {
                for (int k3 = k2 + 1; k3 < 26; k3++) {
                    int size0 = 0;
                    for (int i = 0; i <= k1; i++) size0 += letter_counts[i];
                    
                    int size1 = 0;
                    for (int i = k1 + 1; i <= k2; i++) size1 += letter_counts[i];
                    
                    int size2 = 0;
                    for (int i = k2 + 1; i <= k3; i++) size2 += letter_counts[i];

                    int size3 = 0;
                    for (int i = k3 + 1; i < 27; i++) size3 += letter_counts[i];
                    
                    int ideal = m_RomCount / 4;
                    int d0 = size0 - ideal; if (d0 < 0) d0 = -d0;
                    int d1 = size1 - ideal; if (d1 < 0) d1 = -d1;
                    int d2 = size2 - ideal; if (d2 < 0) d2 = -d2;
                    int d3 = size3 - ideal; if (d3 < 0) d3 = -d3;
                    int diff = d0 + d1 + d2 + d3;
                    
                    if (diff < min_diff) {
                        min_diff = diff;
                        m_TabSplitK1 = k1;
                        m_TabSplitK2 = k2;
                        m_TabSplitK3 = k3;
                    }
                }
            }
        }
    }

    // Now generate labels based on actual games present in each split
    auto label_range = [this, &get_char, &get_letter_idx](int min_k, int max_k, char *out_label, const char *fallback) {
        int start = -1, end = -1;
        for (int i = 0; i < m_RomCount; i++) {
            char c = get_char(i);
            int idx = get_letter_idx(c);
            if (idx > min_k && idx <= max_k) {
                if (start == -1) start = i;
                end = i;
            }
        }
        if (start != -1 && end != -1) {
            char c_start = get_char(start);
            char c_end = get_char(end);
            if (c_start == c_end) {
                snprintf(out_label, 16, "%c", c_start);
            } else {
                snprintf(out_label, 16, "%c-%c", c_start, c_end);
            }
        } else {
            strcpy(out_label, fallback);
        }
    };

    label_range(-1, m_TabSplitK1, m_TabLabels[2], "A-F");
    label_range(m_TabSplitK1, m_TabSplitK2, m_TabLabels[3], "G-M");
    label_range(m_TabSplitK2, m_TabSplitK3, m_TabLabels[4], "N-S");
    label_range(m_TabSplitK3, 26, m_TabLabels[5], "T-Z");
}

void COSDMenu::BuildFilteredList() {
    m_FilteredCount = 0;
    if (m_ActiveTab == 0) {
        // ALL tab: include all scanned roms
        for (int i = 0; i < m_RomCount; i++) {
            m_FilteredIndices[m_FilteredCount++] = i;
        }
    }
    else if (m_ActiveTab == 1) {
        // FAV tab: include only favorited roms
        for (int i = 0; i < m_RomCount; i++) {
            if (m_RomFavorites[i]) {
                m_FilteredIndices[m_FilteredCount++] = i;
            }
        }
    }
    else if (m_ActiveTab >= 2 && m_ActiveTab <= 5) {
        // Alphabetical tabs
        auto get_char = [this](int rom_idx) -> char {
            if (rom_idx < 0 || rom_idx >= m_RomCount) return '?';
            const char *name = m_RomFiles[rom_idx];
            if (name == nullptr || *name == '\0') return '?';
            const char *slash = strchr(name, '/');
            if (slash != nullptr) {
                name = slash + 1;
            }
            char c = *name;
            if (c >= 'a' && c <= 'z') c -= 32;
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
            return '#';
        };

        auto get_letter_idx = [](char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
            return 0;
        };

        int part = m_ActiveTab - 2;
        for (int i = 0; i < m_RomCount; i++) {
            char c = get_char(i);
            int idx = get_letter_idx(c);
            
            if (part == 0 && idx <= m_TabSplitK1) {
                m_FilteredIndices[m_FilteredCount++] = i;
            } else if (part == 1 && idx > m_TabSplitK1 && idx <= m_TabSplitK2) {
                m_FilteredIndices[m_FilteredCount++] = i;
            } else if (part == 2 && idx > m_TabSplitK2 && idx <= m_TabSplitK3) {
                m_FilteredIndices[m_FilteredCount++] = i;
            } else if (part == 3 && idx > m_TabSplitK3) {
                m_FilteredIndices[m_FilteredCount++] = i;
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
    if (m_ActiveTab > 0) {
        m_ActiveTab--;
    } else {
        m_ActiveTab = 5;
    }
    m_SelectedIndex = 0;
    BuildFilteredList();
    Update();
}

void COSDMenu::MoveRight() {
    if (m_ActiveTab < 5) {
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
