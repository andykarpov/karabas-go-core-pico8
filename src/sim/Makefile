# Ф0.2 Verilator SoC: make CPU=VexRiscv|VexRiscv_Lite|VexRiscv_Min|VexRiscv_IMAC -> obj_$(CPU)/tb_soc
CPU ?= VexRiscv
OBJ := obj_$(CPU)

all: $(OBJ)/tb_soc

$(OBJ)/tb_soc: tb_soc.cpp ../rtl/vexriscv/$(CPU).v
	verilator --cc --exe --build -j 8 -O3 -Wno-fatal -Wno-lint -Wno-style \
	  --savable --top-module VexRiscv --prefix Vcpu -Mdir $(OBJ) -o tb_soc \
	  -CFLAGS "-O2" -MAKEFLAGS "OPT_FAST=-O3 OPT_SLOW=-O1" ../rtl/vexriscv/$(CPU).v tb_soc.cpp >$(OBJ).log 2>&1 || { tail -20 $(OBJ).log; exit 1; }

clean:
	rm -rf obj_* obj_*.log
.PHONY: all clean
