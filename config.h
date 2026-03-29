#pragma once

#include <cstddef>
#include <cstdint>
#include <thread>

namespace config {
inline constexpr std::size_t kDefaultArraySize = 100000;
inline constexpr int kDefaultSimpleSortThreshold = 100;
inline constexpr int kDefaultBenchmarkRuns = 5;
inline constexpr int kMinRandomValue = 0;
inline constexpr int kMaxRandomValue = 1000000;
inline constexpr std::uint32_t kDefaultSeed = 42;

inline unsigned int worker_count() 
{
	const unsigned int workers = std::thread::hardware_concurrency();
	return workers == 0 ? 4U : workers;
}

inline unsigned int max_parallel_depth() 
{
	const unsigned int workers = worker_count();
	unsigned int depth = 0;

	while (depth < 31U && (1U << depth) < workers) 
	{
		++depth;
	}

	return depth + 1U;
}
}
