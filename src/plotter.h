#pragma once

#include <boost/compute.hpp>

#include <OptionParser.h>
#include <spdlog/spdlog.h>

#include "poc/cpu_plotter.h"
#include "poc/gpu_plotter.h"

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
    auto dev = compute::system::default_device();
    run_test(dev);
  }

private:
  void run_test(compute::device& gpu) {
    auto plot_id = std::stoull(args_["id"]);
    auto start_nonce = std::stoull(args_["sn"]);
    auto nonces = (int32_t)std::stoull(args_["num"]);

    spdlog::info("do test cpu plot: {}_{}_{}", plot_id, start_nonce, nonces);

    cpu_plotter cplot;
    cplot.plot(plot_id, start_nonce);
    auto&& chash = cplot.to_string();
    spdlog::info("cpu plot hash: 0x{}", chash.substr(0, 64));

    spdlog::info("do test gpu plot: {}_{}_{}", plot_id, start_nonce, nonces);
    gpu_plotter gplot(gpu);
    auto res = gplot.init("./kernel/kernel.cl", "ploting", nonces);
    if (!res)
      spdlog::error("init gpu plotter failed. kernel build log: {}", gplot.program().build_log());
    std::string buff;
    buff.resize(nonces * gpu_plotter::PLOT_SIZE);
    gplot.plot( plot_id
              , start_nonce
              , nonces
              , (uint8_t*)buff.data()
              );
    auto ghash = gplot.to_string((uint8_t*)buff.data(), buff.size());
    spdlog::info("gpu plot hash: 0x{}", ghash.substr(0, 64));
  }

private:
  optparse::Values& args_;
};