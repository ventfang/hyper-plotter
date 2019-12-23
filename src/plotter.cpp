#include "plotter.h"

plotter::plotter(optparse::Values& args) : args_{args} {}

void plotter::run() {
  if ((int)args_.get("plot")) {
    run_plotter();
  } else if ((int)args_.get("test")) {
    run_test();
  }
}

void plotter::run_test() {
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
  auto ghash = gplot.to_string((uint8_t*)buff.data(), 32);
  spdlog::info("gpu plot hash: 0x{}", ghash);
}

void plotter::run_plotter() {
  signal::get().install_signal();
  auto plot_id = std::stoull(args_["id"]);
  auto start_nonce = std::stoull(args_["sn"]);
  auto total_nonces = std::stoull(args_["num"]);
  auto max_mem_to_use = uint64_t((double)std::stod(args_["mem"]) * 1024) * 1024 * 1024;
  auto max_weight_per_file = uint64_t((double)std::stod(args_["weight"]) * 1024) * 1024 * 1024;
  util::block_allocator page_block_allocator{max_mem_to_use};
  
  auto patharg = args_["drivers"];
  std::regex re{", "};
  auto drivers = std::vector<std::string> {
    std::sregex_token_iterator(patharg.begin(), patharg.end(), re, -1),
    std::sregex_token_iterator()
  };
  if (patharg.empty() || drivers.empty()) {
    spdlog::warn("No dirver(directory) specified. exit!!!");
    return;
  }

  // TODO: calc by free space
  // TODO: padding
  auto max_nonces_per_file = max_weight_per_file / plotter_base::PLOT_SIZE;
  auto total_files = std::ceil(total_nonces * 1. / max_nonces_per_file);
  auto max_files_per_driver = std::ceil(total_files * 1. / drivers.size());

  // init writer worker and task
  auto sn_to_gen = start_nonce;
  auto nonces_to_gen = total_nonces;
  for (auto& driver : drivers) {
    auto worker = std::make_shared<writer_worker>(*this, driver);
    workers_.push_back(worker);
    for (int i=0; i<max_files_per_driver && nonces_to_gen>0; ++i) {
      int32_t nonces = (int32_t)std::min(nonces_to_gen, max_nonces_per_file);
      auto task = std::make_shared<writer_task>(plot_id, sn_to_gen, nonces, driver);
      sn_to_gen += nonces;
      nonces_to_gen -= nonces;
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
  auto hashing = std::make_shared<hasher_worker>(*this, ploter);
  workers_.push_back(hashing);

  spdlog::warn("Plotting {} - [{} {}) ...", plot_id, start_nonce, start_nonce+total_nonces);
  for (auto& w : workers_) {
    spdlog::warn(w->info(true));
  }

  std::vector<std::thread> pools;
  for (auto& worker : workers_) {
    pools.emplace_back([=](){ worker->run(); });
  }

  // dispatcher
  util::timer plot_timer;
  int cur_worker_pos{0}, max_worker_pos{(int)workers_.size()-1};
  int dispatched_nonces{0};
  int finished_nonces{0};
  int dispatched_count{0};
  int finished_count{0};
  int total_count{(int)std::ceil(total_nonces * 1. / ploter->global_work_size())};
  int on_going_task = 0;
  int64_t vnpm{0}, vmbps{0};
  while (! signal::get().stopped()) {
    auto report = reporter_.pop_for(std::chrono::milliseconds(1000));
    if (report) {
      page_block_allocator.retain(*report->block);
      finished_nonces += report->nonces;
      ++finished_count;
      --on_going_task;
      vnpm = !!vnpm ? (vnpm * (workers_.size() - 1) + report->npm) / workers_.size() : report->npm;
      vmbps = !!vmbps ? (vmbps * (workers_.size() - 1) + report->mbps) / workers_.size() : report->mbps;
      spdlog::warn("[{}%] PLOTTING at {}|{}|{}|{} nonces/min, {}|{} MB/s, time elapsed {} mins."
                , uint64_t(finished_nonces * 100.) / total_nonces
                , dispatched_nonces * 60ull * 1000 / plot_timer.elapsed()
                , finished_nonces * 60ull * 1000 / plot_timer.elapsed()
                , report->npm, vnpm
                , report->mbps, vmbps
                , int(plot_timer.elapsed() / 60.) / 1000.);
    }
    if (workers_.size() == 0)
      continue;
    if (finished_nonces == total_nonces) {
      spdlog::info("Ploting finished!!!");
      break;
    }
    if (dispatched_nonces == total_nonces) {
      spdlog::info("Ploting dispatched!!!");
      continue;
    }
    
    if (on_going_task > workers_.size())
        continue;

    if (hashing->task_queue_size() > std::min(workers_.size(), 3llu))
      continue;

    if (cur_worker_pos >= max_worker_pos)
      cur_worker_pos = 0;

    auto wr_worker = std::dynamic_pointer_cast<writer_worker>(workers_[cur_worker_pos++]);
    if (wr_worker->task_queue_size() > 0)
      continue;
    auto& nb = page_block_allocator.allocate(ploter->global_work_size() * plotter_base::PLOT_SIZE);
    if (! nb)
      continue;

    auto ht = wr_worker->next_hasher_task((int)(ploter->global_work_size()), nb);
    if (! ht) {
      page_block_allocator.retain(nb);
      continue;
    }
    dispatched_nonces += ht->nonces;
    ++dispatched_count;
    spdlog::debug("[{}] submit task ({}/{}/{}) [{}][{} {}) {}"
                , hashing->task_queue_size()
                , dispatched_count
                , finished_count
                , total_count
                , ht->current_write_task
                , ht->sn
                , ht->sn + ht->nonces
                , ht->writer->info());
    hashing->push_task(std::move(ht));
    ++on_going_task;
  }

  signal::get().signal_stop();
  for (auto& w : workers_)
    w->stop();

  spdlog::warn("[{}%] FINISHED PLOTTING at {} nonces/min.", uint64_t(finished_nonces * 100.) / total_nonces, dispatched_nonces * 60ull * 1000 / plot_timer.elapsed());
  spdlog::warn("Total Nonces Generated: {}, time elapsed {} mins", finished_nonces, int(plot_timer.elapsed() / 60.) / 1000.);
  spdlog::info("allocated blocks: {}.", page_block_allocator.size());
  spdlog::info("dispatcher thread stopped!!!");
  for (auto& t : pools)
    t.join();
  spdlog::info("all worker thread stopped!!!");
}

void plotter::report(std::shared_ptr<hasher_task>&& task) { reporter_.push(std::move(task)); }
