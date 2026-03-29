#include "data_generation.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

std::vector<int> generate_random_data(std::size_t size,
									  int min_value,
									  int max_value,
									  std::uint32_t seed)
{
	if (min_value > max_value)
	{
		throw std::invalid_argument("min_value cannot be greater than max_value");
	}

	std::vector<int> arr(size);
	std::mt19937 rng(seed);
	std::uniform_int_distribution<int> dist(min_value, max_value);

	for (std::size_t i = 0; i < size; ++i)
	{
		arr[i] = dist(rng);
	}

	return arr;
}

bool is_sorted_non_decreasing(const std::vector<int>& arr)
{
	return std::is_sorted(arr.begin(), arr.end());
}

void print_sample(const std::vector<int>& arr, std::size_t max_items)
{
	std::cout << "[";

	const std::size_t display_count = std::min(arr.size(), max_items);
	for (std::size_t i = 0; i < display_count; ++i)
	{
		std::cout << arr[i];
		if (i + 1 < display_count)
		{
			std::cout << ", ";
		}
	}

	if (arr.size() > max_items)
	{
		if (display_count > 0)
		{
			std::cout << ", ";
		}
		std::cout << "...";
	}

	std::cout << "]";
}
