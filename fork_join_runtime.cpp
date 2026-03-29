#include "fork_join_runtime.h"

#include "config.h"
#include "fork_join.h"

#include <atomic>
#include <cstddef>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace
{
thread_local int tls_worker_index = -1;
thread_local const void* tls_scheduler_owner = nullptr;
std::atomic<unsigned int> configured_worker_count{0};
std::atomic<unsigned int> active_pool_worker_count{0};
std::atomic<bool> pool_initialized{false};
using Job = std::function<void()>;

struct WorkerState
{
	std::deque<Job> queue;
	std::mutex mutex;
	std::thread thread;
};

class TaskScheduler final
{
public:
	TaskScheduler(std::vector<WorkerState>& workers,
				  std::deque<Job>& global_queue,
				  std::mutex& global_mutex,
				  std::condition_variable& cv,
				  std::atomic<bool>& stop)
		: workers_(workers),
		  global_queue_(global_queue),
		  global_mutex_(global_mutex),
		  cv_(cv),
		  stop_(stop)
	{
	}

	void set_worker_context(int worker_index)
	{
		tls_worker_index = worker_index;
		tls_scheduler_owner = this;
	}

	void clear_worker_context()
	{
		tls_worker_index = -1;
		tls_scheduler_owner = nullptr;
	}

	void submit(Job job)
	{
		if (is_current_worker())
		{
			WorkerState& worker = workers_[static_cast<std::size_t>(tls_worker_index)];
			{
				std::lock_guard<std::mutex> lock(worker.mutex);
				worker.queue.push_front(std::move(job));
			}
		}
		else
		{
			{
				std::lock_guard<std::mutex> lock(global_mutex_);
				global_queue_.push_back(std::move(job));
			}
		}

		cv_.notify_one();
	}

	bool try_take_for_worker(int worker_index, Job& out)
	{
		return pop_local(worker_index, out) || pop_global(out) || steal_from_other(worker_index, out);
	}

	bool run_one_pending_on_current_thread()
	{
		Job job;
		if (is_current_worker())
		{
			const int worker_index = tls_worker_index;
			if (pop_local(worker_index, job) || pop_global(job) || steal_from_other(worker_index, job))
			{
				job();
				return true;
			}
			return false;
		}

		if (pop_global(job) || steal_from_any(job))
		{
			job();
			return true;
		}

		return false;
	}

	void wait_for_work_signal()
	{
		std::unique_lock<std::mutex> lock(global_mutex_);
		cv_.wait_for(lock,
					 std::chrono::milliseconds(1),
					 [this]() {
						 return stop_.load(std::memory_order_acquire) || !global_queue_.empty();
					 });
	}

	bool should_stop_worker()
	{
		return stop_.load(std::memory_order_acquire) && !has_pending_work();
	}

	void notify_shutdown()
	{
		cv_.notify_all();
	}

private:
	bool is_current_worker() const
	{
		return tls_scheduler_owner == this && tls_worker_index >= 0 &&
			tls_worker_index < static_cast<int>(workers_.size());
	}

	bool pop_local(int worker_index, Job& out)
	{
		if (worker_index < 0 || worker_index >= static_cast<int>(workers_.size()))
		{
			return false;
		}

		WorkerState& worker = workers_[static_cast<std::size_t>(worker_index)];
		std::lock_guard<std::mutex> lock(worker.mutex);
		if (worker.queue.empty())
		{
			return false;
		}

		out = std::move(worker.queue.front());
		worker.queue.pop_front();
		return true;
	}

	bool pop_global(Job& out)
	{
		std::lock_guard<std::mutex> lock(global_mutex_);
		if (global_queue_.empty())
		{
			return false;
		}

		out = std::move(global_queue_.front());
		global_queue_.pop_front();
		return true;
	}

	bool steal_from_other(int thief_index, Job& out)
	{
		const int worker_count = static_cast<int>(workers_.size());
		for (int offset = 1; offset < worker_count; ++offset)
		{
			const int victim_index = (thief_index + offset) % worker_count;
			if (try_steal(victim_index, out))
			{
				return true;
			}
		}
		return false;
	}

	bool steal_from_any(Job& out)
	{
		for (int victim_index = 0; victim_index < static_cast<int>(workers_.size()); ++victim_index)
		{
			if (try_steal(victim_index, out))
			{
				return true;
			}
		}
		return false;
	}

	bool try_steal(int victim_index, Job& out)
	{
		WorkerState& victim = workers_[static_cast<std::size_t>(victim_index)];
		std::lock_guard<std::mutex> lock(victim.mutex);
		if (victim.queue.empty())
		{
			return false;
		}

		out = std::move(victim.queue.back());
		victim.queue.pop_back();
		return true;
	}

	bool has_pending_work()
	{
		{
			std::lock_guard<std::mutex> lock(global_mutex_);
			if (!global_queue_.empty())
			{
				return true;
			}
		}

		for (WorkerState& worker : workers_)
		{
			std::lock_guard<std::mutex> lock(worker.mutex);
			if (!worker.queue.empty())
			{
				return true;
			}
		}

		return false;
	}

	std::vector<WorkerState>& workers_;
	std::deque<Job>& global_queue_;
	std::mutex& global_mutex_;
	std::condition_variable& cv_;
	std::atomic<bool>& stop_;
};

class WorkerPool final
{
public:
	explicit WorkerPool(unsigned int worker_count)
		: stop_(false), workers_(worker_count == 0 ? 1U : worker_count),
		  scheduler_(workers_, global_queue_, global_mutex_, cv_, stop_)
	{
		for (unsigned int i = 0; i < static_cast<unsigned int>(workers_.size()); ++i)
		{
			workers_[i].thread = std::thread([this, i]() { worker_loop(static_cast<int>(i)); });
		}
	}

	~WorkerPool()
	{
		stop_.store(true, std::memory_order_release);
		scheduler_.notify_shutdown();

		for (WorkerState& worker : workers_)
		{
			if (worker.thread.joinable())
			{
				worker.thread.join();
			}
		}
	}

	void submit(Job job)
	{
		scheduler_.submit(std::move(job));
	}

	bool run_one_pending_on_current_thread()
	{
		return scheduler_.run_one_pending_on_current_thread();
	}

private:
	void worker_loop(int worker_index)
	{
		scheduler_.set_worker_context(worker_index);

		while (true)
		{
			Job job;
			if (scheduler_.try_take_for_worker(worker_index, job))
			{
				job();
				continue;
			}

			scheduler_.wait_for_work_signal();

			if (scheduler_.should_stop_worker())
			{
				break;
			}
		}

		scheduler_.clear_worker_context();
	}

	std::atomic<bool> stop_;
	std::vector<WorkerState> workers_;
	std::mutex global_mutex_;
	std::condition_variable cv_;
	std::deque<Job> global_queue_;
	TaskScheduler scheduler_;
};

unsigned int resolved_worker_count()
{
	const unsigned int configured = configured_worker_count.load(std::memory_order_acquire);
	return configured == 0 ? config::worker_count() : configured;
}

WorkerPool& pool_instance()
{
	const unsigned int workers = resolved_worker_count();
	static WorkerPool pool(workers);
	static const bool initialized = [workers]() {
		active_pool_worker_count.store(workers, std::memory_order_release);
		pool_initialized.store(true, std::memory_order_release);
		return true;
	}();
	(void)initialized;
	return pool;
}
}

namespace fork_join_runtime
{
bool configure_worker_count(unsigned int worker_count)
{
	const unsigned int normalized = worker_count == 0 ? 1U : worker_count;

	if (pool_initialized.load(std::memory_order_acquire))
	{
		const unsigned int active = active_pool_worker_count.load(std::memory_order_acquire);
		if (normalized != active)
		{
			std::cerr << "Warning: configure_fork_join_worker_count(" << normalized
					  << ") ignored because fork-join pool is already initialized with "
					  << active
					  << " workers. Configure worker count before starting parallel tasks.\n";
			return false;
		}

		return true;
	}

	configured_worker_count.store(normalized, std::memory_order_release);
	return true;
}

unsigned int worker_count()
{
	if (pool_initialized.load(std::memory_order_acquire))
	{
		return active_pool_worker_count.load(std::memory_order_acquire);
	}

	return resolved_worker_count();
}

void invoke_all(RecursiveAction& first, RecursiveAction& second)
{
	WorkerPool& pool = pool_instance();
	std::exception_ptr first_error;
	std::exception_ptr second_error;
	std::atomic<bool> second_done{false};

	pool.submit([&second, &second_error, &second_done]() {
		try
		{
			second.invoke();
		}
		catch (...)
		{
			second_error = std::current_exception();
		}

		second_done.store(true, std::memory_order_release);
	});

	try
	{
		first.invoke();
	}
	catch (...)
	{
		first_error = std::current_exception();
	}

	while (!second_done.load(std::memory_order_acquire))
	{
		if (!pool.run_one_pending_on_current_thread())
		{
			std::this_thread::yield();
		}
	}

	if (first_error)
	{
		std::rethrow_exception(first_error);
	}

	if (second_error)
	{
		std::rethrow_exception(second_error);
	}
}
}
