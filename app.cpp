#include "app.h"

#include "benchmarking.h"
#include "config.h"
#include "data_generation.h"
#include "fork_join.h"
#include "merge_sort.h"
#include "quick_sort.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
struct CommandLineOptions
{
	std::string algorithm = "both";
	std::size_t array_size = config::kDefaultArraySize;
	int min_parallel_size = config::kDefaultSimpleSortThreshold;
	int runs = config::kDefaultBenchmarkRuns;
	std::uint32_t seed = config::kDefaultSeed;
	unsigned int workers = config::worker_count();
};

struct BenchmarkResult
{
	std::string algorithm;
	double sequential_ms = 0.0;
	double parallel_ms = 0.0;
	bool correct = true;
};

void print_usage()
{
	std::cout << "Usage: ForkJoinSorting [options]\n"
			  << "  --algo <quick|merge|both>    Algorithm to run (default: both)\n"
			  << "  --size <N>                   Number of elements (default: 100000)\n"
			  << "  --threshold <N>              Size cutoff for insertion sort and no-fork (default: 100)\n"
			  << "  --runs <N>                   Benchmark runs per algorithm (default: 5)\n"
			  << "  --workers <N>                Worker threads for fork-join pool (default: hardware concurrency)\n"
			  << "  --seed <N>                   RNG seed (default: 42)\n"
			  << "  --help                       Show this message\n";
}

bool parse_unsigned(const std::string& text, std::size_t& out_value)
{
	try
	{
		std::size_t index = 0;
		const unsigned long long value = std::stoull(text, &index);
		if (index != text.size())
		{
			return false;
		}

		out_value = static_cast<std::size_t>(value);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool parse_int(const std::string& text, int& out_value)
{
	try
	{
		std::size_t index = 0;
		const int value = std::stoi(text, &index);
		if (index != text.size())
		{
			return false;
		}

		out_value = value;
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool parse_seed(const std::string& text, std::uint32_t& out_value)
{
	std::size_t parsed = 0;
	try
	{
		const unsigned long value = std::stoul(text, &parsed);
		if (parsed != text.size())
		{
			return false;
		}

		if (value > std::numeric_limits<std::uint32_t>::max())
		{
			return false;
		}

		out_value = static_cast<std::uint32_t>(value);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool parse_args(int argc, char* argv[], CommandLineOptions& options, bool& help_requested)
{
	help_requested = false;

	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];

		if (arg == "--help")
		{
			help_requested = true;
			return false;
		}

		if (arg == "--algo")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing value for --algo\n";
				return false;
			}

			options.algorithm = argv[++i];
			if (options.algorithm != "quick" && options.algorithm != "merge" &&
				options.algorithm != "both")
			{
				std::cerr << "Invalid value for --algo\n";
				return false;
			}
			continue;
		}

		if (arg == "--size")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing value for --size\n";
				return false;
			}

			std::size_t parsed = 0;
			if (!parse_unsigned(argv[++i], parsed))
			{
				std::cerr << "Invalid value for --size\n";
				return false;
			}
			options.array_size = parsed;
			continue;
		}

		if (arg == "--threshold")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing value for --threshold\n";
				return false;
			}

			int parsed = 0;
			if (!parse_int(argv[++i], parsed) || parsed <= 0)
			{
				std::cerr << "Invalid value for --threshold\n";
				return false;
			}
			options.min_parallel_size = parsed;
			continue;
		}

		if (arg == "--runs")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing value for --runs\n";
				return false;
			}

			int parsed = 0;
			if (!parse_int(argv[++i], parsed) || parsed <= 0)
			{
				std::cerr << "Invalid value for --runs\n";
				return false;
			}
			options.runs = parsed;
			continue;
		}

		if (arg == "--seed")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing value for --seed\n";
				return false;
			}

			std::uint32_t parsed = 0;
			if (!parse_seed(argv[++i], parsed))
			{
				std::cerr << "Invalid value for --seed\n";
				return false;
			}
			options.seed = parsed;
			continue;
		}

		if (arg == "--workers")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing value for --workers\n";
				return false;
			}

			std::size_t parsed = 0;
			if (!parse_unsigned(argv[++i], parsed) || parsed == 0 ||
				parsed > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max()))
			{
				std::cerr << "Invalid value for --workers\n";
				return false;
			}

			options.workers = static_cast<unsigned int>(parsed);
			continue;
		}

		std::cerr << "Unknown argument: " << arg << "\n";
		return false;
	}

	return true;
}

BenchmarkResult run_quicksort(const std::vector<int>& input,
							  int runs,
							  int min_parallel_size,
							  unsigned int max_depth,
							  const std::vector<int>& expected)
{
	BenchmarkResult result;
	result.algorithm = "QuickSort";

	for (int i = 0; i < runs; ++i)
	{
		std::vector<int> arr_seq = input;
		result.sequential_ms += benchmark_ms(
			[&arr_seq]() {
				if (!arr_seq.empty())
				{
					quick_sort_sequential(arr_seq, 0, static_cast<int>(arr_seq.size()) - 1);
				}
			},
			1);

		if (arr_seq != expected)
		{
			result.correct = false;
		}

		std::vector<int> arr_par = input;
		result.parallel_ms += benchmark_ms(
			[&arr_par, min_parallel_size, max_depth]() {
				if (!arr_par.empty())
				{
					parallel_quick_sort(arr_par,
										0,
										static_cast<int>(arr_par.size()) - 1,
										min_parallel_size,
										0,
										max_depth);
				}
			},
			1);

		if (arr_par != expected)
		{
			result.correct = false;
		}
	}

	result.sequential_ms /= static_cast<double>(runs);
	result.parallel_ms /= static_cast<double>(runs);
	return result;
}

BenchmarkResult run_mergesort(const std::vector<int>& input,
							  int runs,
							  int min_parallel_size,
							  unsigned int max_depth,
							  const std::vector<int>& expected)
{
	BenchmarkResult result;
	result.algorithm = "MergeSort";

	for (int i = 0; i < runs; ++i)
	{
		std::vector<int> arr_seq = input;
		result.sequential_ms += benchmark_ms(
			[&arr_seq]() {
				if (!arr_seq.empty())
				{
					merge_sort_sequential(arr_seq, 0, static_cast<int>(arr_seq.size()) - 1);
				}
			},
			1);

		if (arr_seq != expected)
		{
			result.correct = false;
		}

		std::vector<int> arr_par = input;
		result.parallel_ms += benchmark_ms(
			[&arr_par, min_parallel_size, max_depth]() {
				if (!arr_par.empty())
				{
					parallel_merge_sort(arr_par,
										0,
										static_cast<int>(arr_par.size()) - 1,
										min_parallel_size,
										0,
										max_depth);
				}
			},
			1);

		if (arr_par != expected)
		{
			result.correct = false;
		}
	}

	result.sequential_ms /= static_cast<double>(runs);
	result.parallel_ms /= static_cast<double>(runs);
	return result;
}

void print_results(const std::vector<BenchmarkResult>& results)
{
	std::cout << "\nResults\n";
	std::cout << std::left << std::setw(12) << "Algorithm" << std::right << std::setw(18)
			  << "Sequential(ms)" << std::setw(16) << "Parallel(ms)" << std::setw(12)
			  << "Speedup" << std::setw(12) << "Correct" << "\n";

	for (const BenchmarkResult& result : results)
	{
		const double speedup = result.parallel_ms > 0.0 ? (result.sequential_ms / result.parallel_ms) : 0.0;

		std::cout << std::left << std::setw(12) << result.algorithm << std::right << std::fixed
				  << std::setprecision(3) << std::setw(18) << result.sequential_ms << std::setw(16)
				  << result.parallel_ms << std::setw(12) << std::setprecision(2) << speedup
				  << std::setw(12) << (result.correct ? "PASS" : "FAIL") << "\n";
	}
}
}

int run_fork_join_app(int argc, char* argv[])
{
	CommandLineOptions options;
	bool help_requested = false;
	if (!parse_args(argc, argv, options, help_requested))
	{
		if (help_requested)
		{
			print_usage();
			return 0;
		}
		print_usage();
		return 1;
	}

	if (options.array_size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
	{
		std::cerr << "--size is too large for this implementation (must fit in int index range)\n";
		return 1;
	}

	try
	{
		unsigned int workers = options.workers;
		if (!configure_fork_join_worker_count(workers))
		{
			std::cerr << "Continuing with existing fork-join pool configuration.\n";
		}

		workers = fork_join_worker_count();
		const unsigned int max_depth = config::max_parallel_depth_for_workers(workers);

		std::vector<int> input =
			generate_random_data(options.array_size,
								 config::kMinRandomValue,
								 config::kMaxRandomValue,
								 options.seed);

		std::vector<int> expected = input;
		std::sort(expected.begin(), expected.end());

		std::cout << "Fork-Join Sorting Benchmark\n";
		std::cout << "Data size      : " << options.array_size << "\n";
		std::cout << "Threshold      : " << options.min_parallel_size << "\n";
		std::cout << "Runs           : " << options.runs << "\n";
		std::cout << "Worker threads : " << workers << "\n";
		std::cout << "Max depth      : " << max_depth << "\n";
		std::cout << "Input sample   : ";
		print_sample(input, 12);
		std::cout << "\n";

		std::vector<BenchmarkResult> results;
		if (options.algorithm == "quick" || options.algorithm == "both")
		{
			results.push_back(
				run_quicksort(input, options.runs, options.min_parallel_size, max_depth, expected));
		}

		if (options.algorithm == "merge" || options.algorithm == "both")
		{
			results.push_back(
				run_mergesort(input, options.runs, options.min_parallel_size, max_depth, expected));
		}

		print_results(results);

		for (const BenchmarkResult& result : results)
		{
			if (!result.correct)
			{
				return 2;
			}
		}

		std::cout << "\nPress Enter to exit...";
		std::cin.get();

		return 0;
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Error: " << ex.what() << "\n";
		return 1;
	}
}
