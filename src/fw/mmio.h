/* Ф0.2 bench MMIO (simulation testbench, src/sim/tb_soc.cpp) */
#pragma once
#include <stdint.h>

#define MMIO_REG(o) (*(volatile uint32_t *)(0x80000000u + (o)))
#define MMIO_UART   MMIO_REG(0x00) /* write: console byte */
#define MMIO_FRAME  MMIO_REG(0x04) /* write: frame index, the testbench logs the cycle count */
#define MMIO_EXIT   MMIO_REG(0x08) /* write: stop the simulation with this code */
#define MMIO_CYCLES MMIO_REG(0x0c) /* read: clock cycles since reset (low 32 bits) */
#define MMIO_OPS    MMIO_REG(0x10) /* write: Lua VM ops of the last frame (hook build only) */
#define MMIO_CKPT   MMIO_REG(0x14) /* write: init done; the testbench may save its state here (--save) */
