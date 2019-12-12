#pragma once

#include <boost/compute.hpp>

#include "plotter_base.h"

namespace compute = boost::compute;

struct gpu_plotter : public plotter_base {
  gpu_plotter(gpu_plotter&) = delete;
  gpu_plotter(gpu_plotter&&) = delete;
  gpu_plotter& operator=(gpu_plotter&) = delete;
  gpu_plotter& operator=(gpu_plotter&&) = delete;

  gpu_plotter(compute::device& device)
    : device_{device}
    , context_{device} {
    auto cus = device_.compute_units();
    auto dims = device.max_work_item_dimensions();
    local_work_size_ = device_.max_work_group_size();
    global_work_size_ = cus * dims * local_work_size_;
  }

  bool init(const std::string& kernel, const std::string& name, size_t max_nonces) {
    program_ = compute::program::create_with_source_file(kernel, context_);
    try {
      program_.build();
    } catch(...) {
      return false;
    }
    cqueue_ = compute::command_queue{context_, device_};
    dev_buff_ = compute::buffer{context_, max_nonces * PLOT_SIZE};
    kernel_ = compute::kernel{program_, name};
    kernel_.set_arg(0, dev_buff_);
    return true;
  }

  void plot(uint64_t plot_id, uint64_t start_nonce, size_t nonces, uint8_t* host_buff) {
    kernel_.set_arg(1, start_nonce);
    kernel_.set_arg(2, plot_id);
    kernel_.set_arg(3, 0);
    kernel_.set_arg(4, SCOOPS_PER_PLOT * HASHES_PER_SCOOP - 1);
    kernel_.set_arg(5, nonces);

    cqueue_.enqueue_1d_range_kernel(kernel_, 0, global_work_size_, 0);
    cqueue_.finish();

    cqueue_.enqueue_read_buffer(dev_buff_, 0, PLOT_SIZE * nonces, host_buff);
  }

  std::string to_string(uint8_t* buff, size_t len) const { return btoh(buff, len); }

  const compute::device& device() const { return device_; }
  const compute::context& context() const { return context_; }
  const compute::program& program() const { return program_; }

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