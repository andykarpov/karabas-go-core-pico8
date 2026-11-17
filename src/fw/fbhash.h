// Ф0 check: FNV-1a of the PICO-8 screen (128x128 4 bpp = 8 KB) and the 16-entry screen
// palette map. Shared by the host bench and the rv32 firmware to compare runs frame by frame.
#pragma once
#include <stdint.h>
static inline uint32_t fb_hash(const uint8_t *fb, const uint8_t *pal) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 8192; i++) h = (h ^ fb[i]) * 16777619u;
    for (int i = 0; i < 16; i++) h = (h ^ pal[i]) * 16777619u;
    return h;
}
