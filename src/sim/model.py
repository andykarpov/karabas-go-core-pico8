#!/usr/bin/env python3
# Ф0.2 cycle model: cycles = base*instr + (imiss+dmiss)*line + stores*st, line / st from the
# SRAM timing (8-word lines, a word = ovh + 2*rd, a store ~ ovh + 2*wr (sw) / ovh + wr (sb/sh)).
# base is fitted on a tb_soc run against the iss counts of the same frames.
import sys
def frames(fn):
    return {int(l.split(',')[1]): list(map(int, l.strip().split(',')[1:])) for l in open(fn) if l.startswith('F,')}
def fit(iss_fn, tb_fn, rd, wr, ovh, st_words=1.7):
    I, T = frames(iss_fn), frames(tb_fn)
    ins = im = dm = st = cyc = 0
    for f, t in T.items():
        if f in I:
            ins += I[f][1]; im += I[f][3]; dm += I[f][4]; st += I[f][5]; cyc += t[1]
    line = 8 * (ovh + 2 * rd); stc = ovh + st_words * wr
    base = (cyc - (im + dm) * line - st * stc) / ins
    return base, cyc, ins, im, dm, st
if __name__ == '__main__':
    for iss_fn, tb_fn, rd, wr, ovh in [('iss_celeste_ref.txt', 'vl_celeste.txt', 1, 3, 1),
                                       ('iss_celeste_ref.txt', 'vl_celeste_ideal.txt', 0, 0, 1),
                                       ('iss_celeste_ref.txt', 'vl_celeste_r2w2.txt', 2, 2, 1),
                                       ('iss_psychik_ref.txt', 'vl_psychik.txt', 1, 3, 1),
                                       ('iss_psychik_ref.txt', 'vl_psychik_ideal.txt', 0, 0, 1)]:
        b, cyc, ins, im, dm, st = fit(iss_fn, tb_fn, rd, wr, ovh)
        print(f"{tb_fn:24} base CPI {b:.2f}  (CPI {cyc/ins:.2f}, imiss/kinstr {1000*im/ins:.1f}, dmiss/kinstr {1000*dm/ins:.1f}, stores/kinstr {1000*st/ins:.0f})")
