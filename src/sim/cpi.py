#!/usr/bin/env python3
# Ф0.2: CPI per frame = tb_soc cycles / iss instructions on the same frames: cpi.py iss.txt tb.txt
import sys
iss = {int(l.split(',')[1]): l.strip().split(',') for l in open(sys.argv[1]) if l.startswith('F,')}
cyc = ins = 0; bad = 0
for l in open(sys.argv[2]):
    if not l.startswith('F,'): continue
    f = l.strip().split(','); fr = int(f[1])
    if fr not in iss: continue
    if f[3] != iss[fr][3]: bad += 1           # Lua ops must match (same program path)
    cyc += int(f[2]); ins += int(iss[fr][2])
print(f"frames ok, ops mismatches {bad}; cycles {cyc} instr {ins} CPI {cyc / ins:.2f}")
