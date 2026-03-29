#pragma once
#include "config.h"
#include "fork_join.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace quick_sort_detail {
template <typename T, typename Compare>
void insertion_sort_range(std::vector<T>& arr, 
							int left, 
							int right, 
							Compare comp);
}

template <typename T, typename Compare>
int partition_range(std::vector<T>& arr,
					int left, 
					int right, 
					Compare comp);

template <typename T, typename Compare>
void quick_sort_sequential_range(std::vector<T>& arr,
									int left,
									int right,
									int threshold,
									Compare comp);

template <typename T, typename Compare = std::less<T>>
class QuickSortTask final : public RecursiveAction {
public:
	QuickSortTask(std::vector<T>& arr,
					int left,
					int right,
					int threshold,
					unsigned int depth,
					unsigned int max_depth,
					Compare comp = Compare{})
					: arr_(arr),
					left_(left),
					right_(right),
					threshold_(threshold),
					depth_(depth),
					max_depth_(max_depth),
					comp_(std::move(comp)) {}

	void compute() override;
private:
	std::vector<T>& arr_;
	int left_;
	int right_;
	int threshold_;
	unsigned int depth_;
	unsigned int max_depth_;
	Compare comp_;
};

template <typename T, typename Compare = std::less<T>>
void quick_sort_sequential(std::vector<T>& arr,
							int left,
							int right,
							int threshold = config::kDefaultSimpleSortThreshold,
							Compare comp = Compare{});

template <typename T, typename Compare = std::less<T>>
void parallel_quick_sort(std::vector<T>& arr,
							int left,
							int right,
							int threshold = config::kDefaultSimpleSortThreshold,
							unsigned int depth = 0,
							unsigned int max_depth = 0,
							Compare comp = Compare{});