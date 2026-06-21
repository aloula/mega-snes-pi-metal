#include <circle/timer.h>
#include <circle/time.h>
#include <circle/logger.h>
#define __time_t_defined
#define _TIME_T_DECLARED
#include <circle/string.h>
#include <circle/alloc.h>
#include <circle/util.h>
#include <circle/stdarg.h>
#include <circle/synchronize.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <ff.h>
#include "shared_state.h"

extern FATFS *g_pFileSystem;

struct CFileWrapper {
    FIL file;
    char title[128];
    boolean in_use;
};

static CFileWrapper s_OpenFiles[64];

extern "C" {

// static char dummy_reent[1024];
// struct _reent * _impure_ptr = (struct _reent *)dummy_reent;

char *strdup(const char *s) {
    if (s == nullptr) return nullptr;
    size_t len = strlen(s);
    char *dup = (char *)malloc(len + 1);
    if (dup != nullptr) {
        memcpy(dup, s, len + 1);
    }
    return dup;
}

void lprintf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    int len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
        buf[len-1] = '\0';
        len--;
    }
    CLogger::Get()->Write("orchestrator", LogNotice, "%s", buf);
}

FILE *fopen(const char *pathname, const char *mode) {
    // Find a free wrapper slot
    int slot = -1;
    for (int i = 0; i < 64; i++) {
        if (!s_OpenFiles[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        lprintf("clib_stubs: fopen(%s) failed: all 64 file slots in use", pathname);
        return nullptr;
    }

    BYTE flags = 0;
    if (strchr(mode, 'r')) flags |= FA_READ;
    if (strchr(mode, 'w')) flags |= FA_WRITE | FA_CREATE_ALWAYS;
    if (strchr(mode, 'a')) flags |= FA_WRITE | FA_OPEN_APPEND;
    if (strchr(mode, '+')) flags |= FA_READ | FA_WRITE;

    // Convert potential raw path with leading slash to use drive specifier SD:/
    char fullPath[256];
    if (pathname[0] == '/') {
        snprintf(fullPath, sizeof(fullPath), "SD:%s", pathname);
    } else if (strncmp(pathname, "SD:", 3) != 0) {
        snprintf(fullPath, sizeof(fullPath), "SD:/%s", pathname);
    } else {
        strncpy(fullPath, pathname, sizeof(fullPath) - 1);
        fullPath[sizeof(fullPath) - 1] = '\0';
    }

    FRESULT res = f_open(&s_OpenFiles[slot].file, fullPath, flags);
    if (res != FR_OK) {
        if (res != 4) { // Don't log expected FR_NO_FILE (4) failures for optional patches/saves
            lprintf("clib_stubs: fopen(%s) failed: FR_%d", fullPath, (int)res);
        }
        return nullptr;
    }

    strncpy(s_OpenFiles[slot].title, fullPath, 127);
    s_OpenFiles[slot].title[127] = '\0';
    s_OpenFiles[slot].in_use = TRUE;

    return (FILE *)&s_OpenFiles[slot];
}

int fclose(FILE *stream) {
    if (stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;

    f_close(&w->file);
    w->in_use = FALSE;
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (stream == nullptr) {
        lprintf("clib_stubs: fread(NULL stream)");
        return 0;
    }
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) {
        lprintf("clib_stubs: fread(inactive stream)");
        return 0;
    }

    unsigned bytesToRead = size * nmemb;
    if (bytesToRead == 0) return 0;

    UINT read = 0;
    FRESULT res = f_read(&w->file, ptr, bytesToRead, &read);
    if (res != FR_OK) {
        lprintf("clib_stubs: fread(%s, size=%u, nmemb=%u) failed: FR_%d", w->title, (unsigned)size, (unsigned)nmemb, (int)res);
        return 0;
    }
    return read / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (stream == nullptr) return 0;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return 0;

    unsigned bytesToWrite = size * nmemb;
    if (bytesToWrite == 0) return 0;

    UINT written = 0;
    FRESULT res = f_write(&w->file, ptr, bytesToWrite, &written);
    if (res != FR_OK) {
        return 0;
    }
    return written / size;
}

static int fseek_internal(FILE *stream, int64_t offset, int whence) {
    if (stream == nullptr) {
        lprintf("clib_stubs: fseek(NULL stream)");
        return -1;
    }
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) {
        lprintf("clib_stubs: fseek(inactive stream)");
        return -1;
    }

    int64_t target = 0;
    int64_t current_pos = (int64_t)f_tell(&w->file);
    int64_t file_size = (int64_t)f_size(&w->file);

    if (whence == SEEK_SET) {
        target = offset;
    } else if (whence == SEEK_CUR) {
        target = current_pos + offset;
    } else if (whence == SEEK_END) {
        target = file_size + offset;
    } else {
        lprintf("clib_stubs: fseek(%s) failed: invalid whence %d", w->title, whence);
        return -1;
    }

    if (target < 0) {
        lprintf("clib_stubs: fseek(%s) failed: negative target %lld (offset=%lld, whence=%d, size=%lld, pos=%lld)",
                w->title, (long long)target, (long long)offset, whence, (long long)file_size, (long long)current_pos);
        return -1;
    }

    FRESULT res = f_lseek(&w->file, (FSIZE_t)target);
    if (res != FR_OK) {
        lprintf("clib_stubs: f_lseek(%s, %llu) failed: FR_%d", w->title, (unsigned long long)target, (int)res);
        return -1;
    }
    return 0;
}

int fseek(FILE *stream, long offset, int whence) {
    return fseek_internal(stream, (int64_t)offset, whence);
}

long ftell(FILE *stream) {
    if (stream == nullptr) {
        lprintf("clib_stubs: ftell(NULL stream)");
        return -1;
    }
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) {
        lprintf("clib_stubs: ftell(inactive stream)");
        return -1;
    }
    return (long)f_tell(&w->file);
}

int fseeko(FILE *stream, off_t offset, int whence) {
    return fseek_internal(stream, (int64_t)offset, whence);
}

off_t ftello(FILE *stream) {
    if (stream == nullptr) {
        lprintf("clib_stubs: ftello(NULL stream)");
        return -1;
    }
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) {
        lprintf("clib_stubs: ftello(inactive stream)");
        return -1;
    }
    return (off_t)f_tell(&w->file);
}

int fflush(FILE *stream) {
    if (stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;
    f_sync(&w->file);
    return 0;
}

int fputc(int c, FILE *stream) {
    if (stream == nullptr) return -1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return -1;

    unsigned char ch = (unsigned char)c;
    UINT written = 0;
    FRESULT res = f_write(&w->file, &ch, 1, &written);
    if (res != FR_OK || written != 1) return -1;
    return c;
}

FILE *fdopen(int fd, const char *mode) {
    return nullptr;
}

char *strerror(int errnum) {
    return (char *)"Unknown error";
}

/*
int sscanf(const char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = 0;

    if (strcmp(format, "TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d PREGAP:%d PGTYPE:%s PGSUB:%s POSTGAP:%d") == 0) {
        int *track = va_arg(args, int *);
        char *type = va_arg(args, char *);
        char *subtype = va_arg(args, char *);
        int *frames = va_arg(args, int *);
        int *pregap = va_arg(args, int *);
        char *pgtype = va_arg(args, char *);
        char *pgsub = va_arg(args, char *);
        int *postgap = va_arg(args, int *);

        if (str && track && type && subtype && frames && pregap && pgtype && pgsub && postgap) {
            const char *p;
            if ((p = strstr(str, "TRACK:"))) { *track = (int)strtol(p + 6, nullptr, 10); count++; }
            if ((p = strstr(str, "TYPE:"))) {
                char *d = type; p += 5;
                while (*p && *p != ' ') *d++ = *p++;
                *d = '\0'; count++;
            }
            if ((p = strstr(str, "SUBTYPE:"))) {
                char *d = subtype; p += 8;
                while (*p && *p != ' ') *d++ = *p++;
                *d = '\0'; count++;
            }
            if ((p = strstr(str, "FRAMES:"))) { *frames = (int)strtol(p + 7, nullptr, 10); count++; }
            if ((p = strstr(str, "PREGAP:"))) { *pregap = (int)strtol(p + 7, nullptr, 10); count++; }
            if ((p = strstr(str, "PGTYPE:"))) {
                char *d = pgtype; p += 7;
                while (*p && *p != ' ') *d++ = *p++;
                *d = '\0'; count++;
            }
            if ((p = strstr(str, "PGSUB:"))) {
                char *d = pgsub; p += 6;
                while (*p && *p != ' ') *d++ = *p++;
                *d = '\0'; count++;
            }
            if ((p = strstr(str, "POSTGAP:"))) { *postgap = (int)strtol(p + 8, nullptr, 10); count++; }
        }
    }
    else if (strcmp(format, "TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d") == 0) {
        int *track = va_arg(args, int *);
        char *type = va_arg(args, char *);
        char *subtype = va_arg(args, char *);
        int *frames = va_arg(args, int *);

        if (str && track && type && subtype && frames) {
            const char *p;
            if ((p = strstr(str, "TRACK:"))) { *track = (int)strtol(p + 6, nullptr, 10); count++; }
            if ((p = strstr(str, "TYPE:"))) {
                char *d = type; p += 5;
                while (*p && *p != ' ') *d++ = *p++;
                *d = '\0'; count++;
            }
            if ((p = strstr(str, "SUBTYPE:"))) {
                char *d = subtype; p += 8;
                while (*p && *p != ' ') *d++ = *p++;
                *d = '\0'; count++;
            }
            if ((p = strstr(str, "FRAMES:"))) { *frames = (int)strtol(p + 7, nullptr, 10); count++; }
        }
    }
    else if (strcmp(format, "%d:%d:%d") == 0) {
        int *m = va_arg(args, int *);
        int *s = va_arg(args, int *);
        int *f = va_arg(args, int *);

        if (str && m && s && f) {
            const char *p = str;
            while (*p == ' ' || *p == '\t') p++;
            if (*p >= '0' && *p <= '9') {
                *m = (int)strtol(p, nullptr, 10);
                count++;
                while (*p >= '0' && *p <= '9') p++;
            }
            if (*p == ':') {
                p++;
                if (*p >= '0' && *p <= '9') {
                    *s = (int)strtol(p, nullptr, 10);
                    count++;
                    while (*p >= '0' && *p <= '9') p++;
                }
                if (*p == ':') {
                    p++;
                    if (*p >= '0' && *p <= '9') {
                        *f = (int)strtol(p, nullptr, 10);
                        count++;
                    }
                }
            }
        }
    }
    else if (strcmp(format, "%d") == 0) {
        int *val = va_arg(args, int *);
        if (str && val) {
            const char *p = str;
            while (*p == ' ' || *p == '\t') p++;
            if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '+') {
                *val = (int)strtol(p, nullptr, 10);
                count++;
            }
        }
    }
    else {
        lprintf("clib_stubs: unsupported sscanf format: %s", format);
    }

    va_end(args);
    return count;
}
*/

/*
int sprintf(char *buf, const char *fmt, ...) {
    va_list var;
    va_start(var, fmt);
    CString Msg;
    Msg.FormatV(fmt, var);
    va_end(var);
    strcpy(buf, (const char *)Msg);
    return Msg.GetLength();
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    if (size == 0) return 0;
    va_list var;
    va_start(var, fmt);
    CString Msg;
    Msg.FormatV(fmt, var);
    va_end(var);
    size_t len = Msg.GetLength();
    if (size - 1 < len) {
        len = size - 1;
    }
    memcpy(buf, (const char *)Msg, len);
    buf[len] = '\0';
    return len;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list var) {
    if (size == 0) return 0;
    CString Msg;
    Msg.FormatV(fmt, var);
    size_t len = Msg.GetLength();
    if (size - 1 < len) {
        len = size - 1;
    }
    memcpy(buf, (const char *)Msg, len);
    buf[len] = '\0';
    return len;
}
*/

int fprintf(FILE *stream, const char *fmt, ...) {
    return 0;
}

int gettimeofday(struct timeval *tv, void *tz) {
    if (tv) {
        u64 ticks = CTimer::GetClockTicks64(); // 1 tick = 1 microsecond
        tv->tv_sec = ticks / 1000000;
        tv->tv_usec = ticks % 1000000;
    }
    return 0;
}

// Missing string/stdlib functions
char *strrchr(const char *s, int c) {
    const char *last = nullptr;
    do {
        if (*s == (char)c) {
            last = s;
        }
    } while (*s++);
    return (char *)last;
}

int strcoll(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

size_t strxfrm(char *dest, const char *src, size_t n) {
    if (n > 0) {
        strncpy(dest, src, n);
        dest[n - 1] = '\0';
    }
    return strlen(src);
}

int wcscoll(const wchar_t *s1, const wchar_t *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)*s1 - (int)*s2;
}

size_t wcsxfrm(wchar_t *dest, const wchar_t *src, size_t n) {
    size_t len = 0;
    const wchar_t *s = src;
    while (*s++) len++;
    if (n > 0) {
        size_t i;
        for (i = 0; i < n - 1 && src[i]; i++) {
            dest[i] = src[i];
        }
        dest[i] = 0;
    }
    return len;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    while (n--) {
        if (*p == (unsigned char)c) {
            return (void *)p;
        }
        p++;
    }
    return nullptr;
}

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n) {
    while (n--) {
        if (*s == c) {
            return (wchar_t *)s;
        }
        s++;
    }
    return nullptr;
}

wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n) {
    wchar_t *d = dest;
    const wchar_t *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

size_t wcslen(const wchar_t *s) {
    size_t len = 0;
    while (*s++) {
        len++;
    }
    return len;
}

char *setlocale(int category, const char *locale) {
    return (char *)"C";
}

struct tm;

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    if (max > 0) s[0] = '\0';
    return 0;
}

size_t wcsftime(wchar_t *s, size_t max, const wchar_t *format, const struct tm *tm) {
    if (max > 0) s[0] = 0;
    return 0;
}

void abort(void) {
    extern void lprintf(const char *fmt, ...);
    lprintf("clib_stubs: abort() called\n");
    while (1);
}

wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t n) {
    if (dest < src) {
        wchar_t *d = dest;
        const wchar_t *s = src;
        while (n--) {
            *d++ = *s++;
        }
    } else if (dest > src) {
        wchar_t *d = dest + n;
        const wchar_t *s = src + n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

int wmemcmp(const wchar_t *s1, const wchar_t *s2, size_t n) {
    while (n--) {
        if (*s1 != *s2) {
            return (int)*s1 - (int)*s2;
        }
        s1++;
        s2++;
    }
    return 0;
}

char *getenv(const char *name) {
    return nullptr;
}

double strtod(const char *nptr, char **endptr) {
    double val = 0.0;
    double sign = 1.0;
    const char *p = nptr;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '-') {
        sign = -1.0;
        p++;
    } else if (*p == '+') {
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        val = val * 10.0 + (*p - '0');
        p++;
    }
    if (*p == '.') {
        p++;
        double dec = 0.1;
        while (*p >= '0' && *p <= '9') {
            val += (*p - '0') * dec;
            dec *= 0.1;
            p++;
        }
    }
    if (endptr) *endptr = (char *)p;
    return val * sign;
}

float strtof(const char *nptr, char **endptr) {
    return (float)strtod(nptr, endptr);
}

int __locale_mb_cur_max(void) {
    return 1;
}


size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps) {
    if (s == nullptr) return 1;
    *s = (char)wc;
    return 1;
}

size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) {
    if (s == nullptr) return 0;
    if (n == 0) return 0;
    if (pwc) *pwc = (wchar_t)*s;
    return (*s == '\0') ? 0 : 1;
}

wctype_t wctype(const char *name) {
    return 0;
}

wint_t towupper(wint_t wc) {
    if (wc >= 'a' && wc <= 'z') return wc - 'a' + 'A';
    return wc;
}

wint_t towlower(wint_t wc) {
    if (wc >= 'A' && wc <= 'Z') return wc - 'A' + 'a';
    return wc;
}

int iswctype(wint_t wc, wctype_t desc) {
    return 0;
}

int wctob(wint_t wc) {
    if (wc <= 127) return (int)wc;
    return -1;
}

wint_t btowc(int c) {
    if (c == -1) return (wint_t)-1;
    return (wint_t)c;
}

int getentropy(void *buffer, size_t length) {
    unsigned char *p = (unsigned char *)buffer;
    while (length--) {
        *p++ = (unsigned char)rand();
    }
    return 0;
}

__uint32_t arc4random(void) {
    return ((__uint32_t)rand() << 16) | (__uint32_t)rand();
}


int read(int fd, void *buf, size_t count) {
    return -1;
}

int write(int fd, const void *buf, size_t count) {
    return -1;
}

off_t lseek(int fd, off_t offset, int whence) {
    return -1;
}

int puts(const char *s) {
    extern void lprintf(const char *fmt, ...);
    lprintf("%s\n", s);
    return 1;
}

time_t time(time_t *tloc) {
    time_t t = (time_t)(CTimer::GetClockTicks64() / 1000000);
    if (tloc) *tloc = t;
    return t;
}

int setvbuf(FILE *stream, char *buf, int mode, size_t size) {
    return 0;
}

int fileno(FILE *stream) {
    return -1;
}

int fstat(int fd, struct stat *statbuf) {
    return -1;
}

wchar_t *wmemset(wchar_t *dest, wchar_t c, size_t n) {
    wchar_t *d = dest;
    while (n--) {
        *d++ = c;
    }
    return dest;
}

int isalnum(int c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
    return c;
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f');
}

int fgetc(FILE *stream) {
    unsigned char c;
    if (fread(&c, 1, 1, stream) != 1) return -1;
    return c;
}

int getc(FILE *stream) {
    return fgetc(stream);
}

int putchar(int c) {
    extern void lprintf(const char *fmt, ...);
    lprintf("%c", c);
    return c;
}

int ftruncate(int fd, off_t length) {
    return -1;
}

struct tm *localtime(const time_t *timer) {
    static struct tm dummy_tm;
    memset(&dummy_tm, 0, sizeof(dummy_tm));
    dummy_tm.tm_mday = 1;
    dummy_tm.tm_mon = 0;
    dummy_tm.tm_year = 126;
    return &dummy_tm;
}

void exit(int status) {
    extern void lprintf(const char *fmt, ...);
    lprintf("clib_stubs: exit(%d) called\n", status);
    while (1);
}

int remove(const char *pathname) {
    char fullPath[256];
    if (pathname[0] == '/') {
        snprintf(fullPath, sizeof(fullPath), "SD:%s", pathname);
    } else if (strncmp(pathname, "SD:", 3) != 0) {
        snprintf(fullPath, sizeof(fullPath), "SD:/%s", pathname);
    } else {
        strncpy(fullPath, pathname, sizeof(fullPath) - 1);
        fullPath[sizeof(fullPath) - 1] = '\0';
    }
    return f_unlink(fullPath) == FR_OK ? 0 : -1;
}

char *ctime(const time_t *timep) {
    static char buf[26];
    strcpy(buf, "Thu Jan  1 00:00:00 2026\n");
    return buf;
}

int ferror(FILE *stream) {
    return 0;
}

int fputs(const char *s, FILE *stream) {
    if (stream == nullptr || s == nullptr) return -1;
    size_t len = strlen(s);
    return fwrite(s, 1, len, stream) == len ? 0 : -1;
}



#define _U 01
#define _L 02
#define _N 04
#define _S 010
#define _P 020
#define _C 040
#define _X 0100
#define _B 0200

extern "C" const unsigned char _ctype_[257] = {
    0, // EOF
    _C, _C, _C, _C, _C, _C, _C, _C,                     // 0-7
    _C, _C|_S, _C|_S, _C|_S, _C|_S, _C|_S, _C, _C,       // 8-15
    _C, _C, _C, _C, _C, _C, _C, _C,                     // 16-23
    _C, _C, _C, _C, _C, _C, _C, _C,                     // 24-31
    _S|_B, _P, _P, _P, _P, _P, _P, _P,                 // 32-39
    _P, _P, _P, _P, _P, _P, _P, _P,                     // 40-47
    _N|_X, _N|_X, _N|_X, _N|_X, _N|_X, _N|_X, _N|_X, _N|_X, // 48-55
    _N|_X, _N|_X, _P, _P, _P, _P, _P, _P,                 // 56-63
    _P, _U|_X, _U|_X, _U|_X, _U|_X, _U|_X, _U|_X, _U,     // 64-71
    _U, _U, _U, _U, _U, _U, _U, _U,                     // 72-79
    _U, _U, _U, _U, _U, _U, _U, _U,                     // 80-87
    _U, _U, _U, _P, _P, _P, _P, _P,                     // 88-95
    _P, _L|_X, _L|_X, _L|_X, _L|_X, _L|_X, _L|_X, _L,     // 96-103
    _L, _L, _L, _L, _L, _L, _L, _L,                     // 104-111
    _L, _L, _L, _L, _L, _L, _L, _L,                     // 112-119
    _L, _L, _L, _P, _P, _P, _P, _C,                     // 120-127
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};





#ifndef LONG_MAX
#define LONG_MAX 2147483647L
#endif
#ifndef LONG_MIN
#define LONG_MIN (-LONG_MAX - 1L)
#endif

long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long acc;
    int c;
    unsigned long cutoff;
    int neg = 0, any, cutlim;

    do {
        c = *s++;
    } while (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f');

    if (c == '-') {
        neg = 1;
        c = *s++;
    } else if (c == '+') {
        c = *s++;
    }

    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0) {
        base = c == '0' ? 8 : 10;
    }

    cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
    cutlim = cutoff % (unsigned long)base;
    cutoff /= (unsigned long)base;
    for (acc = 0, any = 0;; c = *s++) {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'A' && c <= 'Z') {
            c -= 'A' - 10;
        } else if (c >= 'a' && c <= 'z') {
            c -= 'a' - 10;
        } else {
            break;
        }
        if (c >= base) {
            break;
        }
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = neg ? LONG_MIN : LONG_MAX;
    } else if (neg) {
        acc = -acc;
    }
    if (endptr != 0) {
        *endptr = (char *)(any ? s - 1 : nptr);
    }
    return acc;
}

static unsigned long next_rand = 1;
int rand(void) {
    next_rand = next_rand * 1103515245 + 12345;
    return (unsigned int)(next_rand / 65536) % 32768;
}
void srand(unsigned int seed) {
    next_rand = seed;
}

char *fgets(char *s, int size, FILE *stream) {
    if (s == nullptr || size <= 0 || stream == nullptr) return nullptr;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return nullptr;

    int i = 0;
    while (i < size - 1) {
        char c;
        UINT read = 0;
        FRESULT res = f_read(&w->file, &c, 1, &read);
        if (res != FR_OK || read == 0) {
            if (i == 0) return nullptr; // EOF or error
            break;
        }
        s[i++] = c;
        if (c == '\n') {
            break;
        }
    }
    s[i] = '\0';
    return s;
}

int feof(FILE *stream) {
    if (stream == nullptr) return 1;
    CFileWrapper *w = (CFileWrapper *)stream;
    if (!w->in_use) return 1;
    return f_eof(&w->file);
}

int abs(int x) {
    return x < 0 ? -x : x;
}

int printf(const char *format, ...) {
    char buf[512];
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    extern void lprintf(const char *fmt, ...);
    lprintf("%s", buf);
    return ret;
}

void perror(const char *s) {
    extern void lprintf(const char *fmt, ...);
    lprintf("perror: %s", s);
}

static void swap_bytes(char *a, char *b, size_t size) {
    char tmp;
    while (size--) {
        tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void qsort(void *base, size_t num, size_t size, int (*compar)(const void *, const void *)) {
    if (num <= 1) return;
    char *char_base = (char *)base;
    char *pivot = char_base + (num - 1) * size;
    size_t i = 0;
    for (size_t j = 0; j < num - 1; j++) {
        if (compar(char_base + j * size, pivot) <= 0) {
            swap_bytes(char_base + i * size, char_base + j * size, size);
            i++;
        }
    }
    swap_bytes(char_base + i * size, pivot, size);
    if (i > 1) {
        qsort(char_base, i, size, compar);
    }
    if (num - i - 1 > 1) {
        qsort(char_base + (i + 1) * size, num - i - 1, size, compar);
    }
}

// Picodrive SMS & 32x stubs
void Pico32xPrepare(void) {}
void PicoPrepareMS(void) {}
#ifndef _ASM_MEMORY_C
unsigned int PicoRead8_32x(unsigned int a) { return 0; }
unsigned int PicoRead16_32x(unsigned int a) { return 0; }
void PicoWrite8_32x(unsigned int a, unsigned int d) {}
void PicoWrite16_32x(unsigned int a, unsigned int d) {}
#endif

// Picodrive ZIP stubs
struct ZIP;
struct zipent;
ZIP* openzip(const char* path) { return nullptr; }
void closezip(ZIP* zip) {}
struct zipent* readzip(ZIP* zip) { return nullptr; }
int seekcompresszip(ZIP* zip, struct zipent* ent) { return -1; }

// Picodrive video mode change callback
void emu_video_mode_change(int start_line, int line_count, int start_col, int col_count) {
    int idx = g_SharedState.emu_write_idx;
    g_SharedState.start_line[idx] = start_line;
    g_SharedState.game_h[idx] = line_count;
}

// Picodrive Sega CD MP3 & OGG stubs
int mp3_get_bitrate(void *f, int size) { return 0; }
void mp3_start_play(void *f, int pos) {}
void mp3_update(s32 *buffer, int length, int stereo) {}
int ogg_get_length(void *f_) { return 0; }
void ogg_start_play(void *f_, int sample_offset) {}
void ogg_stop_play(void) {}
void ogg_update(s32 *buffer, int length, int stereo) {}

void cache_flush_d_inval_i(void *start_addr, void *end_addr) {
    unsigned int start = (unsigned int)start_addr;
    unsigned int end = (unsigned int)end_addr;
    if (end > start) {
        CleanAndInvalidateDataCacheRange(start, end - start);
    }
    InvalidateInstructionCache();
}

} // extern "C"
