#ifndef AUDIO_RING_BUFFER_H
#define AUDIO_RING_BUFFER_H

#include <circle/types.h>
#include <circle/string.h>
#include <circle/synchronize.h>
#include <string.h>

class AudioRingBuffer {
public:
    static const unsigned SIZE = 8192;
    static const unsigned MASK = SIZE - 1;

    s16 buffer[SIZE * 2] __attribute__((aligned(64))); // interleaved stereo
    volatile unsigned read_idx __attribute__((aligned(64)));
    volatile unsigned write_idx __attribute__((aligned(64)));

    void Init() {
        read_idx = 0;
        write_idx = 0;
        memset(buffer, 0, sizeof(buffer));
    }

    unsigned GetAvailable() const {
        return (write_idx - read_idx) & MASK;
    }

    unsigned GetFreeSpace() const {
        return (read_idx - write_idx - 1) & MASK;
    }

    void Write(const s16 *samples, unsigned num_stereo_samples) {
        unsigned w_idx = write_idx;
        unsigned r_idx = read_idx;
        unsigned free_space = (r_idx - w_idx - 1) & MASK;
        if (num_stereo_samples > free_space) {
            num_stereo_samples = free_space;
        }
        if (num_stereo_samples == 0) return;

        if (w_idx + num_stereo_samples <= SIZE) {
            memcpy(&buffer[w_idx * 2], samples, num_stereo_samples * 4);
        } else {
            unsigned first_chunk = SIZE - w_idx;
            unsigned second_chunk = num_stereo_samples - first_chunk;
            memcpy(&buffer[w_idx * 2], samples, first_chunk * 4);
            memcpy(&buffer[0], samples + first_chunk * 2, second_chunk * 4);
        }
        DataMemBarrier();
        write_idx = (w_idx + num_stereo_samples) & MASK;
    }

    unsigned Read(s16 *samples, unsigned num_stereo_samples) {
        unsigned w_idx = write_idx;
        unsigned r_idx = read_idx;
        unsigned avail = (w_idx - r_idx) & MASK;
        if (num_stereo_samples > avail) {
            num_stereo_samples = avail;
        }
        if (num_stereo_samples == 0) return 0;

        if (r_idx + num_stereo_samples <= SIZE) {
            memcpy(samples, &buffer[r_idx * 2], num_stereo_samples * 4);
        } else {
            unsigned first_chunk = SIZE - r_idx;
            unsigned second_chunk = num_stereo_samples - first_chunk;
            memcpy(samples, &buffer[r_idx * 2], first_chunk * 4);
            memcpy(samples + first_chunk * 2, &buffer[0], second_chunk * 4);
        }
        DataMemBarrier();
        read_idx = (r_idx + num_stereo_samples) & MASK;
        return num_stereo_samples;
    }
};

#endif
