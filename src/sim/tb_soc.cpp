// Ф0.2 Verilator testbench: a VexRiscv variant on a C++ model of the board SRAM.
//
// The SRAM is 2M x16 asynchronous: a 32-bit word = 2 halfword accesses. Costs in
// clk_sys cycles per halfword (--rd / --wr, defaults = archie_sram.v at ~40 MHz:
// read 1 halfword per 25 ns = 1, write 3 clocks); plus --ovh cycles per bus
// transaction (address / ack registers). --ideal = 1 cycle per access (CPU-only bound).
// Instruction and data buses share the SRAM (data first).
//
// MMIO at 0x80000000 (see src/fw/mmio.h). Prints per frame cycles (and Lua ops)
// as CSV lines "F,<frame>,<cycles>,<ops>" and the console output.
//
// usage: tb_soc <fw.bin> [--rd N] [--wr N] [--ovh N] [--ideal] [--max Mcycles] [--pc Mcycles: print the fetch address]
//        [--frames N: stop after N frames] [--rv addr: reset vector, 0x3ff000 = resume an iss --dump-at image] [--save ckpt: stop at MMIO_CKPT and save] [--restore ckpt: start from it (same CPU build, same fw)]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include "Vcpu.h"
#include "verilated.h"
#include "verilated_save.h"

static const uint32_t MEM_SIZE = 4u << 20;
static std::vector<uint8_t> mem(MEM_SIZE);

struct Bus {
    int busy = 0;       // cycles left of the transaction being served
    bool active = false;
};

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s fw.bin [--rd N] [--wr N] [--ovh N] [--ideal] [--max Mcycles]\n", argv[0]);
        return 2;
    }
    int rd = 1, wr = 3, ovh = 1;
    bool ideal = false;
    uint64_t pc_every = 0;
    const char *save_path = 0, *restore_path = 0;
    uint32_t reset_vector = 0;
    int stop_frames = -1, nframes = 0;
    uint64_t max_cycles = 4000ull * 1000 * 1000;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--rd")) rd = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--wr")) wr = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ovh")) ovh = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ideal")) ideal = true;
        else if (!strcmp(argv[i], "--pc")) pc_every = strtoull(argv[++i], 0, 10) * 1000000ull;
        else if (!strcmp(argv[i], "--frames")) stop_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rv")) reset_vector = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--save")) save_path = argv[++i];
        else if (!strcmp(argv[i], "--restore")) restore_path = argv[++i];
        else if (!strcmp(argv[i], "--max")) max_cycles = strtoull(argv[++i], 0, 10) * 1000000ull;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    size_t n = fread(mem.data(), 1, MEM_SIZE, f);
    fclose(f);
    fprintf(stderr, "loaded %zu bytes, rd %d wr %d ovh %d%s\n", n, rd, wr, ovh, ideal ? " ideal" : "");

    Verilated::commandArgs(argc, argv);
    Vcpu *top = new Vcpu;
    top->externalResetVector = reset_vector;
    top->timerInterrupt = 0;
    top->softwareInterrupt = 0;
    top->externalInterruptArray = 0;
    top->iBusWishbone_ACK = 0;
    top->dBusWishbone_ACK = 0;
    top->iBusWishbone_ERR = 0;
    top->dBusWishbone_ERR = 0;
    top->reset = 1;
    for (int i = 0; i < 8; i++) { top->clk = 0; top->eval(); top->clk = 1; top->eval(); }
    top->reset = 0;

    uint64_t cyc = 0, last_frame = 0, stall_i = 0, stall_d = 0, beats_i = 0, beats_d = 0;
    uint32_t last_ops = 0;
    int exit_code = -1;
    Bus ib, db;
    int sram_owner = 0; // 0 none, 1 i, 2 d
    bool save_now = false;
    // checkpoint = model + SRAM + bus state, taken after the MMIO_CKPT write (init done)
    struct State { uint64_t cyc; Bus ib, db; int sram_owner; } st;
    if (restore_path) {
        VerilatedRestore is;
        is.open(restore_path);
        is >> *top;
        is.read(mem.data(), MEM_SIZE);
        is.read(&st, sizeof st);
        is.close();
        cyc = last_frame = st.cyc; ib = st.ib; db = st.db; sram_owner = st.sram_owner;
        fprintf(stderr, "restored %s at cycle %llu\n", restore_path, (unsigned long long)cyc);
    }
    uint64_t start_cyc = cyc;
    std::string line;

    while (cyc < max_cycles && exit_code < 0) {
        // outputs are those after the last rising edge: decide this cycle's acks
        top->iBusWishbone_ACK = 0;
        top->dBusWishbone_ACK = 0;

        bool dreq = top->dBusWishbone_CYC && top->dBusWishbone_STB;
        bool ireq = top->iBusWishbone_CYC && top->iBusWishbone_STB;
        uint32_t daddr = (uint32_t)top->dBusWishbone_ADR << 2;
        uint32_t iaddr = (uint32_t)top->iBusWishbone_ADR << 2;
        bool dio = daddr & 0x80000000u;
        if (pc_every && cyc % pc_every == 0) fprintf(stderr, "PC %llu %08x\n", (unsigned long long)cyc, iaddr);

        // data bus
        if (dreq) {
            if (!db.active) {
                if (dio) { db.active = true; db.busy = 1; }
                else if (sram_owner == 0) {
                    sram_owner = 2; db.active = true;
                    int hw = 0;
                    if (top->dBusWishbone_WE) hw = ((top->dBusWishbone_SEL & 3) ? 1 : 0) + ((top->dBusWishbone_SEL & 12) ? 1 : 0);
                    db.busy = ideal ? 1 : ovh + (top->dBusWishbone_WE ? hw * wr : 2 * rd);
                }
            }
            if (db.active && --db.busy == 0) {
                db.active = false;
                if (!dio) sram_owner = 0;
                beats_d++;
                top->dBusWishbone_ACK = 1;
                if (dio) {
                    uint32_t o = daddr & 0xff, v = top->dBusWishbone_DAT_MOSI;
                    if (top->dBusWishbone_WE) {
                        if (o == 0x00) { char c = (char)(v & 0xff); if (c == '\n') { printf("%s\n", line.c_str()); line.clear(); } else line += c; }
                        else if (o == 0x04) { printf("F,%u,%llu,%u\n", v, (unsigned long long)(cyc - last_frame), last_ops); last_frame = cyc; if (++nframes == stop_frames) exit_code = 0; }
                        else if (o == 0x08) exit_code = (int)v;
                        else if (o == 0x10) last_ops = v;
                        else if (o == 0x14) { printf("CKPT %llu\n", (unsigned long long)cyc); last_frame = cyc; if (save_path) save_now = true; }
                    } else {
                        top->dBusWishbone_DAT_MISO = (o == 0x0c) ? (uint32_t)cyc : 0;
                    }
                } else {
                    uint32_t a = daddr & (MEM_SIZE - 1);
                    if (top->dBusWishbone_WE) {
                        uint32_t v = top->dBusWishbone_DAT_MOSI;
                        for (int b = 0; b < 4; b++) if (top->dBusWishbone_SEL & (1 << b)) mem[a + b] = (v >> (8 * b)) & 0xff;
                    } else {
                        uint32_t v; memcpy(&v, &mem[a], 4); top->dBusWishbone_DAT_MISO = v;
                    }
                }
            } else stall_d++;
        }
        // instruction bus
        if (ireq) {
            if (!ib.active && sram_owner == 0 && !(dreq && !db.active && !dio)) {
                sram_owner = 1; ib.active = true; ib.busy = ideal ? 1 : ovh + 2 * rd;
            }
            if (ib.active && --ib.busy == 0) {
                ib.active = false; sram_owner = 0; beats_i++;
                uint32_t v; memcpy(&v, &mem[iaddr & (MEM_SIZE - 1)], 4);
                top->iBusWishbone_DAT_MISO = v;
                top->iBusWishbone_ACK = 1;
            } else stall_i++;
        }
        top->clk = 0; top->eval();
        top->clk = 1; top->eval();
        cyc++;
        if (save_now) {
            st.cyc = cyc; st.ib = ib; st.db = db; st.sram_owner = sram_owner;
            VerilatedSave os;
            os.open(save_path);
            os << *top;
            os.write(mem.data(), MEM_SIZE);
            os.write(&st, sizeof st);
            os.close();
            printf("saved %s\n", save_path);
            exit_code = 0;
        }
    }
    if (!line.empty()) printf("%s\n", line.c_str());
    printf("END run %llu cycles %llu exit %d ibeats %llu dbeats %llu istall %llu dstall %llu\n",
           (unsigned long long)(cyc - start_cyc), (unsigned long long)cyc, exit_code, (unsigned long long)beats_i, (unsigned long long)beats_d,
           (unsigned long long)stall_i, (unsigned long long)stall_d);
    top->final();
    delete top;
    return exit_code == 0 ? 0 : 1;
}
