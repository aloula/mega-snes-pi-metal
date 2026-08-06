#ifndef HEAP_BUCKET_ROUNDING_H
#define HEAP_BUCKET_ROUNDING_H

#include <circle/types.h>

// The heap allocator (see deps/circle/Config.mk HEAP_BLOCK_BUCKET_SIZES) serves any
// allocation from a fixed ladder of bucket sizes and can only reuse a freed block for
// a future allocation that rounds to the SAME bucket. ROM files and save-state sizes
// vary almost continuously from game to game, so allocating each one at its exact byte
// count spreads them across many buckets, and every previously-unseen (rounded) size
// permanently claims a fresh slice of the heap arena the first time it's used.
//
// Rounding up to one of a small, fixed ladder here - matching the largest bucket sizes
// in HEAP_BLOCK_BUCKET_SIZES - means different ROMs of similar size share the same
// bucket instead of each opening a new one, so heap usage converges to a fixed ceiling
// after the first few loads instead of growing with every distinct ROM ever played.
inline size_t RoundUpToCanonicalBufferSize(size_t nSize) {
    static const size_t s_Sizes[] = {
        0x10000,  // 64 KB
        0x40000,  // 256 KB
        0x80000,  // 512 KB
        0x100000, // 1 MB
        0x200000, // 2 MB
        0x400000, // 4 MB
        0x800000, // 8 MB
    };
    for (unsigned i = 0; i < sizeof(s_Sizes) / sizeof(s_Sizes[0]); i++) {
        if (nSize <= s_Sizes[i]) {
            return s_Sizes[i];
        }
    }
    return nSize;
}

#endif
