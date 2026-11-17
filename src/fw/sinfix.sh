#!/bin/sh
# karabas: z8lua sin_helper reads sintable[0x1000] (one past its 4096 entries) for angles at
# multiples of 0.25 (a = 0x3fff + 2): garbage, different per platform. The right value there is 0
# (sin(0.25) = -1 exactly). usage: sinfix.sh lpico8lib.c out.c
sed 's/+ sintable\[a >> 2\]);/+ ((a >> 2) < 4096 ? sintable[a >> 2] : 0)); \/* karabas: sintable bound *\//' "$1" > "$2"
grep -q 'karabas: sintable bound' "$2"
