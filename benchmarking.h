#pragma once

#include <functional>

double benchmark_ms(const std::function<void()>& task, int runs);
