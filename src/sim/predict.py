#!/usr/bin/env python3
# Ф0.2 forecast: speed of each cart (% of real time, 60 fake-08 steps/s) per cache size and clock,
# from the iss sweep (sweep_<cart>_<I>_<D>.txt), the fitted base CPI and the SRAM timing in ns
# (read 25 ns per halfword as archie_sram, write 50 ns per halfword), +1 cycle per transaction.
# "-fp" = without the soft-float calls of the fake-08 API casts (share from prof_<cart>.txt).
import math, collections, subprocess, bisect, os
BASE = 1.85
SOFT = ('__muldf3', '__floatsidf', '__fixdfsi', '__adddf3', '__subdf3', '__divdf3', '__truncdfsf2',
        '__extendsfdf2', '__floatunsidf', '__fixunsdfsi', '__ledf2', '__gedf2', '__eqdf2', '__nedf2', '__ltdf2', '__gtdf2', '__unorddf2')
NM = os.path.expanduser('~/opt/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-nm')
def soft_share(cart):
    a, n = [], []
    for l in subprocess.run([NM, '-n', f'../fw/build-rv32imO2/{cart}.elf'], capture_output=True, text=True).stdout.splitlines():
        p = l.split(' ', 2)
        if len(p) == 3 and p[1] in 'tTwW': a.append(int(p[0], 16)); n.append(p[2])
    tot = soft = 0
    for l in open(f'prof_{cart}.txt'):
        pc = int(l.split()[0], 16); i = bisect.bisect_right(a, pc) - 1; tot += 1
        if i >= 0 and n[i] in SOFT: soft += 1
    return soft / tot
def cycles(ins, im, dm, st, mhz):
    rd = math.ceil(25e-3 * mhz); wr = math.ceil(50e-3 * mhz)
    line = 8 * (1 + 2 * rd); stc = 1 + 1.7 * wr
    return BASE * ins + (im + dm) * line + st * stc
carts = ['celeste', 'nuke', 'thndrbld', 'psychik']
cfgs = [(4096, 4096), (16384, 16384), (32768, 16384)]
print(f"{'cart':10}{'soft-fp':>8}" + ''.join(f"{f'I{i//1024}K/D{d//1024}K@{m}':>16}" for i, d in cfgs for m in (40, 60, 80)))
for c in carts:
    sf = soft_share(c)
    row = f"{c:10}{100*sf:7.0f}%"
    for i, d in cfgs:
        _, _, _, n, ins, im, dm, st = open(f'sweep_{c}_{i}_{d}.txt').read().split()
        ins, im, dm, st = int(ins), int(im), int(dm), int(st)
        for m in (40, 60, 80):
            cy = cycles(ins, im, dm, st, m); sp = 100 * m * 1e6 / (60 * cy)
            cy2 = cycles(ins * (1 - sf), im, dm, st * (1 - sf), m); sp2 = 100 * m * 1e6 / (60 * cy2)
            row += f"{sp:7.0f}%/{sp2:4.0f}%  "
    print(row)
