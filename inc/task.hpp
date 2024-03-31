#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

namespace kuro {

struct prev_coroutine_awaiter {
    std::coroutine_handle<> prev_coroutine_ {};
    
    constexpr bool await_ready() const noexcept {
        return false;
    }

    constexpr std::coroutine_handle<>
    await_suspend([[maybe_unused]] std::coroutine_handle<> coroutine) const noexcept {
        // return prev_coroutine_;
        if (prev_coroutine_) {
            return prev_coroutine_;
        } else {
            return std::noop_coroutine();
        }
    }

    constexpr void await_resume() const noexcept {}
};

template <typename T, typename TPromise>
struct task_awaiter {
    std::coroutine_handle<TPromise> coroutine_ {};
    
    constexpr bool await_ready() const noexcept {
        return false;
    }

    constexpr std::coroutine_handle<TPromise>
    await_suspend(this const task_awaiter &self, std::coroutine_handle<> coroutine) noexcept {
        auto &promise = self.coroutine_.promise();
        promise.prev_coroutine_ = coroutine;
        return self.coroutine_;
    }

    constexpr T await_resume(this const task_awaiter &self) {
        if constexpr (std::is_void_v<T>) {
            self.coroutine_.promise().result();
        } else {
            return self.coroutine_.promise().result();
        }
    }
};

template <typename T>
class task_promise {
public:
    task_promise& operator=(const task_promise&) = delete;

    constexpr auto initial_suspend([[maybe_unused]] this const task_promise &self) noexcept {
        return std::suspend_always{};
    }

    constexpr auto final_suspend(this const task_promise &self) noexcept {
        return prev_coroutine_awaiter{ self.prev_coroutine_ };
    }

    constexpr void unhandled_exception(this task_promise &self) noexcept {
        self.except_ = std::current_exception();
    }

    constexpr void return_value(this task_promise &self, T &&result) noexcept {
        self.result_.emplace(std::move(result));
    }

    constexpr void return_value(this task_promise &self, const T &result) noexcept {
        self.result_ = result;
    }

    T result(this task_promise &self) {
        if (self.except_) [[unlikely]] {
            std::rethrow_exception(self.except_);
        }
        assert(self.result_.has_value());
        T result = std::move(*self.result_);
        self.result_.reset();
        return result;
    }

    constexpr auto get_return_object(this task_promise &self) {
        return std::coroutine_handle<task_promise>::from_promise(self);
    }
    
private:
    template <typename, typename>
    friend struct task_awaiter;

    std::coroutine_handle<> prev_coroutine_{};
    std::exception_ptr except_{};
    std::optional<T> result_;
};

template <>
class task_promise<void> {
public:
    constexpr auto initial_suspend([[maybe_unused]] this const task_promise &self) noexcept {
        return std::suspend_always{};
    }

    constexpr auto final_suspend(this const task_promise &self) noexcept {
        return prev_coroutine_awaiter { self.prev_coroutine_ };
    }

    constexpr void unhandled_exception(this task_promise &self) noexcept {
        self.except_ = std::current_exception();
    } 

    void result(this const task_promise &self) {
        if (self.except_) [[unlikely]] {
            std::rethrow_exception(self.except_);
        }
    }

    constexpr void return_void() const noexcept {}

    constexpr auto get_return_object(this task_promise &self) {
        return std::coroutine_handle<task_promise>::from_promise(self);
    }

private:
    template <typename, typename>
    friend struct task_awaiter;

    std::coroutine_handle<> prev_coroutine_{};
    std::exception_ptr except_{};
};

template <typename T = void>
class [[nodiscard]] task {
public:
    using promise_type = task_promise<T>;
    using value_type = T;

public:
    constexpr task(std::coroutine_handle<promise_type> coroutine = nullptr) noexcept : coroutine_(coroutine) {}

    constexpr task(task &&other) noexcept : coroutine_(std::exchange(other.coroutine_, nullptr)) {}

    // constexpr task& operator=(this task &self, task other) noexcept {
    //     std::swap(self.coroutine_, other.coroutine_);
    // }

    constexpr task& operator=(this task &self, task &&other) noexcept {
        std::swap(self.coroutine_, other.coroutine_);
    }

    task(const task&) = delete;
    task& operator=(const task &) = delete;

    constexpr ~task() {
        if (coroutine_) {
            coroutine_.destroy();
        }
    }

    constexpr auto operator co_await(this const task &self) noexcept {
        return task_awaiter<T, promise_type> { self.coroutine_ };
    }

    constexpr auto get_coroutine(this const task &self) noexcept {
        return self.coroutine_;
    }

private:
    std::coroutine_handle<promise_type> coroutine_;
};

template <typename Loop, typename T>
T task_run(Loop &loop, const task<T> &task) {
    auto awaiter = task.operator co_await();
    awaiter.await_suspend(std::noop_coroutine()).resume();
    loop.run();
    return awaiter.await_resume();
}

} // namespace kuro
