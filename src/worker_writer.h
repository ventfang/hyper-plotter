#pragma once

#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>

#include "worker.h"
#include "task_writer.h"
#include "task_hasher.h"

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
    while (auto task = fin_hasher_tasks_.pop()) {
      if (!task)
        continue;
      if (!task->block || !task->writer)
        break;
      // write plot
    }
    spdlog::info("thread writer worker `{}` stopped.", driver_);
  }
  
  void push_task(std::shared_ptr<writer_task>&& task) { writer_tasks_.emplace_back(std::move(task)); }

  void push_fin_hasher_task(std::shared_ptr<hasher_task>&& task) { fin_hasher_tasks_.push(std::move(task)); }

  std::shared_ptr<hasher_task> next_hasher_task(size_t gws) {
    //std::unique_lock<std::mutex> lock(mux_);
    while (cur_writer_task_index_ < writer_tasks_.size()) {
      auto& wt = writer_tasks_[cur_writer_task_index_];
      auto nonces = wt->next(gws);
      if (nonces == -1) {
        ++cur_writer_task_index_;
        continue;
      }
      auto ht = std::make_shared<hasher_task>(wt->pid, wt->sn, nonces, shared_from_this(), nullptr);
      return ht;
    }
    return {nullptr};
  }

private:
  plotter& ctx_;
  std::string driver_;
  
  std::mutex mux_;
  int cur_writer_task_index_{0};
  std::vector<std::shared_ptr<writer_task>> writer_tasks_;
  util::queue<std::shared_ptr<hasher_task>> fin_hasher_tasks_;
};