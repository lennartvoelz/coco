#include <algorithm>
#include <barrier>
#include <concepts>
#include <chrono>
#include <functional>
#include <iterator>
#include <random>
#include <thread>
#include <vector>
#include <ranges>
#include <print>
#include <string>
#include <cstdlib>
#include <execution>
#include <numeric>
#include <cstdint>


auto create_random_input(int seed, int size) -> std::vector<std::int64_t>{
  std::vector<std::int64_t> result(size);
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> uniform_dist(1, 100);

  std::generate(result.begin(), result.end(), [&]() { return uniform_dist(rng); });
  return result;
}


template <std::random_access_iterator It>
auto worker(int id, std::ptrdiff_t n, int num_threads, It iter, std::barrier<>& sync) -> void {
  auto compute_range = [id, num_threads](std::ptrdiff_t active_slots) {
    auto slot_begin = static_cast<std::ptrdiff_t>(id) * active_slots / num_threads;
    auto slot_end   = static_cast<std::ptrdiff_t>(id + 1) * active_slots / num_threads;
    return std::make_pair(slot_begin, slot_end);
  };

  for (std::ptrdiff_t stride = 2; stride <= n; stride *= 2) {
    auto active_slots = n / stride;
    auto [begin, end] = compute_range(active_slots);

    for (std::ptrdiff_t k = begin; k < end; ++k) {
      auto idx = (k + 1) * stride - 1;
      iter[idx] += iter[idx - stride / 2];
    }
    sync.arrive_and_wait();
  }

  if (id == 0) {
    iter[n - 1] = 0;
  }
  sync.arrive_and_wait();

  for (std::ptrdiff_t stride = n; stride >= 2; stride /= 2) {
    auto active_slots = n / stride;
    auto [begin, end] = compute_range(active_slots);

    for (std::ptrdiff_t k = begin; k < end; ++k) {
      auto idx = (k + 1) * stride - 1;
      auto temp = iter[idx - stride / 2];
      iter[idx - stride / 2] = iter[idx];
      iter[idx] += temp;
    }
    sync.arrive_and_wait();
  }
}


template <std::random_access_iterator It>
auto parallel_scan(It first, It last, int num_threads = 8) -> void {
  auto n = last - first;
  std::barrier sync(num_threads);
  std::vector<std::jthread> threads(num_threads);

  for (auto&& [id, t] : threads | std::views::enumerate) {
    t = std::jthread(worker<It>, id, n, num_threads, first, std::ref(sync));
  }
}


template <std::invocable Reset, std::invocable<std::vector<std::int64_t>&> Func>
auto benchmark(Reset&& reset, Func&& func, int iterations = 100) -> double {
  double total_ms = 0.0;

  for (int i = 0; i < iterations; ++i) {
    auto data = reset();
    auto start = std::chrono::high_resolution_clock::now();
    func(data);
    auto end = std::chrono::high_resolution_clock::now();
    total_ms += std::chrono::duration<double, std::milli>(end - start).count();
  }

  return total_ms / iterations;
}


int main(int argc, char* argv[]) {
  int seed = std::stoi(argv[1]);
  int size = std::stoi(argv[2]);
  int num_threads = std::stoi(argv[3]);
  constexpr auto parExec = std::execution::par_unseq;

  auto input = create_random_input(seed, size);
  auto reset = [&input]() { return input; };

  auto scan_stl = [parExec](std::vector<std::int64_t>& data) {
    std::exclusive_scan(parExec, data.begin(), data.end(), data.begin(),
                        std::int64_t{0}, std::plus<>());
  };

  auto scan_custom = [num_threads](std::vector<std::int64_t>& data) {
    parallel_scan(data.begin(), data.end(), num_threads);
  };

  auto result_stl = benchmark(reset, scan_stl);
  auto result_custom = benchmark(reset, scan_custom);

  auto output_custom = input;
  parallel_scan(output_custom.begin(), output_custom.end(), num_threads);

  auto output_stl = input;
  std::exclusive_scan(parExec, output_stl.begin(), output_stl.end(), output_stl.begin(),
                      std::int64_t{0}, std::plus<>());

  bool match = (output_custom == output_stl);

  std::println("{},{},{},{},{}", num_threads, size, result_stl, result_custom, match);
}
