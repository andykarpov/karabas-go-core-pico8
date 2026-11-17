Pre-generated VexRiscv (SpinalHDL/VexRiscv, MIT) from litex-hub/pythondata-cpu-vexriscv 642ecfe.
Generator options per file: see that repo verilog/Makefile.
- VexRiscv_Min: rv32i, no caches, no bypass
- VexRiscv_Lite: rv32im (iterative mul/div), I$ 2K, no D$
- VexRiscv: rv32im (single-cycle mul), I$ 4K, D$ 4K (write-through)
- VexRiscv_IMAC: VexRiscv + C + A, full CSR
IO range = address bit 31 (uncached).
