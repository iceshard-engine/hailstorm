#pragma once
#include <span>
#include <string_view>

namespace hailstorm
{

    enum class Result : uint8_t
    {
        Success,

        //! \brief The given arguments are not valid for the function trying to execute.
        E_InvalidArgument,

        //! \brief Pack data was not recognized, invalid MAGIC value or header version.
        E_InvalidPackData,

        //! \brief Pack header data is not complete and could not be fully read.
        E_IncompleteHeaderData,

        //! \brief Pack data is not compatible with the compiled library version.
        E_IncompatiblePackData,

        //! \brief On 32bit architectures it might not be possible to access packs bigger than 4GiB.
        E_LargePackNotSupported,

        //! \brief There are not chunks stored in the pack.
        //! \details It is allowed to have chunks without resources. Such data is defined by an external tool / application.
        //!   Because of this, even if there are no resources, such a pack is NOT considered empty.
        E_EmptyPack,
    };

    //! \brief Used to pass data to some functions.
    struct Data
    {
        void const* location;
        size_t size;
        size_t align;
    };

    //! \brief Used to represent memory blocks.
    struct Memory
    {
        void* location;
        size_t size;
        size_t align;
    };

    //! \brief Allocator interface allowing to provide your own allocator implementation to some functions.
    struct Allocator
    {
        virtual ~Allocator() noexcept = default;
        virtual auto allocate(size_t size) noexcept -> hailstorm::Memory { return { malloc(size), size, 8 }; }
        virtual void deallocate(void* ptr) noexcept { free(ptr); }
        virtual void deallocate(hailstorm::Memory mem) noexcept { this->deallocate(mem.location); }
    };

    static constexpr size_t Constant_1KiB = 1024;
    static constexpr size_t Constant_1MiB = 1024 * Constant_1KiB;
    static constexpr size_t Constant_1GiB = 1024 * Constant_1MiB;

} // namespace hailstorm
