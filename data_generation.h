#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

std::vector<int> generate_random_data(std::size_t size,
									  int min_value,
									  int max_value,
									  std::uint32_t seed);

bool is_sorted_non_decreasing(const std::vector<int>& arr);

void print_sample(const std::vector<int>& arr, std::size_t max_items);
