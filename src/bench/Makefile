# Ф0 host workload bench (fake-08 headless). make; make prof (gprof build).
F8      ?= ../../upstream/fake-08
SRCDIRS := $(F8)/source $(F8)/libs/z8lua $(F8)/libs/lodepng $(F8)/libs/miniz
INC     := $(addprefix -I,$(SRCDIRS) $(F8)/libs/simpleini $(F8)/libs/utf8-util $(F8)/include $(F8)/test)
SRCS    := $(filter-out $(F8)/source/main.cpp $(F8)/source/logger.cpp $(F8)/source/hostCommonFunctions.cpp $(F8)/libs/z8lua/lua.c $(F8)/libs/z8lua/luac.c $(F8)/libs/z8lua/ltests.c $(F8)/libs/z8lua/lpico8lib.c,\
             $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.cpp $(d)/*.c))) \
           $(F8)/test/stubhost.cpp $(F8)/test/nooplogger.cpp bench.cpp lpico8lib_fix.c
CXXFLAGS := -O2 -std=c++17 -fno-rtti -fexceptions -w $(INC)

all: bench
# karabas: sintable bound fix (../fw/sinfix.sh), same as the rv32 build
lpico8lib_fix.c: $(F8)/libs/z8lua/lpico8lib.c ../fw/sinfix.sh
	../fw/sinfix.sh $< $@
bench: $(SRCS)
	$(CXX) $(CXXFLAGS) -x c++ $(SRCS) -o $@
bench-pg: $(SRCS)
	$(CXX) $(CXXFLAGS) -pg -fno-inline-functions -fno-omit-frame-pointer -x c++ $(SRCS) -o $@
# bench-v: fixed Lua string-hash seed (../fw/seed0.sh), to compare with the rv32 VERIFY=1 build
lstate_seed0.c: $(F8)/libs/z8lua/lstate.c ../fw/seed0.sh
	../fw/seed0.sh $< $@
bench-v: $(SRCS) lstate_seed0.c
	$(CXX) $(CXXFLAGS) -x c++ $(filter-out $(F8)/libs/z8lua/lstate.c,$(SRCS)) lstate_seed0.c -o $@
clean:
	rm -f bench bench-pg bench-v lstate_seed0.c lpico8lib_fix.c gmon.out
.PHONY: all clean
