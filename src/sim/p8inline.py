#!/usr/bin/env python3
# Ф0: inline "#include file" lines of a .p8 (the rv32 bench has no file system): p8inline.py in.p8 out.p8
import os, sys
def expand(path, depth=0):
    out = []
    for line in open(path, encoding='utf-8', errors='replace'):
        if line.strip().startswith('#include') and depth < 8:
            out.extend(expand(os.path.join(os.path.dirname(path), line.strip().split(None, 1)[1].strip()), depth + 1))
        else:
            out.append(line if line.endswith('\n') else line + '\n')
    return out
open(sys.argv[2], 'w', encoding='utf-8').writelines(expand(os.path.abspath(sys.argv[1])))
