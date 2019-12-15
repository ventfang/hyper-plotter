#pragma once

#include <functional>

#include "task.h"

struct hasher_task : public task {
  uint64_t pid;
  uint64_t sn;
  int32_t  nonces;
  std::function<void()> post_task_;
};