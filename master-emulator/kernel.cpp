#include "kernel.h"
#include <circle/usb/usbdevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/timer.h>
#include <circle/alloc.h>
#include <circle/font.h>
#ifdef __cplusplus
extern "C" {
#endif
#define UTYPES_DEFINED 1
#include <pico/pico_int.h>
#ifdef __cplusplus
}
#endif

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define COLOR15(red, green, blue) (((red) & 0x1F) << 10 | ((green) & 0x1F) << 5 | ((blue) & 0x1F))

static const char FromKernel[] = "kernel";

// Global shared state
SharedState g_SharedState;
FATFS *g_pFileSystem = nullptr;

static CKernel *s_pThis = nullptr;

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
        for (int x = x1 + b; x <= x2 - b; x++) {
            top_row[x] = color;
            bottom_row[x] = color;
        }
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

#if AARCH == 32
static void vfp_init(void) {
    unsigned nCACR;
    __asm volatile ("mrc p15, 0, %0, c1, c0, 2" : "=r" (nCACR));
    nCACR |= 3 << 20;
    nCACR |= 3 << 22;
    __asm volatile ("mcr p15, 0, %0, c1, c0, 2" : : "r" (nCACR));
    __asm volatile ("isb" ::: "memory");

#define VFP_FPEXC_EN    (1 << 30)
    __asm volatile ("fmxr fpexc, %0" : : "r" (VFP_FPEXC_EN));

#define VFP_FPSCR_FZ    (1 << 24)
#define VFP_FPSCR_DN    (1 << 25)
    __asm volatile ("fmxr fpscr, %0" : : "r" (VFP_FPSCR_FZ | VFP_FPSCR_DN));
}
#endif

// CEmulatorMultiCore Implementation
CEmulatorMultiCore::CEmulatorMultiCore(CMemorySystem *pMemorySystem, CKernel *pKernel)
    : CMultiCoreSupport(pMemorySystem),
      m_pKernel(pKernel)
{
}

void CEmulatorMultiCore::Run(unsigned nCore) {
#if AARCH == 32
    vfp_init();
#endif

    switch (nCore) {
        case 1:
            m_pKernel->RunVideoDomain();
            break;
        case 2:
            m_pKernel->RunAudioDomain();
            break;
        case 3:
            m_pKernel->RunInputDomain();
            break;
        default:
            break;
    }
}

CKernel::CKernel(void)
    : m_Screen(m_Options.GetWidth(), m_Options.GetHeight()),
      m_Timer(&m_Interrupt),
      m_Logger(m_Options.GetLogLevel(), &m_Timer),
      m_EMMC(&m_Interrupt, &m_Timer, &m_ActLED),
      m_USBHCI(&m_Interrupt, &m_Timer, TRUE),
      m_MultiCore(CMemorySystem::Get(), this),
      m_pKeyboard(nullptr),
      m_pOSDMenu(nullptr),
      m_pEmuOrchestrator(nullptr),
      m_ShutdownMode(ShutdownNone),
      m_PowerPin(3, GPIOModeInput),
      m_ResetPin(2, GPIOModeInput),
      m_LedPin(14, GPIOModeOutput),
      m_PowerEnPin(4, GPIOModeOutput)
{
    s_pThis = this;
    m_pGamePad[0] = nullptr;
    m_pGamePad[1] = nullptr;
    m_ActLED.Blink(5);
}

CKernel::~CKernel(void) {
    if (m_pEmuOrchestrator != nullptr) {
        delete m_pEmuOrchestrator;
        m_pEmuOrchestrator = nullptr;
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

    if (bOK) bOK = m_Interrupt.Initialize();
    if (bOK) bOK = m_Timer.Initialize();

    m_Logger.Write(FromKernel, LogNotice, "Initializing BCM FrameBuffer (%ux%u)...", m_Options.GetWidth(), m_Options.GetHeight());
    if (bOK) bOK = m_Screen.Initialize();
    if (bOK) bOK = m_EMMC.Initialize();

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

    if (bOK) {
        m_Logger.Write(FromKernel, LogNotice, "Mounting SD card filesystem...");
        FRESULT res = f_mount(&m_FileSystem, "SD:", 1);
        if (res != FR_OK) {
            m_Logger.Write(FromKernel, LogError, "Failed to mount SD card filesystem: %d", res);
            bOK = FALSE;
        } else {
            g_pFileSystem = &m_FileSystem;
        }
    }

    if (bOK) {
        m_pOSDMenu = new COSDMenu(&m_FileSystem);
        if (!m_pOSDMenu || !m_pOSDMenu->Initialize()) {
            m_Logger.Write(FromKernel, LogPanic, "Failed to initialize OSD Menu");
            bOK = FALSE;
        }
    }

    if (bOK) {
        m_pEmuOrchestrator = new CEmuOrchestrator(&m_FileSystem);
        if (!m_pEmuOrchestrator || !m_pEmuOrchestrator->Initialize()) {
            m_Logger.Write(FromKernel, LogPanic, "Failed to initialize SMS Emulator Orchestrator");
            bOK = FALSE;
        }
    }

    if (bOK) bOK = m_USBHCI.Initialize();

    if (bOK) {
        m_Logger.Write(FromKernel, LogNotice, "Initializing Multicore Engine...");
        bOK = m_MultiCore.Initialize();
    }

    return bOK;
}

TShutdownMode CKernel::Run(void) {
    m_Logger.Write(FromKernel, LogNotice, "Starting Sega Master System Bare-Metal Emulator System Loop...");
    RunOrchestrator();
    return m_ShutdownMode;
}

void CKernel::RunOrchestrator() {
    m_Logger.Write("orchestrator", LogNotice, "Core 0: Orchestrator Domain Active");

    u64 last_menu_time = 0;
    u64 menu_enter_time = CTimer::GetClockTicks64();
    boolean just_entered_menu = TRUE;

    while (m_ShutdownMode == ShutdownNone) {
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
        if (g_SharedState.in_menu) {
            if (just_entered_menu) {
                menu_enter_time = CTimer::GetClockTicks64();
                just_entered_menu = FALSE;
            }

            u64 current_time = CTimer::GetClockTicks64();
            if (current_time - menu_enter_time < 2000000) {
                g_SharedState.pad1 = 0;
                g_SharedState.pad2 = 0;
                CTimer::SimpleMsDelay(10);
                continue;
            }

            if (current_time - last_menu_time >= 150000) {
                last_menu_time = current_time;

                u16 pad1 = g_SharedState.pad1;
                u16 pad2 = g_SharedState.pad2;
                u16 pad_combined = pad1 | pad2;

                if (pad_combined & (1 << 0)) { // Up
                    m_pOSDMenu->MoveUp();
                } else if (pad_combined & (1 << 1)) { // Down
                    m_pOSDMenu->MoveDown();
                } else if (pad_combined & (1 << 2)) { // Left
                    m_pOSDMenu->MoveLeft();
                } else if (pad_combined & (1 << 3)) { // Right
                    m_pOSDMenu->MoveRight();
                } else if (pad_combined & (1 << 10)) { // Gamesir X -> Unfavorite
                    m_pOSDMenu->UnfavoriteCurrent();
                } else if (pad_combined & (1 << 9)) { // Gamesir Y -> Favorite
                    m_pOSDMenu->FavoriteCurrent();
                } else if (pad_combined & ((1 << 4) | (1 << 5) | (1 << 6) | (1 << 7))) { // Launch Game
                    const char *pRomName = m_pOSDMenu->GetSelectedRom();
                    unsigned nRomSize = m_pOSDMenu->GetSelectedRomSize();
                    if (pRomName != nullptr && nRomSize > 0) {
                        m_Logger.Write("orchestrator", LogNotice, "Starting game: %s (size %u)", pRomName, nRomSize);
                        char fullPath[256];
                        snprintf(fullPath, sizeof(fullPath), "SD:/roms/%s", pRomName);
                        
                        memset((void *)g_SharedState.emu_frame_buffer, 0, sizeof(g_SharedState.emu_frame_buffer));
                        g_SharedState.emu_write_idx = 0;
                        g_SharedState.emu_read_idx = 0;
                        g_SharedState.start_line[0] = 8;
                        g_SharedState.game_w[0] = 256;
                        g_SharedState.game_h[0] = 224;
                        g_SharedState.start_line[1] = 8;
                        g_SharedState.game_w[1] = 256;
                        g_SharedState.game_h[1] = 224;
                        g_SharedState.video_frame_ready = FALSE;
                        DataMemBarrier();

                        if (m_pEmuOrchestrator->LoadROM(fullPath, nRomSize)) {
                            g_SharedState.audio_ring_buffer.Init();
                            g_SharedState.in_menu = FALSE;
                            g_SharedState.escape_pressed = FALSE;
                            just_entered_menu = TRUE;
                        }
                    }
                }
            }
            CTimer::SimpleMsDelay(10);
        } else {
            // Run emulator frame
            m_pEmuOrchestrator->RunFrame();

            // Check if user requested return to OSD menu
            if (g_SharedState.escape_pressed) {
                g_SharedState.in_menu = TRUE;
                g_SharedState.escape_pressed = FALSE;
                m_pOSDMenu->Update();
                just_entered_menu = TRUE;
            }

            // Save / load / rewind states
            if (g_SharedState.save_state_requested) {
                g_SharedState.save_state_requested = FALSE;
                m_pEmuOrchestrator->SaveState(0);
                m_ActLED.Blink(3, 50, 50);
            }
            if (g_SharedState.load_state_requested) {
                g_SharedState.load_state_requested = FALSE;
                m_pEmuOrchestrator->LoadState(0);
                m_ActLED.Blink(3, 50, 50);
            }
            if (g_SharedState.rewind_requested) {
                g_SharedState.rewind_requested = FALSE;
                m_pEmuOrchestrator->RewindState();
                m_ActLED.Blink(1, 50, 50);
            }

            // Lock to 60/50 FPS
            s64 frame_time = 16666; // default 60 FPS (16.666 ms)
            if (m_pEmuOrchestrator->IsPAL()) {
                frame_time = 20000; // 50 FPS for PAL (20 ms)
            }

            static u64 last_time = CTimer::GetClockTicks64();
            u64 current_time = CTimer::GetClockTicks64();
            s64 elapsed = current_time - last_time;
            if (elapsed < frame_time) {
                CTimer::SimpleusDelay(frame_time - elapsed);
            }
            last_time = CTimer::GetClockTicks64();
        }
    }

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
    if (nPitch == SCREEN_WIDTH) {
        memcpy(pBuf, pBackBuffer, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u16));
    } else {
        for (unsigned y = 0; y < SCREEN_HEIGHT; y++) {
            memcpy(pBuf + y * nPitch, pBackBuffer + y * SCREEN_WIDTH, SCREEN_WIDTH * sizeof(u16));
        }
    }
    if (g_SharedState.screensaver_active) {
        if (nPitch == SCREEN_WIDTH) {
            u64 *p64 = (u64 *)pBuf;
            size_t count64 = (SCREEN_WIDTH * SCREEN_HEIGHT) / 4;
            for (size_t i = 0; i < count64; i++) {
                p64[i] = (p64[i] >> 1) & 0x7BEF7BEF7BEF7BEFULL;
            }
        } else {
            for (unsigned y = 0; y < SCREEN_HEIGHT; y++) {
                u64 *p64 = (u64 *)(pBuf + y * nPitch);
                size_t count64 = SCREEN_WIDTH / 4;
                for (size_t i = 0; i < count64; i++) {
                    p64[i] = (p64[i] >> 1) & 0x7BEF7BEF7BEF7BEFULL;
                }
            }
        }
    }
}

void CKernel::RunVideoDomain() {
    m_Logger.Write("video", LogNotice, "Core 1: Video Engine Active");

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

    // Clear screen to pure black
    DrawRect(pBackBuffer, SCREEN_WIDTH, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, COLOR15(0, 0, 0));
    CopyBackBufferToFB(pBuf, nPitch, pBackBuffer);

    boolean was_in_menu = TRUE;
    while (1) {
        // Screensaver logic:
        // 1. If controller is UNTOUCHED for 60s -> enable screensaver (dim screen by 50%).
        // 2. If user presses a button / touches controller -> disable screensaver & reset 60s period.
        u64 now = CTimer::GetClockTicks64();
        if (g_SharedState.last_input_time == 0) {
            g_SharedState.last_input_time = now;
        }

        u16 pad1 = g_SharedState.pad1;
        u16 pad2 = g_SharedState.pad2;
        static u16 s_last_check_pad1 = 0;
        static u16 s_last_check_pad2 = 0;

        boolean input_touched = (pad1 != 0 || pad2 != 0 || pad1 != s_last_check_pad1 || pad2 != s_last_check_pad2 ||
                                 g_SharedState.escape_pressed || g_SharedState.save_state_requested ||
                                 g_SharedState.load_state_requested || g_SharedState.rewind_requested);

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
            if (!g_SharedState.screensaver_active && (now - g_SharedState.last_input_time >= 60000000ULL)) {
                g_SharedState.screensaver_active = TRUE;
                if (g_SharedState.in_menu) {
                    g_SharedState.menu_needs_redraw = TRUE;
                }
            }
        }
        if (m_ShutdownMode != ShutdownNone) {
            DrawRect(pBackBuffer, SCREEN_WIDTH, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, COLOR15(0, 0, 0));
            int box_w = 400, box_h = 120;
            int box_x1 = (SCREEN_WIDTH - box_w) / 2;
            int box_y1 = (SCREEN_HEIGHT - box_h) / 2;
            int box_x2 = box_x1 + box_w - 1;
            int box_y2 = box_y1 + box_h - 1;

            DrawRect(pBackBuffer, SCREEN_WIDTH, box_x1, box_y1, box_x2, box_y2, COLOR15(2, 3, 5));
            DrawBox(pBackBuffer, SCREEN_WIDTH, box_x1, box_y1, box_x2, box_y2, COLOR15(8, 12, 16), 2);

            const char *shutdown_msg = (m_ShutdownMode == ShutdownReboot) ? "REBOOTING SYSTEM..." : "SHUTTING DOWN...";
            int msg_w = strlen(shutdown_msg) * 8;
            int msg_x = (SCREEN_WIDTH - msg_w) / 2;
            int msg_y = (SCREEN_HEIGHT - 16) / 2;

            DrawString(pBackBuffer, SCREEN_WIDTH, shutdown_msg, msg_x, msg_y, COLOR15(24, 28, 28), COLOR15(2, 3, 5));
            CopyBackBufferToFB(pBuf, nPitch, pBackBuffer);
            CTimer::SimpleMsDelay(2000);
            break;
        }

        if (g_SharedState.in_menu) {
            if (!was_in_menu) {
                was_in_menu = TRUE;
                g_SharedState.menu_needs_redraw = TRUE;
            }

            if (g_SharedState.menu_needs_redraw) {
                g_SharedState.menu_needs_redraw = FALSE;

                int num_lines = g_SharedState.menu_num_lines;
                int selected = g_SharedState.menu_selected_idx;

                DrawRect(pBackBuffer, SCREEN_WIDTH, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, COLOR15(0, 0, 0));

                int x1 = 40, y1 = 15, x2 = SCREEN_WIDTH - 40, y2 = SCREEN_HEIGHT - 15;
                DrawRect(pBackBuffer, SCREEN_WIDTH, x1, y1, x2, y2, COLOR15(2, 3, 5));
                DrawBox(pBackBuffer, SCREEN_WIDTH, x1, y1, x2, y2, COLOR15(8, 12, 16), 2);

                char title_str[64];
                snprintf(title_str, sizeof(title_str), "### Master System - 5 in 1 Emulator ###");
                int title_w = strlen(title_str) * 8;
                int title_x = (SCREEN_WIDTH - title_w) / 2;
                DrawString(pBackBuffer, SCREEN_WIDTH, title_str, title_x, y1 + 15, COLOR15(22, 24, 26), 0);
                
                if (num_lines > 0) {
                    char count_str[32];
                    snprintf(count_str, sizeof(count_str), "[%d/%d]", selected + 1, num_lines);
                    DrawString(pBackBuffer, SCREEN_WIDTH, count_str, x2 - 100, y1 + 15, COLOR15(22, 24, 26), COLOR15(2, 3, 5));
                }
                
                DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 20, y1 + 40, x2 - 20, y1 + 41, COLOR15(4, 6, 8));

                int active_tab = g_SharedState.menu_active_tab;
                int tab_width = 80;
                int tab_start_x = x1 + 20;

                for (int t = 0; t < 6; t++) {
                    int tx1 = tab_start_x + t * tab_width;
                    int tx2 = tx1 + tab_width - 4;
                    u16 bg_color = (t == active_tab) ? COLOR15(8, 12, 16) : COLOR15(3, 4, 6);
                    u16 text_color = (t == active_tab) ? COLOR15(31, 31, 31) : COLOR15(12, 14, 16);

                    DrawRect(pBackBuffer, SCREEN_WIDTH, tx1, y1 + 43, tx2, y1 + 61, bg_color);
                    if (t == active_tab) {
                        DrawBox(pBackBuffer, SCREEN_WIDTH, tx1, y1 + 43, tx2, y1 + 61, COLOR15(14, 18, 22), 1);
                    }

                    const char *tab_name = g_SharedState.menu_tab_names[t];
                    int text_w = strlen(tab_name) * 8;
                    int text_x = tx1 + (tab_width - 4 - text_w) / 2;
                    DrawString(pBackBuffer, SCREEN_WIDTH, tab_name, text_x, y1 + 49, text_color, bg_color);
                }

                DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 20, y1 + 63, x2 - 20, y1 + 64, COLOR15(4, 6, 8));

                if (num_lines == 0) {
                    if (active_tab == 1) {
                        DrawString(pBackBuffer, SCREEN_WIDTH, "No favorites added! Press Y on a game to favorite it.", 116, 180, COLOR15(22, 24, 26), 0);
                    } else {
                        DrawString(pBackBuffer, SCREEN_WIDTH, "No SMS ROMs found! Copy .sms files to roms/mastersystem.", 100, 180, COLOR15(24, 14, 10), 0);
                    }
                } else {
                    int visible_lines = 14;
                    int start_idx = selected - visible_lines / 2;
                    if (start_idx < 0) start_idx = 0;
                    if (start_idx + visible_lines > num_lines) {
                        start_idx = num_lines - visible_lines;
                        if (start_idx < 0) start_idx = 0;
                    }

                    for (int i = start_idx; i < num_lines && i < start_idx + visible_lines; i++) {
                        int line_y = (i - start_idx) * 22;
                        int row_y = y1 + 75 + line_y;

                        if (i == selected) {
                            DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 20, row_y - 2, x2 - 20, row_y + 17, COLOR15(6, 9, 13));
                            DrawBox(pBackBuffer, SCREEN_WIDTH, x1 + 20, row_y - 2, x2 - 20, row_y + 17, COLOR15(12, 16, 20), 1);
                            DrawString(pBackBuffer, SCREEN_WIDTH, g_SharedState.menu_lines[i], x1 + 25, row_y, COLOR15(31, 31, 31), COLOR15(6, 9, 13));
                        } else {
                            u16 fg = (g_SharedState.menu_lines[i][0] == '*') ? COLOR15(31, 28, 10) : COLOR15(16, 18, 20);
                            DrawString(pBackBuffer, SCREEN_WIDTH, g_SharedState.menu_lines[i], x1 + 20, row_y, fg, 0);
                        }
                    }
                }

                DrawRect(pBackBuffer, SCREEN_WIDTH, x1 + 20, y2 - 35, x2 - 20, y2 - 34, COLOR15(4, 6, 8));

                const char *footer_text = "A/B: Start | Y: Fav | X: Unfav | D-Pad: Move | START+SELECT: Reset";
                int footer_w = strlen(footer_text) * 8;
                int footer_x = (SCREEN_WIDTH - footer_w) / 2;
                DrawString(pBackBuffer, SCREEN_WIDTH, footer_text, footer_x, y2 - 20, COLOR15(12, 14, 16), 0);

                CopyBackBufferToFB(pBuf, nPitch, pBackBuffer);
            }
            CTimer::SimpleMsDelay(10);
        } else {
            if (was_in_menu) {
                was_in_menu = FALSE;
                DrawRect(pBackBuffer, SCREEN_WIDTH, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, COLOR15(0, 0, 0));
                CopyBackBufferToFB(pBuf, nPitch, pBackBuffer);
            }

            if (g_SharedState.video_frame_ready) {
                g_SharedState.video_frame_ready = FALSE;
                DataMemBarrier();

                int read_idx = g_SharedState.emu_read_idx;
                int game_w = g_SharedState.game_w[read_idx];
                int game_h = g_SharedState.game_h[read_idx];
                int start_l = g_SharedState.start_line[read_idx];

                if (game_w <= 0) game_w = 256;
                if (game_h <= 0) game_h = 192;
                // In SMS 192-line mode, Pico reports the 192 active lines, but some games use
                // additional visible top/bottom area from the 224-line window.
                if (game_h == 192 && start_l >= 16) {
                    start_l -= 16;
                    game_h = 224;
                }
                if (start_l < 0) start_l = 0;
                if (game_h > 320 - start_l) game_h = 320 - start_l;
                if (game_h <= 0) game_h = 192;

                const u16 *pSrc = g_SharedState.emu_frame_buffer[read_idx];
                pSrc += start_l * 320 + 32;

                UpdateScaleTable(game_w);
                u16 line_buf[640];

                int dst_y_offset = (SCREEN_HEIGHT - 480) / 2;
                int dst_x_offset = (SCREEN_WIDTH - 640) / 2;

                int last_src_y = -1;
                for (int y = 0; y < 480; y++) {
                    int src_y = (y * game_h) / 480;
                    u16 *dst_row = pBuf + (dst_y_offset + y) * nPitch + dst_x_offset;

                    if (src_y == last_src_y) {
                        memcpy(dst_row, line_buf, 640 * sizeof(u16));
                        continue;
                    }
                    last_src_y = src_y;

                    const u16 *src_row = pSrc + src_y * 320;

                    for (int x = 0; x < 640; x++) {
                        u16 i1 = s_ScaleTableCache.idx1[x];
                        u16 i2 = s_ScaleTableCache.idx2[x];
                        u8 w = s_ScaleTableCache.weight[x];

                        u16 c1 = src_row[i1];
                        if (w == 0 || i1 == i2) {
                            line_buf[x] = c1;
                        } else {
                            u16 c2 = src_row[i2];
                            u32 r1 = (c1 >> 11) & 0x1F;
                            u32 g1 = (c1 >> 5) & 0x3F;
                            u32 b1 = c1 & 0x1F;

                            u32 r2 = (c2 >> 11) & 0x1F;
                            u32 g2 = (c2 >> 5) & 0x3F;
                            u32 b2 = c2 & 0x1F;

                            u32 r = (r1 * (32 - w) + r2 * w) >> 5;
                            u32 g = (g1 * (32 - w) + g2 * w) >> 5;
                            u32 b = (b1 * (32 - w) + b2 * w) >> 5;

                            line_buf[x] = (r << 11) | (g << 5) | b;
                        }
                    }

                    memcpy(dst_row, line_buf, 640 * sizeof(u16));
                }
                if (g_SharedState.screensaver_active) {
                    if (nPitch == SCREEN_WIDTH) {
                        u64 *p64 = (u64 *)pBuf;
                        size_t count64 = (SCREEN_WIDTH * SCREEN_HEIGHT) / 4;
                        for (size_t i = 0; i < count64; i++) {
                            p64[i] = (p64[i] >> 1) & 0x7BEF7BEF7BEF7BEFULL;
                        }
                    } else {
                        for (unsigned y = 0; y < SCREEN_HEIGHT; y++) {
                            u64 *p64 = (u64 *)(pBuf + y * nPitch);
                            size_t count64 = SCREEN_WIDTH / 4;
                            for (size_t i = 0; i < count64; i++) {
                                p64[i] = (p64[i] >> 1) & 0x7BEF7BEF7BEF7BEFULL;
                            }
                        }
                    }
                }
            }
            CTimer::SimpleusDelay(100);
        }
    }
}

void CKernel::RunAudioDomain() {
    m_Logger.Write("audio", LogNotice, "Core 2: Audio Engine Active");

    CSoundBaseDevice *pSoundDevice = nullptr;

    const char *pSoundDeviceName = m_Options.GetSoundDevice();
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

    if (!pSoundDevice->AllocateQueue(100)) {
        m_Logger.Write("audio", LogPanic, "Cannot allocate sound queue");
        delete pSoundDevice;
        return;
    }

    pSoundDevice->SetWriteFormat(SoundFormatSigned16, 2);
    if (!pSoundDevice->Start()) {
        if (bUsingHDMI) {
            m_Logger.Write("audio", LogWarning, "Cannot start HDMI sound device. Falling back to PWM.");
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

    while (m_ShutdownMode == ShutdownNone) {
        unsigned avail = g_SharedState.audio_ring_buffer.GetAvailable();
        if (avail > 0) {
            if (avail > 1024) avail = 1024;
            unsigned read = g_SharedState.audio_ring_buffer.Read(local_buf, avail);
            if (g_SharedState.screensaver_active) {
                memset(local_buf, 0, read * 4);
            }
            pSoundDevice->Write(local_buf, read * 4);
        } else {
            CTimer::SimpleusDelay(200);
        }
    }
}

void CKernel::RunInputDomain() {
    m_Logger.Write("input", LogNotice, "Core 3: Input Engine Active");

    while (m_ShutdownMode == ShutdownNone) {
        if (m_PowerPin.Read() == LOW) {
            m_Logger.Write(FromKernel, LogNotice, "Hardware Power Button Shutdown Triggered!");
            m_LedPin.Write(LOW);
            m_PowerEnPin.Write(LOW);
            m_ShutdownMode = ShutdownHalt;
            break;
        }

        if (m_ResetPin.Read() == LOW) {
            m_Logger.Write(FromKernel, LogNotice, "Hardware Reset Button Triggered!");
            m_ShutdownMode = ShutdownReboot;
            break;
        }

        m_USBHCI.UpdatePlugAndPlay();

        // Always scan current devices. Some boot-time attached gamepads may
        // already be present before an update event is surfaced.
        for (unsigned nDevice = 1; nDevice <= 2; nDevice++) {
            // Different controllers can enumerate under different aliases
            // depending on USB/Bluetooth mode and boot timing.
            CUSBGamePadDevice *pGamePad = (CUSBGamePadDevice *)m_DeviceNameService.GetDevice("upad", nDevice, FALSE);
            if (pGamePad == nullptr) {
                pGamePad = (CUSBGamePadDevice *)m_DeviceNameService.GetDevice("gpad", nDevice, FALSE);
            }
            if (pGamePad != m_pGamePad[nDevice - 1]) {
                m_pGamePad[nDevice - 1] = pGamePad;
                if (pGamePad != nullptr) {
                    m_Logger.Write("input", LogNotice, "GamePad %u connected!", nDevice);
                    m_pGamePad[nDevice - 1]->RegisterStatusHandler(GamePadStatusHandler);
                    m_pGamePad[nDevice - 1]->RegisterRemovedHandler(GamePadRemovedHandler, this);
                }
            }
        }

        CUSBKeyboardDevice *pKeyboard = (CUSBKeyboardDevice *)m_DeviceNameService.GetDevice("ukbd1", FALSE);
        if (pKeyboard != m_pKeyboard) {
            m_pKeyboard = pKeyboard;
            if (pKeyboard != nullptr) {
                m_Logger.Write("input", LogNotice, "Keyboard connected!");
                m_pKeyboard->RegisterKeyStatusHandlerRaw(KeyboardStatusHandlerRaw);
                m_pKeyboard->RegisterRemovedHandler(KeyboardRemovedHandler, this);
            }
        }

        CTimer::SimpleMsDelay(10);
    }
}

void CKernel::GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState) {
    u16 pad = 0;

    // D-Pad buttons
    if (pState->buttons & GamePadButtonUp)    pad |= (1 << 0); // Up
    if (pState->buttons & GamePadButtonDown)  pad |= (1 << 1); // Down
    if (pState->buttons & GamePadButtonLeft)  pad |= (1 << 2); // Left
    if (pState->buttons & GamePadButtonRight) pad |= (1 << 3); // Right

    // D-Pad hats fallback
    if (pState->nhats > 0 && !(pad & 0xF)) {
        int hat = pState->hats[0];
        if (hat >= 0 && hat <= 7) {
            if (hat == 0 || hat == 1 || hat == 7) pad |= (1 << 0); // Up
            if (hat == 3 || hat == 4 || hat == 5) pad |= (1 << 1); // Down
            if (hat == 5 || hat == 6 || hat == 7) pad |= (1 << 2); // Left
            if (hat == 1 || hat == 2 || hat == 3) pad |= (1 << 3); // Right
        }
    }

    if (g_SharedState.in_menu) {
        if (pState->buttons & GamePadButtonA)     pad |= (1 << 4) | (1 << 6); // Menu Launch
        if (pState->buttons & GamePadButtonB)     pad |= (1 << 5);           // Menu Launch
        if (pState->buttons & GamePadButtonX)     pad |= (1 << 10);          // Menu Unfavorite
        if (pState->buttons & GamePadButtonY)     pad |= (1 << 9);           // Menu Favorite
        if (pState->buttons & GamePadButtonStart) pad |= (1 << 7);           // Start
        if (pState->buttons & GamePadButtonSelect)pad |= (1 << 11);          // Mode/Select
    } else {
        if (pState->buttons & GamePadButtonA)     pad |= (1 << 4);           // SMS Button 1
        if (pState->buttons & GamePadButtonB)     pad |= (1 << 5);           // SMS Button 2
        if (pState->buttons & GamePadButtonX)     pad |= (1 << 4);           // SMS Button 1
        if (pState->buttons & GamePadButtonY)     pad |= (1 << 5);           // SMS Button 2
        if (pState->buttons & (GamePadButtonLT | GamePadButtonLB)) pad |= (1 << 8); // L shoulder
        if (pState->buttons & (GamePadButtonRT | GamePadButtonRB)) pad |= (1 << 9); // R shoulder
        if (pState->buttons & GamePadButtonStart) pad |= (1 << 7);           // SMS Pause
        if (pState->buttons & GamePadButtonSelect)pad |= (1 << 11);          // Select
    }

    // START + SELECT combo -> Exit to menu
    if ((pState->buttons & (GamePadButtonStart | GamePadButtonPlus)) && (pState->buttons & (GamePadButtonSelect | GamePadButtonMinus))) {
        g_SharedState.escape_pressed = TRUE;
    }

    // SELECT + D-pad / Shoulder combos for state save/load/rewind
    if (pState->buttons & GamePadButtonSelect) {
        if (pad & (1 << 0)) { // D-pad Up -> Rewind state
            g_SharedState.rewind_requested = TRUE;
            pad = 0;
        }
        if ((pad & (1 << 2)) || (pState->buttons & (GamePadButtonLB | GamePadButtonLT))) { // D-pad Left or L Shoulder -> Save state
            g_SharedState.save_state_requested = TRUE;
            pad = 0;
        }
        if ((pad & (1 << 3)) || (pState->buttons & (GamePadButtonRB | GamePadButtonRT))) { // D-pad Right or R Shoulder -> Load state
            g_SharedState.load_state_requested = TRUE;
            pad = 0;
        }
    }

    if (nDeviceIndex == 1) {
        g_SharedState.pad1 = pad;
    } else if (nDeviceIndex == 2) {
        g_SharedState.pad2 = pad;
    }
}

void CKernel::GamePadRemovedHandler(CDevice *pDevice, void *pContext) {
    CKernel *pThis = (CKernel *)pContext;
    pThis->m_Logger.Write("input", LogWarning, "GamePad removed.");
}

void CKernel::KeyboardStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6]) {
    u16 pad = 0;

    for (int i = 0; i < 6; i++) {
        unsigned char key = RawKeys[i];
        if (key == 0) continue;

        if (key == 0x52) pad |= (1 << 0); // Up Arrow
        if (key == 0x51) pad |= (1 << 1); // Down Arrow
        if (key == 0x50) pad |= (1 << 2); // Left Arrow
        if (key == 0x4F) pad |= (1 << 3); // Right Arrow
        if (key == 0x04) pad |= (1 << 4) | (1 << 6); // Key A -> SMS Button 1
        if (key == 0x16) pad |= (1 << 5);           // Key S -> SMS Button 2
        if (key == 0x1D) pad |= (1 << 4) | (1 << 10); // Key Z -> SMS Button 1 / Unfavorite
        if (key == 0x1B) pad |= (1 << 5) | (1 << 9);  // Key X -> SMS Button 2 / Favorite
        if (key == 0x28) pad |= (1 << 7);           // Enter -> SMS Pause
        if (key == 0x2C) pad |= (1 << 11);          // Space -> Select
        if (key == 0x29) g_SharedState.escape_pressed = TRUE; // ESC -> Menu
        if (key == 0x3F) g_SharedState.rewind_requested = TRUE; // F6 -> Rewind state
    }

    g_SharedState.pad1 = pad;
}

void CKernel::KeyboardRemovedHandler(CDevice *pDevice, void *pContext) {
    CKernel *pThis = (CKernel *)pContext;
    pThis->m_Logger.Write("input", LogWarning, "Keyboard removed.");
}
