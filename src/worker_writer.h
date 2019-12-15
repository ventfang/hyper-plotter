#pragma once

#include <vector>
#include <spdlog/spdlog.h>

#include "worker.h"
#include "task_writer.h"

class plotter;
class writer_worker : public worker {
public:
  writer_worker() = delete;
  explicit writer_worker(plotter& ctx, std::string& driver)
    : ctx_(ctx), driver_(driver) {
    spdlog::debug("init writer worker {}.", driver);
  }

  void task_do(std::shared_ptr<writer_task>& task) {}

  void run() override {
    spdlog::info("thread writer worker `{}` starting.", driver_);
    for (auto& task : writer_tasks_) {
      spdlog::info("new task {} started.", task->plot_file());
      task_do(task);
      spdlog::info("new task {} finished.", task->plot_file());
    }
    spdlog::info("thread writer worker `{}` stopped.", driver_);
  }
  
  void push_task(std::shared_ptr<writer_task>&& task) { writer_tasks_.emplace_back(std::move(task)); }

private:
  plotter& ctx_;
  std::string driver_;
  
  std::vector<std::shared_ptr<writer_task>> writer_tasks_;
};