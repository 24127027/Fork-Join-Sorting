#include "benchmarking.h"

#include <chrono>
#include <stdexcept>

double benchmark_ms(const std::function<void()>& task, int runs)
{
	if (runs <= 0)
	{
		throw std::invalid_argument("runs must be positive");
	}

	double total_ms = 0.0;

	for (int i = 0; i < runs; ++i)
	{
		const auto start = std::chrono::high_resolution_clock::now();
		task();
		const auto end = std::chrono::high_resolution_clock::now();
		const std::chrono::duration<double, std::milli> elapsed = end - start;
		total_ms += elapsed.count();
	}

	return total_ms / static_cast<double>(runs);
}
