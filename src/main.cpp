#include <string>
#include <cstdint>
#include <numeric>
#include <algorithm>

#include <boost/compute.hpp>

namespace compute = boost::compute;

#include <OptionParser.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "config.h"

namespace opt = optparse;
using namespace std;

const string usage = "usage: %prog [OPTION]... [DIRECTORY]...";

const string version = "%prog " PARALLEL_PLOTER_VER "-" GIT_BRANCH "-" GIT_COMMIT_HASH "\n"
  "Copyright (C) 2019-2020 The FreeStyle developers\n"
  "Distributed under the MIT software license, see the accompanying\n"
  "file COPYING or http://www.opensource.org/licenses/mit-license.php.\n";

const string desc = "The Parallel poc2 gpu ploter.";

int main(int argc, char* argv[]) {
  opt::OptionParser parser = opt::OptionParser()
    .usage(usage)
    .version(version)
    .description(desc);

  parser.add_option("--sn").action("store").type("uint64_t").set_default(0).dest("start nonce").help("default: %default");
  parser.add_option("-n", "--num") .action("store") .type("uint64_t").dest("num of nonce to produce").set_default(10000).help("default: %default");
  parser.add_option("--level").action("store").type("int").set_default(1).dest("log level").help("default: %default");

  opt::Values *popts = nullptr;
  try {
    popts = &parser.parse_args(argc, argv);
  } catch (...) {
    return -1;
  }
  opt::Values& options = *popts;
  vector<string> dirs = parser.args();

  spdlog::set_pattern("%^%v%$");
  spdlog::info(parser.get_version());
  spdlog::set_level(spdlog::level::level_enum((int)options.get("level")));
  
  if (dirs.empty()) {
    spdlog::error("not set output lot file directories will run as nonce calc test mode.\n\n");
  }
  else {
    auto&& directories = accumulate(std::next(dirs.begin()), dirs.end(), *dirs.begin(),
                              [](string &a, const string &b) -> decltype(auto) {
                                return  std::move(a) + ", " + b;
                              });
    spdlog::error("plot file directories: `{}`", directories);
  }

  std::vector<compute::platform> platforms = compute::system::platforms();
  spdlog::info("Listing platforms/devices...");
  for(auto& platform : platforms) {
    spdlog::warn("Platform `{}`\n", platform.name());
    std::vector<compute::device> devices = platform.devices();
    for(auto& device : devices) {
      std::string type;
      if(device.type() & compute::device::gpu)
          type = "GPU Device";
      else if(device.type() & compute::device::cpu)
          type = "CPU Device";
      else if(device.type() & compute::device::accelerator)
          type = "Accelerator Device";
      else
          type = "Unknown Device";

      spdlog::warn("    {}: {}\n", type, device.name());
    }
  }

  spdlog::set_pattern("[%H:%M:%S.%f][%t] %^%v%$");
  return 0;
}