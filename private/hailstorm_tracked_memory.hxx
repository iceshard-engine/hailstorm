#pragma once
#include <hailstorm/hailstorm_types.hxx>

namespace hailstorm
{

    struct TrackedMemory final : hailstorm::Memory
    {
        inline explicit TrackedMemory(hailstorm::Allocator& alloc, size_t req) noexcept
            : Memory{ alloc.allocate(req) }
            , _allocator{ alloc }
        {
        }

        inline ~TrackedMemory() noexcept
        {
            _allocator.deallocate(*this);
        }

        hailstorm::Allocator& _allocator;
    };

} // namespace hailstorm
