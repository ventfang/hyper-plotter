#include <unordered_map>
#include <boost/filesystem.hpp>
#include "plotter.h"
#include "kernel.h"

plotter::plotter(optparse::Values& args) : args_{args} {}

void plotter::run() {
  if ((int)args_.get("plot")) {
    run_plotter();
  } else if ((int)args_.get("test")) {
    run_test();
  } else if ((int)args_.get("diskbench")) {
    run_disk_bench();
  } else if ((int)args_.get("verify")) {
    run_plot_verify();
  }
}

void plotter::run_test() {
  auto gpu = compute::system::default_device();
  auto plot_id_hex = args_["id"];
  auto start_nonce = std::stoull(args_["sn"]);
  auto nonces = (int32_t)std::stoull(args_["num"]);

  bool valid;
  plot_id_t plot_id;
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
  auto&& chash = cplot.to_string(64);
  spdlog::info("cpu plot hash: 0x{}", chash);
  spdlog::info("cpu plot time cost: {} ms.", timer1.elapsed());

  spdlog::info("do test gpu plot: {}_{}_{}_{}", plotter_base::SEED_MAGIC, plot_id_hex, start_nonce, nonces);
  auto plot_args = gpu_plotter::args_t{std::stoull(args_["lws"])
                                      ,std::stoull(args_["gws"])
                                      ,(int32_t)std::stoull(args_["step"])
                                      };
  gpu_plotter gplot(gpu, plot_args);
  auto res = gplot.init(KERNEL_PROG, "plotting", plot_id);
  if (!res)
    spdlog::error("init gpu plotter failed. kernel build log: {}", gplot.program().build_log());
  std::string buff;
  buff.resize(gplot.global_work_size() * gpu_plotter::PLOT_SIZE);
  util::timer timer2;
  for (size_t i=0; i<nonces; i+=gplot.global_work_size())
    gplot.plot( start_nonce
              , nonces
              , (uint8_t*)buff.data()
              );
  spdlog::info("gpu plot time cost: {} ms.", timer2.elapsed());
  uint8_t hash[64];
  transposition((uint8_t*)buff.data(), hash, 0, 0, 1);
  auto ghash = gplot.to_string(hash, 64);
  spdlog::info("gpu plot hash: 0x{}", ghash);
}

void plotter::run_plotter() {
  signal::get().install_signal();
  auto plot_id_hex = args_["id"];
  bool valid;
  plot_id_t plot_id;
  std::tie(valid, plot_id) = plotter_base::to_plot_id_bytes(plot_id_hex);
  if (!valid) {
    spdlog::info("invalid plot id {}", plot_id_hex);
    return;
  }
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

  const auto max_nonces_per_file = (max_weight_per_file / plotter_base::PLOT_SIZE) & ~(63ull);
  const auto total_files = std::ceil(total_nonces * 1. / max_nonces_per_file);

  spdlog::warn("plot id:              {}", plotter_base::btoh(plot_id.data(), 20));
  spdlog::warn("start nonce:          {}", start_nonce);
  auto total_size_gb = total_nonces * 1. * plotter_base::PLOT_SIZE / 1024 / 1024 / 1024;
  spdlog::warn("total nonces:         {} ({} GB)", total_nonces, size_t(total_size_gb*1000)/1000.);
  spdlog::warn("total plot files:     {} in {}", total_files, patharg);
  spdlog::warn("plot file nonces:     {} ({} GB)", max_nonces_per_file, max_weight_per_file / 1024 / 1024 / 1024);

  // init writer worker and task
  std::unordered_map<std::string, std::shared_ptr<writer_worker>> writers;
  std::unordered_map<std::string, int64_t> free_spaces;
  auto sn_to_gen = start_nonce;
  auto nonces_to_gen = total_nonces;
  auto fn_task_allocator = [&](uint64_t nonces_align) {
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
        const auto driver_max_nonces = free_spaces[driver] / plotter_base::PLOT_SIZE;
        auto plot_nonces = (size_t)std::max(0ll, std::min((int64_t)max_nonces_per_file, driver_max_nonces));
        auto nonces = std::min(nonces_to_gen, plot_nonces) & nonces_align;
        if (nonces == 0 || nonces >= INT32_MAX) // max 511T
          continue;
        auto task = std::make_shared<writer_task>(plot_id_hex, sn_to_gen, (uint32_t)nonces, writers[driver]->canonical_driver());
        sn_to_gen += nonces;
        nonces_to_gen -= nonces;
        free_spaces[driver] = free_spaces[driver] - (int64_t)nonces * plotter_base::PLOT_SIZE;
        writers[driver]->push_task(std::move(task));
        task_allocated = true;
      }
    }
  };
  fn_task_allocator(~63ull);
  fn_task_allocator(~0ull);
  
  if (total_nonces != uint64_t(-1) && nonces_to_gen > 0) {
    spdlog::error("DISK SPACE NOT ENOUGH!!! nonces to generate: [{} / {}]", total_nonces - nonces_to_gen, total_nonces);
  }
  total_nonces -= nonces_to_gen;

  // init hasher worker
  auto plot_args = gpu_plotter::args_t{std::stoull(args_["lws"])
                                      ,std::stoull(args_["gws"])
                                      ,(int32_t)std::stoull(args_["step"])
                                      };
  auto device = compute::system::default_device();
  auto plotter = std::make_shared<gpu_plotter>(device, plot_args);
  auto res = plotter->init(KERNEL_PROG, "plotting", plot_id);
  if (!res)
    spdlog::error("init gpu plotter failed. kernel build log: {}", plotter->program().build_log());
  auto hashing = std::make_shared<hasher_worker>(*this, plotter);
  workers_.push_back(hashing);

  auto buffer_size =  std::stol(args_["buffers"]) & 7;
  if (!buffer_size) {
    buffer_size = (plotter->global_work_size() >= 8192) ? 1 : 2;
  }
  auto work_size = plotter->global_work_size() * buffer_size;
  auto max_flying_tasks = max_mem_to_use / work_size / plotter_base::PLOT_SIZE;
  max_flying_tasks = std::min(max_flying_tasks, workers_.size() * 2);
  spdlog::warn("work size:            {} Bytes", work_size);
  spdlog::warn("max mem to use:       {} GB", int64_t((max_mem_to_use / 1024. / 1024 / 1024) * 10) / 10.);
  spdlog::warn("max flying tasks:     {} tasks", max_flying_tasks);
  if (max_flying_tasks == 0) {
    spdlog::error("No Memory to Allocate jobs.");
    return;
  }
  for (auto& w : workers_) {
    spdlog::warn("* {}", w->info(true));
  }
  spdlog::warn("* Plotting {} - [{}, {}), total {} nonces ({} TB)..."
              , plot_id_hex, start_nonce, start_nonce+total_nonces
              , total_nonces
              , int64_t(total_nonces * 256 * 1000 / 1024. / 1024 / 1024) / 1000.);

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
  int total_count{(int)std::ceil(total_nonces * 1. / work_size)};
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
      if (report->mbps)
        vmbps = !!vmbps ? (vmbps * (workers_.size() - 1) + report->mbps) / workers_.size() : report->mbps;
      spdlog::warn("[{}%] PLOTTING at {}|{} nonces/min, {} MB/s, finished: {}/{} GB, time elapsed {} mins."
                , int64_t(finished_nonces * 10000. / total_nonces) / 100.
                , finished_nonces * 60ull * 1000 / plot_timer.elapsed()
                , vnpm
                , vmbps
                , int64_t(finished_nonces * 25600ull / 1024. / 1024) / 100.
                , int64_t(total_nonces * 25600ull / 1024. / 1024) / 100.
                , int(plot_timer.elapsed() / 60.) / 1000.);
    }
    if (workers_.size() == 0)
      continue;
    if (finished_nonces == total_nonces) {
      spdlog::info("Plotting finished!!!");
      break;
    }
    if (dispatched_nonces == total_nonces) {
      continue;
    }
    
    if (on_going_task >= max_flying_tasks)
        continue;

    if (hashing->task_queue_size() > 1llu)
      continue;

    if (cur_worker_pos >= max_worker_pos)
      cur_worker_pos = 0;

    auto wr_worker = std::dynamic_pointer_cast<writer_worker>(workers_[cur_worker_pos]);
    if (wr_worker->task_queue_size() > 0llu) {
      cur_worker_pos++;
      continue;
    }
    auto& nb = page_block_allocator.allocate(work_size * plotter_base::PLOT_SIZE);
    if (! nb)
      continue;

    cur_worker_pos++;

    auto ht = wr_worker->next_hasher_task((int)(work_size), nb);
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
    spdlog::error("error args: prog.exe filename -m Kib -w Kib 0|1|2 [0|1 [0|1]] (file preallocate seek flush)");
    return;
  }

  auto bytes = (args_["weight"] == "0")
                      ? 1ull*1024*1024*1024
                      : std::stoll(args_["weight"]) * 1024;
  auto bytes_per_write = (args_["mem"] == "0")
                                ? 16 * 1024
                                : std::stoll(args_["mem"]) * 1024;
  if (bytes_per_write < 1024)
    bytes_per_write = 1024;
  if (bytes < 100ull*1024*1024)
    bytes = 100ull*1024*1024;
  spdlog::info("start disk bench mode: total: {} GB,  single write: {} KB", bytes/1024./1024/1024, bytes_per_write / 1024.);
  int64_t total_bytes = bytes;
  
  util::file osfile;
  auto create = true;
  if (util::file::exists(argvs[0])) {
    create = false;
  }
  if (!osfile.open(argvs[0], create, true)) {
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
  auto data = new uint8_t[bytes_per_write];
  for (auto i=0; i<bytes_per_write; ++i)
    data[i] = 'a';
  util::timer timer;
  if (!osfile.seek(0))
    spdlog::error("seek failed {}.", osfile.last_error());
  auto doseek = argvs.size() > 2 && argvs[2] == "1";
  auto doflush = argvs.size() > 3 ? std::stoll(argvs[3]) : 0ull;
  size_t bytes_written = 0;
  size_t buffered_bytes = 0;
  for (; total_bytes > 0;) {
    auto bytes_to_write = std::min(total_bytes, bytes_per_write);
    if (doseek && !osfile.seek(std::max(0ll, total_bytes - bytes_to_write))) {
      spdlog::error("seek failed.");
      break;
    }
    if (!osfile.write(data, bytes_to_write)) {
      spdlog::error("write {} failed {}", argvs[0], osfile.last_error());
      spdlog::warn("total_bytes: {}, written: {}", bytes, bytes - total_bytes);
      break;
    }
    bytes_written += bytes_to_write;
    buffered_bytes += bytes_to_write;
    total_bytes -= bytes_to_write;
    if (doflush && buffered_bytes > doflush * 1024 * 1024) {
      buffered_bytes = 0;
      osfile.flush();
    }
    if ((bytes_written & ~0x7ffffff) == bytes_written)
      spdlog::info("bytes_written: {}, speed: {} MB/s, time elapsed: {} secs"
        , bytes_written
        , (bytes_written * 1000 / 1024 / 1024 / timer.elapsed())
        , timer.elapsed() / 1000);
  }
  spdlog::warn("total_bytes: {}, written: {}, time elapsed: {} secs, speed: {} MB/s."
    , bytes, bytes - total_bytes, timer.elapsed() / 1000, bytes * 1000 / 1024 / 1024 / timer.elapsed());
  osfile.close();
  spdlog::warn("finished.");
}

void plotter::run_plot_verify() {
  auto argvs = util::split(args_["drivers"], ", ");
  if (argvs.size() < 1) {
    spdlog::error("error args: prog.exe --verify plot_filename");
    return;
  }
  boost::filesystem::path full(argvs[0]);
  plot_id_t plot_id;
  int64_t start_nonce{0};
  int64_t totla_nonces{0};
  int32_t step = std::stoull(args_["step"]);
  bool valid = boost::filesystem::exists(full);
  if (valid) {
    auto parts = util::split(full.filename().string(), "_");
    valid = parts.size() == 3;
    if (valid) {
      auto plot_id_hex = parts[0];
      start_nonce = std::stoll(parts[1]);
      totla_nonces = std::stoll(parts[2]);
      std::tie(valid, plot_id) = plotter_base::to_plot_id_bytes(plot_id_hex);
    }
  }
  if (!valid) {
    spdlog::info("invalid plot file {}", argvs[0]);
    return;
  }

  util::file osfile;
  if (! osfile.open(argvs[0], false)) {
    spdlog::error("file `{}` failed open [{}].", argvs[0], osfile.last_error());
    return;
  }
  auto file_size = osfile.size();
  auto nonces = file_size / 256 / 1024;
  if (nonces != totla_nonces || (nonces & 7 != nonces) || nonces * 256 * 1024 != file_size) {
    spdlog::error("file `{}` plot file invalide.", argvs[0]);
    return;
  }
  
  auto plot_args = gpu_plotter::args_t{std::stoull(args_["lws"])
                                      ,std::stoull(args_["gws"])
                                      ,(int32_t)std::stoull(args_["step"])
                                      };
  gpu_plotter gplot(compute::system::default_device(), plot_args);
  auto res = gplot.init(KERNEL_PROG, "plotting", plot_id);
  if (!res) {
    spdlog::error("init gpu plotter failed. kernel build log: {}", gplot.program().build_log());
    return;
  }
  
  auto hasher_buffer = new uint8_t[gpu_plotter::PLOT_SIZE * gplot.global_work_size()];
  auto file_cache = new uint8_t[64 * gplot.global_work_size()];
  auto hasher_cache = new uint8_t[64 * gplot.global_work_size()];
  util::timer timer;
  bool ret;
  // TODO: verify_worker thread
  for (int64_t i = 0; i < nonces; i += gplot.global_work_size() + step) {
    int64_t off = std::max(0ll, int64_t(nonces - i - gplot.global_work_size()));
    int64_t sn = start_nonce + off;
    int64_t n = std::min(nonces - i, gplot.global_work_size());
    gplot.plot(sn, n, hasher_buffer);
    for (auto scoop = 0; scoop < 4096; ++scoop) {
      ret  = !osfile.seek((off + scoop * nonces) * 64);
      ret |= !osfile.read(file_cache, 64 * n);
      if (ret) {
        spdlog::error("file `{}` read failed.", argvs[0]);
        return;
      }
      transposition(hasher_buffer, hasher_cache, scoop, 0, n);
      if (memcmp(hasher_cache, file_cache, n * 64) != 0) {
        spdlog::error("file `{}` error at nonce: [{}, {}), scoop: {}.", argvs[0], sn, n, scoop);
        spdlog::info("expect: {}", plotter_base::btoh(hasher_cache, 64));
        spdlog::info("stored: {}", plotter_base::btoh(file_cache, 64));
        return;
      }
    }
    spdlog::info("Verified {}%, speed {} nonces/secs.", (i + n) * 10000 / nonces / 100., (i + n) * 10000 / timer.elapsed() / 10.);
  }
  spdlog::info("Verified {}%, GOOD! speed {} nonces/secs.", 100, nonces * 10000 / timer.elapsed() / 10.);
  osfile.close();
  delete[] hasher_buffer;
  delete[] file_cache;
  delete[] hasher_cache;
}

void plotter::report(std::shared_ptr<hasher_task>&& task) { reporter_.push(std::move(task)); }
