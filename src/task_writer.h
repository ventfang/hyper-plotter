#pragma once

#include <string>
#include <sstream>
#include "task.h"

struct writer_task {
  explicit writer_task(std::string _pid, uint64_t _sn, int32_t _nonces, std::string _driver)
    : sn(_sn), nonces(_nonces), init_nonces(_nonces), init_sn(_sn), prev_(_sn) {
      std::stringstream ss;
      ss << _driver << _pid << "_" << sn << "_" << nonces;
      pf_ = ss.str();
  }

  uint64_t sn;
  int32_t  nonces;
  int64_t  init_nonces{0};
  uint64_t init_sn{0};

  int32_t next(int32_t gws) {
    if (nonces == 0)
      return -1;
    auto n = std::min(gws, nonces);
    nonces -= n;
    sn = prev_;
    prev_ += n;
    return n;
  }

  std::string plot_file() { return pf_; }

  bool submit_nonces(int64_t nonces) {
    submitted_nonces_ += nonces;
    return submitted_nonces_ == init_nonces;
  }

private:
  uint64_t prev_{0};
  int64_t  submitted_nonces_{0};
  std::string pf_;
};