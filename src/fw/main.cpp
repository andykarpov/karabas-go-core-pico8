// Ф0.2 bench firmware: fake-08 on the soft CPU, cart linked in (cart.S), same input
// pattern as the host bench (src/bench/bench.cpp). Marks each frame in MMIO_FRAME so the
// testbench logs cycles per frame. FRAMES / AUDIO / OPS_HOOK are set by the Makefile.
#include <cstdio>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <tuple>
#include <type_traits>
#include <map>
#include <functional>
#define class struct
#include "vm.h"
#include "stubhost.h"
#undef class
#include "lua.h"
#include "mmio.h"
#include "fbhash.h"
#ifndef FBHASH
#define FBHASH 0
#endif

#ifndef FRAMES
#define FRAMES 60
#endif
#ifndef AUDIO
#define AUDIO 0
#endif

extern "C" const unsigned char cart_data[];
extern "C" const unsigned char cart_end[];

#if OPS_HOOK == 1
static uint32_t g_ops;
static void count_hook(lua_State *, lua_Debug *) { g_ops += 64; }
#elif OPS_HOOK == 2
// debug: where the cart is, every 16384 VM ops
static uint32_t g_ops;
static void count_hook(lua_State *L, lua_Debug *ar) {
    g_ops += 16384;
    lua_getinfo(L, "Sl", ar);
    printf("HOOK %lu %s:%d\n", (unsigned long)g_ops, ar->short_src, ar->currentline);
}
#endif
#if LINES
static uint32_t g_ops;

// debug: trace executed Lua lines (src:line) while g_trace is on
static int g_trace;
static void line_hook(lua_State *L, lua_Debug *ar) {
    if (!g_trace) return;
    if (ar->event == LUA_HOOKCOUNT) { g_ops += 64; return; }
    lua_getinfo(L, "Sl", ar);
    printf("L %s:%d", ar->source[0] == '=' ? "?" : (ar->short_src[9] ? ar->short_src + 9 : ar->short_src), ar->currentline);
    // numeric locals as fix32 bits
    for (int i = 1; i < 40; i++) {
        const char *nm = lua_getlocal(L, ar, i);
        if (!nm) break;
        if (lua_type(L, -1) == LUA_TNUMBER) printf(" %s=%08lx", nm, (unsigned long)(uint32_t)lua_tonumber(L, -1).bits());
        lua_pop(L, 1);
    }
    printf("\n");
}
#endif

int main() {
    uint32_t c0 = MMIO_CYCLES;
    StubHost *host = new StubHost();
    PicoRam *memory = new PicoRam();
    memory->Reset();
    Audio *snd = new Audio(memory);
    Vm *vm = new Vm(host, memory, nullptr, nullptr, snd);
    uint32_t c1 = MMIO_CYCLES;
    if (!vm->LoadCart(cart_data, (size_t)(cart_end - cart_data), false)) {
        printf("load failed: %s\n", vm->_cartLoadError.c_str());
        return 1;
    }
#if LINES
    lua_sethook(vm->_luaState, line_hook, LUA_MASKLINE | LUA_MASKCOUNT, 64);
#elif OPS_HOOK
    lua_sethook(vm->_luaState, count_hook, LUA_MASKCOUNT, OPS_HOOK == 2 ? 16384 : 64);
#endif
    vm->vm_run();
    vm->api_srand(z8::fix32(1234)); // vm_run seeds rnd from the clock: same seed as the host bench
    uint32_t c2 = MMIO_CYCLES;
    printf("init vm %lu cart %lu cycles\n", (unsigned long)(c1 - c0), (unsigned long)(c2 - c1));
    MMIO_CKPT = 1;

    static uint32_t abuf[22050 / 60 + 1];
    unsigned seed = 12345;
    for (int f = 0; f < FRAMES; f++) {
        uint8_t held = 0;
        seed = seed * 1103515245u + 12345u;
        if ((f / 20) % 3 == 1) held |= 0x10 << ((f / 60) & 1);
        held |= (seed >> 16) & 0x0f & ((f % 90) < 60 ? 0x0f : 0);
        host->stubInput(held, held);
#if OPS_HOOK
        g_ops = 0;
#endif
#if LINES
        g_trace = f < LINES;
        if (g_trace) printf("S %d\n", f);
#endif
        vm->Step();
        if (AUDIO)
            vm->FillAudioBuffer(abuf, 0, 22050 / 60);
#if OPS_HOOK
        MMIO_OPS = g_ops;
#endif
#if FBHASH
        MMIO_HASH = fb_hash(vm->GetPicoInteralFb(), vm->GetScreenPaletteMap());
#endif
        MMIO_FRAME = (uint32_t)f;
    }
    printf("done err \"%s\"\n", vm->_cartLoadError.c_str());
    return 0;
}
