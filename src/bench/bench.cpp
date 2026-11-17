// Ф0 host workload bench: runs a cart headless on fake-08 and reports per frame
// the Lua VM instruction count, the native time and (with -pg) the split
// between the Lua VM, the drawing API and the synth (see bench_report.py).
//
// usage: bench <cart.p8|.p8.png> [frames] [audio 0|1]
// env BENCH_FRAMES=1: print "F,<frame>,<ops>,<fbhash>" per frame (compare with the rv32 run);
// env BENCH_LINES=N: trace Lua lines of the first N steps ("L src:line"); BENCH_PPM=dir:N,M,...: write dir/<frame>.ppm of the screen after those frames
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include <type_traits>
#include <map>
#include <functional>
// bench only: Vm keeps the lua_State in its (default private) members
#define class struct
#include "vm.h"
#include "stubhost.h"
#undef class
#include "lua.h"
#include "../fw/fbhash.h"

// PICO-8 standard palette (secret colours 128..143 shown as their low nibble)
static const uint32_t P8PAL[16] = {0x000000, 0x1d2b53, 0x7e2553, 0x008751, 0xab5236, 0x5f574f, 0xc2c3c7, 0xfff1e8,
                                   0xff004d, 0xffa300, 0xffec27, 0x00e436, 0x29adff, 0x83769c, 0xff77a8, 0xffccaa};
static void dump_ppm(const char *path, const uint8_t *fb, const uint8_t *pal) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n128 128\n255\n");
    for (int i = 0; i < 128 * 128; i++) {
        uint8_t c = (fb[i >> 1] >> ((i & 1) * 4)) & 15;
        uint32_t rgb = P8PAL[pal[c] & 15];
        uint8_t px[3] = {(uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

static unsigned long long g_ops;
static const int HOOK_STEP = 64;

static void count_hook(lua_State *L, lua_Debug *ar) {
    (void)L; (void)ar;
    g_ops += HOOK_STEP;
}


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
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s cart [frames] [audio]\n", argv[0]);
        return 2;
    }
    int frames = argc > 2 ? atoi(argv[2]) : 600;
    bool audio = argc > 3 ? atoi(argv[3]) != 0 : true;

    StubHost *host = new StubHost();
    PicoRam *memory = new PicoRam();
    memory->Reset();
    Audio *snd = new Audio(memory);
    Vm *vm = new Vm(host, memory, nullptr, nullptr, snd);

    if (!vm->LoadCart(argv[1], false)) {
        fprintf(stderr, "load failed: %s\n", vm->GetBiosError().c_str());
        return 1;
    }
    // before vm_run: coroutines made there inherit the hook
    int trace_steps = getenv("BENCH_LINES") ? atoi(getenv("BENCH_LINES")) : 0;
    if (trace_steps) lua_sethook(vm->_luaState, line_hook, LUA_MASKLINE | LUA_MASKCOUNT, HOOK_STEP);
    else lua_sethook(vm->_luaState, count_hook, LUA_MASKCOUNT, HOOK_STEP);
    vm->vm_run();
    vm->api_srand(z8::fix32(1234)); // vm_run seeds rnd from the clock: same seed as the rv32 run

    int fps = vm->GetTargetFps();
    std::vector<uint32_t> abuf(22050 / (fps > 0 ? fps : 30) + 1);

    bool per_frame_out = getenv("BENCH_FRAMES") != nullptr;
    std::string ppm_dir; std::vector<int> ppm_frames;
    if (const char *e = getenv("BENCH_PPM")) {
        std::string s(e); size_t c = s.find(':');
        ppm_dir = s.substr(0, c);
        for (size_t p = c + 1; p < s.size() && p != std::string::npos + 1;) {
            size_t q = s.find(',', p); ppm_frames.push_back(atoi(s.substr(p, q - p).c_str()));
            if (q == std::string::npos) break; p = q + 1;
        }
    }
    std::vector<unsigned long long> per_frame;
    per_frame.reserve(frames);
    auto t0 = std::chrono::steady_clock::now();
    unsigned long long prev = 0;
    unsigned seed = 12345;
    for (int f = 0; f < frames; f++) {
        // input: X/O taps to get past title screens, plus random d-pad
        uint8_t held = 0;
        seed = seed * 1103515245u + 12345u;
        if ((f / 20) % 3 == 1) held |= 0x10 << ((f / 60) & 1); // O or X
        held |= (seed >> 16) & 0x0f & ((f % 90) < 60 ? 0x0f : 0);
        host->stubInput(held, held);
        g_trace = f < trace_steps;
        if (g_trace) printf("S %d\n", f);
        vm->Step();
        if (audio)
            vm->FillAudioBuffer(abuf.data(), 0, abuf.size() - 1);
        if (per_frame_out)
            printf("F,%d,%llu,%u\n", f, g_ops - prev, fb_hash(vm->GetPicoInteralFb(), vm->GetScreenPaletteMap()));
        for (int pf : ppm_frames)
            if (pf == f) dump_ppm((ppm_dir + "/" + std::to_string(f) + ".ppm").c_str(), vm->GetPicoInteralFb(), vm->GetScreenPaletteMap());
        per_frame.push_back(g_ops - prev);
        prev = g_ops;
    }
    auto t1 = std::chrono::steady_clock::now();
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    // skip the first second (init), take mean and the 95th percentile
    std::vector<unsigned long long> s(per_frame.begin() + (frames > fps * 2 ? fps : 0), per_frame.end());
    std::sort(s.begin(), s.end());
    unsigned long long sum = 0;
    for (auto v : s) sum += v;
    printf("cart %s\nfps %d frames %d err \"%s\"\n", argv[1], fps, frames, vm->_cartLoadError.c_str());
    printf("lua_ops_per_frame mean %llu p95 %llu max %llu\n",
           s.empty() ? 0 : sum / s.size(), s.empty() ? 0 : s[s.size() * 95 / 100], s.empty() ? 0 : s.back());
    printf("native_us_per_frame %.1f\n", us / frames);
    return 0;
}
