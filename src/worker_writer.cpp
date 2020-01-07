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

#define NONCES_VECTOR           16
#define NONCES_VECTOR_MASK      15
#define NONCES_VECTOR_ALIGN     (~15)
#define MESSAGE_CAP             64
#define NUM_HASHES              8192
#define HASH_SIZE_WORDS         8
#define NONCE_SIZE_WORDS        HASH_SIZE_WORDS * NUM_HASHES
#define Address(nonce,hash,word) \
              ((nonce) & NONCES_VECTOR_ALIGN) * NONCE_SIZE_WORDS \
            + (hash) * NONCES_VECTOR * HASH_SIZE_WORDS \
            + (word) * NONCES_VECTOR \
            + ((nonce) & NONCES_VECTOR_MASK)

static constexpr auto PLOTTING_FILE_EXT = ".plotting";

void transposition(uint8_t* data, uint8_t* write_buff, int cur_scoop, int nstart, int nsize) {
  uint32_t* src = (uint32_t*)data;
  uint32_t* des = (uint32_t*)write_buff;

  for (; nsize-->0; ++nstart,des+=16) {
    des[0x00] = src[Address(nstart, cur_scoop * 2, 0)];
    des[0x01] = src[Address(nstart, cur_scoop * 2, 1)];
    des[0x02] = src[Address(nstart, cur_scoop * 2, 2)];
    des[0x03] = src[Address(nstart, cur_scoop * 2, 3)];
    des[0x04] = src[Address(nstart, cur_scoop * 2, 4)];
    des[0x05] = src[Address(nstart, cur_scoop * 2, 5)];
    des[0x06] = src[Address(nstart, cur_scoop * 2, 6)];
    des[0x07] = src[Address(nstart, cur_scoop * 2, 7)];
    des[0x08] = src[Address(nstart, 8192 - (cur_scoop * 2 + 1), 0)];
    des[0x09] = src[Address(nstart, 8192 - (cur_scoop * 2 + 1), 1)];
    des[0x0A] = src[Address(nstart, 8192 - (cur_scoop * 2 + 1), 2)];
    des[0x0B] = src[Address(nstart, 8192 - (cur_scoop * 2 + 1), 3)];
    des[0x0C] = src[Address(nstart, 8192 - (cur_scoop * 2 + 1), 4)];
    des[0x0D] = src[Address(nstart, 8192 - (cur_scoop * 2 + 1), 5)];
    des[0x0E] = src[Address(nstart, 8192 - (cur_scoop * 2 + 1), 6)];
    des[0x0F] = src[Address(nstart, 8192 - (cur_scoop * 2 + 1), 7)];
  }
}

bool writer_worker::perform_write_plot(std::shared_ptr<writer_task>& write_task, std::shared_ptr<hasher_task>& hash_task) {
  size_t size = (size_t)hash_task->nonces * plotter_base::PLOT_SIZE;
  uint8_t* buff = hash_task->block->data();

  for (int cur_scoop=0; !signal::get().stopped() && cur_scoop<4096; ++cur_scoop) {
    auto offset = ((hash_task->sn - write_task->init_sn) + cur_scoop * write_task->init_nonces) * 64;
    if (! osfile_.seek(offset)) {
      spdlog::error("Seek file `{}` failed with code {}.", osfile_.filename(), osfile_.last_error());
      return false;
    }
    int nonces = hash_task->nonces;
    for (int nstart=0; !signal::get().stopped() && nonces>0; nonces-=SCOOPS_PER_WRITE, nstart+=SCOOPS_PER_WRITE) {
      transposition(hash_task->block->data(), write_buffer_, cur_scoop, nstart, std::min(nonces, SCOOPS_PER_WRITE));
      if (!osfile_.write(write_buffer_, std::min(nonces, SCOOPS_PER_WRITE) * 64)) {
        spdlog::error("Write file `{}` failed with code {}.", osfile_.filename(), osfile_.last_error());
        return false;
      }
    }
  }
  return true;
}

void writer_worker::run() {
  spdlog::info("thread writer worker [{}] starting.", driver_);
  auto bench_mode = ctx_.bench_mode();
  bool success = true;
  int last_write_task = -1;
  while (success && ! signal::get().stopped()) {
    auto task = fin_hasher_tasks_.pop();
    if (!task)
      continue;
    if (task->current_write_task == -1 || task->current_write_task >= writer_tasks_.size())
      break;
    if (!task->block || !task->writer)
      break;

    // write plot
    auto& wr_task = writer_tasks_[task->current_write_task];
    if ((bench_mode & 0x01) == 0) {
      auto dio = (wr_task->init_nonces & (~7ull)) == wr_task->init_nonces;
      util::timer timer;
      auto plotting_file = wr_task->plot_file() + PLOTTING_FILE_EXT;
      if (last_write_task != task->current_write_task) {
        last_write_task = task->current_write_task;
        osfile_.close(); // FIXME: out of order task
      }
      do {
        if (! osfile_.is_open()) {
          auto do_create = ! util::file::exists(plotting_file);
          spdlog::debug("nonces={}, open file `{}`, dio mode {} create new {}."
                      , wr_task->init_nonces, wr_task->plot_file(), dio, do_create);
          osfile_.open(plotting_file, do_create, dio);
          osfile_.allocate(wr_task->init_nonces * plotter_base::PLOT_SIZE);
        }
        success = perform_write_plot(wr_task, task);
        if (!success) {
          spdlog::error("writer worker [{}] osfile error with code [{}].", driver_, osfile_.last_error());
          osfile_.close();
          std::this_thread::sleep_for(std::chrono::seconds(5));
        } else {
          if (wr_task->submit_nonces(task->nonces)) {
            osfile_.close();
            auto& filename = osfile_.filename();
            // TODO: failed rename
            util::file::rename(plotting_file, wr_task->plot_file());
          }
        }
      } while (!success && ! signal::get().stopped());
      if (!success)
        task->mbps = 0;
      else
        task->mbps = int(task->nonces * 1000ull * plotter_base::PLOT_SIZE / 1024 / 1024 / timer.elapsed());
    }
    spdlog::debug("write nonce [{}] [{}][{}, {}) ({}) to `{}`"
                  , success
                  , task->current_write_task
                  , task->sn
                  , task->sn+task->nonces
                  , plotter_base::btoh(task->block->data(), 32)
                  , wr_task->plot_file());
    ctx_.report(std::move(task));
  }
  spdlog::info("waiting for file released...");
  osfile_.close();
  if (!success)
    spdlog::error("thread writer worker [{}] stopped with error.", driver_);
  else
    spdlog::error("thread writer worker [{}] stopped.", driver_);
}