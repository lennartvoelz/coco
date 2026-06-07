# Build on the Linux cluster (not macOS): assumes a recent g++ (>= 14) for
# C++23 std::println / std::views::enumerate, and Intel TBB for the parallel
# execution policies (std::execution::par_unseq).
CXX      ?= g++
CXXFLAGS ?= -O3 -std=c++23 -Wall -Wextra -pthread

all: parallel_scan

# parallel_scan.hpp contains main(); compile it as C++ via -x c++.
parallel_scan: parallel_scan.hpp
	$(CXX) $(CXXFLAGS) -x c++ -o $@ $< $(LDLIBS)

clean:
	rm -f parallel_scan

.PHONY: all clean
