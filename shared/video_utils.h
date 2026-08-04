#ifndef VIDEO_UTILS_H
#define VIDEO_UTILS_H

#include <circle/types.h>

// Fast RGB565 blending with zero-weight early returns to avoid arithmetic overhead on unscaled pixels
static inline u16 BlendRGB565(u16 c1, u16 c2, u32 w2) {
    if (w2 == 0) return c1;
    if (w2 == 32) return c2;
    u32 w1 = 32 - w2;
    u32 rb = (((c1 & 0xF81F) * w1 + (c2 & 0xF81F) * w2) >> 5) & 0xF81F;
    u32 g  = (((c1 & 0x07E0) * w1 + (c2 & 0x07E0) * w2) >> 5) & 0x07E0;
    return (u16)(rb | g);
}

// 64-bit vectorized 50% screen dimming for screensaver
static inline void DimScanline50(u16 *buf, int count = 640) {
    u64 *p64 = (u64 *)buf;
    int count64 = count / 4;
    for (int i = 0; i < count64; i++) {
        p64[i] = (p64[i] >> 1) & 0x7BEF7BEF7BEF7BEFULL;
    }
}

// Fast black row detection for border cropping
static inline bool IsRowBlack(const u16 *line, int width) {
    if (line[width / 4] != 0) return false;
    if (line[width / 2] != 0) return false;
    if (line[3 * width / 4] != 0) return false;
    return true;
}

#endif // VIDEO_UTILS_H
