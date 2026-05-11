#include <mutex>
#include <thread>
#include <cstdlib>
#include <chrono>
#include <format>
#include <iostream>

auto shared_counter_basic(int N, int C) -> int {
  std::vector<std::thread> threads(N);
  int counter{0};

  auto increment_counter = [&counter, &C, &N]() -> void {
    for (int i{0}; i < C/N; ++i) {
      ++counter;
    }
  };

  for (int i{0}; i < threads.size(); ++i) {
    threads[i] = std::thread(increment_counter);
  }

  for (auto& t : threads) t.join();

  return counter;
}

auto shared_counter_efficient(int N, int C) -> int {
  std::vector<std::thread> threads(N);
  int counter{0};
  std::mutex mtx;

  auto increment_counter = [&counter, &C, &N, &mtx]() -> void {
    for (int i{0}; i < C/N; ++i) {
      std::lock_guard<std::mutex> lock(mtx);
      ++counter;
    }
  };

  for (int i{0}; i < threads.size(); ++i) {
    threads[i] = std::thread(increment_counter);
  }

  for (auto& t : threads) t.join();

  return counter;
}


int main(int argc, char* argv[]) {
  int N = std::stoi(argv[1]);
  int C = std::stoi(argv[2]);

  using clock = std::chrono::high_resolution_clock;

  auto t0 = clock::now();
  int result = shared_counter_efficient(N, C);
  auto t1 = clock::now();

  double mutex_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

  std::cout << std::format("{},{},{},{:.3f}\n", N, C, result, mutex_ms);

  return 0;
}
