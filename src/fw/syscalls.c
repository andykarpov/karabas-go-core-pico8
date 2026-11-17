/* Ф0.2 bench: newlib system calls. Console = MMIO UART, no files, heap up to __heap_end. */
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include "mmio.h"

extern char __heap_start[], __heap_end[];
extern char __eh_frame_start[];

/* C++ exceptions (Lua errors in z8lua): no dl_iterate_phdr on bare metal, so the
   unwinder needs the .eh_frame registered before the first throw */
void __register_frame_info(const void *begin, void *ob);
static long eh_object[16];
__attribute__((constructor(101))) static void eh_register(void) {
    __register_frame_info(__eh_frame_start, eh_object);
}
static char *brk_ptr = __heap_start;

void *_sbrk(ptrdiff_t incr) {
    char *old = brk_ptr;
    if (brk_ptr + incr > __heap_end) {
        errno = ENOMEM;
        return (void *)-1;
    }
    brk_ptr += incr;
    return old;
}

int _write(int fd, const char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; i++) MMIO_UART = (uint8_t)buf[i];
    return len;
}

int _read(int fd, char *buf, int len) { (void)fd; (void)buf; (void)len; return 0; }
int _open(const char *name, int flags, int mode) { (void)name; (void)flags; (void)mode; errno = ENOENT; return -1; }
int _close(int fd) { (void)fd; return -1; }
int _fstat(int fd, struct stat *st) { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, int off, int whence) { (void)fd; (void)off; (void)whence; return 0; }
int _unlink(const char *name) { (void)name; errno = ENOENT; return -1; }
int _link(const char *a, const char *b) { (void)a; (void)b; errno = EMLINK; return -1; }
int _stat(const char *name, struct stat *st) { (void)name; (void)st; errno = ENOENT; return -1; }
int _mkdir(const char *name, int mode) { (void)name; (void)mode; errno = ENOENT; return -1; }
int mkdir(const char *name, mode_t mode) { (void)name; (void)mode; errno = ENOENT; return -1; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }
clock_t _times(struct tms *t) { (void)t; return (clock_t)MMIO_CYCLES; }
int _gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    uint32_t c = MMIO_CYCLES;
    tv->tv_sec = c / 50000000u;
    tv->tv_usec = (c % 50000000u) / 50u;
    return 0;
}
void _exit(int code) {
    MMIO_EXIT = (uint32_t)code;
    for (;;) {}
}
