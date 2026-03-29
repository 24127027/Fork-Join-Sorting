#pragma once

#include <future>

class RecursiveAction {
public:
	virtual ~RecursiveAction() = default;
	virtual void compute() = 0;

	void invoke();

protected:
	static void invoke_all(RecursiveAction& first, RecursiveAction& second);
};
