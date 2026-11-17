#!/usr/bin/env python3
# Ф0.2: host bench vs rv32 iss, step by step: Lua ops (hook, 64 granularity) and screen hash.
# check_host.py out_dir  -> one line per cart
import glob, os, sys
out = sys.argv[1]
for hf in sorted(glob.glob(os.path.join(out, 'host_*.txt'))):
    n = os.path.basename(hf)[5:-4]
    host = {int(l.split(',')[1]): l.strip().split(',') for l in open(hf) if l.startswith('F,')}
    cf = os.path.join(out, f'chk_{n}.txt')
    if not os.path.exists(cf):
        print(f'{n:12} no rv32 run'); continue
    rv = {int(l.split(',')[1]): l.strip().split(',') for l in open(cf) if l.startswith('F,')}
    ops_bad = [f for f in rv if f in host and rv[f][3] != host[f][2]]
    hash_bad = [f for f in rv if f in host and rv[f][7] != '%u' % int(host[f][3])]
    err = [l.strip() for l in open(cf) if l.startswith('done') or l.startswith('load failed')]
    print(f'{n:12} steps host {len(host)} rv32 {len(rv)}  ops mismatch {len(ops_bad)} '
          f'(first {min(ops_bad) if ops_bad else "-"})  screen mismatch {len(hash_bad)} '
          f'(first {min(hash_bad) if hash_bad else "-"})  {err[0] if err else "no end"}')
