#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include "worker.h"
#include "task_hasher.h"
#include "worker_writer.h"
#include "poc/gpu_plotter.h"
#include "common/queue.h"
#include "common/timer.h"
#include "common/signal.h"

class plotter;
class hasher_worker : public worker {
public:
  hasher_worker() = delete;
  explicit hasher_worker(plotter& ctx, std::shared_ptr<gpu_plotter> plotter)
    : ctx_(ctx), plotter_(plotter) {
    spdlog::debug("init hasher worker {}", plotter->info());
  }

  void run() override {
    spdlog::info("thread hasher worker [{}] starting.", plotter_->info());
    while (! signal::get().stopped()) {
      auto task = hasher_tasks_.pop();
      if (!task)
        continue;
      if (task->current_write_task == -1)
        break;
      if (!task->block || !task->writer)
        break;

      // calc plot
      util::timer timer;
      plotter_->plot( task->pid
                    , task->sn
                    , task->nonces
                    , task->block->data()
                    );
      auto writer = std::dynamic_pointer_cast<writer_worker>(task->writer);
      spdlog::debug("establised hash [{}][{} {}) {}", task->current_write_task
                                                    , task->sn
                                                    , task->sn + task->nonces
                                                    , task->writer->info());
      writer->push_fin_hasher_task(std::move(task));
    }
    spdlog::info("thread hasher worker [{}] stopped.", plotter_->info());
  }
  
  void push_task(std::shared_ptr<hasher_task>&& task) { hasher_tasks_.push(std::move(task)); }

  std::string info() override { return std::string("hasher ") + plotter_->info(); }

  void stop() override {
    hasher_tasks_.stop();
  }

  size_t task_queue_size() override { return hasher_tasks_.size(); }
  
private:
  plotter& ctx_;
  std::shared_ptr<gpu_plotter> plotter_;
  util::queue<std::shared_ptr<hasher_task>> hasher_tasks_;
};