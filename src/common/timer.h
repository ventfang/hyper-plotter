#pragma once

#include <chrono>
namespace util {

class timer
{
public:
  explicit timer() : start_(std::chrono::high_resolution_clock::now()) {}

  template <typename Duration = std::chrono::milliseconds>
  typename Duration::rep elapsed() {
    return std::chrono::duration_cast<Duration>(std::chrono::high_resolution_clock::now() - start_).count();
  }

  void reset() { start_ = std::chrono::high_resolution_clock::now(); }

private:
  std::chrono::high_resolution_clock::time_point start_;
};

}