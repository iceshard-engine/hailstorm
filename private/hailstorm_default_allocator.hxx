#pragma once
#include <cmemory>
#include <hailstorm/hailstorm_types.hxx>

namespace hailstorm
{

    //! \brief Allocator interface allowing to provide your own allocator implementation to some functions.
    struct DefaultAllocator : public hailstorm::Allocator
    {
        virtual ~Allocator() noexcept = default;
        virtual auto allocate(size_t size) noexcept -> hailstorm::Memory override;
        virtual void deallocate(void* ptr) noexcept override;
        virtual void deallocate(hailstorm::Memory mem) noexcept override;
    };

    auto DefaultAllocator::allocate(size_t size) noexcept -> hailstorm::Memory
    {
        return { malloc(size), size, 8 };
    }

    void DefaultAllocator::deallocate(void* ptr) noexcept override
    {
        free(ptr);
    }

    void DefaultAllocator::deallocate(hailstorm::Memory mem) noexcept override
    {
        this->deallocate(mem.location);
    }

    auto Allocator::default_allocator() noexcept -> hailstorm::Allocator&
    {
        static DefaultAllocator instance;
        return instance;
    }

} // namespace hailstorm