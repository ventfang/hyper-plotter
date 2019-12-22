#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <spdlog/spdlog.h>

#include "worker.h"
#include "task_writer.h"
#include "task_hasher.h"
#include "common/signal.h"
#include "plotter.h"

void writer_worker::run() {
  spdlog::info("thread writer worker [{}] starting.", driver_);
  auto bench_mode = ctx_.bench_mode();
  while (! signal::get().stopped()) {
    auto task = fin_hasher_tasks_.pop();
    if (!task)
      continue;
    if (task->current_write_task == -1 || task->current_write_task >= writer_tasks_.size())
      break;
    if (!task->block || !task->writer)
      break;

    // write plot
    auto& wr_task = writer_tasks_[task->current_write_task];
    if ((bench_mode & 0x01) == 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(5120));
    spdlog::debug("write nonce [{}][{}, {}) ({}) to `{}`"
        , task->current_write_task
        , task->sn
        , task->sn + task->nonces
        , plotter_base::btoh(task->block->data(), 32)
        , wr_task->plot_file());
    ctx_.report(std::move(task));
  }
  spdlog::info("thread writer worker [{}] stopped.", driver_);
}