#pragma once

class RecursiveAction;

namespace fork_join_runtime
{
bool configure_worker_count(unsigned int worker_count);

unsigned int worker_count();

void invoke_all(RecursiveAction& first, RecursiveAction& second);
}
