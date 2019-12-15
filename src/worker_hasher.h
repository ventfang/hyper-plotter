#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include "worker.h"
#include "task_hasher.h"
#include "poc/gpu_plotter.h"
#include "common/queue.h"

class plotter;
class hasher_worker : public worker {
public:
  hasher_worker() = delete;
  explicit hasher_worker(plotter& ctx, std::shared_ptr<gpu_plotter> plotter)
    : ctx_(ctx), plotter_(plotter) {
    spdlog::debug("init hasher worker {}", plotter->info());
  }

  void run() override {
    spdlog::info("thread hasher worker `{}` starting.", plotter_->info());
    spdlog::info("thread hasher worker `{}` stopped.", plotter_->info());
  }
  
  void push_task(std::shared_ptr<hasher_task> task) { hasher_tasks_.push(task); }

private:
  plotter& ctx_;
  std::shared_ptr<gpu_plotter> plotter_;
  
  util::queue<std::shared_ptr<hasher_task>> hasher_tasks_;
};