#include "scheduler.hpp"
#include "task.hpp"
#include "when_all.hpp"
#include <iostream>

#include <chrono>

using namespace kuro;

using namespace std::chrono_literals;

task<int> task1() {
    std::cout << "Sleep 1" << '\n';
    co_await sleep_for(10s);
    std::cout << "Sleep 1 wake up" << '\n';
    co_return 1;
}

task<int> task2() {
    std::cout << "Sleep 2" << '\n';
    co_await sleep_for(10s);
    std::cout << "Sleep 2 wake up" << '\n';
    co_return 2;
}

task<> async_main() {
    auto [a, b] = co_await when_all(task1(), task2());

    std::cout << a << ' ' << b << '\n';

    auto [c, d] = co_await when_all(task1(), task2());

    std::cout << c << ' ' << d << '\n';

    co_return;
}

int main() {
    task_run(get_scheduler(), async_main());
}
