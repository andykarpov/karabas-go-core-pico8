#!/bin/sh
# karabas: VERIFY builds only: lstate.c with a fixed Lua string-hash seed (upstream mixes heap /
# stack addresses and time into it, so pairs() order differs between the host and rv32).
# usage: seed0.sh lstate.c out.c
sed 's/^  return luaS_hash(buff, p, h);/  (void)buff; (void)p; return 0; \/* karabas: VERIFY seed *\//' "$1" > "$2"
grep -q 'karabas: VERIFY seed' "$2"
