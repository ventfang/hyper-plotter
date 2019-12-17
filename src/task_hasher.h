#pragma once

#include <memory>
#include <functional>

#include "task.h"
#include "common/paged_block.h"

class worker;

struct hasher_task : public task {
  explicit hasher_task(uint64_t _pid, uint64_t _sn, int32_t _nonces
                      ,std::shared_ptr<worker> _writer
                      ,int cur_wr_task
                      ,std::shared_ptr<util::paged_block> _block)
    : pid(_pid), sn(_sn), nonces(_nonces)
    , writer(_writer)
    , current_write_task(cur_wr_task)
    , block(_block) {}

  uint64_t pid{0};
  uint64_t sn{0};
  int32_t  nonces{0};
  std::shared_ptr<worker> writer;
  int current_write_task{-1};
  std::shared_ptr<util::paged_block> block;
};