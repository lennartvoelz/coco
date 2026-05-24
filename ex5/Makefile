CXX ?= g++
CXXFLAGS ?= -O3 -std=c++20 -Wall -Wextra -pthread

all: barrier_bench

barrier_bench: barrier_bench.cpp barrier.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f barrier_bench

.PHONY: all clean
