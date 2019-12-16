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
  plotter(optparse::Values& args) : args_{args} {}

  void run() {
    if ((int)args_.get("testmem")) {
      run_test_mem();
    } else if ((int)args_.get("plot")) {
      run_plotter();
    } else if ((int)args_.get("test")) {
      run_test();
    }
  }

private:
  void run_test() {
    auto gpu = compute::system::default_device();
    auto plot_id = std::stoull(args_["id"]);
    auto start_nonce = std::stoull(args_["sn"]);
    auto nonces = (int32_t)std::stoull(args_["num"]);

    spdlog::info("do test cpu plot: {}_{}_{}", plot_id, start_nonce, nonces);
    util::timer timer1;
    cpu_plotter cplot;
    cplot.plot(plot_id, start_nonce);
    auto&& chash = cplot.to_string();
    spdlog::info("cpu plot hash: 0x{}", chash.substr(0, 64));
    spdlog::info("cpu plot time cost: {} ms.", timer1.elapsed());

    spdlog::info("do test gpu plot: {}_{}_{}", plot_id, start_nonce, nonces);
    auto plot_args = gpu_plotter::args_t{std::stoull(args_["lws"])
                                        ,std::stoull(args_["gws"])
                                        ,(int32_t)std::stoull(args_["step"])
                                        };
    gpu_plotter gplot(gpu, plot_args);
    auto res = gplot.init("./kernel/kernel.cl", "ploting");
    if (!res)
      spdlog::error("init gpu plotter failed. kernel build log: {}", gplot.program().build_log());
    std::string buff;
    buff.resize(gplot.global_work_size() * gpu_plotter::PLOT_SIZE);
    util::timer timer2;
    for (size_t i=0; i<nonces; i+=gplot.global_work_size())
      gplot.plot( plot_id
                , start_nonce
                , nonces
                , (uint8_t*)buff.data()
                );
    spdlog::info("gpu plot time cost: {} ms.", timer2.elapsed());
    auto ghash = gplot.to_string((uint8_t*)buff.data(), buff.size());
    spdlog::info("gpu plot hash: 0x{}", ghash.substr(0, 64));
  }

  void run_test_mem() {
    util::queue<util::paged_block> q;
    auto mem = uint64_t((double)std::stod(args_["mem"]) * 1024) * 1024 * 1024;
    mem &= ((uint64_t)-1) ^ (plotter_base::PLOT_SIZE - 1);
    spdlog::info("allocate memory: {} bytes ({} GB) ", mem, args_["mem"]);
    util::paged_block block(mem);
    spdlog::info("allocated memory: {:x} {}", (uint64_t)block.data(), block.len());
    q.push(std::move(block));
  }

  void run_plotter() {
    auto dev = compute::system::default_device();
    auto plot_id = std::stoull(args_["id"]);
    auto start_nonce = std::stoull(args_["sn"]);
    auto total_nonces = std::stoull(args_["num"]);
    auto max_mem_to_use = uint64_t((double)std::stod(args_["mem"]) * 1024) * 1024 * 1024;
    auto max_weight_per_file = uint64_t((double)std::stod(args_["weight"]) * 1024) * 1024 * 1024;
    
    auto patharg = args_["drivers"];
    std::regex re{", "};
    auto drivers = std::vector<std::string> {
      std::sregex_token_iterator(patharg.begin(), patharg.end(), re, -1),
      std::sregex_token_iterator()
    };
    if (patharg.empty() || drivers.empty()) {
      spdlog::warn("No dirver specified. exit!!!");
      return;
    }

    // TODO: calc by free space
    // TODO: padding
    auto max_nonces_per_file = max_weight_per_file / plotter_base::PLOT_SIZE;
    auto total_files = std::ceil(total_nonces / max_nonces_per_file);
    auto files_per_driver = std::ceil(total_files / drivers.size());

    // init writer worker and task
    for (auto& driver : drivers) {
      auto worker = std::make_shared<writer_worker>(*this, driver);
      workers_.push_back(worker);
      for (int i=0; i<files_per_driver && total_nonces>0; ++i) {
        int32_t nonces = (int32_t)std::min(total_nonces, max_nonces_per_file);
        auto task = std::make_shared<writer_task>(plot_id, start_nonce, nonces, driver);
        start_nonce += nonces;
        total_nonces -= nonces;
        worker->push_task(std::move(task));
      }
    }

    // init hasher worker
    auto plot_args = gpu_plotter::args_t{std::stoull(args_["lws"])
                                        ,std::stoull(args_["gws"])
                                        ,(int32_t)std::stoull(args_["step"])
                                        };
    auto device = compute::system::default_device();
    auto ploter = std::make_shared<gpu_plotter>(device, plot_args);
    auto res = ploter->init("./kernel/kernel.cl", "ploting");
    if (!res)
      spdlog::error("init gpu plotter failed. kernel build log: {}", ploter->program().build_log());
    auto worker = std::make_shared<hasher_worker>(*this, ploter);
    workers_.push_back(worker);

    spdlog::info("Plotting......");
    std::vector<std::thread> pools;
    for (auto& worker : workers_) {
      pools.emplace_back([=](){ worker->run(); });
    }

    // dispatcher
    while (auto& report = reporter_.pop_for(std::chrono::milliseconds(1000))) {

    }

    for (auto& t : pools)
      t.join();
  }

private:
  optparse::Values& args_;

  std::vector<std::shared_ptr<worker>> workers_;
  std::vector<std::shared_ptr<util::paged_block>> blocks_;
  util::queue<report> reporter_;
};