#pragma once

#include "task.hpp"
#include <chrono>
#include <coroutine>
#include <deque>
#include <queue>
#include <thread>

namespace kuro {
    
struct scheduler {
    struct wait_task_entry {
        std::chrono::system_clock::time_point expire_time_;
        std::coroutine_handle<> coroutine_;

        constexpr auto operator <=> (this const wait_task_entry &self, const wait_task_entry &other) noexcept {
            return self.expire_time_ <=> other.expire_time_;
        }
    };

    std::deque<std::coroutine_handle<>> ready_queue_;
    std::priority_queue<wait_task_entry, std::vector<wait_task_entry>, std::greater<>> wait_queue_;

    scheduler& operator=(const scheduler&) = delete;
    
    void add_task(this scheduler &self, std::coroutine_handle<> coroutine) {
        self.ready_queue_.emplace_front(coroutine);
    }

    void add_wait_task(this scheduler &self, std::chrono::system_clock::time_point expire_time, std::coroutine_handle<> coroutine) {
        self.wait_queue_.emplace(expire_time, coroutine);
    }

    constexpr bool has_task(this const scheduler &self) noexcept {
        return !self.ready_queue_.empty() || !self.wait_queue_.empty();
    }

    void run(this scheduler &self) {
        while (self.has_task()) {
            while (!self.ready_queue_.empty()) {
                auto coroutine = self.ready_queue_.front();
                self.ready_queue_.pop_front();
                coroutine.resume();
            }
            if (!self.wait_queue_.empty()) {
                auto now_time_point = std::chrono::system_clock::now();
                auto entry = self.wait_queue_.top();
                if (entry.expire_time_ < now_time_point) {
                    self.wait_queue_.pop();
                    entry.coroutine_.resume();
                } else {
                    std::this_thread::sleep_until(entry.expire_time_);
                }
            }
        }
    }        
};

inline scheduler& get_scheduler() {
    static scheduler scheduler;
    return scheduler;
} 

struct sleep_awaiter {
    std::chrono::system_clock::time_point expire_time_;

    constexpr bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> coroutine) const {
        get_scheduler().add_wait_task(expire_time_, coroutine);
    }

    constexpr void await_resume() const noexcept {}
};

inline kuro::task<> sleep_until(std::chrono::system_clock::time_point expire_time) {
    co_await sleep_awaiter {expire_time};
    co_return;
}

inline kuro::task<> sleep_for(std::chrono::system_clock::duration duration) {
    co_await sleep_awaiter { std::chrono::system_clock::now() + duration };
    co_return;
}

} // namespace kuro
