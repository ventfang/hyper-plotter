#include <unordered_map>
#include "plotter.h"

plotter::plotter(optparse::Values& args) : args_{args} {}

void plotter::run() {
  if ((int)args_.get("plot")) {
    run_plotter();
  } else if ((int)args_.get("test")) {
    run_test();
  } else if ((int)args_.get("diskbench")) {
    run_disk_bench();
  }
}

void plotter::run_test() {
  auto gpu = compute::system::default_device();
  auto plot_id_hex = args_["id"];
  auto start_nonce = std::stoull(args_["sn"]);
  auto nonces = (int32_t)std::stoull(args_["num"]);

  bool valid;
  std::array<uint8_t, 20> plot_id;
  std::tie(valid, plot_id) = plotter_base::to_plot_id_bytes(plot_id_hex);
  if (!valid) {
    spdlog::info("invalid plot id {}", plot_id_hex);
    return;
  }
  spdlog::info("do test cpu plot: {}_{}_{}_{}"
              , plotter_base::SEED_MAGIC
              , plotter_base::btoh(plot_id.data(), 20)
              , start_nonce, nonces);
  util::timer timer1;
  cpu_plotter cplot;
  cplot.plot(plot_id, start_nonce);
  auto&& chash = cplot.to_string();
  spdlog::info("cpu plot hash: 0x{}", chash.substr(0, 64));
  spdlog::info("cpu plot time cost: {} ms.", timer1.elapsed());
}

void plotter::run_plotter() {
  signal::get().install_signal();
  const auto plot_id = std::stoull(args_["id"]);
  const auto start_nonce = std::stoull(args_["sn"]);
  auto total_nonces = std::stoull(args_["num"]);
  const auto max_mem_to_use = (args_["mem"] == "0")
                            ? uint64_t(util::sys_free_mem_bytes() * 0.8)
                            : uint64_t((double)std::stod(args_["mem"]) * 1024) * 1024 * 1024;
  const auto max_weight_per_file = uint64_t((double)std::stod(args_["weight"]) * 1024) * 1024 * 1024;
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

  const auto max_nonces_per_file = max_weight_per_file / plotter_base::PLOT_SIZE;
  const auto total_files = std::ceil(total_nonces * 1. / max_nonces_per_file);

  spdlog::warn("plot id:              {}", plot_id);
  spdlog::warn("start nonce:          {}", start_nonce);
  auto total_size_gb = total_nonces * 1. * plotter_base::PLOT_SIZE / 1024 / 1024 / 1024;
  spdlog::warn("total nonces:         {} ({} GB)", total_nonces, size_t(total_size_gb*100)/100.);
  spdlog::warn("total plot files:     {} in {}", total_files, patharg);
  spdlog::warn("plot file nonces:     {} ({} GB)", max_nonces_per_file, max_weight_per_file / 1024 / 1024 / 1024);

  // init writer worker and task
  std::unordered_map<std::string, std::shared_ptr<writer_worker>> writers;
  std::unordered_map<std::string, int64_t> free_spaces;
  auto sn_to_gen = start_nonce;
  auto nonces_to_gen = total_nonces;
  auto task_allocated = true;
  for (; nonces_to_gen > 0 && task_allocated;) {
    task_allocated = false;
    for (auto& driver : drivers) {
      if (! writers[driver]) {
        auto canonical = driver;
        if (canonical[canonical.size() - 1] != '\\' && canonical[canonical.size() - 1] != '/')
          canonical += "\\";
        // TODO: check disk root
        free_spaces[driver] = util::sys_free_disk_bytes(driver);
        auto worker = std::make_shared<writer_worker>(*this, canonical);
        writers[driver] = worker;
        workers_.push_back(std::move(worker));
      }
      const auto driver_max_nonces = (free_spaces[driver] - 16 * 1024) / plotter_base::PLOT_SIZE;
      auto plot_nonces = (size_t)std::max(0ll, std::min((int64_t)max_nonces_per_file, driver_max_nonces));
      auto nonces = std::min(nonces_to_gen, plot_nonces);
      if (nonces < 16 || nonces >= INT32_MAX) // max 511T
        continue;
      if (nonces_to_gen < 16)
        break;
      auto task = std::make_shared<writer_task>(plot_id, sn_to_gen, (uint32_t)nonces, writers[driver]->canonical_driver());
      sn_to_gen += nonces;
      nonces_to_gen -= nonces;
      free_spaces[driver] = free_spaces[driver] - (int64_t)nonces * plotter_base::PLOT_SIZE - 16 * 1024;
      writers[driver]->push_task(std::move(task));
      task_allocated = true;
    }
  }
  if (nonces_to_gen > 0 && total_nonces >= 16) {
    spdlog::error("DISK SPACE NOT ENOUGH!!! nonces to generate: [{} / {}]", total_nonces - nonces_to_gen, total_nonces);
  }
  total_nonces -= nonces_to_gen;

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

  auto max_flying_tasks = max_mem_to_use / ploter->global_work_size() / plotter_base::PLOT_SIZE;
  max_flying_tasks = std::min(max_flying_tasks, workers_.size() * 2);
  spdlog::warn("max mem to use:       {} GB", max_mem_to_use / 1024 / 1024 / 1024);
  spdlog::warn("max flying tasks:     {} tasks", max_flying_tasks);
  for (auto& w : workers_) {
    spdlog::warn("* {}", w->info(true));
  }
  spdlog::error("* Plotting {} - [{}, {}) ...", plot_id, start_nonce, start_nonce+total_nonces);

  std::cout << "Confirm and Continue [y/N]: ";
  auto yn = std::getc(stdin);
  if (yn != 'Y' && yn != 'y')
    return;

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
    auto report = reporter_.pop_for(std::chrono::milliseconds(200));
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
      continue;
    }
    
    if (on_going_task >= max_flying_tasks)
        continue;

    if (hashing->task_queue_size() > 0llu)
      continue;

    if (cur_worker_pos >= max_worker_pos)
      cur_worker_pos = 0;

    auto wr_worker = std::dynamic_pointer_cast<writer_worker>(workers_[cur_worker_pos]);
    if (wr_worker->task_queue_size() > 0) {
      cur_worker_pos++;
      continue;
    }
    auto& nb = page_block_allocator.allocate(ploter->global_work_size() * plotter_base::PLOT_SIZE);
    if (! nb)
      continue;

    cur_worker_pos++;

    auto ht = wr_worker->next_hasher_task((int)(ploter->global_work_size()), nb);
    if (! ht) {
      page_block_allocator.retain(nb);
      continue;
    }
    dispatched_nonces += ht->nonces;
    ++dispatched_count;
    ++on_going_task;
    spdlog::debug("[{}] submit task ({}/{}/{}) [{}.{}][{} {}) {}"
                , on_going_task
                , dispatched_count
                , finished_count
                , total_count
                , cur_worker_pos
                , ht->current_write_task
                , ht->sn
                , ht->sn + ht->nonces
                , ht->writer->info());
    hashing->push_task(std::move(ht));
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

void plotter::run_disk_bench() {
  auto args = args_["drivers"];
  std::regex re{", "};
  auto argvs = std::vector<std::string> {
    std::sregex_token_iterator(args.begin(), args.end(), re, -1),
    std::sregex_token_iterator()
  };
  if (args.empty() || argvs.size() < 2) {
    spdlog::error("error args: prog.exe filename 0|1|2 [0|1 [0|1]] (file preallocate seek flush)");
    return;
  }

  constexpr auto bytes = 20ull*1024*1024*1024;
  constexpr auto bytes_per_write = 16 * 1024;
  spdlog::info("start disk bench mode");
  auto total_bytes = bytes;
  
  util::file osfile;
  auto create = true;
  if (util::file::exists(argvs[0])) {
    create = false;
  }
  if (!osfile.open(argvs[0], create)) {
    spdlog::error("open {} failed {}", argvs[0], osfile.last_error());
    return;
  }
  if (argvs[1] != "0") {
    auto sparse = argvs[1] == "2";
    if (!osfile.allocate(total_bytes, sparse)) {
      spdlog::error("allocate {} failed {}", argvs[0], osfile.last_error());
      return;
    }
  }
  uint8_t data[bytes_per_write];
  for (auto i=0; i<bytes_per_write; ++i)
    data[i] = 'a';
  util::timer timer;
  osfile.seek(0);
  auto doseek = argvs.size() > 2 && argvs[2] == "1";
  auto doflush = argvs.size() > 3 ? std::stoll(argvs[3]) : 0ull;
  size_t bytes_written = 0;
  for (; total_bytes > 0; total_bytes-=bytes_per_write) {
    if (doseek && !osfile.seek(total_bytes - bytes_per_write)) {
      spdlog::error("seek failed.");
      break;
    }
    if (!osfile.write(data, bytes_per_write)) {
      spdlog::error("write {} failed {}", argvs[0], osfile.last_error());
      spdlog::warn("total_bytes: {}, written: {}", bytes, bytes - total_bytes);
      break;
    }
    bytes_written += bytes_per_write;
    if (doflush && bytes_written > doflush * 1024 * 1024) {
      bytes_written = 0;
      osfile.flush();
    }
  }
  spdlog::warn("total_bytes: {}, written: {}, time elapsed: {} secs, speed: {} MB/s."
    , bytes, bytes - total_bytes, timer.elapsed() / 1000, bytes * 1000 / 1024 / 1024 / timer.elapsed());
}

void plotter::report(std::shared_ptr<hasher_task>&& task) { reporter_.push(std::move(task)); }
