#pragma once

#include <string>
#include <sstream>
#include "task.h"

struct writer_task {
  explicit writer_task(uint64_t _pid, uint64_t _sn, int32_t _nonces, std::string _driver)
    : pid(_pid), sn(_sn), nonces(_nonces), driver(_driver) {}

  uint64_t pid;
  uint64_t sn;
  int32_t  nonces;

  std::string driver;

  std::string plot_file() {
    std::stringstream ss;
    ss << pid << "_" << sn << "_" << nonces;
    return ss.str();
  }
};