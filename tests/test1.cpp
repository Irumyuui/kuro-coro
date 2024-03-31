#include <iostream>
#include "task.hpp"

kuro::task<int> get_int(int x) {
    co_return x;
}

kuro::task<> coro_task() {
    std::cout << (co_await get_int(10)) << '\n';
    std::cout << (co_await get_int(1)) << '\n';
    std::cout << (co_await get_int(2)) << '\n';
    co_return;
}

int main() {
    auto task = coro_task();
    while (!task.get_coroutine().done()) {
        task.get_coroutine().resume();
    }
}
