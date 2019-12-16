#pragma once

#include <memory>
#include <functional>

#include "task.h"
#include "common/paged_block.h"

class worker;

struct hasher_task : public task {
  explicit hasher_task(uint64_t _pid, uint64_t _sn, int32_t _nonces
                      ,std::shared_ptr<worker> _writer
                      ,std::shared_ptr<util::paged_block> _block)
    : pid(_pid), sn(_sn), nonces(_nonces), writer(_writer), block(_block) {}
  uint64_t pid;
  uint64_t sn;
  int32_t  nonces;
  std::shared_ptr<worker> writer;
  std::shared_ptr<util::paged_block> block;
};