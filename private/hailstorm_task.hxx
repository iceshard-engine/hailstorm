#pragma once
#include <coroutine>

namespace hailstorm
{

    class Task;

    struct TaskPromise
    {
        inline auto get_return_object() noexcept -> Task;

        constexpr auto initial_suspend() const noexcept { return std::suspend_never{}; }
        constexpr auto final_suspend() const noexcept { return std::suspend_never{}; }
        constexpr void unhandled_exception() const noexcept { }

        inline void return_value(hailstorm::Memory memory) noexcept
        {
            _result = memory;
        }

        hailstorm::Memory _result;
    };

    class Task final
    {
    public:
        using promise_type = TaskPromise;

        inline Task(std::coroutine_handle<promise_type> coro) noexcept
            : _coro{ coro }
        {
        }

        // We don't destroy the coro, since we already never suspend on the final suspend point.
        inline ~Task() noexcept = default;

        inline operator bool() const noexcept { return _coro.done(); }

        inline auto result_memory() const noexcept { return _coro.promise()._result; }

    private:
        std::coroutine_handle<promise_type> _coro;
    };

    inline auto TaskPromise::get_return_object() noexcept -> Task
    {
        return Task{ std::coroutine_handle<TaskPromise>::from_promise(*this) };
    }

} // namespace hailstorm
