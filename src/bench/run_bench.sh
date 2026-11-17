#!/bin/sh
# Ф0: run the host bench over carts; prints one line per cart:
# cart fps lua_ops_mean lua_ops_p95 us_noaudio us_audio err
# usage: run_bench.sh [frames] cart...
FR=${1:-1800}; shift
for c in "$@"; do
  a=$(timeout 300 ./bench "$c" "$FR" 0 2>&1)
  b=$(timeout 300 ./bench "$c" "$FR" 1 2>&1 | sed -n 's/^native_us_per_frame //p')
  fps=$(echo "$a" | sed -n 's/^fps \([0-9]*\).*/\1/p')
  ops=$(echo "$a" | sed -n 's/^lua_ops_per_frame mean \([0-9]*\) p95 \([0-9]*\).*/\1 \2/p')
  us=$(echo "$a" | sed -n 's/^native_us_per_frame //p')
  err=$(echo "$a" | sed -n 's/.*err "\(.*\)"/\1/p' | cut -c1-60)
  printf '%-22s %3s %8s %8s %8s %8s %s\n' "$(basename "$c")" "$fps" $ops "$us" "$b" "$err"
done
