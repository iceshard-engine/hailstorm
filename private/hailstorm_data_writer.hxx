#pragma once
#include "hailstorm_memutils.hxx"
#include <hailstorm/hailstorm_operations.hxx>
#include <coroutine>
#include <cassert>
#include <cstring>

namespace hailstorm
{

    enum class DataWriterMode
    {
        Synchronous,
        Asynchronous
    };

    struct DataWriterStage
    {
        bool const is_valid;
        inline bool await_ready() const noexcept { return is_valid; }
        // Since we know the coroutine is not exposed to the outside we can destroy it when an error happens and
        //   it will clean up all resources on destruction.
        inline void await_suspend(std::coroutine_handle<> coro) const noexcept { coro.destroy(); }
        inline void await_resume() const noexcept { }
    };

    template<typename T>
    concept IDataWriter = requires(T t, hailstorm::v1::HailstormWriteInfo& write_info) {
        { t.write_header(hailstorm::Data{}, size_t{}) } -> std::convertible_to<DataWriterStage>;
        { t.write_resource(hailstorm::v1::HailstormWriteData{}, write_info, size_t{}) } -> std::convertible_to<DataWriterStage>;
        { t.write_metadata(hailstorm::v1::HailstormWriteData{}, uint32_t{}, size_t{}) } -> std::convertible_to<DataWriterStage>;
        { t.finalize() } -> std::convertible_to<hailstorm::Memory>;
    };

    template<DataWriterMode Mode>
    struct DataWriter;

    template<>
    struct DataWriter<DataWriterMode::Synchronous> final
    {
        DataWriter(
            hailstorm::v1::HailstormWriteParams const& params,
            hailstorm::Allocator& allocator,
            size_t size
        ) noexcept
            : _params{ params }
            , _allocator{ allocator }
            , _memory{ allocator.allocate(size) }
        {
        }

        ~DataWriter()
        {
            if (_memory.location == nullptr)
            {
                _allocator.deallocate(_memory);
            }
        }

        auto write_header(hailstorm::Data data, size_t offset) noexcept
        {
            std::memcpy(ptr_add(_memory.location, offset), data.location, data.size);
            return DataWriterStage{ true };
        }

        auto write_resource(
            hailstorm::v1::HailstormWriteData const& data, hailstorm::v1::HailstormWriteInfo& write_info, size_t write_offset
        ) noexcept
        {
            uint32_t const res_idx = write_info.resource_index;
            hailstorm::Memory const target_mem = ptr_add(_memory, write_offset);

            // If data has a nullptr locations, call the write callback to access resource data.
            // This allows us to "stream" input data to the final buffer.
            // TODO: It would be also good to provide a way to stream final data to a file also.
            if (data.data[res_idx].location == nullptr)
            {
                _params.fn_resource_write(data, write_info, target_mem, _params.userdata);
            }
            else // We got the data so just copy it over
            {
                memcpy(target_mem.location, data.data[res_idx].location, data.data[res_idx].size);
            }
            return DataWriterStage{ true };
        }

        auto write_metadata(
            hailstorm::v1::HailstormWriteData const& data, uint32_t idx, size_t offset
        ) noexcept
        {
            std::memcpy(ptr_add(_memory.location, offset), data.metadata[idx].location, data.metadata[idx].size);
            return DataWriterStage{ true };
        }

        auto write_custom_chunk_data(
            hailstorm::v1::HailstormWriteData const& data,
            hailstorm::v1::HailstormChunk const& chunk
        ) noexcept
        {
            hailstorm::Memory const target_mem = ptr_add(_memory, chunk.offset);
            return DataWriterStage{ _params.fn_custom_chunk_write(data, chunk, target_mem, _params.userdata) };
        }

        auto finalize() noexcept -> hailstorm::Memory
        {
            return std::exchange(_memory, {});
        }

        hailstorm::v1::HailstormWriteParams const& _params;
        hailstorm::Allocator& _allocator;
        hailstorm::Memory _memory;
    };

    template<>
    struct DataWriter<DataWriterMode::Asynchronous> final
    {
        DataWriter(
            hailstorm::v1::HailstormAsyncWriteParams const& params,
            size_t size
        ) noexcept
            : _params{ params }
            , _open{ params.fn_async_open(size, params.async_userdata) }
        {
        }

        auto write_header(hailstorm::Data data, size_t offset) noexcept
        {
            return DataWriterStage{ _params.fn_async_write_header(data, offset, _params.async_userdata) };
        }

        auto write_resource(
            hailstorm::v1::HailstormWriteData const& data, hailstorm::v1::HailstormWriteInfo& write_info, size_t write_offset
        ) noexcept
        {
            return DataWriterStage{ _params.fn_async_write_resource(data, write_info, write_offset, _params.async_userdata) };
        }

        auto write_metadata(
            hailstorm::v1::HailstormWriteData const& data, uint32_t idx, size_t write_offset
        ) noexcept
        {
            return DataWriterStage{ _params.fn_async_write_metadata(data, idx, write_offset, _params.async_userdata) };
        }

        auto write_custom_chunk_data(
            hailstorm::v1::HailstormWriteData const& data,
            hailstorm::v1::HailstormChunk const& chunk
        ) noexcept
        {
            return DataWriterStage{ _params.fn_async_write_custom_chunk(data, chunk, chunk.offset, _params.async_userdata) };
        }

        auto finalize() noexcept -> hailstorm::Memory
        {
            if (std::exchange(_open, false))
            {
                _params.fn_async_close(_params.async_userdata);
            }
            return {};
        }

        ~DataWriter() noexcept
        {
            assert(_open == false);
            finalize();
        }

        hailstorm::v1::HailstormAsyncWriteParams const& _params;
        bool _open;
    };

} // namespace hailstorm
