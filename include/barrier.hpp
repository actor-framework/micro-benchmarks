#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>

// Drop-in replacement for std::barrier (based on the TS API as of 2020).
class barrier {
public:
  explicit barrier(ptrdiff_t num_threads)
    : num_threads_(num_threads), count_(0) {
    // nop
  }

  void arrive_and_wait() {
    std::unique_lock<std::mutex> guard{mx_};
    auto new_count = ++count_;
    if (new_count == num_threads_) {
      cv_.notify_all();
    } else if (new_count > num_threads_) {
      count_ = 1;
      cv_.wait(guard, [this] { return count_.load() == num_threads_; });
    } else {
      cv_.wait(guard, [this] { return count_.load() == num_threads_; });
    }
  }

private:
  ptrdiff_t num_threads_;
  std::mutex mx_;
  std::atomic<ptrdiff_t> count_;
  std::condition_variable cv_;
};
