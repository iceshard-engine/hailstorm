#pragma once
#include <hailstorm/hailstorm_types.hxx>
#include <memory_resource>
#include <vector>

namespace hailstorm
{

    struct AllocatorResource : std::pmr::memory_resource
    {
        inline AllocatorResource(hailstorm::Allocator& alloc) noexcept;

    protected: // Implementing 'std::pmr::memory_resource' interface
        inline auto do_allocate(size_t size, size_t align) -> void* override;
        inline void do_deallocate(void* ptr, size_t size, size_t aling) override;
        inline bool do_is_equal(std::pmr::memory_resource const& other) const noexcept override;

    private:
        hailstorm::Allocator* _allocator;
    };

    inline AllocatorResource::AllocatorResource(hailstorm::Allocator& alloc) noexcept
        : _allocator{ &alloc }
    {
    }

    inline auto AllocatorResource::do_allocate(size_t size, size_t align) -> void*
    {
        return _allocator->allocate(size).location;
    }

    inline void AllocatorResource::do_deallocate(void* ptr, size_t size, size_t align)
    {
        _allocator->deallocate(hailstorm::Memory{ ptr, size, uint32_t(align) });
    }

    inline bool AllocatorResource::do_is_equal(std::pmr::memory_resource const& other) const noexcept
    {
        return _allocator == static_cast<AllocatorResource const&>(other)._allocator;
    }

    template<typename T>
    struct Array
    {
        Array(hailstorm::Allocator& alloc) noexcept;
        ~Array() noexcept = default;

        inline bool any() const noexcept;
        inline bool empty() const noexcept;
        inline auto count() const noexcept -> uint32_t;

        inline void reserve(uint32_t count) noexcept;
        inline void resize(uint32_t count) noexcept;
        inline void memset(uint8_t value) noexcept requires (std::is_trivial_v<T>);

        inline void push_back(T&& val) noexcept;
        inline void push_back(T const& val) noexcept;
        inline void push_back(std::span<T const> val) noexcept;

        inline auto operator[](uint32_t idx) noexcept -> T& { return _vector[idx]; }
        inline auto operator[](uint32_t idx) const noexcept -> T const& { return _vector[idx]; }

        inline auto begin() noexcept -> T* { return _vector.data(); }
        inline auto end() noexcept -> T* { return _vector.data() + _vector.size(); }

        inline auto data_view() noexcept -> hailstorm::Data
        {
            return {
                .location = _vector.data(),
                .size = _vector.size() * sizeof(T),
                .align = alignof(T)
            };
        }

        inline operator std::span<T>() noexcept { return _vector; }
        inline operator std::span<T const>() const noexcept { return _vector; }

    private:
        AllocatorResource _alloc_resource;
        std::pmr::vector<T> _vector;
    };

    template<typename T>
    inline Array<T>::Array(hailstorm::Allocator& alloc) noexcept
        : _alloc_resource{ alloc }
        , _vector{ std::pmr::polymorphic_allocator<T>{ &_alloc_resource } }
    {
    }

    template<typename T>
    inline bool Array<T>::any() const noexcept
    {
        return _vector.empty() == false;
    }

    template<typename T>
    inline bool Array<T>::empty() const noexcept
    {
        return _vector.empty();
    }

    template<typename T>
    inline auto Array<T>::count() const noexcept -> uint32_t
    {
        return uint32_t(_vector.size());
    }

    template<typename T>
    inline void Array<T>::reserve(uint32_t count) noexcept
    {
        _vector.reserve(count);
    }

    template<typename T>
    inline void Array<T>::resize(uint32_t count) noexcept
    {
        _vector.resize(count);
    }

    template<typename T>
    inline void Array<T>::memset(uint8_t value) noexcept requires (std::is_trivial_v<T>)
    {
        std::memset(_vector.data(), value, _vector.size() * sizeof(T));
    }

    template<typename T>
    inline void Array<T>::push_back(T&& val) noexcept
    {
        _vector.push_back(std::forward<T>(val));
    }

    template<typename T>
    inline void Array<T>::push_back(T const& val) noexcept
    {
        _vector.push_back(val);
    }

    template<typename T>
    inline void Array<T>::push_back(std::span<T const> val) noexcept
    {
        _vector.insert(_vector.end(), val.begin(), val.end());
    }


} // namespace hailstorm
