#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <pthread.h>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>


struct qNode {
  std::atomic<qNode*> next{nullptr};
  std::atomic<bool> locked{false};
};


class mcsLock {
public:
  void acquire(qNode& node) {
    node.next.store(nullptr, std::memory_order_relaxed);
    qNode* pred = tail_.exchange(&node, std::memory_order_acq_rel);
    if (pred) {
      node.locked.store(true, std::memory_order_relaxed);
      pred->next.store(&node, std::memory_order_release);
      while (node.locked.load(std::memory_order_acquire)) { /* spin */ }
    }
  }

  void release(qNode& node) {
    if (!node.next.load(std::memory_order_acquire)) {
      qNode* pred = &node;
      if (tail_.compare_exchange_strong(pred, nullptr,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
        return;
      }
      while (!node.next.load(std::memory_order_acquire)) { /* spin */ }
    }
    node.next.load(std::memory_order_acquire)
        ->locked.store(false, std::memory_order_release);
  }

private:
  std::atomic<qNode*> tail_{nullptr};
};


class PthreadMutex {
public:
  PthreadMutex() { pthread_mutex_init(&mu_, nullptr); }
  ~PthreadMutex() { pthread_mutex_destroy(&mu_); }

  struct Guard {
    pthread_mutex_t* m;
    explicit Guard(PthreadMutex& mu) : m(&mu.mu_) { pthread_mutex_lock(m); }
    ~Guard() { pthread_mutex_unlock(m); }
  };

private:
  pthread_mutex_t mu_;
};

class McsMutex {
public:
  struct Guard {
    mcsLock* l;
    qNode n;
    explicit Guard(McsMutex& m) : l(&m.lock_) { l->acquire(n); }
    ~Guard() { l->release(n); }
  };

private:
  mcsLock lock_;
};


struct Task {
  int priority;
  int payload;
  bool operator<(const Task& other) const { return priority < other.priority; }
};

template <class Lock>
class PriorityQueue {
public:
  void push(const Task& t) {
    typename Lock::Guard g(lock_);
    q_.push(t);
  }

  bool pop(Task& out) {
    typename Lock::Guard g(lock_);
    if (q_.empty()) return false;
    out = q_.top();
    q_.pop();
    return true;
  }

private:
  Lock lock_;
  std::priority_queue<Task> q_;
};

// ---------------------------------------------------------------------------
// Workload: each thread runs `ops_per_thread` iterations. On each iteration
// it flips a thread-local coin: heads -> push a random-priority task, tails
// -> pop. Pops on an empty queue still count as an operation (and are
// tallied separately).
// ---------------------------------------------------------------------------
struct Result {
  double time_ms;
  std::uint64_t total_ops;
  std::uint64_t pops_empty;
};


template <class Lock>
Result run(int threads, int ops_per_thread, std::uint64_t seed) {
  PriorityQueue<Lock> pq;
  std::atomic<std::uint64_t> empties{0};
  std::vector<std::thread> ts;
  ts.reserve(threads);

  auto t0 = std::chrono::high_resolution_clock::now();
  for (int tid = 0; tid < threads; ++tid) {
    ts.emplace_back([&, tid] {
      std::mt19937_64 rng(seed ^
                          (0x9E3779B97F4A7C15ULL * std::uint64_t(tid + 1)));
      std::uint64_t local = 0;
      Task out;
      for (int i = 0; i < ops_per_thread; ++i) {
        std::uint64_t r = rng();
        if (r & 1ULL) {
          pq.push(Task{int((r >> 1) & 0xFFFF), i});
        } else if (!pq.pop(out)) {
          ++local;
        }
      }
      empties.fetch_add(local, std::memory_order_relaxed);
    });
  }
  for (auto& t : ts) t.join();
  auto t1 = std::chrono::high_resolution_clock::now();

  return {
      std::chrono::duration<double, std::milli>(t1 - t0).count(),
      std::uint64_t(threads) * std::uint64_t(ops_per_thread),
      empties.load(),
  };
}


static void usage(const char* prog) {
  std::cerr << "usage: " << prog
            << " <mcs|mutex> <threads> <ops_per_thread> [seed]\n";
}

int main(int argc, char* argv[]) {
  if (argc < 4) {
    usage(argv[0]);
    return 2;
  }

  std::string mode = argv[1];
  int threads = std::stoi(argv[2]);
  int ops_per_thread = std::stoi(argv[3]);
  std::uint64_t seed =
      (argc >= 5) ? std::uint64_t(std::stoull(argv[4])) : 42ULL;

  if (threads <= 0 || ops_per_thread <= 0) {
    usage(argv[0]);
    return 2;
  }

  Result r;
  if (mode == "mcs") {
    r = run<McsMutex>(threads, ops_per_thread, seed);
  } else if (mode == "mutex") {
    r = run<PthreadMutex>(threads, ops_per_thread, seed);
  } else {
    usage(argv[0]);
    return 2;
  }

  double ops_per_sec =
      (r.time_ms > 0.0) ? double(r.total_ops) / (r.time_ms / 1000.0) : 0.0;

  std::cout << std::format("{},{},{},{},{},{:.3f},{:.0f}\n", mode, threads,
                           ops_per_thread, r.total_ops, r.pops_empty,
                           r.time_ms, ops_per_sec);
  return 0;
}
