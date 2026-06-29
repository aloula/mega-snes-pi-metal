#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <circle/types.h>
#include "audio_ring_buffer.h"

#define MAX_ROMS 4096

enum EmuMode {
    EmuMode_SNES,
    EmuMode_MD
};

struct SharedState {
    volatile EmuMode active_emu_mode __attribute__((aligned(64)));
    volatile u16 pad1 __attribute__((aligned(64)));
    volatile u16 pad2 __attribute__((aligned(64)));
    volatile boolean escape_pressed __attribute__((aligned(64)));
    volatile boolean save_state_requested __attribute__((aligned(64)));
    volatile boolean load_state_requested __attribute__((aligned(64)));
    volatile boolean rewind_requested __attribute__((aligned(64)));

    volatile boolean in_menu __attribute__((aligned(64)));
    volatile boolean menu_needs_redraw __attribute__((aligned(64)));
    char menu_lines[MAX_ROMS][80] __attribute__((aligned(64)));
    int menu_num_lines;
    int menu_selected_idx;
    char menu_tab_names[6][16] __attribute__((aligned(64)));
    int menu_active_tab;

    // Double-buffered emulator frame buffer (512x480, RGB555) to support SNES resolutions up to high-res interlace
    u16 emu_frame_buffer[2][512 * 480] __attribute__((aligned(64)));
    volatile int emu_write_idx;
    volatile int emu_read_idx;
    volatile boolean video_frame_ready __attribute__((aligned(64)));
    volatile int start_line[2];
    volatile int game_w[2];
    volatile int game_h[2];

    // Audio ring buffer
    AudioRingBuffer audio_ring_buffer __attribute__((aligned(64)));
} __attribute__((aligned(64)));

extern SharedState g_SharedState;

#endif
