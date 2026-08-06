#ifndef VIDEO_UTILS_H
#define VIDEO_UTILS_H

#include <circle/types.h>

// Fast RGB565 blending. Deliberately branchless: the weighted average is exact at
// w2==0 (result==c1) and w2==32 (result==c2) since 32 is a power of two, so special
// casing those weights changes nothing but adds a per-pixel data-dependent branch.
// Measured ~1.3-3x faster than the branching form once the compiler can vectorize
// the straight-line arithmetic across pixels.
static inline u16 BlendRGB565(u16 c1, u16 c2, u32 w2) {
    u32 w1 = 32 - w2;
    u32 rb = (((c1 & 0xF81F) * w1 + (c2 & 0xF81F) * w2) >> 5) & 0xF81F;
    u32 g  = (((c1 & 0x07E0) * w1 + (c2 & 0x07E0) * w2) >> 5) & 0x07E0;
    return (u16)(rb | g);
}

// Applies a horizontal Sharp-Bilinear scale table (idx1/idx2/weight, as built by
// each kernel's UpdateScaleTable) to one scanline. Shared so all four kernels use
// the same optimized loop instead of hand-duplicating it.
static inline void ScaleLineRGB565(u16 * __restrict dest, const u16 * __restrict src,
                                    const u16 * __restrict idx1, const u16 * __restrict idx2,
                                    const u8 * __restrict weight, int count) {
    for (int x = 0; x < count; x++) {
        dest[x] = BlendRGB565(src[idx1[x]], src[idx2[x]], weight[x]);
    }
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
