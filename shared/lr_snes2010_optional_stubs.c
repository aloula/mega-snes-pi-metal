#include <stdint.h>
#include <stdlib.h>

#include <ff.h>

struct retro_vfs_interface_info;
struct RFILE {
    FIL file;
};

void filestream_vfs_init(const struct retro_vfs_interface_info *info) {
    (void)info;
}

struct RFILE *filestream_open(const char *path, unsigned mode, unsigned hints) {
    struct RFILE *stream;

    (void)mode;
    (void)hints;
    stream = malloc(sizeof(*stream));
    if (stream == NULL || f_open(&stream->file, path, FA_READ) != FR_OK) {
        free(stream);
        return NULL;
    }
    return stream;
}

int64_t filestream_read(struct RFILE *stream, void *data, int64_t len) {
    UINT bytes_read = 0;

    if (stream == NULL || len < 0 || f_read(&stream->file, data, (UINT)len, &bytes_read) != FR_OK) {
        return -1;
    }
    return bytes_read;
}

int filestream_close(struct RFILE *stream) {
    int result;

    if (stream == NULL) {
        return -1;
    }
    result = f_close(&stream->file) == FR_OK ? 0 : -1;
    free(stream);
    return result;
}

int S9xHdPackInit(const char *rom_path) {
    (void)rom_path;
    return 0;
}

void S9xHdPackDeinit(void) {
}

int S9xHdPackActive(void) {
    return 0;
}

int S9xHdPackRecording(void) {
    return 0;
}

uint32_t S9xHdPackScale(void) {
    return 1;
}

void S9xHdPackFrameBegin(void) {
}

void S9xHdPackWrapRenderers(int normal1x1) {
    (void)normal1x1;
}

uint16_t *S9xHdPackComposite(int width, int height, int *out_width, int *out_height, int *out_pitch) {
    (void)width;
    (void)height;
    (void)out_width;
    (void)out_height;
    (void)out_pitch;
    return 0;
}
