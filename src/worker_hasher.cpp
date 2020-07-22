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
  util::block_allocator page_block_allocator{10ull*1024*1024*1024};
  spdlog::info("thread hasher worker [{}] starting.", plotter_->info());
  uint64_t pid, sn{0};
  size_t nonces{plotter_->global_work_size()}; 
  int idx=0;
  int count = 1;
  util::timer timer;
  int64_t total_nonces = 0;
  auto& nb = page_block_allocator.allocate(plotter_->global_work_size() * plotter_base::PLOT_SIZE);
  if (! nb){
    return;
  }
  while (! signal::get().stopped()) {
    plotter_->enqueue_cl_task(pid, sn, nonces);
    plotter_->enqueue_read_buffer(idx, (uint8_t*)nb.data());
    sn += nonces;
    total_nonces += nonces;
    if (count++ % 20 == 0) {
      plotter_->enqueue_finish(0);
      spdlog::warn("speed2 {}", total_nonces * 60 * 1000 / timer.elapsed());
      continue;
    }
    continue;
    plotter_->enqueue_finish(0);
    plotter_->enqueue_read_buffer(idx, (uint8_t*)nb.data());
    plotter_->enqueue_cl_task(pid, sn, nonces);
    plotter_->enqueue_finish(1);
    plotter_->enqueue_finish(0);
  }

  return;
  
  auto bench_mode = ctx_.bench_mode();
  std::deque<gpu_plotter::qitem_t::ptr> pending_hashings;
  while (! signal::get().stopped()) {
    auto task = hasher_tasks_.pop();
    if (!task) {
      spdlog::error("no task");
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
      spdlog::error("new task");
      auto qi = plotter_->enqueue_cl_task(task->pid, task->sn, task->nonces);
      if (pending_hashings.size() > 0) {
        auto qi = pending_hashings.front();
        pending_hashings.pop_front();
        plotter_->enqueue_finish(1);
        report(qi->htask);
      }
      plotter_->enqueue_finish(0);
      qi->htask.swap(task);
      plotter_->enqueue_wait_task(qi, qi->htask->block->data());
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