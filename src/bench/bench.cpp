// Ф0 host workload bench: runs a cart headless on fake-08 and reports per frame
// the Lua VM instruction count, the native time and (with -pg) the split
// between the Lua VM, the drawing API and the synth (see bench_report.py).
//
// usage: bench <cart.p8|.p8.png> [frames] [audio 0|1]
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

static unsigned long long g_ops;
static const int HOOK_STEP = 64;

static void count_hook(lua_State *L, lua_Debug *ar) {
    (void)L; (void)ar;
    g_ops += HOOK_STEP;
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
    lua_sethook(vm->_luaState, count_hook, LUA_MASKCOUNT, HOOK_STEP);
    vm->vm_run();

    int fps = vm->GetTargetFps();
    std::vector<uint32_t> abuf(22050 / (fps > 0 ? fps : 30) + 1);

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
        vm->Step();
        if (audio)
            vm->FillAudioBuffer(abuf.data(), 0, abuf.size() - 1);
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
