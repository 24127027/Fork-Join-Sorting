#include "fork_join.h"

#include "fork_join_runtime.h"

bool configure_fork_join_worker_count(unsigned int worker_count)
{
	return fork_join_runtime::configure_worker_count(worker_count);
}

unsigned int fork_join_worker_count()
{
	return fork_join_runtime::worker_count();
}

void RecursiveAction::invoke() 
{
	compute();
}

void RecursiveAction::invoke_all(RecursiveAction& first, RecursiveAction& second) 
{
	fork_join_runtime::invoke_all(first, second);
}