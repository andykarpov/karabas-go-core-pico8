# Ф0.2 bench firmware: fake-08 for rv32 (xPack riscv-none-elf-gcc).
#   make CART=/path/cart.p8 [ARCH=rv32im|rv32imc|rv32i] [FRAMES=60] [AUDIO=0] [OPS_HOOK=0] [FBHASH=0: screen hash per frame] [VERIFY=1: fixed Lua hash seed] [LINES=N: trace Lua lines of N steps] [LOG=1: fake-08 log on the console]
# The fake-08 objects are built once per ARCH in build-$(ARCH)/; the cart only relinks.
XPACK   ?= $(HOME)/opt/xpack-riscv-none-elf-gcc-15.2.0-1/bin
CROSS   := $(XPACK)/riscv-none-elf-
ARCH    ?= rv32im
OPT     ?= -O2
FRAMES  ?= 60
AUDIO   ?= 0
OPS_HOOK ?= 0
FBHASH  ?= 0
LINES   ?= 0
LOG     ?= 0
CART    ?=
NAME    ?= $(basename $(basename $(notdir $(CART))))

F8      := ../../upstream/fake-08
VERIFY  ?= 0
B       := build-$(ARCH)$(subst -,,$(OPT))$(if $(filter 1,$(VERIFY)),-v)
LIBDIRS := $(F8)/source $(F8)/libs/z8lua $(F8)/libs/lodepng $(F8)/libs/miniz
INC     := -I. $(addprefix -I,$(LIBDIRS) $(F8)/libs/simpleini $(F8)/libs/utf8-util $(F8)/include $(F8)/test)
LIBSRC  := $(filter-out $(F8)/source/main.cpp $(F8)/source/logger.cpp $(F8)/source/hostCommonFunctions.cpp \
             $(F8)/libs/z8lua/lua.c $(F8)/libs/z8lua/luac.c $(F8)/libs/z8lua/ltests.c $(F8)/libs/z8lua/lvm.c $(F8)/libs/z8lua/lpico8lib.c, \
             $(foreach d,$(LIBDIRS),$(wildcard $(d)/*.cpp $(d)/*.c))) \
           $(F8)/test/stubhost.cpp
LIBOBJ  := $(addprefix $(B)/,$(notdir $(addsuffix .o,$(LIBSRC)))) $(B)/lvm_o2.c.o $(B)/lpico8lib_fix.c.o
ifeq ($(VERIFY),1)
LIBOBJ  := $(filter-out $(B)/lstate.c.o,$(LIBOBJ)) $(B)/lstate_seed0.c.o
endif
ABI     := -march=$(ARCH) -mabi=ilp32
# rv32 newlib has int32_t = long: z8lua fix32 then converts to int through double (soft-float).
# Make int32_t = int as on the host (only names / mangling change, same size); C++ code here
# does not pass int32_t to the prebuilt libstdc++.
INT32    := -U__INT32_TYPE__ -D__INT32_TYPE__=int -U__UINT32_TYPE__ "-D__UINT32_TYPE__=unsigned int" \
            -U__INT_LEAST32_TYPE__ -D__INT_LEAST32_TYPE__=int -U__UINT_LEAST32_TYPE__ "-D__UINT_LEAST32_TYPE__=unsigned int"
# LUA_USE_LONGJMP: Lua errors / yields by longjmp, not C++ throw (fake-08 has no catch;
# a throw per frame cost ~14 %). MINIZ_NO_TIME: no utime() in newlib.
# karabas: fake-08 z8lua builds luaV_execute with optimize("O0") (2022, for loops near the
# fix32 maximum looped forever = signed overflow UB). -fwrapv makes the overflow defined, and
# lvm_o2.c (a copy without the attribute, made in the build dir) gets the normal -O2.
CXXFLAGS := $(ABI) $(OPT) -fwrapv $(INT32) -DLUA_USE_LONGJMP -DMINIZ_NO_TIME -std=c++17 -fno-rtti -fexceptions -ffunction-sections -fdata-sections -w $(INC)
CFLAGS   := $(ABI) $(OPT) -ffunction-sections -fdata-sections -w -I.
# crti / crtbegin / crtend / crtn kept (own crt0 only): __dso_handle and the EH frame registration for C++ exceptions
CRT      = $(shell $(CROSS)gcc $(ABI) -print-file-name=$(1))
LDFLAGS  := $(ABI) -nostartfiles -T link.ld -Wl,--gc-sections -Wl,-Map,$(B)/$(NAME).map

vpath %.cpp $(LIBDIRS) $(F8)/test
vpath %.c $(LIBDIRS)

all: $(B)/$(NAME).bin

$(B)/%.cpp.o: %.cpp | $(B)
	$(CROSS)g++ $(CXXFLAGS) -c $< -o $@
$(B)/%.c.o: %.c | $(B)
	$(CROSS)g++ $(CXXFLAGS) -x c++ -c $< -o $@
$(B):
	mkdir -p $@
$(B)/lvm_o2.c: $(F8)/libs/z8lua/lvm.c | $(B)
	sed 's/__attribute__((optimize("O0"))) //' $< > $@
	@! grep -q 'optimize("O0")' $@
# VERIFY=1: fixed Lua string-hash seed (seed0.sh), same pairs() order as the host bench with
# BENCH_SEED0 build (make bench-v there); for the host / rv32 checks only, own build dir.
$(B)/lstate_seed0.c: $(F8)/libs/z8lua/lstate.c seed0.sh | $(B)
	./seed0.sh $< $@
$(B)/lstate_seed0.c.o: $(B)/lstate_seed0.c
	$(CROSS)g++ $(CXXFLAGS) -x c++ -c $< -o $@
$(B)/lpico8lib_fix.c: $(F8)/libs/z8lua/lpico8lib.c sinfix.sh | $(B)
	./sinfix.sh $< $@
$(B)/lpico8lib_fix.c.o: $(B)/lpico8lib_fix.c
	$(CROSS)g++ $(CXXFLAGS) -I$(F8)/libs/z8lua -x c++ -c $< -o $@
$(B)/lvm_o2.c.o: $(B)/lvm_o2.c
	$(CROSS)g++ $(CXXFLAGS) -x c++ -c $< -o $@

OPTS := $(B)/opts-$(NAME)-$(FRAMES)-$(AUDIO)-$(OPS_HOOK)-$(FBHASH)-$(LOG)-$(LINES)
$(OPTS): | $(B)
	rm -f $(B)/opts-$(NAME)-*; touch $@
$(B)/$(NAME).elf: $(LIBOBJ) main.cpp uartlogger.cpp fbhash.h $(OPTS) crt0.S syscalls.c cart.S link.ld $(CART)
	@test -n "$(CART)" || { echo "CART= not set"; exit 1; }
	$(CROSS)g++ $(CXXFLAGS) -DFRAMES=$(FRAMES) -DAUDIO=$(AUDIO) -DOPS_HOOK=$(OPS_HOOK) -DFBHASH=$(FBHASH) -DLINES=$(LINES) -c main.cpp -o $(B)/main.o
	$(CROSS)gcc $(CFLAGS) -c syscalls.c -o $(B)/syscalls.o
	$(CROSS)g++ $(CXXFLAGS) -c $(if $(filter 1,$(LOG)),uartlogger.cpp,$(F8)/test/nooplogger.cpp) -o $(B)/logger.o
	$(CROSS)gcc $(ABI) -c crt0.S -o $(B)/crt0.o
	$(CROSS)gcc $(ABI) -DCART_FILE='"$(CART)"' -c cart.S -o $(B)/cart.o
	$(CROSS)g++ $(LDFLAGS) $(B)/crt0.o $(call CRT,crti.o) $(call CRT,crtbegin.o) $(B)/main.o $(B)/syscalls.o $(B)/logger.o $(B)/cart.o $(LIBOBJ) $(call CRT,crtend.o) $(call CRT,crtn.o) -o $@
	$(CROSS)size $@

$(B)/$(NAME).bin: $(B)/$(NAME).elf
	$(CROSS)objcopy -O binary $< $@

clean:
	rm -rf build-*
.PHONY: all clean
