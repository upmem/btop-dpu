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
#include <iterator>
#include <net/if.h>
#include <netdb.h>
#include <numeric>
#include <ranges>
#include <span>
#include <sys/statvfs.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#if !(defined(STATIC_BUILD) && defined(__GLIBC__))
#include <pwd.h>
#endif

#include <flux.hpp>

#include "../btop_config.hpp"
#include "../btop_shared.hpp"
#include "../btop_tools.hpp"

using std::async;
using std::clamp;
using std::cmp_greater;
using std::cmp_less;
using std::error_code;
using std::future;
using std::ifstream;
using std::max;
using std::min;
using std::numeric_limits;
using std::optional;
using std::pair;
using std::round;
using std::span;
using std::streamsize;
using std::vector;

namespace fs = std::filesystem;
namespace rng = std::ranges;
namespace rv = std::ranges::views;

namespace Dpu {
vector<rank_info> dpus;
#ifdef DPU_SUPPORT
enum hw_type_t {
	DIMM,
	FPGA,
	SIMU,
	UNKNOWN,
};

//? UPMEM data collection
// NOLINTBEGIN(*-non-const-global-variables)
constexpr int UPMEM_SUCCESS { 0 };

bool initialized { false };
auto init() -> bool;
auto shutdown() -> bool;
template <bool is_init>
auto collect(span<rank_info> dpus_slice, vector<fs::path> ranks) -> bool;
uint32_t device_count { 0 };

hw_type_t hw_type { UNKNOWN };
// NOLINTEND(*-non-const-global-variables)
#endif
}

#ifdef DPU_SUPPORT
namespace Dpu {
//? UPMEM
auto init() -> bool
{
	if (initialized) {
		return false;
	}

	const fs::path ranks_path { "/sys/class/dpu_rank" };
	if (!fs::is_directory(ranks_path)) {
		Logger::info("DPU module is not loaded");
		return false;
	}

	if (hw_type == SIMU) {
		Logger::info("DPU module is in simulation mode");
		return false;
	}

	auto ranks_view = fs::directory_iterator { ranks_path }
		| rv::transform([](const auto& rank) { return rank.path(); })
		| rv::filter([](const auto& rank_path) {
			  return rank_path.filename().string().find("dpu_rank") == string::npos
				  || (Logger::warning("Invalid device: " + rank_path.string()), false);
		  });

	vector<fs::path> ranks;
	rng::copy(ranks_view, std::back_inserter(ranks));

	device_count = ranks.size();

	if (device_count == 0) {
		Logger::info("No actual hardware present");
		initialized = true;
		return false;
	}

	auto driver = ranks
		| rv::transform([](const auto& rank_path) { return rank_path / "device" / "driver"; })
		| rv::transform([](const auto& driver_path) -> optional<fs::path> {
			  error_code ec;
			  auto actual_path { fs::canonical(driver_path, ec) };
			  if (ec) {
				  Logger::warning("Failed to get" + actual_path.string() + "canonical path: " + ec.message());
				  return {};
			  }
			  return actual_path;
		  })
		| rv::filter([](const auto& driver_path) { return driver_path.has_value(); })
		| rv::transform([](const auto& driver_path) { return driver_path->filename(); })
		| rv::take(1);

	if (driver.empty()) {
		Logger::warning("Failed to get driver path");
		return false;
	}

	hw_type = driver.front() == "dpu_region_mem" ? DIMM : FPGA;

	dpus.resize(device_count);
	initialized = true;
	Dpu::collect<1>(dpus, ranks);

	return true;
}

auto shutdown() -> bool
{
	return initialized ? (initialized = false, true) : false;
}

template <bool is_init> // collect<1> is called in Dpu::init(), and populates dpus.supported_functions
auto collect(std::span<rank_info> ranks_slice, vector<fs::path> ranks_path) -> bool
{ // raw pointer to vector data, size == device_count
	if (!initialized) {
		return false;
	}

	if constexpr (is_init) {
		auto part_numbers = ranks_path
			| rv::transform([](const auto& rank_path) { return rank_path / "part_number"; })
			| rv::transform([](const auto& part_number_path) {
				  ifstream part_number_file { part_number_path };
				  if (!part_number_file.is_open()) {
					  Logger::warning("Failed to open " + part_number_path.string());
					  return "Unkown"s;
				  }
				  string part_number;
				  part_number_file >> part_number;
				  return part_number;
			  });
		for (auto [rank, part_number] : rv::zip(ranks_slice, part_numbers)) {
			rank.part_number = part_number;
		}

		auto mram_sizes = ranks_path
			| rv::transform([](const auto& rank_path) { return rank_path / "mram_size"; })
			| rv::transform([](const auto& mram_size_path) {
				  ifstream mram_size_file { mram_size_path };
				  if (!mram_size_file.is_open()) {
					  Logger::warning("Failed to open " + mram_size_path.string());
					  return "Unkown"s;
				  }
				  long long mram_size {};
				  mram_size_file >> mram_size;
				  return mram_size;
			  });
		auto mem_refs = ranks_slice
			| rv::transform([](auto& rank) -> string& { return rank.mem_total; });
		rng::copy(mram_sizes, mem_refs.begin());
	}
}
}
#endif
