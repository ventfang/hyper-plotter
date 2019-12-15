#pragma once

#include <memory>
#include <functional>

#include "task.h"
#include "common/paged_block.h"

struct writer_worker;

struct hasher_task : public task {
  uint64_t pid;
  uint64_t sn;
  int32_t  nonces;
  std::shared_ptr<writer_worker> writer;
  std::shared_ptr<util::paged_block> block;
};