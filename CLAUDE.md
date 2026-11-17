# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A PICO-8 fantasy console core for the Karabas Go, Mini and Mini G boards (Xilinx Spartan-6 XC6SLX25, ISE 14.7, XST). There is no FPGA upstream: the plan is a soft CPU (VexRiscv) running the fake-08 runtime (MIT, z8lua inside), with drawing and the synth in RTL if Ф0 says so. Docs, in Russian: `docs/PLAN.md` (survey, Ф0 measurements, go / no-go, decisions). Follow the phases; record a changed decision in PLAN.md.

PICO-8 itself is proprietary: only clean-room code (fake-08 / zepto-8 / own), no PICO-8 binaries or BIOS. Test carts stay out of git (staged locally in `~/Downloads/karabas-sd-pico8`).

## Layout

- `upstream/fake-08/`: git submodule, jtothebell/fake-08 at 814991a (with its `libs/z8lua` submodule).
- `src/bench/`: Ф0.1 host workload bench (`make`, `make bench-pg` for gprof, `run_bench.sh frames carts...`). The bench reads `Vm` internals through `#define class struct` (bench only).

## Rules of the core

The rules of the other karabas-go cores apply: standard video timing with syncs always present, `FT_CLK_OUT` about 8 MHz on Mini / Mini G, Go `FDC_*` outputs 0 when unused, Mini / Mini G drive T3 (`FLASH_CS_N`) = 0, OSD switch index = item position unless `"index"`, an OSD value shows 10 characters at most, FL slots 0..7 only, Verilog-2001 for XST, imported code marked `karabas:`.

- **Ask the user before every check on the board** (and the HDMI capture). Ask before a firmware flash.
- One branch per phase of `docs/PLAN.md`, one PR at its end.
- Heavy builds: Mini G only while working on one feature, all boards for a release; map with cost table 2 first.
- Tests and sims run in seconds; longer runs only after asking.

## Memory

The Claude Code memory of this repo is shared with all karabas-go repos: `~/.claude/projects/<this project>/memory` is a symlink to `../karabas-go-notes` (private repo `andykarpov/karabas-go-notes`). Its `MEMORY.md` is split by repo; commit and push it after every memory change.
