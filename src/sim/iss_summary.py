#!/usr/bin/env python3
# Ф0.2: steady-state per-frame stats from iss outputs: iss_summary.py [skip] files...
import sys
skip = int(sys.argv[1]); print(f"{'cart':10}{'frames':>7}{'instr/fr':>11}{'p95':>11}{'ops/fr':>8}{'ins/op':>8}{'I$miss':>8}{'D$miss':>8}{'stores':>8}")
for fn in sys.argv[2:]:
    rows = [list(map(int, l.split(',')[1:])) for l in open(fn) if l.startswith('F,')][skip:]
    if not rows: continue
    n = len(rows); s = lambda k: sum(r[k] for r in rows)
    ins = sorted(r[1] for r in rows)
    print(f"{fn[4:-4]:10}{n:7}{s(1)//n:11}{ins[n*95//100]:11}{s(2)//n:8}{s(1)/max(1,s(2)):8.1f}{s(3)//n:8}{s(4)//n:8}{s(5)//n:8}")
