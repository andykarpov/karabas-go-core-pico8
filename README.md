# karabas-go-core-pico8

PICO-8 fantasy console for the Karabas Go, Mini and Mini G boards: a feasibility study only (Ф0), **stopped** (2026-11-17). fake-08 on a VexRiscv soft CPU runs light carts at 20-70 % of real time and heavy ones at 3-5 %; even with bigger caches, 80 MHz and RTL drawing, Lua-bound carts stay at 10-20 %. Measurements, tools and the verdict: `docs/PLAN.md` (Russian).

Clean-room: based on [fake-08](https://github.com/jtothebell/fake-08) (MIT). PICO-8 is a trademark of Lexaloffle Games; this project is not affiliated with it.

## Licence

GPL-3.0 (`LICENSE`), as the other karabas-go cores. Imported parts keep their own licences: fake-08, z8lua and VexRiscv are MIT (their notices are kept with the code).
