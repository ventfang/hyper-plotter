#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <spdlog/spdlog.h>

#include "worker.h"
#include "task_writer.h"
#include "task_hasher.h"
#include "common/signal.h"
#include "common/file.h"

class plotter;
class writer_worker : public worker {
  static constexpr int SCOOPS_PER_WRITE = 16384;
  static constexpr int SCOOP_BYTES_PER_WRITE = SCOOPS_PER_WRITE * 64;
public:
  writer_worker() = delete;
  explicit writer_worker(plotter& ctx, std::string& driver)
    : ctx_(ctx), driver_(driver) {
    write_buffer_ = (uint8_t*) operator new[](SCOOP_BYTES_PER_WRITE, std::align_val_t(4096));
    spdlog::debug("init writer worker {}.", driver);
  }
  ~writer_worker() {
    operator delete[](write_buffer_, std::align_val_t(4096));
  }

  void task_do(std::shared_ptr<writer_task>& task) {}

  void run() override;
  
  void push_task(std::shared_ptr<writer_task>&& task) { writer_tasks_.emplace_back(std::move(task)); }

  void push_fin_hasher_task(std::shared_ptr<hasher_task>&& task) { fin_hasher_tasks_.push(std::move(task)); }

  bool perform_write_plot(std::shared_ptr<writer_task>& write_task, std::shared_ptr<hasher_task>& hash_task);

  std::shared_ptr<hasher_task> next_hasher_task(int gws, util::paged_block& block) {
    //std::unique_lock<std::mutex> lock(mux_);
    while (cur_writer_task_index_ < writer_tasks_.size()) {
      auto& wt = writer_tasks_[cur_writer_task_index_];
      auto nonces = wt->next(gws);
      if (nonces == -1) {
        ++cur_writer_task_index_;
        continue;
      }
      auto ht = std::make_shared<hasher_task>(wt->pid, wt->sn, nonces
                                            , shared_from_this()
                                            , cur_writer_task_index_
                                            , &block);
      return ht;
    }
    return {nullptr};
  }

  std::string info(bool detail = false) override {
    std::stringstream ss;
    ss << "writer [" << driver_ << "] (";
    if (detail && writer_tasks_.size()) {
      ss << std::accumulate(std::next(writer_tasks_.begin())
                          , writer_tasks_.end()
                          , writer_tasks_[0]->plot_file()
                          , [](std::string &a, const std::shared_ptr<writer_task> &b) -> decltype(auto) {
                              return  std::move(a) + ", " + b->plot_file();
                            });
    } else {
      ss << writer_tasks_.size() << " files";
    }
    ss << ").";
    return ss.str();
  }

  size_t writer_task_count() const { return writer_tasks_.size(); }

  void stop() override {
    fin_hasher_tasks_.stop();
  }

  size_t task_queue_size() override { return fin_hasher_tasks_.size(); }
  
  const std::string canonical_driver() const {
    return driver_;
  }

private:
  plotter& ctx_;
  std::string driver_;
  util::file osfile_;
  uint8_t* write_buffer_;
  
  std::mutex mux_;
  int cur_writer_task_index_{0};
  std::vector<std::shared_ptr<writer_task>> writer_tasks_;
  util::queue<std::shared_ptr<hasher_task>> fin_hasher_tasks_;
};

extern void transposition(uint8_t* data, uint8_t* write_buff, int cur_scoop, int nstart, int nsize);