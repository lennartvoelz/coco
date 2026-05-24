CXX      ?= g++
CXXSTD   ?= -std=c++20
OPT      ?= -O2
WARN     ?= -Wall -Wextra
CXXFLAGS ?= $(CXXSTD) $(OPT) $(WARN) -pthread
LDFLAGS  ?= -pthread

BINS = mcs pq_bench shared_counter_1 shared_counter_2

.PHONY: all clean
all: $(BINS)

mcs: mcs.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

pq_bench: pq_bench.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

shared_counter_1: ../ex3/shared_counter_1.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

shared_counter_2: ../ex3/shared_counter_2.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(BINS)
