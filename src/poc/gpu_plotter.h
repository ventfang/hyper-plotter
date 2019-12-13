#pragma once

#include <boost/compute.hpp>

#include <spdlog/spdlog.h>

#include "plotter_base.h"

namespace compute = boost::compute;

struct gpu_plotter : public plotter_base {
  gpu_plotter(gpu_plotter&) = delete;
  gpu_plotter(gpu_plotter&&) = delete;
  gpu_plotter& operator=(gpu_plotter&) = delete;
  gpu_plotter& operator=(gpu_plotter&&) = delete;

  gpu_plotter(compute::device& device, size_t lws = 0, size_t gws = 0)
    : device_{device}
    , context_{device} {
    auto cus = device_.compute_units();
    auto dims = device.max_work_item_dimensions();
    local_work_size_ = lws ? lws : device_.max_work_group_size();
    global_work_size_ = gws ? gws : cus * dims * local_work_size_;
    spdlog::info("gpu_plotter cus:{}, dims: {}, gws: {}, lws: {}.", cus, dims, global_work_size_, local_work_size_);
    // TODO: check host free memory?
    auto max_nonces = size_t(device_.global_memory_size() * 0.9 / PLOT_SIZE);
    global_work_size_ = std::min(max_nonces, global_work_size_);
    global_work_size_  = global_work_size_ / local_work_size_ * local_work_size_;
    assert(global_work_size_ >= 16);
    if (global_work_size_ < 16)
      global_work_size_ = 16;
    spdlog::info("gpu_plotter gws: {}, lws: {}.", global_work_size_, local_work_size_);
  }

  bool init(const std::string& kernel, const std::string& name) {
    program_ = compute::program::create_with_source_file(kernel, context_);
    try {
      program_.build();
    } catch(...) {
      return false;
    }
    cqueue_ = compute::command_queue{context_, device_};
    dev_buff_ = compute::buffer{context_, global_work_size_ * PLOT_SIZE};
    kernel_ = compute::kernel{program_, name};
    kernel_.set_arg(0, dev_buff_);
    return true;
  }

  void plot(uint64_t plot_id, uint64_t start_nonce, size_t nonces, uint8_t* host_buff) {
    nonces = std::min(nonces, global_work_size_);
    kernel_.set_arg(1, start_nonce);
    kernel_.set_arg(2, hton_ull(plot_id));
    kernel_.set_arg(3, 0);
    kernel_.set_arg(4, SCOOPS_PER_PLOT * HASHES_PER_SCOOP);
    kernel_.set_arg(5, nonces);

    cqueue_.enqueue_1d_range_kernel(kernel_, 0, global_work_size_, local_work_size_);
    cqueue_.finish();

    cqueue_.enqueue_read_buffer(dev_buff_, 0, PLOT_SIZE * nonces, host_buff);
  }

  std::string to_string(uint8_t* buff, size_t len) const { return btoh(buff, len); }

  const compute::device& device() const { return device_; }
  const compute::context& context() const { return context_; }
  const compute::program& program() const { return program_; }
  size_t global_work_size() const { return global_work_size_; }

private:
  compute::device device_;
  compute::context context_;
  compute::program program_;
  compute::kernel kernel_;
  compute::command_queue cqueue_;
  compute::buffer dev_buff_;

  size_t global_work_size_{0};
  size_t local_work_size_{0};
};