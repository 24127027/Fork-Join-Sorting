namespace quick_sort_detail {
template <typename T, typename Compare>
void insertion_sort_range(std::vector<T>& arr,
                          int left,
                          int right,
                          Compare comp)
{
	for (int i = left + 1; i <= right; ++i)
	{
		T key = arr[i];
		int j = i - 1;

		while (j >= left && comp(key, arr[j]))
		{
			arr[j + 1] = arr[j];
			--j;
		}

		arr[j + 1] = key;
	}
}

template <typename T, typename Compare>
int partition_range(std::vector<T>& arr,
                    int left,
                    int right,
                    Compare comp)
{
	const T pivot = arr[right];
	int i = left;

	for (int j = left; j < right; ++j)
	{
		if (!comp(pivot, arr[j]))
		{
			std::swap(arr[i], arr[j]);
			++i;
		}
	}

	std::swap(arr[i], arr[right]);
	return i;
}

template <typename T, typename Compare>
void quick_sort_sequential_range(std::vector<T>& arr,
                                 int left,
                                 int right,
                                 int threshold,
                                 Compare comp)
{
	if (left >= right)
	{
		return;
	}

	if (right - left + 1 <= threshold)
	{
		insertion_sort_range(arr, left, right, comp);
		return;
	}

	const int pivot_index = partition_range(arr, left, right, comp);
	quick_sort_sequential_range(arr, left, pivot_index - 1, threshold, comp);
	quick_sort_sequential_range(arr, pivot_index + 1, right, threshold, comp);
}
}

template <typename T, typename Compare>
void QuickSortTask<T, Compare>::compute()
{
	if (left_ >= right_)
	{
		return;
	}

	if (right_ - left_ + 1 <= threshold_)
	{
		quick_sort_detail::insertion_sort_range(arr_, left_, right_, comp_);
		return;
	}

	const int pivot_index = quick_sort_detail::partition_range(arr_, left_, right_, comp_);

	if (depth_ < max_depth_)
	{
		QuickSortTask<T, Compare> left_task(arr_, left_, pivot_index - 1, threshold_, depth_ + 1, max_depth_, comp_);
		QuickSortTask<T, Compare> right_task(arr_, pivot_index + 1, right_, threshold_, depth_ + 1, max_depth_, comp_);

		invoke_all(left_task, right_task);
	}
	else
	{
		quick_sort_detail::quick_sort_sequential_range(arr_, left_, pivot_index - 1, threshold_, comp_);
		quick_sort_detail::quick_sort_sequential_range(arr_, pivot_index + 1, right_, threshold_, comp_);
	}
}

template <typename T, typename Compare>
void quick_sort_sequential(std::vector<T>& arr,
                           int left,
                           int right,
                           int threshold,
                           Compare comp)
{
	if (arr.empty() || left >= right)
	{
		return;
	}

	quick_sort_detail::quick_sort_sequential_range(arr, left, right, threshold, std::move(comp));
}

template <typename T, typename Compare>
void parallel_quick_sort(std::vector<T>& arr,
                         int left,
                         int right,
                         int threshold,
                         unsigned int depth,
                         unsigned int max_depth,
                         Compare comp)
{
	if (arr.empty() || left >= right)
	{
		return;
	}

	const unsigned int effective_max_depth =
		max_depth == 0 ? config::max_parallel_depth() : max_depth;

	QuickSortTask<T, Compare> root_task(
		arr, left, right, threshold, depth, effective_max_depth, std::move(comp));

	root_task.invoke();
}
