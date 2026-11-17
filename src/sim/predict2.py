#!/usr/bin/env python3
# Ф0.2 forecast over the whole run (steps from SKIP on), by deadline:
#   game frame = 1 step (_update60 carts) or 2 steps (30 fps carts, fake-08 runs every other step);
#   speed = budget (1/fps) / cost of the frame; mean and p95 (the 95 % slowest frame).
# cycles = BASE*instr + (imiss+dmiss)*line + stores*st  (model of model.py, BASE fitted on Verilator)
# Configurations:
#   M   measured: VexRiscv as generated (I$ 4K / D$ 4K), 40 MHz, tb_soc SRAM rd 1 / wr 3 (Verilator-calibrated)
#   E1  extrapolated: I$ 16K / D$ 16K (VexRiscv to regenerate: sbt), 40 MHz, same SRAM
#   E2  extrapolated: I$ 32K / D$ 16K, 80 MHz (Fmax unknown: Ф0.3), read 25 ns / write 75 ns per halfword (archie_sram)
#   E3  E2 + posted writes (a store costs 1 clock) and without the soft-float of the fake-08 API casts
# usage: predict2.py out_dir   (out_dir of run_f02.sh, plus prof_<cart>.txt there for E3)
import bisect, glob, math, os, re, subprocess, sys
BASE = 1.85
SKIP = 60
OUT = sys.argv[1]
NM = os.path.expanduser('~/opt/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-nm')
FW = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../fw/build-rv32imO2')
SOFT = re.compile(r'^__(mul|add|sub|div)df3$|^__(float|fixuns|fix|floatun)\w*df\w*$|^__(le|ge|eq|ne|lt|gt|unord)df2$|^__(extend|trunc)\w+')

def steps(fn):
    return [list(map(int, l.strip().split(',')[1:7])) for l in open(fn) if l.startswith('F,')][SKIP:]

def soft_share(cart):
    pf = os.path.join(OUT, f'prof_{cart}.txt')
    if not os.path.exists(pf): return 0.0
    a, n = [], []
    for l in subprocess.run([NM, '-n', f'{FW}/{cart}.elf'], capture_output=True, text=True).stdout.splitlines():
        p = l.split(' ', 2)
        if len(p) == 3 and p[1] in 'tTwW': a.append(int(p[0], 16)); n.append(p[2])
    tot = soft = 0
    for l in open(pf):
        i = bisect.bisect_right(a, int(l.split()[0], 16)) - 1; tot += 1
        soft += i >= 0 and bool(SOFT.match(n[i]))
    return soft / max(1, tot)

def cyc(s, line, st, soft=0.0, st1=False):
    _, ins, _, im, dm, stores = s
    return BASE * ins * (1 - soft) + (im + dm) * line + stores * (1 - soft) * (1 if st1 else st)

def fps_of(st):
    # fake-08 runs a 30 fps cart's update / draw every other 60 Hz step: half of the steps are near idle
    ins = sorted(x[1] for x in st); med = ins[len(ins) // 2]
    idle = sum(1 for x in st if x[1] < 0.1 * ins[-1])
    return 30 if idle > 0.4 * len(st) else 60

def speeds(st, line, stc, mhz, fps, soft=0.0, st1=False):
    per = [cyc(s, line, stc, soft, st1) for s in st]
    k = 60 // fps
    frames = sorted(sum(per[i:i + k]) for i in range(0, len(per) - k + 1, k))
    budget = mhz * 1e6 / fps
    mean = budget / (sum(frames) / len(frames)); p95 = budget / frames[len(frames) * 95 // 100]
    return mean, p95

carts = sorted(os.path.basename(f)[5:-14] for f in glob.glob(os.path.join(OUT, 'perf_*_4096_4096.txt')))
print(f"{'cart':24}{'fps':>4}{'soft':>6}{'Minstr/fr':>10}   {'M (4K/4K 40)':>14}{'E1 (16K 40)':>14}{'E2 (32K/16K 80)':>16}{'E3 (+wbuf -fp)':>16}   (mean / p95)")
for c in carts:
    soft = soft_share(c)
    s4 = steps(os.path.join(OUT, f'perf_{c}_4096_4096.txt'))
    if not s4: continue
    fps = fps_of(s4)
    s16 = steps(os.path.join(OUT, f'perf_{c}_16384_16384.txt'))
    s32 = steps(os.path.join(OUT, f'perf_{c}_32768_16384.txt'))
    if not s4: continue
    k = 60 // fps
    mi = sum(x[1] for x in s4) / len(s4) * k / 1e6
    rd80, wr80 = math.ceil(25e-3 * 80), math.ceil(75e-3 * 80)
    cols = [speeds(s4, 8 * 3, 1 + 1.7 * 3, 40, fps), speeds(s16, 8 * 3, 1 + 1.7 * 3, 40, fps),
            speeds(s32, 8 * (1 + 2 * rd80), 1 + 1.7 * wr80, 80, fps),
            speeds(s32, 8 * (1 + 2 * rd80), 1, 80, fps, soft, True)]
    print(f"{c:24}{fps:4}{100*soft:5.0f}%{mi:10.2f}   " + ''.join(f"{100*m:7.0f}/{100*p:4.0f}%  " for m, p in cols))
