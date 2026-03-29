#pragma once

class RecursiveAction 
{
public:
	virtual ~RecursiveAction() = default;
	virtual void compute() = 0;

	void invoke();

protected:
	static void invoke_all(RecursiveAction& first, RecursiveAction& second);
};

bool configure_fork_join_worker_count(unsigned int worker_count);

unsigned int fork_join_worker_count();
