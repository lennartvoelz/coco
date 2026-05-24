// Benchmark and correctness test for the barrier implementations in
// barrier.hpp, plus a pthread_barrier reference. Each thread performs N
// barrier crossings; total wallclock time divided by N gives the average
// barrier latency.
//
// Usage: barrier_bench <mode> <threads> <iterations> [slow_tid=-1]
//   mode: central | central_opt | dissemination | pthread
//   slow_tid: if >= 0, that thread occasionally stalls to exercise the
//             straggler case during correctness checking.
// Output (one CSV line on stdout):
//   mode,threads,iterations,total_ops,time_ms,avg_latency_ns,ok

#include "barrier.hpp"

#if defined(__linux__)
  #include <pthread.h>
  #define HAVE_PTHREAD_BARRIER 1
#else
  #define HAVE_PTHREAD_BARRIER 0
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

#if HAVE_PTHREAD_BARRIER
class PthreadBarrier {
  public:
    explicit PthreadBarrier(int procs) : procs_(procs) {
      pthread_barrier_init(&b_, nullptr, static_cast<unsigned>(procs));
    }
    ~PthreadBarrier() { pthread_barrier_destroy(&b_); }
    PthreadBarrier(const PthreadBarrier&) = delete;
    PthreadBarrier& operator=(const PthreadBarrier&) = delete;

    auto procs() const -> int { return procs_; }
    auto wait() -> void { pthread_barrier_wait(&b_); }

  private:
    int procs_;
    pthread_barrier_t b_;
};
#endif

struct Result {
  double total_ms{};
  double avg_ns{};
  bool ok{};
};

// run_loop:
//   setup(tid) is called once per thread and returns a no-arg callable that
//   performs a single barrier wait (capturing any per-thread state).
//
// The loop also enforces a barrier-correctness invariant:
// every thread bumps a shared counter before its barrier call, and after
// the barrier the counter must be >= (i+1)*threads. A buggy barrier that
// releases threads before all have arrived would fail this check.
template <typename Setup>
auto run_loop(int threads, int64_t iters, int slow_tid, Setup setup) -> Result {
  std::atomic<int64_t> work_done{0};
  std::atomic<int> errors{0};
  std::vector<std::thread> ths;
  ths.reserve(static_cast<size_t>(threads));

  auto t0 = std::chrono::steady_clock::now();
  for (int t = 0; t < threads; ++t) {
    ths.emplace_back([&, t]() {
      auto wait = setup(t);
      for (int64_t i = 0; i < iters; ++i) {
        if (slow_tid == t && (i & 1023) == 0) {
          // Brief stall to exercise the straggler case.
          for (int s = 0; s < 1000; ++s) {
            asm volatile("" ::: "memory");
          }
        }
        work_done.fetch_add(1, std::memory_order_relaxed);
        wait();
        int64_t expected = (i + 1) * static_cast<int64_t>(threads);
        if (work_done.load(std::memory_order_acquire) < expected) {
          errors.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }
  for (auto& th : ths) th.join();
  auto t1 = std::chrono::steady_clock::now();

  double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  double avg_ns = total_ms * 1e6 / static_cast<double>(iters);
  bool ok = errors.load() == 0
         && work_done.load() == static_cast<int64_t>(threads) * iters;
  return {total_ms, avg_ns, ok};
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0]
              << " <mode> <threads> <iterations> [slow_tid=-1]\n"
              << "  mode: central | central_opt | dissemination | pthread\n";
    return 1;
  }
  std::string mode = argv[1];
  int threads = std::atoi(argv[2]);
  int64_t iters = std::atoll(argv[3]);
  int slow_tid = (argc >= 5) ? std::atoi(argv[4]) : -1;

  if (threads < 1 || iters < 1) {
    std::cerr << "threads and iterations must be >= 1\n";
    return 1;
  }

  Result r;
  if (mode == "central") {
    CentralBarrier b(threads);
    r = run_loop(threads, iters, slow_tid, [&](int) {
      BarrierState st{};
      return [&b, st = std::move(st)]() mutable { b.arrive_and_wait(st); };
    });
  } else if (mode == "central_opt") {
    CentralBarrierOpt b(threads);
    r = run_loop(threads, iters, slow_tid, [&](int) {
      BarrierState st{};
      return [&b, st = std::move(st)]() mutable { b.arrive_and_wait(st); };
    });
  } else if (mode == "dissemination") {
    DisseminationBarrier b(threads);
    r = run_loop(threads, iters, slow_tid, [&](int t) {
      auto st = b.make_state(t);
      return [&b, st = std::move(st)]() mutable { b.arrive_and_wait(st); };
    });
  } else if (mode == "pthread") {
#if HAVE_PTHREAD_BARRIER
    PthreadBarrier b(threads);
    r = run_loop(threads, iters, slow_tid, [&](int) {
      return [&b]() { b.wait(); };
    });
#else
    std::cerr << "pthread_barrier_t is not available on this platform "
                 "(Linux only); rebuild on the Slurm node.\n";
    return 1;
#endif
  } else {
    std::cerr << "unknown mode: " << mode << "\n";
    return 1;
  }

  std::cout << mode << "," << threads << "," << iters << ","
            << static_cast<int64_t>(threads) * iters << ","
            << r.total_ms << "," << r.avg_ns << ","
            << (r.ok ? 1 : 0) << "\n";

  if (!r.ok) {
    std::cerr << "CORRECTNESS FAILURE: mode=" << mode
              << " threads=" << threads << " iters=" << iters << "\n";
    return 2;
  }
  return 0;
}
