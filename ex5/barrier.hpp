#include <array>
#include <atomic>
#include <memory>
#include <new>
#include <vector>

struct BarrierState {
  bool local_sense{true};
};

class CentralBarrier {
  public:
    CentralBarrier(int procs) : procs_(procs), counter_(procs) {}
    ~CentralBarrier() = default;
    // Not copyable/moveable
    CentralBarrier(const CentralBarrier&) = delete;
    CentralBarrier(CentralBarrier&&) = delete;
    CentralBarrier& operator=(const CentralBarrier&) = delete;
    CentralBarrier& operator=(CentralBarrier&&) = delete;
    
    auto procs() const -> int {
      return procs_;
    }

    auto arrive_and_wait(BarrierState& st) -> void {
      auto& l_sense = st.local_sense;
      l_sense = !l_sense;
      if (counter_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        counter_.store(procs_, std::memory_order_release);
        sense_.store(l_sense, std::memory_order_release);
      } else {
        while (sense_.load(std::memory_order_acquire) != l_sense) { /* Spin */ }
      }
    }
  private:
    const int procs_{};
    static constexpr auto alignment = std::hardware_destructive_interference_size;
    alignas(alignment) std::atomic<bool> sense_{true};
    alignas(alignment) std::atomic<int> counter_{};
};


class CentralBarrierOpt {
  public:
    CentralBarrierOpt(int procs) : procs_(procs), counter_(procs) {}
    ~CentralBarrierOpt() = default;
    // Not copyable/moveable
    CentralBarrierOpt(const CentralBarrierOpt&) = delete;
    CentralBarrierOpt(CentralBarrierOpt&&) = delete;
    CentralBarrierOpt& operator=(const CentralBarrierOpt&) = delete;
    CentralBarrierOpt& operator=(CentralBarrierOpt&&) = delete;
    
    auto procs() const -> int {
      return procs_;
    }

    auto arrive_and_wait(BarrierState& st) -> void {
      auto& l_sense = st.local_sense;
      l_sense = !l_sense;
      if (counter_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        counter_.store(procs_, std::memory_order_relaxed);
        sense_.store(l_sense, std::memory_order_release);
        sense_.notify_all();
      } else {
        auto old = !l_sense;
        while (sense_.load(std::memory_order_acquire) != l_sense) {
          sense_.wait(old, std::memory_order_acquire);
        }
      }  
    }
  private:
    const int procs_{};
    static constexpr auto alignment = std::hardware_destructive_interference_size;
    alignas(alignment) std::atomic<bool> sense_{true};
    alignas(alignment) std::atomic<int> counter_{};
};


struct alignas(std::hardware_destructive_interference_size) AlignedFlag {
  std::atomic<bool> value{false};
};

struct Flags {
  // Indexed as [parity][round]: my_flags[r][k] is set by my partner in round k
  // of parity r, and read by me. Each flag occupies its own cache line so
  // partners writing to adjacent rounds do not cause false sharing.
  std::array<std::vector<AlignedFlag>, 2> my_flags;
  // partner_flags[r][k] points to allnodes[(i + 2^k) mod P].my_flags[r][k].
  std::array<std::vector<AlignedFlag*>, 2> partner_flags;

  explicit Flags(int log_p)
      : my_flags{std::vector<AlignedFlag>(log_p),
                 std::vector<AlignedFlag>(log_p)},
        partner_flags{std::vector<AlignedFlag*>(log_p),
                      std::vector<AlignedFlag*>(log_p)} {}

  // Non-copyable/non-movable (atomic members)
  Flags(const Flags&) = delete;
  Flags(Flags&&) = delete;
  Flags& operator=(const Flags&) = delete;
  Flags& operator=(Flags&&) = delete;
};

struct BarrierStateDissemination {
  int parity{0};
  bool sense{true};
  Flags* local_flags{nullptr};
};

class DisseminationBarrier {
  public:
    DisseminationBarrier(int procs)
        : procs_(procs), log_p_(log2_ceil(procs)) {
      nodes_.reserve(procs_);
      for (int i = 0; i < procs_; ++i) {
        nodes_.push_back(std::make_unique<Flags>(log_p_));
      }
      // Wire up each node's partner pointers: round k -> (i + 2^k) mod P
      for (int i = 0; i < procs_; ++i) {
        for (int k = 0; k < log_p_; ++k) {
          int j = (i + (1 << k)) % procs_;
          for (int r = 0; r < 2; ++r) {
            nodes_[i]->partner_flags[r][k] = &nodes_[j]->my_flags[r][k];
          }
        }
      }
    }
    ~DisseminationBarrier() = default;
    DisseminationBarrier(const DisseminationBarrier&) = delete;
    DisseminationBarrier(DisseminationBarrier&&) = delete;
    DisseminationBarrier& operator=(const DisseminationBarrier&) = delete;
    DisseminationBarrier& operator=(DisseminationBarrier&&) = delete;

    auto procs() const -> int {
      return procs_;
    }

    auto make_state(int thread_id) -> BarrierStateDissemination {
      return BarrierStateDissemination{0, true, nodes_[thread_id].get()};
    }

    auto arrive_and_wait(BarrierStateDissemination& st) -> void {
      for (int instance = 0; instance < log_p_; ++instance) {
        st.local_flags->partner_flags[st.parity][instance]
            ->value.store(st.sense, std::memory_order_release);
        while (st.local_flags->my_flags[st.parity][instance]
                   .value.load(std::memory_order_acquire) != st.sense) {
          /* Spin */
        }
      }
      if (st.parity == 1) {
        st.sense = !st.sense;
      }
      st.parity = 1 - st.parity;
    }

  private:
    static auto log2_ceil(int n) -> int {
      int log = 0;
      int v = 1;
      while (v < n) { v <<= 1; ++log; }
      return log;
    }

    const int procs_{};
    const int log_p_{};
    std::vector<std::unique_ptr<Flags>> nodes_;
};
