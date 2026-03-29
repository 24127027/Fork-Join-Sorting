namespace merge_sort_detail {
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
void merge_ranges(std::vector<T>& arr,
                  int left,
                  int mid,
                  int right,
                  Compare comp)
{
	std::vector<T> temp(static_cast<std::size_t>(right - left + 1));
	int i = left;
	int j = mid + 1;
	int k = 0;

	while (i <= mid && j <= right)
	{
		if (comp(arr[i], arr[j]))
		{
			temp[k++] = arr[i++];
		}
		else
		{
			temp[k++] = arr[j++];
		}
	}

	while (i <= mid)
	{
		temp[k++] = arr[i++];
	}

	while (j <= right)
	{
		temp[k++] = arr[j++];
	}

	std::copy(temp.begin(), temp.end(), arr.begin() + left);
}

template <typename T, typename Compare>
void merge_sort_sequential_range(std::vector<T>& arr,
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

	const int mid = left + (right - left) / 2;
	merge_sort_sequential_range(arr, left, mid, threshold, comp);
	merge_sort_sequential_range(arr, mid + 1, right, threshold, comp);
	merge_ranges(arr, left, mid, right, comp);
}
}

template <typename T, typename Compare>
void MergeSortTask<T, Compare>::compute()
{
	if (left_ >= right_)
	{
		return;
	}

	if (right_ - left_ + 1 <= threshold_)
	{
		merge_sort_detail::insertion_sort_range(arr_, left_, right_, comp_);
		return;
	}

	const int mid = left_ + (right_ - left_) / 2;

	if (depth_ < max_depth_)
	{
		MergeSortTask<T, Compare> left_task(arr_, left_, mid, threshold_, depth_ + 1, max_depth_, comp_);
		MergeSortTask<T, Compare> right_task(arr_, mid + 1, right_, threshold_, depth_ + 1, max_depth_, comp_);

		invoke_all(left_task, right_task);
	}
	else
	{
		merge_sort_detail::merge_sort_sequential_range(arr_, left_, mid, threshold_, comp_);
		merge_sort_detail::merge_sort_sequential_range(arr_, mid + 1, right_, threshold_, comp_);
	}

	merge_sort_detail::merge_ranges(arr_, left_, mid, right_, comp_);
}

template <typename T, typename Compare>
void merge_sort_sequential(std::vector<T>& arr,
                           int left,
                           int right,
                           int threshold,
                           Compare comp)
{
	if (arr.empty() || left >= right)
	{
		return;
	}

	merge_sort_detail::merge_sort_sequential_range(arr, left, right, threshold, std::move(comp));
}

template <typename T, typename Compare>
void parallel_merge_sort(std::vector<T>& arr,
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

	MergeSortTask<T, Compare> root_task(
		arr, left, right, threshold, depth, effective_max_depth, std::move(comp));

	root_task.invoke();
}
