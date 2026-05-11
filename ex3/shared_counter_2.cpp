#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <thread>

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

auto lock_counter(int N, int C) {
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

int main(int argc, char* argv[]) {
  int N = std::stoi(argv[1]);
  int C = std::stoi(argv[2]);

  using clock = std::chrono::high_resolution_clock;

  auto t0 = clock::now();
  int atomic_result = atomic_counter(N, C);
  auto t1 = clock::now();
  int lock_result = lock_counter(N, C);
  auto t2 = clock::now();

  double atomic_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  double lock_ms   = std::chrono::duration<double, std::milli>(t2 - t1).count();

  std::cout << std::format("{},{},{},{},{:.3f},{:.3f}\n",
                           N, C, atomic_result, lock_result, atomic_ms, lock_ms);

  return 0;
}
