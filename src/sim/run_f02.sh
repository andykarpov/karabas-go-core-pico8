#!/bin/sh
# Ф0.2 full run, ISS only (seconds to a few minutes on 8 cores):
#  (host bench-v and the rv32 check build share a fixed Lua string-hash seed: same pairs() order)
#  1. host bench: Lua ops + screen hash per step, PPM screens at steps 120 / 240 / last;
#  2. rv32 check build (OPS_HOOK + FBHASH) in the iss: ops and hashes must match the host step by step;
#  3. rv32 clean build (no hook, no hash) in the iss for I$/D$ 4K/4K, 16K/16K, 32K/16K -> perf_*.txt.
# usage: run_f02.sh frames out_dir cart...   (then: predict.py out_dir)
set -e
FR=$1; OUT=$2; shift 2
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
for c in "$@"; do
  n=$(basename "$c"); n=${n%%.*}
  mkdir -p "$OUT/ppm_$n"
  BENCH_FRAMES=1 BENCH_PPM="$OUT/ppm_$n:120,240,$((FR - 1))" "$HERE/../bench/bench-v" "$c" "$FR" 0 > "$OUT/host_$n.txt"
  make -s -C "$HERE/../fw" CART="$c" NAME="${n}_chk" FRAMES="$FR" OPS_HOOK=1 FBHASH=1 VERIFY=1 >/dev/null
  make -s -C "$HERE/../fw" CART="$c" NAME="$n" FRAMES="$FR" >/dev/null
done
B="$HERE/../fw/build-rv32imO2"
{
  for c in "$@"; do
    n=$(basename "$c"); n=${n%%.*}
    echo "$B-v/${n}_chk.bin --icache 4096 --dcache 4096 > $OUT/chk_$n.txt"
    for cfg in 4096:4096 16384:16384 32768:16384; do
      ic=${cfg%:*}; dc=${cfg#*:}
      echo "$B/$n.bin --icache $ic --dcache $dc > $OUT/perf_${n}_${ic}_${dc}.txt"
    done
  done
} | xargs -P 8 -I CMD sh -c "$HERE/iss CMD 2>/dev/null || true"
python3 "$HERE/check_host.py" "$OUT"
