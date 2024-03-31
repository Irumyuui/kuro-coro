#include <chrono>
#include <iostream>

#include "task.hpp"
#include "scheduler.hpp"

using namespace kuro;
using namespace std::chrono_literals;

task<int> task1() {
    std::cout << "Sleep 1" << '\n';
    co_await sleep_for(1s);
    std::cout << "Sleep 1 wake up" << '\n';
    co_return 1;
}

task<int> task2() {
    std::cout << "Sleep 2" << '\n';
    co_await sleep_for(2s);
    std::cout << "Sleep 2 wake up" << '\n';
    co_return 2;
}

int main() {
    auto t1 = task1();
    auto t2 = task2();

    get_scheduler().add_task(t1.get_coroutine());
    get_scheduler().add_task(t2.get_coroutine());

    get_scheduler().run();

    std::cout << "Get result from t1: " << t1.get_coroutine().promise().result() << '\n';
    std::cout << "Get result from t2: " << t2.get_coroutine().promise().result() << '\n';
}
