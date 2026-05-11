#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

static constexpr size_t ARRAY_BYTES = 1ULL << 30; // 1 GiB
static constexpr size_t CACHELINE_SZ = 64; // bytes per cacheline
static constexpr int NUM_TRIALS = 10;

// Pointer-to-volatile: the compiler must emit a load for every dereference
// and may not reorder or eliminate accesses.
static volatile char* g_buf = nullptr;

// Sequential cacheline-stride read over [offset, offset + length).
// The XOR accumulator prevents the compiler from treating reads as dead.
static void load_region(size_t offset, size_t length)
{
    volatile char acc = 0;
    const volatile char* p = g_buf + offset;
    const volatile char* end = p + length;
    while (p < end) {
        acc ^= *p;
        p += CACHELINE_SZ;
    }
    (void)acc;
}

static double measure_bw(int nthreads)
{
    size_t slice = (ARRAY_BYTES / static_cast<size_t>(nthreads) / CACHELINE_SZ) * CACHELINE_SZ;

    std::vector<std::thread> pool;
    pool.reserve(nthreads);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < nthreads; ++i) {
        size_t off = static_cast<size_t>(i) * slice;
        size_t len = (i == nthreads - 1) ? (ARRAY_BYTES - off) : slice;
        pool.emplace_back(load_region, off, len);
    }
    for (auto& t : pool) t.join();
    auto t1 = std::chrono::steady_clock::now();

    double sec = std::chrono::duration<double>(t1 - t0).count();
    return static_cast<double>(ARRAY_BYTES) / sec / 1e9; // GB/s
}

int main()
{
    g_buf = new volatile char[ARRAY_BYTES];

    // First-touch
    std::memset(const_cast<char*>(g_buf), 0xCC, ARRAY_BYTES);

    int max_threads = static_cast<int>(std::thread::hardware_concurrency());
    if (max_threads <= 0) max_threads = 8;

    std::cout << "# Memory Bandwidth Benchmark\n"
              << "# Array size : " << (ARRAY_BYTES >> 20) << " MiB\n"
              << "# Trials     : " << NUM_TRIALS << "\n"
              << "# Max threads: " << max_threads << "\n#\n"
              << "threads,avg_GBps,peak_GBps\n";

    std::ofstream csv("bandwidth_results.csv");
    csv << "threads,avg_GBps,peak_GBps\n";

    for (int t = 1; t <= max_threads; ++t) {
        measure_bw(t);

        std::vector<double> samples(NUM_TRIALS);
        for (int r = 0; r < NUM_TRIALS; ++r)
            samples[r] = measure_bw(t);

        double avg  = std::accumulate(samples.begin(), samples.end(), 0.0) / NUM_TRIALS;
        double peak = *std::max_element(samples.begin(), samples.end());

        std::cout << t << ","
                  << std::fixed << std::setprecision(2) << avg << ","
                  << std::fixed << std::setprecision(2) << peak << "\n";
        csv       << t << ","
                  << std::fixed << std::setprecision(2) << avg << ","
                  << std::fixed << std::setprecision(2) << peak << "\n";

        std::cerr << "  threads=" << std::setw(2) << t
                  << "  avg=" << std::setw(7) << std::fixed
                  << std::setprecision(2) << avg  << " GB/s"
                  << "  peak=" << std::setw(7) << std::fixed
                  << std::setprecision(2) << peak << " GB/s\n";
    }

    delete[] g_buf;
    return 0;
}
