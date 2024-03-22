/* Copyright 2021 Aristocratos (jakob@qvantnet.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

indent = tab
tab-size = 4
*/
#include <arpa/inet.h> // for inet_ntop()
#include <cmath>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <numeric>
#include <ranges>
#include <sys/statvfs.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#if !(defined(STATIC_BUILD) && defined(__GLIBC__))
#include <pwd.h>
#endif

#include "../btop_config.hpp"
#include "../btop_shared.hpp"
#include "../btop_tools.hpp"

using std::async;
using std::clamp;
using std::cmp_greater;
using std::cmp_less;
using std::future;
using std::ifstream;
using std::max;
using std::min;
using std::numeric_limits;
using std::pair;
using std::round;
using std::streamsize;
using std::vector;

namespace fs = std::filesystem;
namespace rng = std::ranges;
namespace rv = std::ranges::views;

#define DPU_SUPPORT

namespace Dpu {
vector<dpu_info> dpus;
#ifdef DPU_SUPPORT
//? UPMEM data collection
#define UPMEM_SUCCESS 0

bool initialized = false;
bool init();
bool shutdown();
template <bool is_init>
bool collect(dpu_info* dpus_slice);
uint32_t device_count = 0;

enum hw_type_t {
	DIMM,
	FPGA,
	SIMU,
	UNKNOWN,
};

hw_type_t hw_type { UNKNOWN };
#endif
}

#ifdef DPU_SUPPORT
namespace Dpu {
//? UPMEM
bool init()
{
	if (initialized)
		return false;

	const fs::path ranks_path { "/sys/class/dpu_rank" };
	if (!fs::is_directory(ranks_path)) {
		Logger::info("DPU module is not loaded");
		return false;
	}

	if (hw_type == SIMU) {
		Logger::info("DPU module is in simulation mode");
		return false;
	}

	auto valid_driver = fs::directory_iterator { ranks_path }
		| rv::transform([](const auto& rank) { return rank.path(); })
		| rv::filter([](const auto& rank_path) {
			  return rank_path.filename().string().find("dpu_rank") == string::npos
				  || (Logger::warning("Invalid device: " + rank_path.string()), false);
		  })
		| rv::transform([](const auto& rank_path) { return rank_path / "device" / "driver"; })
		| rv::transform([](const auto& driver_path) {
			  std::error_code ec;
			  auto actual_path { fs::canonical(driver_path, ec) };
			  return pair(actual_path, ec);
		  })
		| rv::filter([](const auto& pair) {
			  const auto& [actual_path, ec] = pair;
			  return !ec
				  || (Logger::warning("Failed to get" + actual_path.string() + "canonical path: " + ec.message()), false);
		  })
		| rv::elements<0>
		| rv::transform([](const auto& actual_path) { return actual_path.filename(); })
		| rv::take(1);

	if (valid_driver.begin() == valid_driver.end()) {
		Logger::info("No actual hardware present");
		initialized = true;
		return false;
	}

	hw_type = *valid_driver.begin() == "dpu_region_mem" ? DIMM : FPGA;

	dpus.resize(device_count);
	initialized = true;
	Dpu::collect<1>(dpus.data());

	return true;
}

bool shutdown()
{
	return initialized ? (initialized = false, true) : false;
}

template <bool is_init> // collect<1> is called in Dpu::init(), and populates dpus.supported_functions
bool collect(dpu_info* dpus_slice)
{ // raw pointer to vector data, size == device_count
	if (!initialized)
		return false;
}
}
#endif
