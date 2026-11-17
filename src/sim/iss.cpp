// Ф0.2 fast RV32IM instruction-set simulator for the bench firmware (same memory map
// and MMIO as tb_soc.cpp). No timing: MMIO_CYCLES = retired instructions. Optional
// cache model (--icache/--dcache bytes, 32-byte lines, direct-mapped like VexRiscv
// 1-way) counts misses per frame so SRAM cost can be estimated without Verilator.
//
// usage: iss <fw.bin> [--max Minstr] [--icache B] [--dcache B] [--prof file: pc + ra every 256 instr] [--prof-from N] [--dump-at F file: memory + resume stub after frame F]
// prints "F,<frame>,<instr>,<ops>,<imiss>,<dmiss_rd>,<stores>,<screen hash>" per frame.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>

static const uint32_t MEM_SIZE = 4u << 20;
static std::vector<uint8_t> mem(MEM_SIZE + 8);

struct Cache {
    uint32_t lines = 0;
    std::vector<uint32_t> tag;
    uint64_t miss = 0;
    void init(uint32_t bytes) { lines = bytes / 32; tag.assign(lines ? lines : 1, 0xffffffffu); }
    void access(uint32_t a) {
        if (!lines) { miss++; return; }
        uint32_t l = a >> 5, i = l % lines;
        if (tag[i] != l) { tag[i] = l; miss++; }
    }
};

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s fw.bin [--max Minstr] [--icache B] [--dcache B]\n", argv[0]); return 2; }
    uint64_t max_instr = 20000ull * 1000 * 1000;
    uint32_t icb = 4096, dcb = 4096;
    FILE *prof = 0;
    bool prof_frames = false, sampling = true; // --prof-from N: sample only after frame N
    int prof_from = 0;
    int dump_at = -1; const char *dump_path = 0; bool dump_now = false;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--max")) max_instr = strtoull(argv[++i], 0, 10) * 1000000ull;
        else if (!strcmp(argv[i], "--icache")) icb = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dcache")) dcb = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--prof")) prof = fopen(argv[++i], "w");
        else if (!strcmp(argv[i], "--dump-at")) { dump_at = atoi(argv[++i]); dump_path = argv[++i]; }
        else if (!strcmp(argv[i], "--prof-from")) { prof_frames = true; prof_from = atoi(argv[++i]); }
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    size_t n = fread(mem.data(), 1, MEM_SIZE, f);
    fclose(f);
    fprintf(stderr, "loaded %zu bytes, I$ %u D$ %u\n", n, icb, dcb);
    if (prof_frames) sampling = false;
    Cache ic, dc; ic.init(icb); dc.init(dcb);

    uint32_t x[32] = {0}, pc = 0;
    uint64_t ninstr = 0, last_i = 0, last_im = 0, last_dm = 0, stores = 0, last_st = 0;
    uint32_t last_ops = 0, last_hash = 0;
    int exit_code = -1;
    std::string line;

    auto ld = [&](uint32_t a, int sz) -> uint32_t {
        if (a & 0x80000000u) {
            if ((a & 0xff) == 0x0c) return (uint32_t)ninstr;
            return 0;
        }
        dc.access(a);
        a &= MEM_SIZE - 1;
        uint32_t v = 0;
        memcpy(&v, &mem[a], sz);
        return v;
    };
    auto st = [&](uint32_t a, uint32_t v, int sz) {
        if (a & 0x80000000u) {
            uint32_t o = a & 0xff;
            if (o == 0x00) { char c = (char)v; if (c == '\n') { printf("%s\n", line.c_str()); line.clear(); } else line += c; }
            else if (o == 0x04) {
                printf("F,%u,%llu,%u,%llu,%llu,%llu,%u\n", v, (unsigned long long)(ninstr - last_i), last_ops,
                       (unsigned long long)(ic.miss - last_im), (unsigned long long)(dc.miss - last_dm),
                       (unsigned long long)(stores - last_st), last_hash);
                last_i = ninstr; last_im = ic.miss; last_dm = dc.miss; last_st = stores;
                if (prof_frames && (int)v + 1 >= prof_from) sampling = true;
                if ((int)v == dump_at) dump_now = true;
            }
            else if (o == 0x08) exit_code = (int)v;
            else if (o == 0x10) last_ops = v;
            else if (o == 0x18) last_hash = v;
            else if (o == 0x14) {
                printf("CKPT %llu\n", (unsigned long long)ninstr);
                last_i = ninstr; last_im = ic.miss; last_dm = dc.miss; last_st = stores;
            }
            return;
        }
        stores++; // write-through D$: every store is a bus write (no allocate)
        a &= MEM_SIZE - 1;
        memcpy(&mem[a], &v, sz);
    };

    while (exit_code < 0 && ninstr < max_instr) {
        if (prof && sampling && (ninstr & 255) == 0) fprintf(prof, "%x %x\n", pc, x[1]);
        ic.access(pc);
        uint32_t in; memcpy(&in, &mem[pc & (MEM_SIZE - 1)], 4);
        uint32_t op = in & 0x7f, rd = (in >> 7) & 31, f3 = (in >> 12) & 7, rs1 = (in >> 15) & 31, rs2 = (in >> 20) & 31, f7 = in >> 25;
        int32_t immi = (int32_t)in >> 20;
        int32_t imms = ((int32_t)in >> 25 << 5) | ((in >> 7) & 31);
        int32_t immb = (((int32_t)in >> 31) << 12) | (((in >> 7) & 1) << 11) | (((in >> 25) & 63) << 5) | (((in >> 8) & 15) << 1);
        int32_t immj = (((int32_t)in >> 31) << 20) | (in & 0xff000) | (((in >> 20) & 1) << 11) | (((in >> 21) & 1023) << 1);
        uint32_t a = x[rs1], b = x[rs2], r = 0, npc = pc + 4;
        bool wr = true;
        switch (op) {
        case 0x37: r = in & 0xfffff000u; break;
        case 0x17: r = pc + (in & 0xfffff000u); break;
        case 0x6f: r = pc + 4; npc = pc + immj; break;
        case 0x67: r = pc + 4; npc = (a + immi) & ~1u; break;
        case 0x63: {
            bool t = false;
            switch (f3) {
            case 0: t = a == b; break; case 1: t = a != b; break;
            case 4: t = (int32_t)a < (int32_t)b; break; case 5: t = (int32_t)a >= (int32_t)b; break;
            case 6: t = a < b; break; case 7: t = a >= b; break;
            default: fprintf(stderr, "bad branch at %08x\n", pc); return 3;
            }
            if (t) npc = pc + immb;
            wr = false; break;
        }
        case 0x03: {
            uint32_t ea = a + immi;
            switch (f3) {
            case 0: r = (uint32_t)(int32_t)(int8_t)ld(ea, 1); break;
            case 1: r = (uint32_t)(int32_t)(int16_t)ld(ea, 2); break;
            case 2: r = ld(ea, 4); break;
            case 4: r = ld(ea, 1) & 0xff; break;
            case 5: r = ld(ea, 2) & 0xffff; break;
            default: fprintf(stderr, "bad load at %08x\n", pc); return 3;
            }
            break;
        }
        case 0x23: {
            uint32_t ea = a + imms;
            st(ea, b, f3 == 0 ? 1 : f3 == 1 ? 2 : 4);
            wr = false; break;
        }
        case 0x13: {
            uint32_t sh = immi & 31;
            switch (f3) {
            case 0: r = a + immi; break;
            case 2: r = (int32_t)a < immi; break;
            case 3: r = a < (uint32_t)immi; break;
            case 4: r = a ^ immi; break;
            case 6: r = a | immi; break;
            case 7: r = a & immi; break;
            case 1: r = a << sh; break;
            case 5: r = (in >> 30 & 1) ? (uint32_t)((int32_t)a >> sh) : a >> sh; break;
            }
            break;
        }
        case 0x33:
            if (f7 == 1) {
                int64_t sa = (int32_t)a, sb = (int32_t)b;
                switch (f3) {
                case 0: r = a * b; break;
                case 1: r = (uint32_t)((sa * sb) >> 32); break;
                case 2: r = (uint32_t)((sa * (int64_t)(uint64_t)b) >> 32); break;
                case 3: r = (uint32_t)(((uint64_t)a * b) >> 32); break;
                case 4: r = b == 0 ? 0xffffffffu : (a == 0x80000000u && b == 0xffffffffu) ? a : (uint32_t)((int32_t)a / (int32_t)b); break;
                case 5: r = b == 0 ? 0xffffffffu : a / b; break;
                case 6: r = b == 0 ? a : (a == 0x80000000u && b == 0xffffffffu) ? 0 : (uint32_t)((int32_t)a % (int32_t)b); break;
                case 7: r = b == 0 ? a : a % b; break;
                }
            } else {
                switch (f3) {
                case 0: r = (f7 & 0x20) ? a - b : a + b; break;
                case 1: r = a << (b & 31); break;
                case 2: r = (int32_t)a < (int32_t)b; break;
                case 3: r = a < b; break;
                case 4: r = a ^ b; break;
                case 5: r = (f7 & 0x20) ? (uint32_t)((int32_t)a >> (b & 31)) : a >> (b & 31); break;
                case 6: r = a | b; break;
                case 7: r = a & b; break;
                }
            }
            break;
        case 0x0f: wr = false; break; // fence
        case 0x73: wr = (rd != 0); r = 0; break; // csr reads as 0, ecall/ebreak ignored
        default:
            fprintf(stderr, "illegal %08x at %08x after %llu\n", in, pc, (unsigned long long)ninstr);
            return 3;
        }
        if (wr && rd) x[rd] = r;
        pc = npc;
        ninstr++;
        if (dump_now) {
            // resume stub at 0x3ff000 (fw link.ld __resume): registers at +0x100, pc at +0x180;
            // tb_soc --rv 0x3ff000 starts there: x31 = base, mepc = pc, load x1..x31, mret
            const uint32_t base = 0x3ff000;
            auto I = [](int imm, int rs1, int f3, int rd, int op) { return (uint32_t)((imm & 0xfff) << 20 | rs1 << 15 | f3 << 12 | rd << 7 | op); };
            std::vector<uint32_t> code;
            code.push_back((0x3ffu << 12) | (31 << 7) | 0x37);   // lui x31, 0x3ff
            code.push_back(I(0x180, 31, 2, 5, 0x03));             // lw x5, pc
            code.push_back(I(0x341, 5, 1, 0, 0x73));              // csrw mepc, x5
            code.push_back((0x2u << 12) | (5 << 7) | 0x37);       // lui x5, 2
            code.push_back(I(-2048, 5, 0, 5, 0x13));              // addi x5, x5, -2048 (0x1800: MPP = M)
            code.push_back(I(0x300, 5, 1, 0, 0x73));              // csrw mstatus, x5
            for (int r = 1; r <= 31; r++) code.push_back(I(0x100 + 4 * r, 31, 2, r, 0x03));
            code.push_back(0x30200073);                           // mret
            memcpy(&mem[base], code.data(), code.size() * 4);
            for (int r = 0; r < 32; r++) memcpy(&mem[base + 0x100 + 4 * r], &x[r], 4);
            memcpy(&mem[base + 0x180], &pc, 4);
            FILE *d = fopen(dump_path, "wb");
            fwrite(mem.data(), 1, MEM_SIZE, d);
            fclose(d);
            printf("DUMP %s frame %d pc %08x\n", dump_path, dump_at, pc);
            exit_code = 0;
        }
    }
    if (!line.empty()) printf("%s\n", line.c_str());
    if (prof) fclose(prof);
    printf("END pc %08x ra %08x\n", pc, x[1]);
    printf("END instr %llu exit %d imiss %llu dmiss %llu stores %llu\n", (unsigned long long)ninstr, exit_code,
           (unsigned long long)ic.miss, (unsigned long long)dc.miss, (unsigned long long)stores);
    return exit_code == 0 ? 0 : 1;
}
