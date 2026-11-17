# Ф0 host workload bench (fake-08 headless). make; make prof (gprof build).
F8      ?= ../../upstream/fake-08
SRCDIRS := $(F8)/source $(F8)/libs/z8lua $(F8)/libs/lodepng $(F8)/libs/miniz
INC     := $(addprefix -I,$(SRCDIRS) $(F8)/libs/simpleini $(F8)/libs/utf8-util $(F8)/include $(F8)/test)
SRCS    := $(filter-out $(F8)/source/main.cpp $(F8)/source/logger.cpp $(F8)/source/hostCommonFunctions.cpp $(F8)/libs/z8lua/lua.c $(F8)/libs/z8lua/luac.c $(F8)/libs/z8lua/ltests.c,\
             $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.cpp $(d)/*.c))) \
           $(F8)/test/stubhost.cpp $(F8)/test/nooplogger.cpp bench.cpp
CXXFLAGS := -O2 -std=c++17 -fno-rtti -fexceptions -w $(INC)

all: bench
bench: $(SRCS)
	$(CXX) $(CXXFLAGS) -x c++ $(SRCS) -o $@
bench-pg: $(SRCS)
	$(CXX) $(CXXFLAGS) -pg -fno-inline-functions -fno-omit-frame-pointer -x c++ $(SRCS) -o $@
clean:
	rm -f bench bench-pg gmon.out
.PHONY: all clean
