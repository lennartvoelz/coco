#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <vector>

using lock_t = std::atomic<int>;

auto atomic_counter(int N, int C) -> int {
  std::vector<std::thread> threads(N);
  std::atomic<int> counter{0};

  auto increment_counter = [&counter, &C, &N]() -> void {
    for (int i{0}; i < C/N; ++i) counter.fetch_add(1);
  };

  for (auto& t : threads) {
    t = std::thread(increment_counter);
  }

  for (auto& t : threads) {
    t.join();
  }

  return counter.load();
}

auto lock_rmw(lock_t& lock) -> void {
  while (lock.exchange(1) == 1) { /* spin */ }
}

auto unlock_rmw(lock_t& lock) -> void {
  lock.store(0);
}

auto lock_counter(int N, int C) -> int {
  std::vector<std::thread> threads(N);
  lock_t lock{0};
  int counter{0};

  auto increment_counter = [&lock, &counter, &C, &N]() -> void {
    for (int i{0}; i < C/N; ++i) {
      lock_rmw(lock);
      ++counter;
      unlock_rmw(lock);
    }
  };

  for (auto& t : threads) {
    t = std::thread(increment_counter);
  }

  for (auto& t : threads) {
    t.join();
  }

  return counter;
}

struct qNode {
  std::atomic<qNode*> next{nullptr};
  std::atomic<bool> locked{false};
};

class mcsLock {
  public:
    auto acquire(qNode& node) -> void {
      node.next.store(nullptr, std::memory_order_relaxed);
      qNode* pred = tail.exchange(&node, std::memory_order_acq_rel);
      if (pred) {
        node.locked.store(true, std::memory_order_relaxed);
        pred->next.store(&node, std::memory_order_release);
        while (node.locked.load(std::memory_order_acquire)) {
          // Spin
        }
      }
    }

    auto release(qNode& node) -> void {
      if (!node.next.load(std::memory_order_acquire)) {
        qNode* pred = &node;
        if (tail.compare_exchange_strong(pred, nullptr,
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {
          return;
        }
        while (!node.next.load(std::memory_order_acquire)) {
          // Spin
        }
      }
      node.next.load(std::memory_order_acquire)
          ->locked.store(false, std::memory_order_release);
    }

  private:
    std::atomic<qNode*> tail{nullptr};
};

auto mcs_counter(int N, int C) -> int {
  std::vector<std::thread> threads(N);
  mcsLock lock;
  int counter{0};

  auto increment_counter = [&lock, &counter, &C, &N]() -> void {
    qNode node;
    for (int i{0}; i < C/N; ++i) {
      lock.acquire(node);
      ++counter;
      lock.release(node);
    }
  };

  for (auto& t : threads) {
    t = std::thread(increment_counter);
  }

  for (auto& t : threads) {
    t.join();
  }

  return counter;
}

auto pthread_counter(int N, int C) -> int {
  std::vector<std::thread> threads(N);
  pthread_mutex_t mu;
  pthread_mutex_init(&mu, nullptr);
  int counter{0};

  auto increment_counter = [&mu, &counter, &C, &N]() -> void {
    for (int i{0}; i < C/N; ++i) {
      pthread_mutex_lock(&mu);
      ++counter;
      pthread_mutex_unlock(&mu);
    }
  };

  for (auto& t : threads) {
    t = std::thread(increment_counter);
  }

  for (auto& t : threads) {
    t.join();
  }

  pthread_mutex_destroy(&mu);
  return counter;
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cerr << "usage: " << argv[0] << " <threads> <total_increments>\n";
    return 2;
  }

  int N = std::stoi(argv[1]);
  int C = std::stoi(argv[2]);

  using clock = std::chrono::high_resolution_clock;

  auto t0 = clock::now();
  int atomic_result = atomic_counter(N, C);
  auto t1 = clock::now();
  int lock_result = lock_counter(N, C);
  auto t2 = clock::now();
  int mcs_result = mcs_counter(N, C);
  auto t3 = clock::now();
  int pthread_result = pthread_counter(N, C);
  auto t4 = clock::now();

  double atomic_ms  = std::chrono::duration<double, std::milli>(t1 - t0).count();
  double lock_ms    = std::chrono::duration<double, std::milli>(t2 - t1).count();
  double mcs_ms     = std::chrono::duration<double, std::milli>(t3 - t2).count();
  double pthread_ms = std::chrono::duration<double, std::milli>(t4 - t3).count();

  std::cout << std::format(
      "{},{},{},{},{},{},{:.3f},{:.3f},{:.3f},{:.3f}\n",
      N, C, atomic_result, lock_result, mcs_result, pthread_result,
      atomic_ms, lock_ms, mcs_ms, pthread_ms);

  return 0;
}
