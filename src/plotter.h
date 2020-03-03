#pragma once

#include <vector>
#include <memory>
#include <regex>
#include <chrono>
#include <boost/compute.hpp>

#include <OptionParser.h>
#include <spdlog/spdlog.h>

#include "common/paged_block.h"
#include "common/queue.h"
#include "common/timer.h"
#include "common/signal.h"
#include "common/utils.h"
#include "poc/cpu_plotter.h"
#include "poc/gpu_plotter.h"
#include "task_hasher.h"
#include "task_writer.h"
#include "worker_hasher.h"
#include "worker_writer.h"
#include "report.h"

namespace compute = boost::compute;

class plotter {
  plotter() = delete;
  plotter(gpu_plotter&) = delete;
  plotter(gpu_plotter&&) = delete;
  plotter& operator=(gpu_plotter&) = delete;
  plotter& operator=(gpu_plotter&&) = delete;

public:
  explicit plotter(optparse::Values& args);

  bool run();

  void report(std::shared_ptr<hasher_task>&& task);
  
  int bench_mode() { return (int)args_.get("bench"); }

private:
  void run_test();

  bool run_plotter();

  void run_disk_bench();

  void run_plot_verify();

private:
  optparse::Values& args_;
  std::vector<std::shared_ptr<worker>> workers_;
  util::queue<std::shared_ptr<hasher_task>> reporter_;
};