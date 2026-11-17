#!/usr/bin/env python3
# Ф0.2: flat profile from iss --prof samples: prof.py <fw.elf> <samples> [top]
import bisect, subprocess, sys, collections
elf, smp = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 25
nm = subprocess.run([sys.argv[0].rsplit('/', 1)[0] + '/../../toolchain-nm' if False else
                     __import__('os').path.expanduser('~/opt/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-nm'),
                     '-n', '-C', elf], capture_output=True, text=True).stdout
addrs, names = [], []
for l in nm.splitlines():
    p = l.split(' ', 2)
    if len(p) == 3 and p[1] in 'tTwW':
        addrs.append(int(p[0], 16)); names.append(p[2])
c = collections.Counter(); n = 0
for l in open(smp):
    pc = int(l.split()[0], 16); n += 1
    i = bisect.bisect_right(addrs, pc) - 1
    c[names[i][:90] if i >= 0 else '?'] += 1
for k, v in c.most_common(top):
    print(f'{100.0 * v / n:6.2f} {k}')
