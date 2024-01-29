#pragma once
#include "hailstorm_tracked_memory.hxx"
#include <algorithm>

namespace hailstorm
{

    constexpr inline auto data_view(hailstorm::Memory memory) noexcept -> hailstorm::Data
    {
        return { memory.location, memory.size, memory.align };
    }

    constexpr inline auto data_view(hailstorm::TrackedMemory const& memory) noexcept -> hailstorm::Data
    {
        return { memory.location, memory.size, memory.align };
    }

    template<typename T>
    inline auto data_view(T const& object) noexcept -> hailstorm::Data
    {
        return { std::addressof(object), sizeof(T), alignof(T) };
    }

    template<typename T = size_t>
    inline auto ptr_distance(void const* from, void const* to) noexcept -> T
    {
        return static_cast<T>(std::abs(reinterpret_cast<char const*>(to) - reinterpret_cast<char const*>(from)));
    }

    inline auto ptr_add(void* ptr, size_t offset) noexcept
    {
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + offset);
    }

    inline auto ptr_add(void const* ptr, size_t offset) noexcept
    {
        return reinterpret_cast<void const*>(reinterpret_cast<uintptr_t>(ptr) + offset);
    }

    inline auto ptr_add(Memory ptr, size_t offset) noexcept
    {
        ptr.location = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr.location) + offset);
        ptr.size -= offset;
        return ptr;
    }

    inline auto align_to(void* ptr, uint32_t alignment) noexcept
    {
        uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
        if (uintptr_t alignment_miss = (ptr_val % alignment); alignment_miss != 0)
        {
            ptr_val = alignment - alignment_miss;
        }
        return reinterpret_cast<void*>(ptr_val);
    }

    inline auto align_to(uint64_t ptr_val, uint32_t alignment) noexcept
    {
        if (uintptr_t alignment_miss = (ptr_val % alignment); alignment_miss != 0)
        {
            ptr_val += alignment - alignment_miss;
        }
        return ptr_val;
    }

} // namespace hailstorm
