#include <deque>
#include <spdlog/spdlog.h>

#include "plotter.h"
#include "worker.h"
#include "task_hasher.h"
#include "worker_hasher.h"
#include "worker_writer.h"
#include "poc/gpu_plotter.h"
#include "common/queue.h"
#include "common/timer.h"
#include "common/signal.h"

void hasher_worker::run() {
  spdlog::info("thread hasher worker [{}] starting.", plotter_->info());
  auto bench_mode = ctx_.bench_mode();
  std::deque<gpu_plotter::qitem_t::ptr> pending_hashings;
  while (! signal::get().stopped()) {
    if (! plotter_->is_free()) {
      if (pending_hashings.size() > 0) {
        auto qi = pending_hashings.front();
        pending_hashings.pop_front();
        plotter_->enqueue_wait_task(qi, qi->htask->block->data());
        report(qi->htask);
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      continue;
    }

    auto task = hasher_tasks_.pop_for(std::chrono::milliseconds(100));
    if (!task) {
      if (pending_hashings.size() > 0) {
        auto qi = pending_hashings.front();
        pending_hashings.pop_front();
        plotter_->enqueue_wait_task(qi, qi->htask->block->data());
        report(qi->htask);
      }
      continue;
    }
    if (task->current_write_task == -1)
      break;
    if (!task->block || !task->writer)
      break;

    // calc plot
    if ((bench_mode & 0x02) == 0) {
      auto qi = plotter_->enqueue_cl_task(task->pid, task->sn, task->nonces);
      qi->htask.swap(task);
      pending_hashings.emplace_back(std::move(qi));
    } else {
      report(task);
    }
  }

  while (! signal::get().stopped() && pending_hashings.size() > 0) {
    auto qi = pending_hashings.front();
    pending_hashings.pop_front();
    plotter_->enqueue_wait_task(qi, qi->htask->block->data());
    report(qi->htask);
  }

  spdlog::info("thread hasher worker [{}] stopped.", plotter_->info());
}

void hasher_worker::report(std::shared_ptr<hasher_task>& task) {
  auto writer = std::dynamic_pointer_cast<writer_worker>(task->writer);
  spdlog::debug("establised hash [{}][{} {}) {}", task->current_write_task
                                                , task->sn
                                                , task->sn + task->nonces
                                                , task->writer->info());
  writer->push_fin_hasher_task(std::move(task));
}