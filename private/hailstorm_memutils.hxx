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
        // TODO: Pre (alignment is a power of '2')
        uint32_t const align_mask = alignment - 1;

        uintptr_t const ptr_val = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t const padding = (uintptr_t{0} - ptr_val) & align_mask;
        return reinterpret_cast<void*>(ptr_val + padding);
    }

    template<typename T>
    inline auto align_to(T ptr_val, uint32_t alignment) noexcept -> T
    {
        // TODO: Pre (alignment is a power of '2')
        uint32_t const align_mask = alignment - 1;
        T const padding = (T{0} - ptr_val) & align_mask;
        return ptr_val + padding;
    }

} // namespace hailstorm
