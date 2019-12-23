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

bool perform_write_plot(util::file& osfile, uint8_t* tmp_buf, std::shared_ptr<writer_task>& write_task, std::shared_ptr<hasher_task>& hash_task) {
  size_t size = hash_task->nonces * plotter_base::PLOT_SIZE;
  uint8_t* buff = hash_task->block->data();

  for (int i=0; i<4096; ++i) {
    auto offset = ((hash_task->sn - write_task->sn) + i * write_task->nonces) * 64;
    osfile.seek(offset);
    for (int j=0; j<hash_task->nonces/256; ++j)
      osfile.write(buff, 16*1024);
  }
  return true;
}

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
    spdlog::debug("write nonce [{}][{}, {}) ({}) to `{}`"
                  , task->current_write_task
                  , task->sn
                  , task->sn+task->nonces
                  , plotter_base::btoh(task->block->data(), 32)
                  , wr_task->plot_file());
    if ((bench_mode & 0x01) == 0) {
      util::timer timer;
      auto file_path = wr_task->plot_file();
      if (! util::file::exists(file_path)) {
        if (osfile_.is_open())
          osfile_.close();
        osfile_.open(file_path, true);
        osfile_.allocate(wr_task->init_nonces * plotter_base::PLOT_SIZE);
      }
      if (! osfile_.is_open()) {
        osfile_.open(file_path, false);
        osfile_.allocate(wr_task->init_nonces * plotter_base::PLOT_SIZE);
      } else {
        if (osfile_.filename() == file_path) {
          osfile_.close();
          osfile_.open(file_path, false);
          osfile_.allocate(wr_task->init_nonces * plotter_base::PLOT_SIZE);
        }
      }
      perform_write_plot(osfile_, nullptr, wr_task, task);
      task->mbps = task->nonces * 1000ull * plotter_base::PLOT_SIZE / 1024 / 1024 / timer.elapsed();
    }
    ctx_.report(std::move(task));
  }
  spdlog::info("thread writer worker [{}] stopped.", driver_);
}