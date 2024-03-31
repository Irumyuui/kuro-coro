#pragma once

#include <coroutine>
#include <utility>

namespace kuro::concepts {

template <typename T>
concept Awaiter = requires(T awaiter, std::coroutine_handle<> coroutine) {
    { awaiter.await_ready() };
    { awaiter.await_suspend(coroutine) };
    { awaiter.await_resume() };
};

template <typename T>
concept Awaitable = Awaiter<T> || requires(T a) {
    { a.operator co_await() } -> Awaiter;
};

template <typename T>
struct awaitable_traits {
    using type = T;
};

template <Awaiter T>
struct awaitable_traits<T> {
    using result_type = decltype(std::declval<T>().await_resume());
    using awaiter_type = T;
};

template <typename T>
    requires (!Awaiter<T> && Awaitable<T>)
struct awaitable_traits<T>
    : awaitable_traits<decltype(std::declval<T>().operator co_await())> {};



}
