#pragma once

#include <cassert>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <optional>
#include <span>
#include <tuple>
#include <utility>

#include "concepts.hpp"
#include "task.hpp"

namespace kuro {

struct [[nodiscard]] return_prev_task {
public:
    struct promise_type {
        promise_type& operator=(const promise_type&) = delete;
        
        constexpr auto initial_suspend() const noexcept {
            return std::suspend_always{};
        }

        constexpr auto final_suspend() const noexcept {
            return prev_coroutine_awaiter { prev_coroutine_ };
        }

        constexpr void unhandled_exception([[maybe_unused]] this promise_type &self) {
            std::rethrow_exception(std::current_exception());
        }

        constexpr void return_value(this promise_type &self, std::coroutine_handle<> prev_coroutine) noexcept {
            self.prev_coroutine_ = prev_coroutine;
        }
        
        constexpr auto get_return_object(this promise_type &self) {
            return std::coroutine_handle<promise_type>::from_promise(self);
        }
        
        std::coroutine_handle<> prev_coroutine_;
    };

public:
    constexpr return_prev_task(std::coroutine_handle<promise_type> coroutine) noexcept : coroutine_(coroutine) {}

    constexpr return_prev_task(return_prev_task &&other) = delete;

    constexpr ~return_prev_task() {
        if (coroutine_) {
            coroutine_.destroy();
        }
    }

    constexpr auto get_coroutine(this const return_prev_task &self) noexcept {
        return self.coroutine_;
    }

private:
    std::coroutine_handle<promise_type> coroutine_;
};

struct when_all_ctl_block {
    std::size_t task_count_;
    std::coroutine_handle<> prev_coroutine_{};
    std::exception_ptr except_{};
};

struct when_all_awaiter {
public:
    constexpr when_all_awaiter(when_all_ctl_block &ctl_block, std::span<const return_prev_task> tasks)
        noexcept : ctl_block_(ctl_block), tasks_(tasks) {}
    
    constexpr bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<> await_suspend(this const when_all_awaiter &self, std::coroutine_handle<> coroutine) {
        if (self.tasks_.empty()) {
            return coroutine;
        }
        self.ctl_block_.prev_coroutine_ = coroutine;
        for (const auto &task : self.tasks_.subspan(0, self.tasks_.size() - 1)) {
            task.get_coroutine().resume();
        }
        return self.tasks_.back().get_coroutine();
    }

    constexpr void await_resume(this const when_all_awaiter &self) {
        if (self.ctl_block_.except_) [[unlikely]] {
            std::rethrow_exception(self.ctl_block_.except_);
        }
    }

private:
    when_all_ctl_block& ctl_block_;
    std::span<const return_prev_task> tasks_;
};

template <typename T>
struct maybe_result {
    T take_result(this maybe_result &self) {
        assert(self.result_.has_value());
        T t = std::move(*self.result_);
        self.result_.reset();
        return t;
    }

    std::optional<T> result_;
};

template <>
struct maybe_result<void> {
    constexpr auto take_result() const noexcept {
        return std::nullopt;
    }
    // result_ = std::nullopt;
};

// template <typename T>
//     requires std::is_void_v<typename T::value_type>
inline return_prev_task make_when_all_task(auto &&task, when_all_ctl_block &ctl_block, maybe_result<void> &) {
    try {
        co_await std::forward<decltype(task)>(task);
    } catch (...) {
        ctl_block.except_ = std::current_exception();
        co_return ctl_block.prev_coroutine_;
    }
    ctl_block.task_count_ -= 1;
    if (!ctl_block.task_count_) {
        co_return ctl_block.prev_coroutine_;
    }
    co_return std::noop_coroutine();
}

template <typename T>
    // requires (!std::is_void_v<typename T::value_type>)
inline return_prev_task make_when_all_task(auto &&task, when_all_ctl_block &ctl_block, maybe_result<T> &result) {
    try {
        // co_await std::forward<T>(task);
        result.result_ = std::move(co_await std::forward<decltype(task)>(task));
    } catch (...) {
        ctl_block.except_ = std::current_exception();
        co_return ctl_block.prev_coroutine_;
    }
    ctl_block.task_count_ -= 1;
    if (!ctl_block.task_count_) {
        co_return ctl_block.prev_coroutine_;
    }
    co_return std::noop_coroutine();
}

template <typename T>
struct result_type_wrapper {
    using type = T;
};

template <>
struct result_type_wrapper<void> {
    using type = std::nullopt_t;
};

template <typename T>
using result_type_wrapper_t = typename result_type_wrapper<T>::type;

template <std::size_t... Is, concepts::Awaitable... Ts>
kuro::task<std::tuple<result_type_wrapper_t<typename concepts::awaitable_traits<Ts>::result_type>...>> 
when_all_impl(std::index_sequence<Is...>, Ts &&...ts) {
    when_all_ctl_block ctl_block { sizeof...(Ts) };

    std::tuple<maybe_result<typename concepts::awaitable_traits<Ts>::result_type>...> result;
    return_prev_task tasks[] {
        make_when_all_task(std::forward<Ts>(ts), ctl_block, std::get<Is>(result))...
    };
    
    co_await when_all_awaiter(ctl_block, tasks);
    co_return std::tuple {
        std::get<Is>(result).take_result()...
    };
}

template <concepts::Awaitable... Ts>
auto when_all(Ts &&...ts) {
    return when_all_impl(std::make_index_sequence<sizeof...(Ts)>{}, std::forward<Ts>(ts)...);
}

} // namespace kuro
