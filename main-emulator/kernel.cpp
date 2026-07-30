#include "kernel.h"
#include <video_utils.h>
#include <circle/usb/usbdevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/timer.h>
#include <circle/alloc.h>
#include <circle/font.h>
#include <circle/synchronize.h>
#include <stdio.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
// OSD colors are configured as 5-bit-per-channel values (0..31), but the
// framebuffer is RGB565. Expand green to 6-bit while packing to avoid tint shifts.
#define COLOR15(red, green, blue) \
    ((((red) & 0x1F) << 11 | ((((green) & 0x1F) * 63 / 31) << 5) | ((blue) & 0x1F)))
#define OSD_VERBOSE_REDRAW_LOG 0

// Global shared state
SharedState g_SharedState;
FATFS *g_pFileSystem = nullptr;

static u16 s_SplashBuf[640 * 480] __attribute__((aligned(64)));

static CKernel *s_pThis = nullptr;
static boolean s_Is3ButtonGame = TRUE;

struct OSDThemeColors {
    u16 screen_bg;
    u16 panel_bg;
    u16 panel_border;
    u16 title_text;
    u16 separator;
    u16 tab_active_bg;
    u16 tab_active_border;
    u16 tab_active_text;
    u16 tab_inactive_bg;
    u16 tab_inactive_border;
    u16 tab_inactive_text;
    u16 list_selected_text;
    u16 list_text;
    u16 list_selected_bg;
    u16 scroll_track;
    u16 scroll_thumb;
    u16 footer_text;
    u16 warning_text;
};

static const OSDThemeColors s_OSDThemeDefault = {
    COLOR15(0, 0, 0),      // screen_bg
    COLOR15(2, 3, 5),      // panel_bg
    COLOR15(8, 12, 16),    // panel_border
    COLOR15(22, 24, 26),   // title_text
    COLOR15(4, 6, 8),      // separator
    COLOR15(4, 10, 15),    // tab_active_bg
    COLOR15(10, 24, 28),   // tab_active_border
    COLOR15(24, 28, 28),   // tab_active_text
    COLOR15(2, 4, 6),      // tab_inactive_bg
    COLOR15(3, 7, 10),     // tab_inactive_border
    COLOR15(12, 14, 16),   // tab_inactive_text
    COLOR15(12, 24, 28),   // list_selected_text
    COLOR15(14, 16, 18),   // list_text
    COLOR15(4, 6, 9),      // list_selected_bg
    COLOR15(3, 4, 6),      // scroll_track
    COLOR15(8, 12, 16),    // scroll_thumb
    COLOR15(12, 14, 16),   // footer_text
    COLOR15(24, 14, 10)    // warning_text
};

static const OSDThemeColors s_OSDThemeGreenCRT = {
    COLOR15(0, 0, 0),      // screen_bg
    COLOR15(0, 1, 0),      // panel_bg
    COLOR15(5, 22, 5),     // panel_border
    COLOR15(16, 31, 16),   // title_text
    COLOR15(2, 10, 2),     // separator
    COLOR15(1, 7, 1),      // tab_active_bg
    COLOR15(10, 30, 10),   // tab_active_border
    COLOR15(18, 31, 18),   // tab_active_text
    COLOR15(0, 3, 0),      // tab_inactive_bg
    COLOR15(3, 12, 3),     // tab_inactive_border
    COLOR15(10, 22, 10),   // tab_inactive_text
    COLOR15(18, 31, 18),   // list_selected_text
    COLOR15(10, 20, 10),   // list_text
    COLOR15(0, 6, 0),      // list_selected_bg
    COLOR15(0, 4, 0),      // scroll_track
    COLOR15(6, 18, 6),     // scroll_thumb
    COLOR15(10, 20, 10),   // footer_text
    COLOR15(20, 31, 18)    // warning_text
};

static const OSDThemeColors s_OSDThemeGrayscale = {
    COLOR15(0, 0, 0),      // screen_bg
    COLOR15(2, 2, 2),      // panel_bg
    COLOR15(14, 14, 14),   // panel_border
    COLOR15(27, 27, 27),   // title_text
    COLOR15(8, 8, 8),      // separator
    COLOR15(8, 8, 8),      // tab_active_bg
    COLOR15(21, 21, 21),   // tab_active_border
    COLOR15(30, 30, 30),   // tab_active_text
    COLOR15(4, 4, 4),      // tab_inactive_bg
    COLOR15(10, 10, 10),   // tab_inactive_border
    COLOR15(17, 17, 17),   // tab_inactive_text
    COLOR15(28, 28, 28),   // list_selected_text
    COLOR15(18, 18, 18),   // list_text
    COLOR15(7, 7, 7),      // list_selected_bg
    COLOR15(4, 4, 4),      // scroll_track
    COLOR15(12, 12, 12),   // scroll_thumb
    COLOR15(16, 16, 16),   // footer_text
    COLOR15(24, 24, 24)    // warning_text
};

static OSDThemeColors s_OSDThemeActive = s_OSDThemeDefault;
static const OSDThemeColors *s_pOSDTheme = &s_OSDThemeActive;
static boolean s_bUseCustomOSDColors = FALSE;

static EmuMode s_SystemOrder[6] = { EmuMode_SNES, EmuMode_NES, EmuMode_SMS, EmuMode_PCE, EmuMode_MD, EmuMode_FAV };
static int s_NumSystems = 6;
static int s_SystemOrderIdx = 0;
static EmuMode s_MenuEmuMode = EmuMode_SNES;

static char *TrimToken(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
    return s;
}

static boolean ParseSystemToken(const char *token, EmuMode *outMode) {
    if (!token || !*token || !outMode) return FALSE;

    if (strstr(token, "snes") || strstr(token, "super nintendo")) {
        *outMode = EmuMode_SNES;
        return TRUE;
    }
    if (strstr(token, "nes") || strstr(token, "famicom") || strstr(token, "nintendo")) {
        *outMode = EmuMode_NES;
        return TRUE;
    }
    if (strstr(token, "md") || strstr(token, "megadrive") || strstr(token, "mega drive") || strstr(token, "genesis")) {
        *outMode = EmuMode_MD;
        return TRUE;
    }
    if (strstr(token, "sms") || strstr(token, "mastersystem") || strstr(token, "master system")) {
        *outMode = EmuMode_SMS;
        return TRUE;
    }
    if (strstr(token, "pce") || strstr(token, "pcengine") || strstr(token, "pc engine") || strstr(token, "turbografx") || strstr(token, "tg16")) {
        *outMode = EmuMode_PCE;
        return TRUE;
    }
    if (strstr(token, "fav") || strstr(token, "favorite") || strstr(token, "favorites")) {
        *outMode = EmuMode_FAV;
        return TRUE;
    }

    return FALSE;
}

static boolean ParseThemeToken(const char *token, const OSDThemeColors **outTheme) {
    if (!token || !*token || !outTheme) return FALSE;

    if (strstr(token, "default") || strstr(token, "classic")) {
        *outTheme = &s_OSDThemeDefault;
        return TRUE;
    }
    if (strstr(token, "green") || strstr(token, "crt") || strstr(token, "phosphor")) {
        *outTheme = &s_OSDThemeGreenCRT;
        return TRUE;
    }
    if (strstr(token, "gray") || strstr(token, "grey") || strstr(token, "grayscale") || strstr(token, "mono")) {
        *outTheme = &s_OSDThemeGrayscale;
        return TRUE;
    }

    return FALSE;
}

static boolean ParseHexDigit(char c, unsigned *out) {
    if (c >= '0' && c <= '9') {
        *out = (unsigned)(c - '0');
        return TRUE;
    }
    if (c >= 'a' && c <= 'f') {
        *out = 10u + (unsigned)(c - 'a');
        return TRUE;
    }
    if (c >= 'A' && c <= 'F') {
        *out = 10u + (unsigned)(c - 'A');
        return TRUE;
    }
    return FALSE;
}

static boolean ParseOSDColorValue(const char *value, u16 *outColor) {
    if (!value || !*value || !outColor) return FALSE;

    // Accept #RRGGBB
    if (value[0] == '#' && strlen(value) == 7) {
        unsigned nibbles[6];
        for (int i = 0; i < 6; i++) {
            if (!ParseHexDigit(value[i + 1], &nibbles[i])) return FALSE;
        }
        unsigned r8 = (nibbles[0] << 4) | nibbles[1];
        unsigned g8 = (nibbles[2] << 4) | nibbles[3];
        unsigned b8 = (nibbles[4] << 4) | nibbles[5];
        unsigned r5 = (r8 * 31 + 127) / 255;
        unsigned g5 = (g8 * 31 + 127) / 255;
        unsigned b5 = (b8 * 31 + 127) / 255;
        *outColor = COLOR15((int)r5, (int)g5, (int)b5);
        return TRUE;
    }

    // Accept 0x7FFF style RGB555 and convert to framebuffer RGB565.
    if ((value[0] == '0') && (value[1] == 'x' || value[1] == 'X')) {
        unsigned n = 0;
        for (int i = 2; value[i] != '\0'; i++) {
            unsigned d = 0;
            if (!ParseHexDigit(value[i], &d)) return FALSE;
            n = (n << 4) | d;
        }
        n &= 0x7FFF;
        unsigned r5 = (n >> 10) & 0x1F;
        unsigned g5 = (n >> 5) & 0x1F;
        unsigned b5 = n & 0x1F;
        *outColor = COLOR15((int)r5, (int)g5, (int)b5);
        return TRUE;
    }

    // Accept decimal RGB triplet: r,g,b (0..31 or 0..255)
    int r = 0, g = 0, b = 0;
    if (sscanf(value, " %d , %d , %d ", &r, &g, &b) == 3) {
        if (r < 0 || g < 0 || b < 0) return FALSE;
        if (r <= 31 && g <= 31 && b <= 31) {
            *outColor = COLOR15(r, g, b);
            return TRUE;
        }
        if (r <= 255 && g <= 255 && b <= 255) {
            int r5 = (r * 31 + 127) / 255;
            int g5 = (g * 31 + 127) / 255;
            int b5 = (b * 31 + 127) / 255;
            *outColor = COLOR15(r5, g5, b5);
            return TRUE;
        }
    }

    return FALSE;
}

static boolean ApplyOSDColorOverride(const char *key, u16 color) {
    if (!key || !*key) return FALSE;

    if (strcmp(key, "background") == 0 || strcmp(key, "screen_bg") == 0) {
        s_OSDThemeActive.screen_bg = color;
        return TRUE;
    }
    if (strcmp(key, "panel_background") == 0 || strcmp(key, "panel_bg") == 0 || strcmp(key, "menu_background") == 0) {
        s_OSDThemeActive.panel_bg = color;
        return TRUE;
    }
    if (strcmp(key, "border") == 0 || strcmp(key, "panel_border") == 0) {
        s_OSDThemeActive.panel_border = color;
        return TRUE;
    }
    if (strcmp(key, "title_text") == 0) {
        s_OSDThemeActive.title_text = color;
        return TRUE;
    }
    if (strcmp(key, "separator") == 0) {
        s_OSDThemeActive.separator = color;
        return TRUE;
    }
    if (strcmp(key, "tab_active_bg") == 0) {
        s_OSDThemeActive.tab_active_bg = color;
        return TRUE;
    }
    if (strcmp(key, "tab_active_border") == 0) {
        s_OSDThemeActive.tab_active_border = color;
        return TRUE;
    }
    if (strcmp(key, "tab_active_text") == 0) {
        s_OSDThemeActive.tab_active_text = color;
        return TRUE;
    }
    if (strcmp(key, "tab_inactive_bg") == 0) {
        s_OSDThemeActive.tab_inactive_bg = color;
        return TRUE;
    }
    if (strcmp(key, "tab_inactive_border") == 0) {
        s_OSDThemeActive.tab_inactive_border = color;
        return TRUE;
    }
    if (strcmp(key, "tab_inactive_text") == 0) {
        s_OSDThemeActive.tab_inactive_text = color;
        return TRUE;
    }
    if (strcmp(key, "list_selected_text") == 0) {
        s_OSDThemeActive.list_selected_text = color;
        return TRUE;
    }
    if (strcmp(key, "list_text") == 0) {
        s_OSDThemeActive.list_text = color;
        return TRUE;
    }
    if (strcmp(key, "list_selected_bg") == 0) {
        s_OSDThemeActive.list_selected_bg = color;
        return TRUE;
    }
    if (strcmp(key, "scroll_track") == 0) {
        s_OSDThemeActive.scroll_track = color;
        return TRUE;
    }
    if (strcmp(key, "scroll_thumb") == 0) {
        s_OSDThemeActive.scroll_thumb = color;
        return TRUE;
    }
    if (strcmp(key, "footer_text") == 0) {
        s_OSDThemeActive.footer_text = color;
        return TRUE;
    }
    if (strcmp(key, "warning_text") == 0) {
        s_OSDThemeActive.warning_text = color;
        return TRUE;
    }
    if (strcmp(key, "text") == 0) {
        s_OSDThemeActive.title_text = color;
        s_OSDThemeActive.tab_active_text = color;
        s_OSDThemeActive.tab_inactive_text = color;
        s_OSDThemeActive.list_selected_text = color;
        s_OSDThemeActive.list_text = color;
        s_OSDThemeActive.footer_text = color;
        return TRUE;
    }

    return FALSE;
}

static void HandleCycleTheme(FATFS *pFileSystem) {
    FIL file;
    int currentIdx = 0;
    const char *statePaths[] = { "SD:/osd_theme_state.txt", "SD:/roms/osd_theme_state.txt", nullptr };
    const char *stateReadPath = nullptr;

    for (int i = 0; statePaths[i] != nullptr; i++) {
        if (f_open(&file, statePaths[i], FA_READ) == FR_OK) {
            stateReadPath = statePaths[i];
            char buf[32];
            UINT bytesRead = 0;
            if (f_read(&file, buf, sizeof(buf) - 1, &bytesRead) == FR_OK && bytesRead > 0) {
                buf[bytesRead] = '\0';
                int val = -1;
                if (sscanf(buf, "%d", &val) == 1 && val >= 0 && val < 3) {
                    currentIdx = val;
                }
            }
            f_close(&file);
            break;
        }
    }

    switch (currentIdx) {
        case 0:
            s_OSDThemeActive = s_OSDThemeDefault;
            break;
        case 1:
            s_OSDThemeActive = s_OSDThemeGreenCRT;
            break;
        case 2:
            s_OSDThemeActive = s_OSDThemeGrayscale;
            break;
        default:
            s_OSDThemeActive = s_OSDThemeDefault;
            break;
    }
    s_bUseCustomOSDColors = FALSE;

    int nextIdx = (currentIdx + 1) % 3;
    const char *writePath = stateReadPath ? stateReadPath : "SD:/osd_theme_state.txt";
    if (f_open(&file, writePath, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d\n", nextIdx);
        UINT bytesWritten = 0;
        f_write(&file, buf, (UINT)len, &bytesWritten);
        f_close(&file);
    }
}

static void LoadOSDTheme(FATFS *pFileSystem) {
    FIL file;
    const char *configPaths[] = { "SD:/osd_theme.txt", "SD:/roms/osd_theme.txt", nullptr };
    const char *foundPath = nullptr;

    for (int i = 0; configPaths[i] != nullptr; i++) {
        if (f_open(&file, configPaths[i], FA_READ) == FR_OK) {
            foundPath = configPaths[i];
            break;
        }
    }

    s_bUseCustomOSDColors = FALSE;

    if (!foundPath) {
        s_OSDThemeActive = s_OSDThemeDefault;
        return; // Keep default theme when config file is missing.
    }

    char line[128];
    while (f_gets(line, sizeof(line), &file) != nullptr) {
        char *p = line;
        if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
            p += 3;
        }
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#' || *p == '\r' || *p == '\n' || (*p == '/' && *(p + 1) == '/')) {
            continue;
        }

        char lower[128];
        int len = 0;
        for (int i = 0; p[i] && p[i] != '\r' && p[i] != '\n' && p[i] != '#'; i++) {
            char c = p[i];
            if (c >= 'A' && c <= 'Z') c += 32;
            lower[len++] = c;
        }
        lower[len] = '\0';

        for (char *token = lower; token != nullptr && *token != '\0'; ) {
            char *next = nullptr;
            for (char *q = token; *q != '\0'; q++) {
                if (*q == ',' || *q == ';' || *q == '|') {
                    *q = '\0';
                    next = q + 1;
                    break;
                }
            }

            char *trimmed = TrimToken(token);
            if (strcmp(trimmed, "custom") == 0) {
                // Custom uses default palette as base, then applies osd_colors.txt overrides.
                s_OSDThemeActive = s_OSDThemeDefault;
                s_bUseCustomOSDColors = TRUE;
                f_close(&file);
                return;
            }
            if (strcmp(trimmed, "all") == 0 || strcmp(trimmed, "all_colors") == 0 ||
                strcmp(trimmed, "all-colors") == 0 || strcmp(trimmed, "allcolors") == 0 ||
                strcmp(trimmed, "cycle") == 0 || strcmp(trimmed, "rotate") == 0 ||
                strcmp(trimmed, "all_themes") == 0 || strcmp(trimmed, "all-themes") == 0) {
                f_close(&file);
                HandleCycleTheme(pFileSystem);
                return;
            }
            const OSDThemeColors *theme = nullptr;
            if (ParseThemeToken(trimmed, &theme)) {
                s_OSDThemeActive = *theme;
                s_bUseCustomOSDColors = FALSE;
                f_close(&file);
                return;
            }

            token = next;
        }
    }

    f_close(&file);
    s_OSDThemeActive = s_OSDThemeDefault;
    s_bUseCustomOSDColors = FALSE;
}

static void LoadOSDColorOverrides(FATFS *pFileSystem) {
    FIL file;
    const char *configPaths[] = { "SD:/osd_colors.txt", "SD:/roms/osd_colors.txt", nullptr };
    const char *foundPath = nullptr;

    for (int i = 0; configPaths[i] != nullptr; i++) {
        if (f_open(&file, configPaths[i], FA_READ) == FR_OK) {
            foundPath = configPaths[i];
            break;
        }
    }

    if (!foundPath) return;

    char line[160];
    while (f_gets(line, sizeof(line), &file) != nullptr) {
        char *p = line;
        if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
            p += 3;
        }
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#' || *p == '\r' || *p == '\n' || (*p == '/' && *(p + 1) == '/')) {
            continue;
        }

        char *sep = strchr(p, '=');
        if (!sep) sep = strchr(p, ':');
        if (!sep) continue;
        *sep = '\0';

        char *key = TrimToken(p);
        char *value = TrimToken(sep + 1);

        // Preserve #RRGGBB values; only strip inline comments for other formats.
        if (value[0] == '#') {
            if (strlen(value) > 7) {
                value[7] = '\0';
            }
        } else {
            for (int i = 0; value[i] != '\0'; i++) {
                if (value[i] == '#') {
                    value[i] = '\0';
                    break;
                }
                if (value[i] == '/' && value[i + 1] == '/') {
                    value[i] = '\0';
                    break;
                }
            }
        }
        value = TrimToken(value);
        if (*key == '\0' || *value == '\0') continue;

        // Normalize key to lowercase.
        for (int i = 0; key[i] != '\0'; i++) {
            if (key[i] >= 'A' && key[i] <= 'Z') key[i] += ('a' - 'A');
        }

        u16 color = 0;
        if (!ParseOSDColorValue(value, &color)) continue;
        ApplyOSDColorOverride(key, color);
    }

    f_close(&file);
}

static void LoadSystemOrder(FATFS *pFileSystem) {
    FIL file;
    const char *configPaths[] = { "SD:/system_order.txt", "SD:/roms/system_order.txt", nullptr };
    const char *foundPath = nullptr;

    for (int i = 0; configPaths[i] != nullptr; i++) {
        if (f_open(&file, configPaths[i], FA_READ) == FR_OK) {
            foundPath = configPaths[i];
            break;
        }
    }

    if (!foundPath) return; // Use default system order if file doesn't exist

    EmuMode newOrder[6];
    int count = 0;
    char line[128];

    while (f_gets(line, sizeof(line), &file) != nullptr && count < 6) {
        char *p = line;
        // Skip UTF-8 BOM if present at start of line/file.
        if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
            p += 3;
        }
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '\0' || *p == '#' || *p == '\r' || *p == '\n' || (*p == '/' && *(p+1) == '/')) {
            continue;
        }

        char lower[128];
        int len = 0;
        for (int i = 0; p[i] && p[i] != '\r' && p[i] != '\n' && p[i] != '#'; i++) {
            char c = p[i];
            if (c >= 'A' && c <= 'Z') c += 32;
            lower[len++] = c;
        }
        while (len > 0 && (lower[len-1] == ' ' || lower[len-1] == '\t')) len--;
        lower[len] = '\0';

        if (len == 0) {
            continue;
        }

        // Support one system per line and CSV/semicolon/pipe lists on a line.
        for (char *token = lower; token != nullptr && *token != '\0' && count < 6; ) {
            char *next = nullptr;
            for (char *q = token; *q != '\0'; q++) {
                if (*q == ',' || *q == ';' || *q == '|') {
                    *q = '\0';
                    next = q + 1;
                    break;
                }
            }

            char *trimmed = TrimToken(token);
            EmuMode mode;
            if (ParseSystemToken(trimmed, &mode)) {
                boolean alreadyAdded = FALSE;
                for (int i = 0; i < count; i++) {
                    if (newOrder[i] == mode) {
                        alreadyAdded = TRUE;
                        break;
                    }
                }

                if (!alreadyAdded) {
                    newOrder[count++] = mode;
                }
            }

            token = next;
        }
    }

    f_close(&file);

    if (count > 0) {
        const EmuMode allModes[6] = { EmuMode_SNES, EmuMode_NES, EmuMode_SMS, EmuMode_PCE, EmuMode_MD, EmuMode_FAV };
        for (int m = 0; m < 6; m++) {
            boolean present = FALSE;
            for (int i = 0; i < count; i++) {
                if (newOrder[i] == allModes[m]) {
                    present = TRUE;
                    break;
                }
            }
            if (!present && count < 6) {
                newOrder[count++] = allModes[m];
            }
        }

        for (int i = 0; i < count; i++) {
            s_SystemOrder[i] = newOrder[i];
            g_SharedState.system_order[i] = newOrder[i];
        }
        s_NumSystems = count;
        g_SharedState.num_systems = count;
        s_SystemOrderIdx = 0;
    }
}

static boolean strcontains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return FALSE;
    for (int i = 0; haystack[i]; i++) {
        int j = 0;
        while (haystack[i + j] && needle[j] && haystack[i + j] == needle[j]) {
            j++;
        }
        if (!needle[j]) {
            return TRUE;
        }
    }
    return FALSE;
}

boolean is6ButtonGame(const char *pRomName) {
    if (!pRomName) return FALSE;
    char name[256];
    int len = 0;
    for (int i = 0; pRomName[i] && len < 255; i++) {
        char c = pRomName[i];
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
        name[len++] = c;
    }
    name[len] = '\0';
    
    if (strcontains(name, "(6b)") || strcontains(name, "(6-button)") || strcontains(name, "(6button)")) {
        return TRUE;
    }
    if (strcontains(name, "(3b)") || strcontains(name, "(3-button)") || strcontains(name, "(3button)")) {
        return FALSE;
    }
    
    if (strcontains(name, "street fighter") ||
        strcontains(name, "sf2") ||
        strcontains(name, "mortal kombat") ||
        strcontains(name, "mk2") ||
        strcontains(name, "mk3") ||
        strcontains(name, "umk3") ||
        strcontains(name, "comix zone") ||
        strcontains(name, "ranger x") ||
        strcontains(name, "lost vikings") ||
        strcontains(name, "streets of rage 3") ||
        strcontains(name, "bare knuckle 3") ||
        strcontains(name, "eternal champions") ||
        strcontains(name, "virtua fighter") ||
        strcontains(name, "world heroes") ||
        strcontains(name, "primal rage") ||
        strcontains(name, "clayfighter") ||
        strcontains(name, "fatal fury") ||
        strcontains(name, "samurai shodown") ||
        strcontains(name, "samurai spirits") ||
        strcontains(name, "art of fighting") ||
        strcontains(name, "yu yu hakusho") ||
        strcontains(name, "justice league") ||
        strcontains(name, "weaponlord") ||
        strcontains(name, "wwf") ||
        strcontains(name, "batman forever") ||
        strcontains(name, "shinobi iii") ||
        strcontains(name, "after burner") ||
        strcontains(name, "doom") ||
        strcontains(name, "star wars arcade") ||
        strcontains(name, "toejam & earl") ||
        strcontains(name, "pitfall") ||
        strcontains(name, "forgotten worlds") ||
        strcontains(name, "story of thor") ||
        strcontains(name, "beyond oasis") ||
        strcontains(name, "metal head") ||
        strcontains(name, "marsupilami") ||
        strcontains(name, "outrunners") ||
        strcontains(name, "duke nukem") ||
        strcontains(name, "bruce lee")) {
        return TRUE;
    }
    return FALSE;
}

// Helper drawing utilities
static void DrawRect(u16 *pBuffer, u32 nPitch, int x1, int y1, int x2, int y2, u16 color) {
    int width = x2 - x1 + 1;
    if (width <= 0) return;
    u16 *row = pBuffer + y1 * nPitch + x1;
    for (int y = y1; y <= y2; y++) {
        for (int x = 0; x < width; x++) {
            row[x] = color;
        }
        row += nPitch;
    }
}

static void DrawBox(u16 *pBuffer, u32 nPitch, int x1, int y1, int x2, int y2, u16 color, int border_width = 1) {
    for (int b = 0; b < border_width; b++) {
        int ty = y1 + b;
        int by = y2 - b;
        u16 *top_row = pBuffer + ty * nPitch;
        u16 *bottom_row = pBuffer + by * nPitch;
        // Top and bottom lines
        for (int x = x1 + b; x <= x2 - b; x++) {
            top_row[x] = color;
            bottom_row[x] = color;
        }
        // Left and right lines
        int lx = x1 + b;
        int rx = x2 - b;
        u16 *row = pBuffer + ty * nPitch;
        for (int y = ty; y <= by; y++) {
            row[lx] = color;
            row[rx] = color;
            row += nPitch;
        }
    }
}

static void DrawChar(u16 *pBuffer, u32 nPitch, char c, int x, int y, u16 fg, u16 bg) {
    unsigned char uc = (unsigned char)c;
    if (uc < Font8x16.first_char || uc > Font8x16.last_char) return;
    const u8 *char_data = (const u8 *)Font8x16.data + (uc - Font8x16.first_char) * Font8x16.height;
    u16 *dest_row = pBuffer + y * nPitch + x;
    for (unsigned row = 0; row < Font8x16.height; row++) {
        u8 pixels = char_data[row];
        for (unsigned col = 0; col < Font8x16.width; col++) {
            if (pixels & (0x80 >> col)) {
                dest_row[col] = fg;
            } else if (bg != 0) {
                dest_row[col] = bg;
            }
        }
        dest_row += nPitch;
    }
}

static void DrawString(u16 *pBuffer, u32 nPitch, const char *str, int x, int y, u16 fg, u16 bg) {
    int cur_x = x;
    while (*str) {
        DrawChar(pBuffer, nPitch, *str, cur_x, y, fg, bg);
        cur_x += Font8x16.width;
        str++;
    }
}

// CEmulatorMultiCore Implementation
CEmulatorMultiCore::CEmulatorMultiCore(CMemorySystem *pMemorySystem, CKernel *pKernel)
    : CMultiCoreSupport(pMemorySystem),
      m_pKernel(pKernel)
{
}

#if AARCH == 32
static void vfp_init(void) {
    // Coprocessor Access Control Register
    unsigned nCACR;
    __asm volatile ("mrc p15, 0, %0, c1, c0, 2" : "=r" (nCACR));
    nCACR |= 3 << 20;   // cp10 (single precision)
    nCACR |= 3 << 22;   // cp11 (double precision)
    __asm volatile ("mcr p15, 0, %0, c1, c0, 2" : : "r" (nCACR));
    __asm volatile ("isb" ::: "memory");

#define VFP_FPEXC_EN    (1 << 30)
    __asm volatile ("fmxr fpexc, %0" : : "r" (VFP_FPEXC_EN));

#define VFP_FPSCR_FZ    (1 << 24)    // enable Flush-to-zero mode
#define VFP_FPSCR_DN    (1 << 25)    // enable Default NaN mode
    __asm volatile ("fmxr fpscr, %0" : : "r" (VFP_FPSCR_FZ | VFP_FPSCR_DN));
}
#endif

void CEmulatorMultiCore::Run(unsigned nCore) {
#if AARCH == 32
    vfp_init();
#endif

    switch (nCore) {
        case 0:
            m_pKernel->RunOrchestrator();
            break;
        case 1:
            m_pKernel->RunVideoDomain();
            break;
        case 2:
            m_pKernel->RunAudioDomain();
            break;
        case 3:
            m_pKernel->RunInputDomain();
            break;
    }
}

// CKernel Implementation
CKernel::CKernel(void)
    : m_Screen(SCREEN_WIDTH, SCREEN_HEIGHT),
      m_Timer(&m_Interrupt),
      m_Logger(m_Options.GetLogLevel(), &m_Timer),
      m_EMMC(&m_Interrupt, &m_Timer, &m_ActLED),
      m_USBHCI(&m_Interrupt, &m_Timer, TRUE),
      m_MultiCore(CMemorySystem::Get(), this),
      m_pKeyboard(nullptr),
      m_pOSDMenu(nullptr),
      m_pSNESOrchestrator(nullptr),
      m_pMDOrchestrator(nullptr),
      m_pNESOrchestrator(nullptr),
      m_pPCEOrchestrator(nullptr),
      m_pSMSOrchestrator(nullptr),
      m_ShutdownMode(ShutdownNone)
{
    s_pThis = this;
    m_pGamePad[0] = nullptr;
    m_pGamePad[1] = nullptr;
    m_ActLED.Blink(5);
}

CKernel::~CKernel(void) {
    if (m_pSMSOrchestrator != nullptr) {
        delete m_pSMSOrchestrator;
        m_pSMSOrchestrator = nullptr;
    }
    if (m_pPCEOrchestrator != nullptr) {
        delete m_pPCEOrchestrator;
        m_pPCEOrchestrator = nullptr;
    }
    if (m_pNESOrchestrator != nullptr) {
        delete m_pNESOrchestrator;
        m_pNESOrchestrator = nullptr;
    }
    if (m_pMDOrchestrator != nullptr) {
        delete m_pMDOrchestrator;
        m_pMDOrchestrator = nullptr;
    }
    if (m_pSNESOrchestrator != nullptr) {
        delete m_pSNESOrchestrator;
        m_pSNESOrchestrator = nullptr;
    }
    if (m_pOSDMenu != nullptr) {
        delete m_pOSDMenu;
        m_pOSDMenu = nullptr;
    }

    if (m_ShutdownMode == ShutdownHalt) {
        m_PowerEnPin.Write(LOW);
        m_LedPin.AssignPin(14);
        m_LedPin.SetMode(GPIOModeOutput);
        m_LedPin.Write(LOW);
    }
    s_pThis = nullptr;
}

boolean CKernel::Initialize(void) {
    boolean bOK = TRUE;

    // Check if serial logging is enabled in cmdline.txt (e.g. logdev=ttyS1)
    boolean bEnableSerial = FALSE;
    const char *pLogDevice = m_Options.GetLogDevice();
    if (pLogDevice != nullptr && strncmp(pLogDevice, "ttyS", 4) == 0) {
        bEnableSerial = TRUE;
    }

    if (bEnableSerial) {
        bOK = m_Serial.Initialize(115200);
        if (bOK) {
            bOK = m_Logger.Initialize(&m_Serial);
        }
    } else {
        bOK = m_Logger.Initialize(nullptr);
    }

    if (bOK) {
        m_Logger.Write("kernel", LogNotice, "MEGA-SNES Pi Metal Kernel Initializing...");
    }

    // Initialize safe shutdown GPIO pins for Retroflag PiCase as early as possible
    m_PowerEnPin.AssignPin(4);
    m_PowerEnPin.SetMode(GPIOModeOutput);
    m_PowerEnPin.Write(HIGH);

    if (!bEnableSerial) {
        // If serial logging is disabled, BCM 14 is configured as general output to drive the LED
        m_LedPin.AssignPin(14);
        m_LedPin.SetMode(GPIOModeOutput);
        m_LedPin.Write(HIGH);
    }

    m_PowerPin.AssignPin(3);
    m_PowerPin.SetMode(GPIOModeInput);

    m_ResetPin.AssignPin(2);
    m_ResetPin.SetMode(GPIOModeInput);

    // 3. Initialize Screen (non-fatal if it fails)
    boolean bScreenOK = FALSE;
    if (bOK) {
        bScreenOK = m_Screen.Initialize();
        if (!bScreenOK) {
            m_Logger.Write("kernel", LogWarning, "Screen device initialization failed!");
        } else {
            m_Logger.Write("kernel", LogNotice, "Screen device initialized successfully.");
        }
    }

    // 4. Initialize Core Subsystems
    if (bOK) {
        bOK = m_Interrupt.Initialize();
        if (!bOK) m_Logger.Write("kernel", LogError, "Interrupt system initialization failed!");
    }
    if (bOK) {
        bOK = m_Timer.Initialize();
        if (!bOK) m_Logger.Write("kernel", LogError, "Timer initialization failed!");
    }
    if (bOK) {
        bOK = m_EMMC.Initialize();
        if (!bOK) m_Logger.Write("kernel", LogError, "EMMC driver initialization failed!");
    }
    if (bOK) {
        bOK = m_USBHCI.Initialize();
        if (!bOK) m_Logger.Write("kernel", LogError, "USB HCI initialization failed!");
    }
    if (bOK) {
        bOK = m_MultiCore.Initialize();
        if (!bOK) m_Logger.Write("kernel", LogError, "MultiCore system initialization failed!");
    }

    return bOK;
}

TShutdownMode CKernel::Run(void) {
    m_Logger.Write("kernel", LogNotice, "MD-NES-SNES Baremetal Emulator Booting...");
    
    // Start secondary cores
    m_MultiCore.Run(0);

    return m_ShutdownMode;
}

void CKernel::RunOrchestrator() {
    m_Logger.Write("orchestrator", LogNotice, "Core 0: Orchestrator Active");

    // Mount Elm ChaN's FAT Filesystem on SD card
    FRESULT mount_res = f_mount(&m_FileSystem, "SD:", 1);
    if (mount_res != FR_OK) {
        m_Logger.Write("orchestrator", LogPanic, "Cannot mount Elm ChaN fatfs filesystem on SD card (error %d)", mount_res);
        return;
    }

    g_pFileSystem = &m_FileSystem;

    g_SharedState.num_systems = s_NumSystems;
    for (int i = 0; i < s_NumSystems; i++) {
        g_SharedState.system_order[i] = s_SystemOrder[i];
    }

    LoadSystemOrder(&m_FileSystem);
    LoadOSDTheme(&m_FileSystem);
    if (s_bUseCustomOSDColors) {
        LoadOSDColorOverrides(&m_FileSystem);
    }
    g_SharedState.active_emu_mode = s_SystemOrder[0];

    m_pOSDMenu = new COSDMenu(&m_FileSystem);
    m_pOSDMenu->Initialize();

    m_pSNESOrchestrator = new CSNESOrchestrator(&m_FileSystem);
    m_pSNESOrchestrator->Initialize();

    m_pMDOrchestrator = new CMDOrchestrator(&m_FileSystem);
    m_pMDOrchestrator->Initialize();

    m_pNESOrchestrator = new CNESOrchestrator(&m_FileSystem);
    m_pNESOrchestrator->Initialize();

    m_pPCEOrchestrator = new CPCEOrchestrator(&m_FileSystem);
    m_pPCEOrchestrator->Initialize();

    m_pSMSOrchestrator = new CSMSOrchestrator(&m_FileSystem);
    m_pSMSOrchestrator->Initialize();

    // Turn activity LED OFF once the emulator has fully booted (HDD LED style)
    m_ActLED.Off();

    g_SharedState.in_menu = TRUE;
    g_SharedState.rom_loading = FALSE;
    g_SharedState.emu_write_idx = 0;
    g_SharedState.emu_read_idx = 0;
    g_SharedState.start_line[0] = 0;
    g_SharedState.game_w[0] = 256;
    g_SharedState.game_h[0] = 224;
    g_SharedState.start_line[1] = 0;
    g_SharedState.game_w[1] = 256;
    g_SharedState.game_h[1] = 224;

    static boolean just_entered_menu = TRUE;

    while (1) {
        // Check safe shutdown / reset GPIO pins for Retroflag PiCase (every 100ms to reduce overhead/bus lag)
        {
            static u64 last_button_check = 0;
            u64 now = CTimer::GetClockTicks64();
            if (now - last_button_check >= 100000) { // 100ms (100,000 microseconds)
                last_button_check = now;
                if (m_PowerPin.Read() == LOW) {
                    m_Logger.Write("orchestrator", LogNotice, "Safe shutdown signal detected (Power Button LOW). Shutting down...");
                    m_ShutdownMode = ShutdownHalt;
                    CTimer::SimpleMsDelay(2000); // Give user time to see OSD message
                    break;
                }
                if (m_ResetPin.Read() == LOW) {
                    m_Logger.Write("orchestrator", LogNotice, "Reboot signal detected (Reset Button LOW). Rebooting...");
                    m_ShutdownMode = ShutdownReboot;
                    CTimer::SimpleMsDelay(2000); // Give user time to see OSD message
                    break;
                }
            }
        }

        // Periodically check SoC temperature and throttle CPU speed if necessary (every 4 seconds)
        static u64 last_temp_check = 0;
        u64 now = CTimer::GetClockTicks64();
        if (now - last_temp_check >= 4000000) {
            m_CPUThrottle.SetOnTemperature();
            last_temp_check = now;
        }

        if (g_SharedState.in_menu) {
            // OSD Menu navigation using gamepad/keyboard pad state with auto scrolling
            static u16 prev_pad1 = 0;
            static boolean start_released = TRUE;
            static u64 menu_enter_time = 0;
            u16 pad1 = g_SharedState.pad1 | g_SharedState.pad2;
            
            if (just_entered_menu) {
                prev_pad1 = pad1;
                start_released = FALSE;
                just_entered_menu = FALSE;
                menu_enter_time = CTimer::GetClockTicks64();
            }

            if ((pad1 & ((1 << 6) | (1 << 7))) == 0) {
                start_released = TRUE;
            }
            
            u16 pressed = pad1 & ~prev_pad1;
            prev_pad1 = pad1;

            boolean isUpHeld = (pad1 & (1 << 0)) != 0;
            boolean isDownHeld = (pad1 & (1 << 1)) != 0;

            int repeatKey = 0;
            if (isUpHeld) {
                repeatKey = 1; // UP
            } else if (isDownHeld) {
                repeatKey = 2; // DOWN
            }

            static int activeRepeatKey = 0;
            static u64 lastRepeatTime = 0;
            static u64 repeatKeyStartTime = 0;
            static boolean repeatPhase = FALSE;

            boolean doUp = (pressed & (1 << 0)) != 0;
            boolean doDown = (pressed & (1 << 1)) != 0;

            if (repeatKey != 0) {
                if (activeRepeatKey != repeatKey) {
                    activeRepeatKey = repeatKey;
                    lastRepeatTime = CTimer::GetClockTicks64();
                    repeatKeyStartTime = lastRepeatTime;
                    repeatPhase = FALSE;
                } else {
                    u64 now = CTimer::GetClockTicks64();
                    u64 elapsedMs = (now - lastRepeatTime) / 1000;
                    u64 heldTotalMs = (now - repeatKeyStartTime) / 1000;
                    
                    boolean isLongList = g_SharedState.menu_num_lines > 17;
                    boolean isFastScroll = isLongList && (heldTotalMs >= 4000);
                    
                    u64 threshold = repeatPhase ? (isFastScroll ? 40 : 80) : 400;
                    if (elapsedMs >= threshold) {
                        if (repeatKey == 1) doUp = TRUE;
                        if (repeatKey == 2) doDown = TRUE;
                        repeatPhase = TRUE;
                        lastRepeatTime = CTimer::GetClockTicks64();
                    }
                }
            } else {
                activeRepeatKey = 0;
                repeatPhase = FALSE;
            }

            boolean doLeft = (pressed & (1 << 2)) != 0;
            boolean doRight = (pressed & (1 << 3)) != 0;
            boolean doX = (pressed & (1 << 6)) != 0; // SNES X for Favorite
            boolean doY = (pressed & (1 << 7)) != 0; // SNES Y for Unfavorite
            boolean doL = (pressed & (1 << 8)) != 0; // SNES L to switch systems
            boolean doR = (pressed & (1 << 9)) != 0; // SNES R to switch systems

            if (doUp) {
                m_pOSDMenu->MoveUp();
            }
            if (doDown) {
                m_pOSDMenu->MoveDown();
            }
            if (doLeft) {
                m_pOSDMenu->MoveLeft();
            }
            if (doRight) {
                m_pOSDMenu->MoveRight();
            }
            if (doX) {
                if (pad1 & (1 << 11)) { // SELECT + X -> Rescan SD card & rebuild library cache
                    m_pOSDMenu->RebuildLibrary();
                } else {
                    m_pOSDMenu->FavoriteCurrent();
                }
            }
            if (doY) {
                m_pOSDMenu->UnfavoriteCurrent();
            }
            if (doL || doR) {
                if (doL) {
                    s_SystemOrderIdx = (s_SystemOrderIdx + s_NumSystems - 1) % s_NumSystems;
                } else {
                    s_SystemOrderIdx = (s_SystemOrderIdx + 1) % s_NumSystems;
                }
                g_SharedState.active_emu_mode = s_SystemOrder[s_SystemOrderIdx];
                m_pOSDMenu->OnEmuModeChanged();
            }

            u64 now_ticks = CTimer::GetClockTicks64();
            boolean is_locked_out = ((now_ticks - menu_enter_time) / 1000) < 2000; // 2 seconds lockout

            if (!is_locked_out && (pressed & ((1 << 4) | (1 << 5) | (1 << 10))) && start_released) { // SNES A, B or Start -> Select ROM
                const char *pRomName = m_pOSDMenu->GetSelectedRom();
                unsigned nRomSize = m_pOSDMenu->GetSelectedRomSize();
                COSDMenu::RomSystem romSys = m_pOSDMenu->GetSelectedRomSystem();
                if (pRomName) {
                    char fullPath[256];
                    snprintf(fullPath, sizeof(fullPath), "SD:/roms/%s", pRomName);
                    // Clear emulator frame buffer to black to prevent previous game flash
                    memset((void *)g_SharedState.emu_frame_buffer, 0, sizeof(g_SharedState.emu_frame_buffer));
                    g_SharedState.emu_write_idx = 0;
                    g_SharedState.emu_read_idx = 0;

                    s_MenuEmuMode = g_SharedState.active_emu_mode;
                    EmuMode targetMode = s_MenuEmuMode;
                    if (targetMode == EmuMode_FAV) {
                        if (romSys == COSDMenu::RomSystem_SNES) targetMode = EmuMode_SNES;
                        else if (romSys == COSDMenu::RomSystem_MD || romSys == COSDMenu::RomSystem_MCD) targetMode = EmuMode_MD;
                        else if (romSys == COSDMenu::RomSystem_NES) targetMode = EmuMode_NES;
                        else if (romSys == COSDMenu::RomSystem_SMS) targetMode = EmuMode_SMS;
                        else targetMode = EmuMode_PCE;
                    }

                    if (targetMode == EmuMode_SNES) {
                        g_SharedState.start_line[0] = 0;
                        g_SharedState.game_w[0] = 256;
                        g_SharedState.game_h[0] = 224;
                        g_SharedState.start_line[1] = 0;
                        g_SharedState.game_w[1] = 256;
                        g_SharedState.game_h[1] = 224;
                    } else if (targetMode == EmuMode_MD) {
                        g_SharedState.start_line[0] = 8;
                        g_SharedState.game_w[0] = 320;
                        g_SharedState.game_h[0] = 224;
                        g_SharedState.start_line[1] = 8;
                        g_SharedState.game_w[1] = 320;
                        g_SharedState.game_h[1] = 224;
                    } else if (targetMode == EmuMode_NES) {
                        g_SharedState.start_line[0] = 0;
                        g_SharedState.game_w[0] = 256;
                        g_SharedState.game_h[0] = 240;
                        g_SharedState.start_line[1] = 0;
                        g_SharedState.game_w[1] = 256;
                        g_SharedState.game_h[1] = 240;
                    } else if (targetMode == EmuMode_SMS) {
                        g_SharedState.start_line[0] = 8;
                        g_SharedState.game_w[0] = 256;
                        g_SharedState.game_h[0] = 224;
                        g_SharedState.start_line[1] = 8;
                        g_SharedState.game_w[1] = 256;
                        g_SharedState.game_h[1] = 224;
                    } else { // EmuMode_PCE
                        g_SharedState.start_line[0] = 0;
                        g_SharedState.game_w[0] = 256;
                        g_SharedState.game_h[0] = 240;
                        g_SharedState.start_line[1] = 0;
                        g_SharedState.game_w[1] = 256;
                        g_SharedState.game_h[1] = 240;
                    }

                    g_SharedState.active_emu_mode = targetMode;

                    g_SharedState.video_frame_ready = FALSE;
                    g_SharedState.rom_loading = TRUE;
                    DataMemBarrier();
                    CTimer::SimpleMsDelay(50); // Delay to let Video Core render the LOADING box

                    boolean loaded = FALSE;
                    if (targetMode == EmuMode_SNES) {
                        loaded = m_pSNESOrchestrator->LoadROM(fullPath, nRomSize);
                    } else if (targetMode == EmuMode_MD) {
                        s_Is3ButtonGame = !is6ButtonGame(pRomName);
                        loaded = m_pMDOrchestrator->LoadROM(fullPath, nRomSize);
                    } else if (targetMode == EmuMode_NES) {
                        loaded = m_pNESOrchestrator->LoadROM(fullPath, nRomSize);
                    } else if (targetMode == EmuMode_SMS) {
                        loaded = m_pSMSOrchestrator->LoadROM(fullPath, nRomSize);
                    } else { // EmuMode_PCE
                        loaded = m_pPCEOrchestrator->LoadROM(fullPath, nRomSize);
                    }

                    g_SharedState.rom_loading = FALSE;
                    DataMemBarrier();

                    if (loaded) {
                        g_SharedState.audio_ring_buffer.Init();
                        g_SharedState.in_menu = FALSE;
                        g_SharedState.escape_pressed = FALSE;
                        just_entered_menu = TRUE;
                    }
                }
            }
            CTimer::SimpleMsDelay(10);
        } else {
            // Run emulator frame based on active system
            if (g_SharedState.active_emu_mode == EmuMode_SNES) {
                m_pSNESOrchestrator->RunFrame();
            } else if (g_SharedState.active_emu_mode == EmuMode_MD) {
                m_pMDOrchestrator->RunFrame();
            } else if (g_SharedState.active_emu_mode == EmuMode_NES) {
                m_pNESOrchestrator->RunFrame();
            } else if (g_SharedState.active_emu_mode == EmuMode_SMS) {
                m_pSMSOrchestrator->RunFrame();
            } else {
                m_pPCEOrchestrator->RunFrame();
            }

            // Check if user requested return to OSD menu
            if (g_SharedState.escape_pressed) {
                g_SharedState.in_menu = TRUE;
                g_SharedState.escape_pressed = FALSE;
                g_SharedState.active_emu_mode = s_MenuEmuMode;
                m_pOSDMenu->OnEmuModeChanged();
                just_entered_menu = TRUE;
            }

            // Check if save or load state is requested
            if (g_SharedState.save_state_requested) {
                g_SharedState.save_state_requested = FALSE;
                if (g_SharedState.active_emu_mode == EmuMode_SNES) {
                    m_pSNESOrchestrator->SaveState(0);
                } else if (g_SharedState.active_emu_mode == EmuMode_MD) {
                    m_pMDOrchestrator->SaveState(0);
                } else if (g_SharedState.active_emu_mode == EmuMode_NES) {
                    m_pNESOrchestrator->SaveState(0);
                } else if (g_SharedState.active_emu_mode == EmuMode_SMS) {
                    m_pSMSOrchestrator->SaveState(0);
                } else {
                    m_pPCEOrchestrator->SaveState(0);
                }
                // Blink the activity LED 3 times quickly to confirm save
                m_ActLED.Blink(3, 50, 50);
            }
            if (g_SharedState.load_state_requested) {
                g_SharedState.load_state_requested = FALSE;
                if (g_SharedState.active_emu_mode == EmuMode_SNES) {
                    m_pSNESOrchestrator->LoadState(0);
                } else if (g_SharedState.active_emu_mode == EmuMode_MD) {
                    m_pMDOrchestrator->LoadState(0);
                } else if (g_SharedState.active_emu_mode == EmuMode_NES) {
                    m_pNESOrchestrator->LoadState(0);
                } else if (g_SharedState.active_emu_mode == EmuMode_SMS) {
                    m_pSMSOrchestrator->LoadState(0);
                } else {
                    m_pPCEOrchestrator->LoadState(0);
                }
                // Blink the activity LED 3 times quickly to confirm load
                m_ActLED.Blink(3, 50, 50);
            }
            if (g_SharedState.rewind_requested) {
                g_SharedState.rewind_requested = FALSE;
                if (g_SharedState.active_emu_mode == EmuMode_SNES) {
                    m_pSNESOrchestrator->RewindState();
                } else if (g_SharedState.active_emu_mode == EmuMode_MD) {
                    m_pMDOrchestrator->RewindState();
                } else if (g_SharedState.active_emu_mode == EmuMode_NES) {
                    m_pNESOrchestrator->RewindState();
                } else if (g_SharedState.active_emu_mode == EmuMode_SMS) {
                    m_pSMSOrchestrator->RewindState();
                } else {
                    m_pPCEOrchestrator->RewindState();
                }
            }

            // Lock to 60/50 FPS (using microsecond precision ticks depending on active ROM region)
            s64 frame_time = 16666; // default 60 FPS (16.666 ms)
            if (g_SharedState.active_emu_mode == EmuMode_SNES) {
                if (m_pSNESOrchestrator->IsPAL()) {
                    frame_time = 20000; // 50 FPS for PAL (20 ms)
                }
            } else if (g_SharedState.active_emu_mode == EmuMode_MD) {
                if (m_pMDOrchestrator->IsPAL()) {
                    frame_time = 20000; // 50 FPS for PAL (20 ms)
                }
            } else if (g_SharedState.active_emu_mode == EmuMode_NES) {
                if (m_pNESOrchestrator->IsPAL()) {
                    frame_time = 20000; // 50 FPS for PAL (20 ms)
                }
            } else if (g_SharedState.active_emu_mode == EmuMode_SMS) {
                if (m_pSMSOrchestrator->IsPAL()) {
                    frame_time = 20000; // 50 FPS for PAL (20 ms)
                }
            } else {
                if (m_pPCEOrchestrator->IsPAL()) {
                    frame_time = 20000; // 50 FPS for PAL (20 ms)
                }
            }

            static u64 next_frame_tick = 0;
            u64 now = CTimer::GetClockTicks64();
            if (next_frame_tick == 0) {
                next_frame_tick = now + frame_time;
            } else {
                // Resync after long stalls or timing discontinuities.
                s64 delta = (s64)now - (s64)next_frame_tick;
                if (delta > frame_time * 4 || delta < -(frame_time * 4)) {
                    next_frame_tick = now + frame_time;
                }
            }
            if (now < next_frame_tick) {
                CTimer::SimpleusDelay(next_frame_tick - now);
            }
            next_frame_tick += frame_time;
        }
    }

    // Cleanly unmount the SD card filesystem
    m_Logger.Write("orchestrator", LogNotice, "Unmounting SD card filesystem...");
    f_mount(nullptr, "SD:", 0);
}

struct ScaleTable {
    int src_w;
    u16 idx1[640];
    u16 idx2[640];
    u8 weight[640]; // 0..32
};

static ScaleTable s_ScaleTableCache = {0};

static void UpdateScaleTable(int src_w) {
    if (s_ScaleTableCache.src_w == src_w) return;
    s_ScaleTableCache.src_w = src_w;
    u32 step = (src_w << 16) / 640;
    u32 scale = (step > 0) ? (32ULL << 16) / step : 0;
    u32 transition_start = 65536 - step;
    u32 accum = 0;
    for (int x = 0; x < 640; x++) {
        u32 idx = accum >> 16;
        u32 frac_part = accum & 0xFFFF;
        s_ScaleTableCache.idx1[x] = idx;
        if (frac_part >= transition_start && step > 0) {
            s_ScaleTableCache.idx2[x] = (idx + 1 < (u32)src_w) ? idx + 1 : idx;
            u32 frac = ((frac_part - transition_start) * scale) >> 16;
            s_ScaleTableCache.weight[x] = (frac > 32) ? 32 : frac;
        } else {
            s_ScaleTableCache.idx2[x] = idx;
            s_ScaleTableCache.weight[x] = 0;
        }
        accum += step;
    }
}

static void CopyBackBufferToFB(u16 *pBuf, u32 nPitch, const u16 *pBackBuffer) {
    if (!g_SharedState.screensaver_active) {
        if (nPitch == SCREEN_WIDTH) {
            memcpy(pBuf, pBackBuffer, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u16));
        } else {
            for (unsigned y = 0; y < SCREEN_HEIGHT; y++) {
                memcpy(pBuf + y * nPitch, pBackBuffer + y * SCREEN_WIDTH, SCREEN_WIDTH * sizeof(u16));
            }
        }
    } else {
        if (nPitch == SCREEN_WIDTH) {
            const u64 *src64 = (const u64 *)pBackBuffer;
            u64 *dst64 = (u64 *)pBuf;
            size_t count64 = (SCREEN_WIDTH * SCREEN_HEIGHT) / 4;
            for (size_t i = 0; i < count64; i++) {
                dst64[i] = (src64[i] >> 1) & 0x7BEF7BEF7BEF7BEFULL;
            }
        } else {
            for (unsigned y = 0; y < SCREEN_HEIGHT; y++) {
                const u64 *src64 = (const u64 *)(pBackBuffer + y * SCREEN_WIDTH);
                u64 *dst64 = (u64 *)(pBuf + y * nPitch);
                size_t count64 = SCREEN_WIDTH / 4;
                for (size_t i = 0; i < count64; i++) {
                    dst64[i] = (src64[i] >> 1) & 0x7BEF7BEF7BEF7BEFULL;
                }
            }
        }
    }
}

void CKernel::RunVideoDomain() {
    m_Logger.Write("video", LogNotice, "Core 1: Video Engine Active");

    m_Logger.Write("video", LogNotice, "Font8x16: width=%u, height=%u, first=%u, last=%u, data=%p",
                   Font8x16.width, Font8x16.height, Font8x16.first_char, Font8x16.last_char, Font8x16.data);

    CBcmFrameBuffer *pFB = m_Screen.GetFrameBuffer();
    if (pFB == nullptr) {
        m_Logger.Write("video", LogPanic, "Cannot get screen frame buffer");
        return;
    }

    u16 *pBuf = (u16 *)pFB->GetBuffer();
    u32 nPitch = pFB->GetPitch() / 2;

    u16 *pBackBuffer = new u16[SCREEN_WIDTH * SCREEN_HEIGHT];
    if (pBackBuffer == nullptr) {
        m_Logger.Write("video", LogPanic, "Cannot allocate OSD backbuffer");
        return;
    }

    m_Logger.Write("video", LogNotice, "FrameBuffer: buf=%p, pitch=%u", pBuf, nPitch);

    // Clear screen to pure black
    DrawRect(pBackBuffer, SCREEN_WIDTH, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, COLOR15(0, 0, 0));
    CopyBackBufferToFB(pBuf, nPitch, pBackBuffer);

    // Wait until filesystem is mounted by Core 0
    while (g_pFileSystem == nullptr) {
        CTimer::SimpleMsDelay(10);
    }

    // Load and display splash screen if configured in cmdline.txt (default: 4 seconds; 0 to disable)
    int splash_sec = m_Options.GetAppOptionSignedDecimal("splash", -1);
    if (splash_sec < 0) {
        splash_sec = m_Options.GetAppOptionSignedDecimal("splash_time", 4);
    }

    if (splash_sec > 0) {
        FIL splashFile;
        if (f_open(&splashFile, "SD:/Splash_Screen.raw16", FA_READ) == FR_OK) {
        u64 fileSize = f_size(&splashFile);
        int img_h = fileSize / (640 * sizeof(u16));
        if (img_h > 480) img_h = 480;

        if (img_h >= 1) {
            UINT bytesRead = 0;
            UINT bytesToRead = 640 * img_h * sizeof(u16);
            FRESULT read_res = f_read(&splashFile, s_SplashBuf, bytesToRead, &bytesRead);
            f_close(&splashFile);

            if (read_res == FR_OK && bytesRead == bytesToRead) {
                // Synchronize data cache for Core 1 to see the raw DMA bytes cleanly
                CleanAndInvalidateDataCacheRange((u32)s_SplashBuf, bytesToRead);

                // Vertically center splash image on screen
                int start_y = (SCREEN_HEIGHT - img_h) / 2;
                int src_y_offset = 0;
                if (start_y < 0) {
                    src_y_offset = -start_y;
                    start_y = 0;
                }
                int draw_h = img_h - src_y_offset;
                if (draw_h > SCREEN_HEIGHT - start_y) {
                    draw_h = SCREEN_HEIGHT - start_y;
                }
                if (draw_h < 1) {
                    draw_h = 1;
                }

                // 1. Fade-in (1 second, 50 steps of 20ms)
                for (int step = 0; step <= 50; step++) {
                    u32 scale = (step << 16) / 50;
                    for (int y = 0; y < draw_h; y++) {
                        const u16 *src_row = s_SplashBuf + (src_y_offset + y) * 640;
                        u16 *dst_row = pBackBuffer + (start_y + y) * SCREEN_WIDTH;
                        for (int x = 0; x < 640; x++) {
                            u16 color = src_row[x];
                            u32 r = ((color >> 11) & 0x1F) * scale >> 16;
                            u32 g = ((color >> 5) & 0x3F) * scale >> 16;
                            u32 b = (color & 0x1F) * scale >> 16;
                            dst_row[x] = (r << 11) | (g << 5) | b;
                        }
                    }
                    CopyBackBufferToFB(pBuf, nPitch, pBackBuffer);
                    CTimer::SimpleMsDelay(20);
                }

                // 2. Solid Display (hold duration based on splash_sec, default 2 seconds)
                int hold_ms = (splash_sec > 2) ? ((splash_sec - 2) * 1000) : 0;
                for (int y = 0; y < draw_h; y++) {
                    memcpy(pBackBuffer + (start_y + y) * SCREEN_WIDTH, s_SplashBuf + (src_y_offset + y) * 640, 640 * sizeof(u16));
                }
                CopyBackBufferToFB(pBuf, nPitch, pBackBuffer);
                if (hold_ms > 0) {
                    CTimer::SimpleMsDelay(hold_ms);
                }

                // 3. Fade-out (1 second, 50 steps of 20ms)
                for (int step = 50; step >= 0; step--) {
                    u32 scale = (step << 16) / 50;
                    for (int y = 0; y < draw_h; y++) {
                        const u16 *src_row = s_SplashBuf + (src_y_offset + y) * 640;
                        u16 *dst_row = pBackBuffer + (start_y + y) * SCREEN_WIDTH;
                        for (int x = 0; x < 640; x++) {
                            u16 color = src_row[x];
                            u32 r = ((color >> 11) & 0x1F) * scale >> 16;
                            u32 g = ((color >> 5) & 0x3F) * scale >> 16;
                            u32 b = (color & 0x1F) * scale >> 16;
                            dst_row[x] = (r << 11) | (g << 5) | b;
                        }
                    }
                    CopyBackBufferToFB(pBuf, nPitch, pBackBuffer);
                    CTimer::SimpleMsDelay(20);
                }
            }
        }
    }
}

    boolean was_in_menu = TRUE;
    static int s_PceLayoutMode = -1; // -1: Undetected, 0: 240-line, 1: Centered 224, 2: Top-aligned 224
    static int s_PceSrcYOffset = 0;
    static int s_PceDrawH = 240;
    while (1) {
        // Screensaver logic:
        // 1. Configured via cmdline.txt: screensaver=<seconds> (default: 60, 0 to disable).
        // 2. If controller is UNTOUCHED for <seconds> -> enable screensaver (dim screen by 50% & mute audio).
        // 3. If user presses a button / touches controller -> disable screensaver & reset period.
        int ss_sec = m_Options.GetAppOptionSignedDecimal("screensaver", 60);
        u64 ss_timeout_us = (ss_sec > 0) ? ((u64)ss_sec * 1000000ULL) : 0ULL;

        u64 now = CTimer::GetClockTicks64();
        if (g_SharedState.last_input_time == 0) {
            g_SharedState.last_input_time = now;
        }

        u16 pad1 = g_SharedState.pad1;
        u16 pad2 = g_SharedState.pad2;
        static u16 s_last_check_pad1 = 0;
        static u16 s_last_check_pad2 = 0;
        static boolean s_prev_in_menu = TRUE;

        // Reset pad tracking references when transitioning from game to menu mode
        if (g_SharedState.in_menu && !s_prev_in_menu) {
            s_last_check_pad1 = 0;
            s_last_check_pad2 = 0;
            if (g_SharedState.screensaver_active) {
                g_SharedState.menu_needs_redraw = TRUE;
            } else {
                g_SharedState.last_input_time = now;
            }
        }
        s_prev_in_menu = g_SharedState.in_menu;

        boolean input_touched = (pad1 != 0 || pad2 != 0 || pad1 != s_last_check_pad1 || pad2 != s_last_check_pad2 ||
                                 g_SharedState.save_state_requested || g_SharedState.load_state_requested ||
                                 g_SharedState.rewind_requested);

        if (input_touched) {
            g_SharedState.last_input_time = now;
            s_last_check_pad1 = pad1;
            s_last_check_pad2 = pad2;
            if (g_SharedState.screensaver_active) {
                g_SharedState.screensaver_active = FALSE;
                if (g_SharedState.in_menu) {
                    g_SharedState.menu_needs_redraw = TRUE;
                }
            }
        } else {
            if (ss_timeout_us > 0 && !g_SharedState.screensaver_active && (now - g_SharedState.last_input_time >= ss_timeout_us)) {
                g_SharedState.screensaver_active = TRUE;
                if (g_SharedState.in_menu) {
                    g_SharedState.menu_needs_redraw = TRUE;
                }
            }
        }

        const OSDThemeColors &theme = *s_pOSDTheme;

        if (m_ShutdownMode != ShutdownNone) {
            DrawRect(pBackBuffer, SCREEN_WIDTH, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, theme.screen_bg);
            
            // Draw a nice centered dark container (matching OSD colors)
            int bx1 = SCREEN_WIDTH / 2 - 180;
            int by1 = SCREEN_HEIGHT / 2 - 50;
            int bx2 = SCREEN_WIDTH / 2 + 180;
            int by2 = SCREEN_HEIGHT / 2 + 50;
            
            DrawRect(pBackBuffer, SCREEN_WIDTH, bx1, by1, bx2, by2, theme.panel_bg);
            DrawBox(pBackBuffer, SCREEN_WIDTH, bx1, by1, bx2, by2, theme.panel_border, 2);
            
            const char* msg = (m_ShutdownMode == ShutdownHalt) ? "SHUTTING DOWN..." : "REBOOTING SYSTEM...";
            int msg_w = strlen(msg) * 8;
            int msg_x = bx1 + ((bx2 - bx1) - msg_w) / 2;
            int msg_y = by1 + 42;
            
            DrawString(pBackBuffer, SCREEN_WIDTH, msg, msg_x, msg_y, theme.tab_active_text, theme.panel_bg);
            
            CopyBackBufferToFB(pBuf, nPitch, pBackBuffer);
            CTimer::SimpleMsDelay(50);
            continue;
        }

        if (g_SharedState.in_menu) {
            was_in_menu = TRUE;
            if (g_SharedState.menu_needs_redraw) {
                g_SharedState.menu_needs_redraw = FALSE;

                int selected = g_SharedState.menu_selected_idx;
                int num_lines = g_SharedState.menu_num_lines;
#if OSD_VERBOSE_REDRAW_LOG
                m_Logger.Write("video", LogNotice, "OSD Redraw started. num_lines=%d, selected=%d", num_lines, selected);
#endif

                // Draw menu elements onto OSD backbuffer to prevent flickering (pure black background)
                DrawRect(pBackBuffer, SCREEN_WIDTH, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, theme.screen_bg);

                // Draw central card/container (cool dark slate-indigo background!)
                int x1 = 40, y1 = 15, x2 = SCREEN_WIDTH - 40, y2 = SCREEN_HEIGHT - 15;
                DrawRect(pBackBuffer, SCREEN_WIDTH, x1, y1, x2, y2, theme.panel_bg);
                
                // Draw clean steel blue border
                DrawBox(pBackBuffer, SCREEN_WIDTH, x1, y1, x2, y2, theme.panel_border, 2);

                // Draw Title (soft cool white)
                char title_str[64];
                if (g_SharedState.active_emu_mode == EmuMode_SNES) {
                    snprintf(title_str, sizeof(title_str), "### SNES / Super Famicom ###");
                } else if (g_SharedState.active_emu_mode == EmuMode_NES) {
                    snprintf(title_str, sizeof(title_str), "### NES - Famicom ###");
                } else if (g_SharedState.active_emu_mode == EmuMode_PCE) {
                    snprintf(title_str, sizeof(title_str), "### PC Engine / TurboGrafx-16 ###");
                } else if (g_SharedState.active_emu_mode == EmuMode_SMS) {
                    snprintf(title_str, sizeof(title_str), "### Master System ###");
                } else if (g_SharedState.active_emu_mode == EmuMode_MD) {
                    snprintf(title_str, sizeof(title_str), "### Mega Drive / Genesis ###");
                } else { // EmuMode_FAV
                    snprintf(title_str, sizeof(title_str), "### Favorites Games ###");
                }
                int title_w = strlen(title_str) * 8;
                int title_x = (SCREEN_WIDTH - title_w) / 2;
                DrawString(pBackBuffer, SCREEN_WIDTH, title_str, title_x, y1 + 15, theme.title_text, 0);
                
                if (num_lines > 0) {
                    char count_str[32];
                    snprintf(count_str, sizeof(count_str), "[%d/%d]", selected + 1, num_lines);
                    DrawString(pBackBuffer, SCREEN_WIDTH, count_str, x2 - 100, y1 + 15, theme.title_text, theme.panel_bg);
                }
                
                // Draw separator 1 (dark steel blue)
                DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 20, y1 + 40, x2 - 20, y1 + 41, theme.separator);

                // Draw tabs
                int num_tabs = g_SharedState.menu_num_tabs;
                if (num_tabs <= 0) num_tabs = 8;
                int active_tab = g_SharedState.menu_active_tab;
                int tab_spacing = 6;
                int available_tab_width = (x2 - x1) - 20;
                int tab_width = (available_tab_width - (num_tabs - 1) * tab_spacing) / num_tabs;
                if (tab_width < 56) {
                    tab_spacing = 4;
                    tab_width = (available_tab_width - (num_tabs - 1) * tab_spacing) / num_tabs;
                }
                int tab_area_width = num_tabs * tab_width + (num_tabs - 1) * tab_spacing;
                int tab_start_x = x1 + ((x2 - x1) - tab_area_width) / 2;
                int tab_y1 = y1 + 46;
                int tab_y2 = y1 + 68;
                int tab_gap = tab_y1 - (y1 + 41); // keep same gap from separators to tabs (top and bottom)

                for (int t = 0; t < num_tabs; t++) {
                    int tx1 = tab_start_x + t * (tab_width + tab_spacing);
                    int tx2 = tx1 + tab_width;
                    
                    u16 bg_color, border_color, text_color;
                    if (t == active_tab) {
                        bg_color = theme.tab_active_bg;
                        border_color = theme.tab_active_border;
                        text_color = theme.tab_active_text;
                    } else {
                        bg_color = theme.tab_inactive_bg;
                        border_color = theme.tab_inactive_border;
                        text_color = theme.tab_inactive_text;
                    }
                    
                    // Draw tab box
                    DrawRect(pBackBuffer, SCREEN_WIDTH, tx1, tab_y1, tx2, tab_y2, bg_color);
                    DrawBox(pBackBuffer, SCREEN_WIDTH, tx1, tab_y1, tx2, tab_y2, border_color, 1);
                    
                    // Center tab title text
                    const char *tab_name = g_SharedState.menu_tab_names[t];
                    int text_w = strlen(tab_name) * 8;
                    int text_x = tx1 + (tab_width - text_w) / 2;
                    DrawString(pBackBuffer, SCREEN_WIDTH, tab_name, text_x, y1 + 49, text_color, bg_color);
                }

                // Draw separator 2 with the same spacing used above the tabs
                int sep2_y1 = tab_y2 + tab_gap;
                DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 20, sep2_y1, x2 - 20, sep2_y1 + 1, theme.separator);

                if (num_lines == 0) {
                    if (active_tab == 1 || g_SharedState.active_emu_mode == EmuMode_FAV) {
                        DrawString(pBackBuffer, SCREEN_WIDTH, "No favorites added! Press Y on a game to favorite it.", 116, 180, theme.title_text, 0);
                    } else {
                        DrawString(pBackBuffer, SCREEN_WIDTH, "No ROMs found! Copy ROM files to SD card.", 150, 180, theme.warning_text, 0);
                    }
                } else {
                    // List ROM files with centered viewport scrolling (mega-pi-metal style)
                    int view_size = 17;
                    int start_i = 0;
                    if (num_lines > view_size) {
                        start_i = selected - view_size / 2;
                        if (start_i < 0) start_i = 0;
                        if (start_i + view_size > num_lines) {
                            start_i = num_lines - view_size;
                        }
                    }

                    int start_y = y1 + 79;
                    for (int v = 0; v < view_size && (start_i + v) < num_lines; v++) {
                        int i = start_i + v;
                        int row_y = start_y + v * 20;
                        u16 fg = (i == selected) ? theme.list_selected_text : theme.list_text;
                        
                        if (i == selected) {
                            // Highlight selector bar (ends at x2 - 22 to not overlap scrollbar) (soft slate highlight)
                            DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 10, row_y - 2, x2 - 22, row_y + 16, theme.list_selected_bg);
                        }
                        
                        DrawString(pBackBuffer, SCREEN_WIDTH, g_SharedState.menu_lines[i], x1 + 20, row_y, fg, 0);
                    }

                    // Draw scrollbar indicator if list is scrollable
                    if (num_lines > view_size) {
                        int track_x = x2 - 18;
                        int track_y = start_y;
                        int track_w = 4;
                        int track_h = view_size * 20 - 4;
                        
                        // Track (dark slate)
                        DrawRect(pBackBuffer, SCREEN_WIDTH, track_x, track_y, track_x + track_w, track_y + track_h, theme.scroll_track);
                        
                        // Thumb (muted steel blue)
                        int thumb_h = (view_size * track_h) / num_lines;
                        if (thumb_h < 10) thumb_h = 10;
                        int thumb_y = track_y + (start_i * track_h) / num_lines;
                        if (thumb_y + thumb_h > track_y + track_h) {
                            thumb_y = track_y + track_h - thumb_h;
                        }
                        DrawRect(pBackBuffer, SCREEN_WIDTH, track_x, thumb_y, track_x + track_w, thumb_y + thumb_h, theme.scroll_thumb);
                    }
                }

                // Draw separator before instructions (dark steel blue)
                DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 20, y2 - 28, x2 - 20, y2 - 27, theme.separator);

                // Center instructions footer inside the screen limits
                const char *footer_text = "D-PAD:Nav | A/B:Start | L/R:System | Y/X:Fav | START+SEL:Reset";
                int footer_w = strlen(footer_text) * 8;
                int footer_x = (SCREEN_WIDTH - footer_w) / 2;
                DrawString(pBackBuffer, SCREEN_WIDTH, footer_text, footer_x, y2 - 20, theme.footer_text, 0);

                if (g_SharedState.rom_loading) {
                    // Draw a beautiful centered loading box overlay on top of the menu
                    int bx1 = (SCREEN_WIDTH - 280) / 2;
                    int by1 = (SCREEN_HEIGHT - 70) / 2;
                    int bx2 = bx1 + 280;
                    int by2 = by1 + 70;
                    
                    // Box background (very dark slate/indigo)
                    DrawRect(pBackBuffer, SCREEN_WIDTH, bx1, by1, bx2, by2, theme.panel_bg);
                    // Accent border (steel blue)
                    DrawBox(pBackBuffer, SCREEN_WIDTH, bx1, by1, bx2, by2, theme.panel_border, 2);
                    
                    const char *loading_msg = "LOADING GAME, PLEASE WAIT...";
                    int msg_w = strlen(loading_msg) * 8;
                    int msg_x = bx1 + (280 - msg_w) / 2;
                    int msg_y = by1 + (70 - 16) / 2;
                    DrawString(pBackBuffer, SCREEN_WIDTH, loading_msg, msg_x, msg_y, theme.tab_active_text, theme.panel_bg);
                }

                // Copy fully rendered backbuffer to the active framebuffer in a single fast operation
                CopyBackBufferToFB(pBuf, nPitch, pBackBuffer);
            }
            pFB->WaitForVerticalSync();
        } else {
            // Scale and draw game frame
            if (g_SharedState.video_frame_ready) {
                // Wait for vertical sync before drawing directly to the hardware framebuffer to prevent tearing and flickering
                pFB->WaitForVerticalSync();

                if (was_in_menu) {
                    was_in_menu = FALSE;
                    s_PceLayoutMode = -1; // Reset PCE layout detection on game start
                    // Clear both pBackBuffer and the hardware framebuffer to black once when entering the game
                    memset(pBackBuffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u16));
                    if (nPitch == SCREEN_WIDTH) {
                        memset(pBuf, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u16));
                    } else {
                        for (u32 y = 0; y < SCREEN_HEIGHT; y++) {
                            memset(pBuf + y * nPitch, 0, SCREEN_WIDTH * sizeof(u16));
                        }
                    }
                }

                static u32 draw_count = 0;
                draw_count++;
 
                int read_idx = g_SharedState.emu_read_idx;
                int game_h = g_SharedState.game_h[read_idx];
                int start_line = g_SharedState.start_line[read_idx];
                alignas(64) u16 scanline_buf[640];
 
                if (game_h < 1) game_h = 224;
 
                if (g_SharedState.active_emu_mode == EmuMode_SNES) {
                    int game_w = g_SharedState.game_w[read_idx];
                    if (game_w < 1) game_w = 256;
                    if (game_w > 512) game_w = 512;
                    if (game_h > 480) game_h = 480;
 
                    int out_h = (game_h <= 240) ? game_h * 2 : game_h;
                    int start_y = (SCREEN_HEIGHT - out_h) / 2;
                    if (start_y < 0) start_y = 0;

                    if (game_w == 256) {
                        UpdateScaleTable(256);
                        const u16 *idx1 = s_ScaleTableCache.idx1;
                        const u16 *idx2 = s_ScaleTableCache.idx2;
                        const u8 *weight = s_ScaleTableCache.weight;
                        if (game_h <= 240) {
                            // Case 1: 256x224/239 -> scale 2.5x horizontally and 2x vertically to 640x448/478 (Sharp Bilinear)
                            for (int y = 0; y < game_h; y++) {
                                const u16 *src = g_SharedState.emu_frame_buffer[read_idx] + (start_line + y) * 256;
                                u16 *dest1 = pBuf + (start_y + 2 * y) * nPitch;
                                u16 *dest2 = dest1 + nPitch;
                                for (int x = 0; x < 640; x++) {
                                    scanline_buf[x] = BlendRGB565(src[idx1[x]], src[idx2[x]], weight[x]);
                                }
                                if (g_SharedState.screensaver_active) DimScanline50(scanline_buf);
                                memcpy(dest1, scanline_buf, 640 * sizeof(u16));
                                memcpy(dest2, scanline_buf, 640 * sizeof(u16));
                            }
                        } else {
                            // Case 3: 256x448/478 -> scale 2.5x horizontally and 1x vertically to 640x448/478 (Sharp Bilinear)
                            for (int y = 0; y < game_h; y++) {
                                const u16 *src = g_SharedState.emu_frame_buffer[read_idx] + (start_line + y) * 256;
                                u16 *dest = pBuf + (start_y + y) * nPitch;
                                for (int x = 0; x < 640; x++) {
                                    scanline_buf[x] = BlendRGB565(src[idx1[x]], src[idx2[x]], weight[x]);
                                }
                                if (g_SharedState.screensaver_active) DimScanline50(scanline_buf);
                                memcpy(dest, scanline_buf, 640 * sizeof(u16));
                            }
                        }
                    } else { // game_w == 512
                        UpdateScaleTable(512);
                        const u16 *idx1 = s_ScaleTableCache.idx1;
                        const u16 *idx2 = s_ScaleTableCache.idx2;
                        const u8 *weight = s_ScaleTableCache.weight;
                        if (game_h <= 240) {
                            // Case 2: 512x224/239 -> scale 1.25x horizontally and 2x vertically to 640x448/478 (Sharp Bilinear)
                            for (int y = 0; y < game_h; y++) {
                                const u16 *src = g_SharedState.emu_frame_buffer[read_idx] + (start_line + y) * 512;
                                u16 *dest1 = pBuf + (start_y + 2 * y) * nPitch;
                                u16 *dest2 = dest1 + nPitch;
                                for (int x = 0; x < 640; x++) {
                                    scanline_buf[x] = BlendRGB565(src[idx1[x]], src[idx2[x]], weight[x]);
                                }
                                if (g_SharedState.screensaver_active) DimScanline50(scanline_buf);
                                memcpy(dest1, scanline_buf, 640 * sizeof(u16));
                                memcpy(dest2, scanline_buf, 640 * sizeof(u16));
                            }
                        } else {
                            // Case 4: 512x448/478 -> scale 1.25x horizontally and 1x vertically to 640x448/478 (Sharp Bilinear)
                            for (int y = 0; y < game_h; y++) {
                                const u16 *src = g_SharedState.emu_frame_buffer[read_idx] + (start_line + y) * 512;
                                u16 *dest = pBuf + (start_y + y) * nPitch;
                                for (int x = 0; x < 640; x++) {
                                    scanline_buf[x] = BlendRGB565(src[idx1[x]], src[idx2[x]], weight[x]);
                                }
                                if (g_SharedState.screensaver_active) DimScanline50(scanline_buf);
                                memcpy(dest, scanline_buf, 640 * sizeof(u16));
                            }
                        }
                    }
                } else if (g_SharedState.active_emu_mode == EmuMode_NES) {
                    // NES video rendering (stretches 256 active pixels from stride 512 to 640x480 for a perfect 4:3 using Sharp Bilinear)
                    int out_h = game_h * 2; // 240 * 2 = 480
                    int start_y = (SCREEN_HEIGHT - out_h) / 2;
                    if (start_y < 0) start_y = 0;

                    UpdateScaleTable(256);
                    const u16 *idx1 = s_ScaleTableCache.idx1;
                    const u16 *idx2 = s_ScaleTableCache.idx2;
                    const u8 *weight = s_ScaleTableCache.weight;
                    for (int y = 0; y < game_h; y++) {
                        const u16 *src = g_SharedState.emu_frame_buffer[read_idx] + (start_line + y) * 512;
                        u16 *dest1 = pBuf + (start_y + 2 * y) * nPitch;
                        u16 *dest2 = dest1 + nPitch;
                        for (int x = 0; x < 640; x++) {
                            scanline_buf[x] = BlendRGB565(src[idx1[x]], src[idx2[x]], weight[x]);
                        }
                        if (g_SharedState.screensaver_active) DimScanline50(scanline_buf);
                        memcpy(dest1, scanline_buf, 640 * sizeof(u16));
                        memcpy(dest2, scanline_buf, 640 * sizeof(u16));
                    }
                } else if (g_SharedState.active_emu_mode == EmuMode_PCE) {
                    // PCE video rendering (stretches any game resolution to 640x480 for a perfect 4:3 aspect ratio using a high-quality software Sharp Bilinear filter)
                    int game_w = g_SharedState.game_w[read_idx];
                    if (game_w < 1) game_w = 256;
                    
                    if (s_PceLayoutMode == -1 && game_h >= 240) {
                        // We check if the screen is currently active (middle line is not black) before locking in a mode
                        const u16 *mid_row = g_SharedState.emu_frame_buffer[read_idx] + (start_line + 120) * game_w;
                        if (!IsRowBlack(mid_row, game_w)) {
                            // Check if row 235 is black to determine if this is a 224-line game (with black padding at the bottom)
                            const u16 *bottom_row = g_SharedState.emu_frame_buffer[read_idx] + (start_line + 235) * game_w;
                            const u16 *top_row = g_SharedState.emu_frame_buffer[read_idx] + (start_line + 4) * game_w;
                            
                            bool bottom_black = IsRowBlack(bottom_row, game_w);
                            bool top_black = IsRowBlack(top_row, game_w);
                            
                            if (bottom_black) {
                                if (top_black) {
                                    // Centered 224-line layout: crop 8 top, 8 bottom
                                    s_PceLayoutMode = 1;
                                    s_PceSrcYOffset = 8;
                                    s_PceDrawH = 224;
                                } else {
                                    // Top-aligned 224-line layout: crop 0 top, 16 bottom
                                    s_PceLayoutMode = 2;
                                    s_PceSrcYOffset = 0;
                                    s_PceDrawH = 224;
                                }
                            } else {
                                // Full 240-line game: crop 0 top, 0 bottom (draw full 240)
                                s_PceLayoutMode = 0;
                                s_PceSrcYOffset = 0;
                                s_PceDrawH = 240;
                            }
                        }
                    }
                    
                    int draw_h = (s_PceLayoutMode != -1) ? s_PceDrawH : game_h;
                    int src_y_offset = (s_PceLayoutMode != -1) ? s_PceSrcYOffset : 0;
                    if (s_PceLayoutMode == -1 && draw_h > 240) draw_h = 240;
                    
                    int out_h = draw_h * 2;
                    int start_x = 0;
                    int start_y = (SCREEN_HEIGHT - out_h) / 2;
                    if (start_y < 0) start_y = 0;

                    // Clear top border to black on every frame
                    if (start_y > 0) {
                        memset(pBuf, 0, start_y * nPitch * sizeof(u16));
                    }
                    // Clear bottom border to black on every frame
                    if (start_y + out_h < SCREEN_HEIGHT) {
                        memset(pBuf + (start_y + out_h) * nPitch, 0, (SCREEN_HEIGHT - (start_y + out_h)) * nPitch * sizeof(u16));
                    }

                    UpdateScaleTable(game_w);
                    const u16 *idx1 = s_ScaleTableCache.idx1;
                    const u16 *idx2 = s_ScaleTableCache.idx2;
                    const u8 *weight = s_ScaleTableCache.weight;
                    for (int y = 0; y < draw_h; y++) {
                        const u16 *src = g_SharedState.emu_frame_buffer[read_idx] + (start_line + src_y_offset + y) * game_w;
                        u16 *dest1 = pBuf + (start_y + 2 * y) * nPitch + start_x;
                        u16 *dest2 = dest1 + nPitch;
                        for (int x = 0; x < 640; x++) {
                            scanline_buf[x] = BlendRGB565(src[idx1[x]], src[idx2[x]], weight[x]);
                        }
                        if (g_SharedState.screensaver_active) DimScanline50(scanline_buf);
                        memcpy(dest1, scanline_buf, 640 * sizeof(u16));
                        memcpy(dest2, scanline_buf, 640 * sizeof(u16));
                    }
                } else if (g_SharedState.active_emu_mode == EmuMode_SMS) {
                    // Master System video rendering in a 512-pitch frame buffer (active area starts at x offset 32).
                    int game_w = g_SharedState.game_w[read_idx];
                    if (game_w < 1) game_w = 256;
                    if (game_h < 1) game_h = 192;
                    // In SMS 192-line mode, Pico reports the 192 active lines, but some games use
                    // additional visible top/bottom area from the 224-line window.
                    if (game_h == 192 && start_line >= 16) {
                        start_line -= 16;
                        game_h = 224;
                    }
                    if (start_line < 0) start_line = 0;
                    if (game_h > 240) game_h = 240;
                    if (start_line + game_h > 240) game_h = 240 - start_line;
                    if (game_h < 1) game_h = 192;

                    UpdateScaleTable(game_w);
                    const u16 *idx1 = s_ScaleTableCache.idx1;
                    const u16 *idx2 = s_ScaleTableCache.idx2;
                    const u8 *weight = s_ScaleTableCache.weight;

                    int last_src_y = -1;
                    for (int y = 0; y < 480; y++) {
                        int src_y = (y * game_h) / 480;
                        u16 *dest = pBuf + y * nPitch;

                        if (src_y == last_src_y) {
                            memcpy(dest, scanline_buf, 640 * sizeof(u16));
                            continue;
                        }
                        last_src_y = src_y;

                        const u16 *src = g_SharedState.emu_frame_buffer[read_idx] + (start_line + src_y) * 512 + 32;
                        for (int x = 0; x < 640; x++) {
                            scanline_buf[x] = BlendRGB565(src[idx1[x]], src[idx2[x]], weight[x]);
                        }
                        if (g_SharedState.screensaver_active) DimScanline50(scanline_buf);
                        memcpy(dest, scanline_buf, 640 * sizeof(u16));
                    }
                } else {
                    // Mega Drive video rendering
                    if (game_h > 240) game_h = 240;
                    if (game_h < 1) {
                        start_line = 0;
                        game_h = 224;
                    }

                    int out_h = game_h * 2;
                    int start_y = (SCREEN_HEIGHT - out_h) / 2;
                    if (start_y < 0) start_y = 0;

                    // Upscale to full 4:3 (640x448) based on game mode width
                    int game_w = g_SharedState.game_w[read_idx];
                    if (game_w != 256) {
                        // H40 Mode: 320x224 scaled 2x to 640x448 (using optimized nearest-neighbor)
                        for (int y = 0; y < game_h; y++) {
                            const u32 * __restrict src32 = (const u32 *)(g_SharedState.emu_frame_buffer[read_idx] + (start_line + y) * 320);
                            u64 * __restrict dest64_1 = (u64 *)(pBuf + (start_y + 2 * y) * nPitch);
                            u64 * __restrict dest64_2 = dest64_1 + (nPitch / 4);

                            for (int x = 0; x < 160; x++) {
                                u32 pixels = src32[x];
                                u32 p1 = pixels & 0xFFFF;
                                u32 p2 = pixels >> 16;
                                
                                u32 p1_32 = (p1 << 16) | p1;
                                u32 p2_32 = (p2 << 16) | p2;
                                u64 color64 = ((u64)p2_32 << 32) | p1_32;
                                if (g_SharedState.screensaver_active) {
                                    color64 = (color64 >> 1) & 0x7BEF7BEF7BEF7BEFULL;
                                }

                                dest64_1[x] = color64;
                                dest64_2[x] = color64;
                            }
                        }
                    } else {
                        // H32 Mode: 256x224 scaled 2.5x to 640x448 (using Sharp Bilinear filter to avoid pixel shimmering/aliasing)
                        UpdateScaleTable(256);
                        const u16 *idx1 = s_ScaleTableCache.idx1;
                        const u16 *idx2 = s_ScaleTableCache.idx2;
                        const u8 *weight = s_ScaleTableCache.weight;
                        for (int y = 0; y < game_h; y++) {
                            // Active 256 pixels start at offset 32 in the 320-pixel stride
                            const u16 *src = g_SharedState.emu_frame_buffer[read_idx] + (start_line + y) * 320 + 32;
                            u16 *dest1 = pBuf + (start_y + 2 * y) * nPitch;
                            u16 *dest2 = dest1 + nPitch;
                            for (int x = 0; x < 640; x++) {
                                scanline_buf[x] = BlendRGB565(src[idx1[x]], src[idx2[x]], weight[x]);
                            }
                            if (g_SharedState.screensaver_active) DimScanline50(scanline_buf);
                            memcpy(dest1, scanline_buf, 640 * sizeof(u16));
                            memcpy(dest2, scanline_buf, 640 * sizeof(u16));
                        }
                    }
                }
                DataMemBarrier();
                g_SharedState.video_frame_ready = FALSE;
            } else {
                // Short adaptive wait to keep latency low without pegging the core.
                CTimer::SimpleusDelay(5);
            }
        }
    }
}

void CKernel::RunAudioDomain() {
    m_Logger.Write("audio", LogNotice, "Core 2: Audio Engine Active");

    // Dynamic selection of audio device (HDMI or PWM) based on options
    const char *pSoundDeviceName = m_Options.GetSoundDevice();
    if (pSoundDeviceName == nullptr || pSoundDeviceName[0] == '\0') {
        pSoundDeviceName = m_Options.GetAppOptionString("snddevice");
    }

    CSoundBaseDevice *pSoundDevice = nullptr;
    boolean bUsingHDMI = (pSoundDeviceName != nullptr && strcmp(pSoundDeviceName, "sndhdmi") == 0);
    if (bUsingHDMI) {
        m_Logger.Write("audio", LogNotice, "Using HDMI audio device");
        pSoundDevice = new CHDMISoundBaseDevice(&m_Interrupt, 44100, 384 * 10);
    } else {
        m_Logger.Write("audio", LogNotice, "Using PWM audio device (headphones)");
        pSoundDevice = new CPWMSoundBaseDevice(&m_Interrupt, 44100, 2048);
    }

    if (pSoundDevice == nullptr) {
        m_Logger.Write("audio", LogPanic, "Failed to instantiate sound device");
        return;
    }

    if (!pSoundDevice->AllocateQueue(100)) { // Allocate 100ms queue
        m_Logger.Write("audio", LogPanic, "Cannot allocate sound queue");
        delete pSoundDevice;
        return;
    }

    pSoundDevice->SetWriteFormat(SoundFormatSigned16, 2);
    if (!pSoundDevice->Start()) {
        if (bUsingHDMI) {
            m_Logger.Write("audio", LogWarning, "Cannot start HDMI sound device (is the monitor off?). Falling back to PWM.");
            delete pSoundDevice;
            pSoundDevice = new CPWMSoundBaseDevice(&m_Interrupt, 44100, 2048);
            if (pSoundDevice == nullptr) {
                m_Logger.Write("audio", LogPanic, "Failed to instantiate fallback PWM sound device");
                return;
            }
            if (!pSoundDevice->AllocateQueue(100)) {
                m_Logger.Write("audio", LogPanic, "Cannot allocate fallback sound queue");
                delete pSoundDevice;
                return;
            }
            pSoundDevice->SetWriteFormat(SoundFormatSigned16, 2);
            if (!pSoundDevice->Start()) {
                m_Logger.Write("audio", LogPanic, "Cannot start fallback sound device");
                delete pSoundDevice;
                return;
            }
        } else {
            m_Logger.Write("audio", LogPanic, "Cannot start sound device");
            delete pSoundDevice;
            return;
        }
    }

    s16 local_buf[1024 * 2];

    while (1) {
        unsigned avail = g_SharedState.audio_ring_buffer.GetAvailable();
        if (avail > 0) {
            if (avail > 1024) avail = 1024;
            unsigned read = g_SharedState.audio_ring_buffer.Read(local_buf, avail);
            if (g_SharedState.screensaver_active) {
                memset(local_buf, 0, read * 4);
            }
            pSoundDevice->Write(local_buf, read * 4); // each stereo sample is 4 bytes
        } else {
            // Audio starvation wait: keep short to reduce audible gaps while still yielding CPU.
            CTimer::SimpleusDelay(100);
        }
    }

    delete pSoundDevice;
}

void CKernel::RunInputDomain() {
    m_Logger.Write("input", LogNotice, "Core 3: Input Engine Active");

    while (1) {
        m_USBHCI.UpdatePlugAndPlay();

        // Detect and register gamepads. Reconcile slots every cycle because
        // some controllers (e.g. SN30 Pro) can re-enumerate after boot.
        CUSBGamePadDevice *detectedPads[2] = {nullptr, nullptr};
        unsigned detectedPorts[2] = {0, 0};
        const char *aliases[2] = {"upad", "gpad"};
        for (unsigned nDevice = 1; nDevice <= 8; nDevice++) {
            for (unsigned alias = 0; alias < 2; alias++) {
                CUSBGamePadDevice *pGamePad = (CUSBGamePadDevice *)
                    m_DeviceNameService.GetDevice(aliases[alias], nDevice, FALSE);
                if (pGamePad == nullptr || pGamePad == detectedPads[0] || pGamePad == detectedPads[1]) {
                    continue;
                }

                unsigned hubPort = pGamePad->GetDevice()->GetHubPortNumber();
                if ((detectedPorts[0] != 0 && detectedPorts[0] == hubPort) ||
                    (detectedPorts[1] != 0 && detectedPorts[1] == hubPort)) {
                    continue;
                }

                if (detectedPads[0] == nullptr) {
                    detectedPads[0] = pGamePad;
                    detectedPorts[0] = hubPort;
                } else if (detectedPads[1] == nullptr) {
                    detectedPads[1] = pGamePad;
                    detectedPorts[1] = hubPort;
                }
            }
        }
        for (unsigned slot = 0; slot < 2; slot++) {
            if (detectedPads[slot] != m_pGamePad[slot]) {
                m_pGamePad[slot] = detectedPads[slot];
                if (m_pGamePad[slot] != nullptr) {
                    m_pGamePad[slot]->RegisterRemovedHandler(GamePadRemovedHandler, this);
                    m_pGamePad[slot]->RegisterStatusHandler(GamePadStatusHandler);
                    m_Logger.Write("input", LogNotice, "USB Gamepad %u Connected", slot + 1);
                }
            }
        }

        // Detect and register keyboard
        if (m_pKeyboard == nullptr) {
            const char *kbd_name = "ukbd1";
            m_pKeyboard = (CUSBKeyboardDevice *)
                m_DeviceNameService.GetDevice(kbd_name, FALSE);
            
            if (m_pKeyboard != nullptr) {
                m_pKeyboard->RegisterRemovedHandler(KeyboardRemovedHandler, this);
                m_pKeyboard->RegisterKeyStatusHandlerRaw(KeyboardStatusHandlerRaw);
                m_Logger.Write("input", LogNotice, "USB Keyboard Connected");
            }
        }

        if (m_pKeyboard != nullptr) {
            m_pKeyboard->UpdateLEDs();
        }

        CTimer::SimpleMsDelay(100);
    }
}

// Event handlers
void CKernel::GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState) {
    u16 pad = 0;

    // 1. Check D-pad buttons (for Xbox 360 and others mapping D-pad to buttons)
    if (pState->buttons & GamePadButtonUp)    pad |= (1 << 0); // Up
    if (pState->buttons & GamePadButtonDown)  pad |= (1 << 1); // Down
    if (pState->buttons & GamePadButtonLeft)  pad |= (1 << 2); // Left
    if (pState->buttons & GamePadButtonRight) pad |= (1 << 3); // Right

    // 2. Check hats (D-pad) as fallback
    if (pState->nhats > 0 && !(pad & 0xF)) {
        int hat = pState->hats[0];
        if (hat >= 0 && hat <= 7) {
            if (hat == 0 || hat == 1 || hat == 7) pad |= (1 << 0); // Up
            if (hat == 3 || hat == 4 || hat == 5) pad |= (1 << 1); // Down
            if (hat == 5 || hat == 6 || hat == 7) pad |= (1 << 2); // Left
            if (hat == 1 || hat == 2 || hat == 3) pad |= (1 << 3); // Right
        }
    }

    // 3. Check analog axes as fallback (Disabled: not needed for 16-bit games and reduces processing/noise overhead)
    /*
    if (pState->naxes >= 2 && !(pad & 0xF)) {
        int x = pState->axes[0].value;
        int y = pState->axes[1].value;
        if (x < 64)  pad |= (1 << 2); // Left
        if (x > 192) pad |= (1 << 3); // Right
        if (y < 64)  pad |= (1 << 0); // Up
        if (y > 192) pad |= (1 << 1); // Down
    }
    */

    // 4. Map Action buttons for SNES: A, B, X, Y, L, R, Start, Select
    // pad bit 4: SNES A
    // pad bit 5: SNES B
    // pad bit 6: SNES X
    // pad bit 7: SNES Y
    // pad bit 8: SNES L (LT/LB -> L)
    // pad bit 9: SNES R (RT/RB -> R)
    // pad bit 10: SNES Start
    // pad bit 11: SNES Select
    if (g_SharedState.in_menu || g_SharedState.active_emu_mode == EmuMode_SNES || g_SharedState.active_emu_mode == EmuMode_NES || g_SharedState.active_emu_mode == EmuMode_PCE) {
        if (pState->buttons & GamePadButtonA)     pad |= (1 << 5);  // SNES B (bottom button)
        if (pState->buttons & GamePadButtonB)     pad |= (1 << 4);  // SNES A (right button)
        if (pState->buttons & GamePadButtonX)     pad |= (1 << 7);  // SNES Y (left button)
        if (pState->buttons & GamePadButtonY)     pad |= (1 << 6);  // SNES X (top button)
        if (pState->buttons & (GamePadButtonLT | GamePadButtonLB)) pad |= (1 << 8);  // SNES L
        if (pState->buttons & (GamePadButtonRT | GamePadButtonRB)) pad |= (1 << 9);  // SNES R
        if (pState->buttons & GamePadButtonStart) pad |= (1 << 10); // SNES Start
        if (pState->buttons & GamePadButtonSelect)pad |= (1 << 11); // SNES Select
    } else if (g_SharedState.active_emu_mode == EmuMode_SMS) {
        // Master System gamepad mapping:
        // bit 4: Button 1, bit 5: Button 2, bit 7: Pause (Z80 NMI)
        if (pState->buttons & GamePadButtonA)     pad |= (1 << 4);           // SMS Button 1
        if (pState->buttons & GamePadButtonB)     pad |= (1 << 5);           // SMS Button 2
        if (pState->buttons & GamePadButtonX)     pad |= (1 << 4);           // SMS Button 1
        if (pState->buttons & GamePadButtonY)     pad |= (1 << 5);           // SMS Button 2
        if (pState->buttons & (GamePadButtonLT | GamePadButtonLB)) pad |= (1 << 8); // L shoulder
        if (pState->buttons & (GamePadButtonRT | GamePadButtonRB)) pad |= (1 << 9); // R shoulder
        if (pState->buttons & GamePadButtonStart) pad |= (1 << 7);           // SMS Pause
        if (pState->buttons & GamePadButtonSelect)pad |= (1 << 11);          // Select
    } else {
        if (s_Is3ButtonGame) {
            if (pState->buttons & GamePadButtonA)     pad |= (1 << 6);  // Sega A
            if (pState->buttons & GamePadButtonB)     pad |= (1 << 4);  // Sega B
            if (pState->buttons & GamePadButtonX)     pad |= (1 << 5);  // Sega C (Gamesir X -> Sega C in 3-button games)
            if (pState->buttons & GamePadButtonRT)    pad |= (1 << 5);  // Sega C (fallback RT -> C)
            if (pState->buttons & GamePadButtonStart) pad |= (1 << 7);  // Sega Start
            if (pState->buttons & GamePadButtonSelect)pad |= (1 << 11); // Sega Mode
        } else {
            if (pState->buttons & GamePadButtonA)     pad |= (1 << 6);  // Sega A
            if (pState->buttons & GamePadButtonB)     pad |= (1 << 4);  // Sega B
            if (pState->buttons & GamePadButtonRT)    pad |= (1 << 5);  // Sega C (R2/RT -> C)
            if (pState->buttons & GamePadButtonLT)    pad |= (1 << 8);  // Sega Z (L2/LT -> Z)
            if (pState->buttons & GamePadButtonX)     pad |= (1 << 10); // Sega X
            if (pState->buttons & GamePadButtonY)     pad |= (1 << 9);  // Sega Y
            if (pState->buttons & GamePadButtonRB)    pad |= (1 << 8);  // Sega Z (fallback RB -> Z)
            if (pState->buttons & GamePadButtonLB)    pad |= (1 << 10); // Sega X (fallback LB -> X)
            if (pState->buttons & GamePadButtonStart) pad |= (1 << 7);  // Sega Start
            if (pState->buttons & GamePadButtonSelect)pad |= (1 << 11); // Sega Mode
        }
    }

    // START + SELECT combo -> Exit to menu
    if ((pState->buttons & (GamePadButtonStart | GamePadButtonPlus)) && (pState->buttons & (GamePadButtonSelect | GamePadButtonMinus))) {
        g_SharedState.escape_pressed = TRUE;
    }

    // SELECT + D-pad / Shoulder combos for state save/load/rewind
    static boolean s_bRewindComboPressed = FALSE;
    static boolean s_bSaveComboPressed = FALSE;
    static boolean s_bLoadComboPressed = FALSE;

    if (pState->buttons & (GamePadButtonSelect | GamePadButtonMinus)) {
        if (pad & (1 << 0)) { // D-pad Up -> Rewind state (5-second jump back, one-shot per press)
            if (!s_bRewindComboPressed) {
                s_bRewindComboPressed = TRUE;
                g_SharedState.rewind_requested = TRUE;
            }
            pad = 0; // Mask inputs when combo is held
        } else {
            s_bRewindComboPressed = FALSE;
        }

        if ((pad & (1 << 2)) || (pState->buttons & (GamePadButtonLB | GamePadButtonLT))) { // D-pad Left or L Shoulder -> Save state
            if (!s_bSaveComboPressed) {
                s_bSaveComboPressed = TRUE;
                g_SharedState.save_state_requested = TRUE;
            }
            pad = 0; // Mask inputs when combo is held
        } else {
            s_bSaveComboPressed = FALSE;
        }

        if ((pad & (1 << 3)) || (pState->buttons & (GamePadButtonRB | GamePadButtonRT))) { // D-pad Right or R Shoulder -> Load state
            if (!s_bLoadComboPressed) {
                s_bLoadComboPressed = TRUE;
                g_SharedState.load_state_requested = TRUE;
            }
            pad = 0; // Mask inputs when combo is held
        } else {
            s_bLoadComboPressed = FALSE;
        }
    } else {
        s_bRewindComboPressed = FALSE;
        s_bSaveComboPressed = FALSE;
        s_bLoadComboPressed = FALSE;
    }

    static u16 last_pad1 = 0xFFFF;
    static u16 last_pad2 = 0xFFFF;

    boolean use_dev0_as_pad1 = TRUE;
    boolean single_gamepad_connected = FALSE;

    if (s_pThis != nullptr) {
        if (s_pThis->m_pGamePad[0] != nullptr && s_pThis->m_pGamePad[1] != nullptr) {
            unsigned port0 = s_pThis->m_pGamePad[0]->GetDevice()->GetHubPortNumber();
            unsigned port1 = s_pThis->m_pGamePad[1]->GetDevice()->GetHubPortNumber();
            if (port0 > port1) {
                use_dev0_as_pad1 = FALSE;
            }
        } else if (s_pThis->m_pGamePad[0] == nullptr && s_pThis->m_pGamePad[1] != nullptr) {
            use_dev0_as_pad1 = FALSE;
            single_gamepad_connected = TRUE;
        } else if (s_pThis->m_pGamePad[0] != nullptr && s_pThis->m_pGamePad[1] == nullptr) {
            single_gamepad_connected = TRUE;
        }
    }

    if (single_gamepad_connected) {
        if (pad != last_pad1) {
            last_pad1 = pad;
            g_SharedState.pad1 = pad;
            DataMemBarrier();
        }
        if (g_SharedState.pad2 != 0) {
            last_pad2 = 0;
            g_SharedState.pad2 = 0;
            DataMemBarrier();
        }
        return;
    }

    boolean is_pad1 = (nDeviceIndex == 0) ? use_dev0_as_pad1 : !use_dev0_as_pad1;

    if (is_pad1) {
        if (pad != last_pad1) {
            last_pad1 = pad;
            g_SharedState.pad1 = pad;
            DataMemBarrier();
        }
    } else {
        if (pad != last_pad2) {
            last_pad2 = pad;
            g_SharedState.pad2 = pad;
            DataMemBarrier();
        }
    }
}

void CKernel::GamePadRemovedHandler(CDevice *pDevice, void *pContext) {
    CKernel *pThis = (CKernel *)pContext;
    pThis->m_Logger.Write("input", LogDebug, "USB Gamepad removed");
    for (int i = 0; i < 2; i++) {
        if (pThis->m_pGamePad[i] == pDevice) {
            pThis->m_pGamePad[i] = nullptr;
        }
    }
}

void CKernel::KeyboardStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6]) {
    u16 pad = 0;
    boolean escape = FALSE;

    for (unsigned i = 0; i < 6; i++) {
        unsigned char key = RawKeys[i];
        if (key == 0) continue;

        // Map keys to SNES format: SELECT, START, R, L, Y, X, B, A, RLDU
        if (key == 0x52) pad |= (1 << 0); // Up arrow
        if (key == 0x51) pad |= (1 << 1); // Down arrow
        if (key == 0x50) pad |= (1 << 2); // Left arrow
        if (key == 0x4F) pad |= (1 << 3); // Right arrow

        if (g_SharedState.in_menu || g_SharedState.active_emu_mode == EmuMode_SNES || g_SharedState.active_emu_mode == EmuMode_NES || g_SharedState.active_emu_mode == EmuMode_PCE) {
            if (key == 0x1B) pad |= (1 << 4); // X key -> SNES A (bit 4)
            if (key == 0x1D) pad |= (1 << 5); // Z key -> SNES B (bit 5)
            if (key == 0x19) pad |= (1 << 6); // V key -> SNES X (bit 6)
            if (key == 0x06) pad |= (1 << 7); // C key -> SNES Y (bit 7)
            if (key == 0x04) pad |= (1 << 8); // A key -> SNES L (bit 8)
            if (key == 0x16) pad |= (1 << 9); // S key -> SNES R (bit 9)
            if (key == 0x28) pad |= (1 << 10); // Enter -> SNES Start (bit 10)
            if (key == 0x2C) pad |= (1 << 11); // Space -> SNES Select (bit 11)
        } else if (g_SharedState.active_emu_mode == EmuMode_SMS) {
            if (key == 0x1D) pad |= (1 << 4); // Z -> SMS Button 1
            if (key == 0x1B) pad |= (1 << 5); // X -> SMS Button 2
            if (key == 0x06) pad |= (1 << 4); // C -> SMS Button 1
            if (key == 0x19) pad |= (1 << 5); // V -> SMS Button 2
            if (key == 0x28) pad |= (1 << 7); // Enter -> SMS Pause
            if (key == 0x2C) pad |= (1 << 11); // Space -> Select
        } else {
            if (key == 0x1D) pad |= (1 << 6); // Z -> A
            if (key == 0x1B) pad |= (1 << 4); // X -> B
            if (key == 0x06) pad |= (1 << 5); // C -> C
            if (key == 0x28) pad |= (1 << 7); // Enter -> Start
            if (key == 0x04) pad |= (1 << 10); // A -> X
            if (key == 0x16) pad |= (1 << 9);  // S -> Y
            if (key == 0x07) pad |= (1 << 8);  // D -> Z
            if (key == 0x2C) pad |= (1 << 11); // Space -> Mode
        }

        if (key == 0x29) escape = TRUE; // Escape -> return to menu
        static boolean s_bF5Pressed = FALSE;
        static boolean s_bF6Pressed = FALSE;
        static boolean s_bF8Pressed = FALSE;

        if (key == 0x3E) { // F5 -> save state
            if (!s_bF5Pressed) {
                s_bF5Pressed = TRUE;
                g_SharedState.save_state_requested = TRUE;
            }
        } else {
            s_bF5Pressed = FALSE;
        }

        if (key == 0x3F) { // F6 -> rewind state
            if (!s_bF6Pressed) {
                s_bF6Pressed = TRUE;
                g_SharedState.rewind_requested = TRUE;
            }
        } else {
            s_bF6Pressed = FALSE;
        }

        if (key == 0x41) { // F8 -> load state
            if (!s_bF8Pressed) {
                s_bF8Pressed = TRUE;
                g_SharedState.load_state_requested = TRUE;
            }
        } else {
            s_bF8Pressed = FALSE;
        }
    }

    static u16 last_kbd_pad = 0xFFFF;
    if (pad != last_kbd_pad) {
        last_kbd_pad = pad;
        g_SharedState.pad1 = pad;
        DataMemBarrier();
    }
    if (escape) {
        g_SharedState.escape_pressed = TRUE;
        DataMemBarrier();
    }
}

void CKernel::KeyboardRemovedHandler(CDevice *pDevice, void *pContext) {
    CKernel *pThis = (CKernel *)pContext;
    pThis->m_Logger.Write("input", LogDebug, "USB Keyboard removed");
    pThis->m_pKeyboard = nullptr;
}
