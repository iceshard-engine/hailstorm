/// Copyright 2023 - 2023, Dandielo <dandielo@iceshard.net>
/// SPDX-License-Identifier: MIT

#include <hailstorm/hailstorm_operations.hxx>
#include "hailstorm_data_writer.hxx"
#include "hailstorm_array.hxx"
#include "hailstorm_task.hxx"
#include <cassert>

namespace hailstorm::v1
{

    namespace detail
    {

        struct Offsets
        {
            size_t chunks;
            size_t ids;
            size_t resources;
            size_t data;
            size_t paths_info;
            size_t paths_data;
        };

        template<typename T>
        auto increase_size(size_t& inout_size, size_t count = 1, size_t align = alignof(T)) noexcept -> size_t
        {
            assert(align >= alignof(T));
            size_t const previous_size = align_to(inout_size, align);
            inout_size += count * sizeof(T);
            return previous_size;
        };

    } // namespace detail

    static constexpr size_t Constant_MetadataMinAlign = 8;
    static constexpr size_t Constant_MaxSupportedPackSize = std::numeric_limits<size_t>::max();
    static constexpr uint8_t Constant_U8Max = std::numeric_limits<uint8_t>::max();
    static constexpr uint32_t Constant_U32Max = std::numeric_limits<uint32_t>::max();
    static constexpr HailstormChunk Constant_EmptyChunk{ };

    auto read_header(
        hailstorm::Data data,
        hailstorm::v1::HailstormData& out_hailstorm
    ) noexcept -> hailstorm::Result
    {
        if (data.location == nullptr || data.size < sizeof(HailstormHeaderBase))
        {
            return Result::E_InvalidPackData;
        }
        else if (data.size < sizeof(HailstormHeaderBase))
        {
            return Result::E_IncompleteHeaderData;
        }

        HailstormHeaderBase const* const base_header = reinterpret_cast<HailstormHeaderBase const*>(data.location);
        if (base_header->magic != Constant_HailstormMagic
            || base_header->header_version != Constant_HailstormHeaderVersionV0
            || base_header->header_size >= Constant_1GiB)
        {
            return Result::E_InvalidPackData;
        }

        if (data.size < base_header->header_size)
        {
            return Result::E_IncompleteHeaderData;
        }

        HailstormHeader const* const v1_header = reinterpret_cast<HailstormHeader const*>(base_header);
        HailstormPaths const* const paths = reinterpret_cast<HailstormPaths const*>(v1_header + 1);

        // Return the header data.
        out_hailstorm.header = *v1_header;

        if (v1_header->count_chunks == 0)
        {
            return Result::E_EmptyPack;
        }

        // Set paths information
        out_hailstorm.paths = *paths;

        auto* const chunks_ptr = reinterpret_cast<HailstormChunk const*>(paths + 1);
        out_hailstorm.chunks = std::span{ chunks_ptr, v1_header->count_chunks };
        auto* const resources_ptr = reinterpret_cast<HailstormResource const*>(chunks_ptr + v1_header->count_chunks);
        out_hailstorm.resources = std::span{ resources_ptr, v1_header->count_resources };

        // Safely check (with no overflow) that we can represent the packs data offsets.
        HailstormChunk const& last_chunk = out_hailstorm.chunks.back();
        if ((Constant_MaxSupportedPackSize - last_chunk.offset) < last_chunk.size)
        {
            return Result::E_LargePackNotSupported;
        }

        if (paths->size <= ptr_distance(resources_ptr + v1_header->count_resources, ptr_add(data.location, data.size)))
        {
            out_hailstorm.paths_data = Data{
                ptr_add(data.location, paths->offset),
                paths->size,
                1
            };
        }
        else
        {
            out_hailstorm.paths_data = Data{};
        }
        return Result::Success;
    }

    auto cluster_size_info(
        uint32_t resource_count,
        std::span<hailstorm::v1::HailstormChunk const> chunks,
        hailstorm::v1::HailstormPaths paths,
        hailstorm::v1::detail::Offsets& out_offsets
    ) noexcept -> size_t
    {
        size_t final_size = sizeof(HailstormHeader);
        out_offsets.paths_info = detail::increase_size<HailstormPaths>(final_size);
        out_offsets.chunks = detail::increase_size<HailstormChunk>(final_size, chunks.size());
        out_offsets.resources = detail::increase_size<HailstormResource>(final_size, resource_count);
        out_offsets.paths_data = detail::increase_size<char>(final_size, paths.size, 8);
        out_offsets.data = final_size;

        for (HailstormChunk const& chunk : chunks)
        {
            final_size += chunk.size;
        }
        return final_size;
    }

    bool estimate_cluster_chunks(
        hailstorm::v1::HailstormWriteParams const& params,
        hailstorm::v1::HailstormWriteData const& write_data,
        hailstorm::Array<HailstormChunk>& chunks,
        hailstorm::Array<HailstormWriteChunkRef>& refs,
        hailstorm::Array<size_t>& sizes,
        hailstorm::Array<uint32_t>& metatracker,
        HailstormPaths& paths_info,
        uint32_t res_count
    ) noexcept
    {
        bool requires_data_writer_callback = false;

        for (uint32_t idx = 0; idx < res_count;)
        {
            // If the metadata is shared, check for the already assigned chunk
            uint32_t const metadata_idx = metatracker.any()
                ? write_data.metadata_mapping[idx]
                : idx;

            Data const meta = write_data.metadata[metadata_idx];
            Data const data = write_data.data[idx];

            // Check if even one data object is not provided.
            requires_data_writer_callback |= data.location == nullptr;

            // Get the selected chunks for the data and metadata.
            HailstormWriteChunkRef ref = params.fn_select_chunk(meta, data, chunks, params.userdata);

            bool shared_metadata = false;
            if (ref.data_create == false && ref.meta_create == false)
            {
                assert(ref.data_chunk < chunks.count());
                assert(ref.meta_chunk < chunks.count());

                // If the metadata is shared, check for the already assigned chunk
                if (metatracker.any())
                {
                    if (metatracker[metadata_idx] != Constant_U32Max)
                    {
                        shared_metadata = true;
                        ref.meta_chunk = refs[metadata_idx].meta_chunk;
                    }
                }

                size_t const data_remaining = (chunks[ref.data_chunk].size - sizes[ref.data_chunk]) - static_cast<size_t>(data.align);
                size_t const meta_remaining = (chunks[ref.meta_chunk].size - sizes[ref.meta_chunk]) - Constant_MetadataMinAlign;

                // Check if we need to create a new chunk due to size restrictions.
                if (ref.data_chunk == ref.meta_chunk)
                {
                    ref.data_create |= (data_remaining - meta.size) < data.size;
                    ref.meta_create = false; // We only want to create one chunk if both data and meta are the same.
                }
                else
                {
                    ref.data_create |= data_remaining < data.size;
                    ref.meta_create |= meta_remaining < meta.size;
                }
            }

            if (ref.data_create)
            {
                HailstormChunk new_chunk = params.fn_create_chunk(
                    meta, data, chunks[ref.data_chunk], params.userdata
                );
                new_chunk.offset = 0;
                new_chunk.size_origin = 0;
                new_chunk.count_entries = 0;

                // Either mixed or data only chunks
                assert(
                    (ref.data_chunk == ref.meta_chunk && new_chunk.type == 3)
                    || (new_chunk.type == 2)
                );

                // Push the new chunk
                chunks.push_back(new_chunk);
                sizes.push_back(0);
            }

            if (ref.meta_create)
            {
                assert(shared_metadata == false);
                HailstormChunk const new_chunk = params.fn_create_chunk(
                    meta, data, chunks[ref.meta_chunk], params.userdata
                );

                // Meta only chunks
                assert(new_chunk.type == 1);

                // Push the new chunk
                chunks.push_back(new_chunk);
            }

            // If chunks where created, re-do the selection
            if (ref.data_create || ref.meta_create)
            {
                // We don't want to increase the index yet
                continue;
            }

            // Only update the tracker once we are sure we have a final chunk selected
            if (metatracker.any())
            {
                if (metatracker[metadata_idx] == Constant_U32Max)
                {
                    metatracker[metadata_idx] = idx;
                }
            }

            refs[idx] = ref;

            assert(chunks[ref.data_chunk].type & 0x2); // Data capable chunks
            assert(chunks[ref.meta_chunk].type & 0x1); // Meta capable chunks

            chunks[ref.data_chunk].count_entries += 1;

            // Udpate the sizes array, however only update meta if it's not shared (not assigned to a chunk yet)
            if (shared_metadata == false)
            {
                // Add an entry to a meta chunk only if it's not duplicated and if it's not mixed
                if (ref.data_chunk != ref.meta_chunk)
                {
                    chunks[ref.meta_chunk].count_entries += 1;
                }

                sizes[ref.meta_chunk] = align_to(sizes[ref.meta_chunk], 8) + meta.size;
            }
            sizes[ref.data_chunk] = align_to(sizes[ref.data_chunk], data.align) + data.size;

            // Calculate total size needed for all paths to be stored
            uint32_t const path_size = uint32_t(write_data.paths[idx].size());
            paths_info.size += size_t{ path_size + 1 };

            // Increase index at the end
            idx += 1;
        }

        return requires_data_writer_callback;
    }

    bool prepare_cluster_info(
        hailstorm::v1::HailstormWriteParams const& params,
        hailstorm::v1::HailstormWriteData const& write_data,
        hailstorm::Array<HailstormChunk>& out_chunks,
        hailstorm::Array<HailstormWriteChunkRef>& out_chunks_refs,
        hailstorm::Array<size_t>& out_chunk_sizes,
        hailstorm::Array<uint32_t>& out_metatracker,
        hailstorm::v1::HailstormPaths& out_paths
    ) noexcept
    {
        uint32_t const res_count = uint32_t(write_data.paths.size());

        out_chunks.reserve(params.estimated_chunk_count);
        out_chunks.push_back(params.initial_chunks);

        if (out_chunks.count() == 0)
        {
            HailstormChunk const new_chunk = params.fn_create_chunk(
                Data{.align = 8}, Data{.align = 8}, Constant_EmptyChunk, params.userdata
            );
            out_chunks.push_back(new_chunk);
        }

        // Keep an array for all final chunk references.
        out_chunks_refs.resize(res_count);

        // Sizes start with 0_B fill.
        out_chunk_sizes.resize(out_chunks.count());
        out_chunk_sizes.memset(0);

        // Metadata saved tracker.
        out_metatracker.resize(uint32_t(write_data.metadata_mapping.size()));
        out_metatracker.memset(Constant_U8Max);

        out_paths.size = 8;
        bool const requires_data_writer_callback = estimate_cluster_chunks(
            params, write_data, out_chunks, out_chunks_refs, out_chunk_sizes, out_metatracker, out_paths, res_count
        );

        // Paths needs to be aligned to boundary of 8
        out_paths.size = align_to(out_paths.size, 8);

        // Reduce chunk sizes but align them to their alignment boundary.
        uint32_t chunk_idx = 0;
        for (HailstormChunk& chunk : out_chunks)
        {
            chunk.size = align_to(out_chunk_sizes[chunk_idx], chunk.align);
            chunk_idx += 1;
        }

        return requires_data_writer_callback;
    }

    template<hailstorm::DataWriterMode WriterMode>
    auto write_cluster_internal(
        hailstorm::v1::HailstormWriteParams const& params,
        hailstorm::v1::HailstormAsyncWriteParams const& stream_params,
        hailstorm::v1::HailstormWriteData const& write_data
    ) noexcept -> hailstorm::Task
    {
        uint32_t const res_count = uint32_t(write_data.paths.size());

        Array<HailstormChunk> chunks{ params.temp_alloc };
        Array<HailstormWriteChunkRef> refs{ params.temp_alloc };
        Array<size_t> sizes{ params.temp_alloc };
        Array<uint32_t> metatracker{ params.temp_alloc };
        HailstormPaths paths_info{ };
        bool const requires_writer_callback = prepare_cluster_info(
            params, write_data, chunks, refs, sizes, metatracker, paths_info
        );

        if constexpr (WriterMode == DataWriterMode::Synchronous)
        {
            // Either we don't need the callback or we need to have it provided!
            assert(requires_writer_callback == false || params.fn_resource_write != nullptr);
        }

        // Calculate an estimated size for the whole cluster.
        // TODO: This size is currently exact, but once we start compressing / encrypting this will no longer be the case.
        detail::Offsets offsets;
        size_t const final_cluster_size = cluster_size_info(res_count, chunks, paths_info, offsets);

        // Fill-in header data
        HailstormHeader header{
            .offset_next = final_cluster_size,
            .offset_data = offsets.data,
            .version = { },
            .is_encrypted = false,
            .is_expansion = false,
            .is_patch = false,
            .is_baked = false,
            .count_chunks = uint16_t(chunks.count()),
            .count_resources = uint16_t(res_count),
        };
        header.magic = Constant_HailstormMagic;
        header.header_version = Constant_HailstormHeaderVersionV0;
        header.header_size = offsets.paths_data;
        paths_info.offset = offsets.paths_data;

        // Copy custom values into the final header.
        static_assert(sizeof(write_data.custom_values) == sizeof(header.app_custom_values));
        std::memcpy(header.app_custom_values, write_data.custom_values, sizeof(header.app_custom_values));

        // Place chunk offsets at their proper location.
        size_t chunk_offset = offsets.data;
        for (HailstormChunk& chunk : chunks)
        {
            chunk.size_origin = chunk.size;
            chunk.offset = std::exchange(
                chunk_offset,
                align_to(chunk_offset + chunk.size, 8)
            );
        }

        IDataWriter auto writer = [&]() noexcept
        {
            if constexpr (WriterMode == DataWriterMode::Asynchronous)
            {
                return DataWriter<WriterMode>{ stream_params, final_cluster_size };
            }
            else
            {
                return DataWriter<WriterMode>{ params, params.cluster_alloc, final_cluster_size };
            }
        }();

        // Copy over all chunk data
        co_await writer.write_header(data_view(header), 0);
        co_await writer.write_header(data_view(paths_info), offsets.paths_info);
        co_await writer.write_header(chunks.data_view(), offsets.chunks);

        // Prepare temporary data for resources and paths
        TrackedMemory temp_resource_mem{ params.temp_alloc, sizeof(HailstormResource) * res_count };
        TrackedMemory temp_paths_mem{ params.temp_alloc, paths_info.size };

        HailstormResource* const pack_resources = reinterpret_cast<HailstormResource*>(
            temp_resource_mem.location
        );

        uint32_t paths_offset = 0;
        char* const paths_data = reinterpret_cast<char*>(
            temp_paths_mem.location
        );

        // Clear sizes
        sizes.memset(0);
        metatracker.memset(Constant_U8Max);

        // We now go over the list again, this time already filling data in.
        for (uint32_t idx = 0; idx < res_count; ++idx)
        {
            HailstormResource& res = pack_resources[idx];
            res.chunk = refs[idx].data_chunk;
            res.meta_chunk = refs[idx].meta_chunk;

            HailstormChunk const& data_chunk = chunks[res.chunk];
            HailstormChunk const& meta_chunk = chunks[res.meta_chunk];

            // Get the index of the resource that stored the meta originally, or 'u32_max' if this is the first occurence.
            uint32_t meta_idx = idx;
            uint32_t meta_map_idx = Constant_U32Max;
            if (metatracker.any())
            {
                meta_idx = write_data.metadata_mapping[idx];
                meta_map_idx = std::exchange(metatracker[meta_idx], idx);
            }

            // Calc, Store and ralign the used space so the next value can be already copied onto the proper location.
            if (meta_map_idx == Constant_U32Max)
            {
                size_t& meta_chunk_used = sizes[res.meta_chunk];
                Data const data = write_data.data[idx];

                // Store meta location
                res.meta_size = uint32_t(data.size);
                res.meta_offset = uint32_t(meta_chunk_used);

                co_await writer.write_metadata(
                    write_data, meta_idx, meta_chunk.offset + meta_chunk_used
                );

                // Need to update the 'used' variable after we wrote the metadata
                meta_chunk_used = align_to(meta_chunk_used + data.size, meta_chunk.align);
            }
            else
            {
                res.meta_size = pack_resources[meta_map_idx].meta_size;
                res.meta_offset = pack_resources[meta_map_idx].meta_offset;
            }

            {
                size_t& data_chunk_used = sizes[res.chunk];
                Data const data = write_data.data[idx];

                // Store data location
                res.size = uint32_t(data.size);
                res.offset = uint32_t(data_chunk_used);

                co_await writer.write_resource(
                    write_data, idx, data_chunk.offset + data_chunk_used
                );

                // Ensure the data view has an alignment smaller or equal to the chunk alignment.
                assert(data.align <= data_chunk.align);

                data_chunk_used = align_to(data_chunk_used + data.size, data_chunk.align);
            }

            {
                res.path_size = uint32_t(write_data.paths[idx].size());
                res.path_offset = paths_offset;

                // Copy and increment the offset with an '\0' character added.
                size_t const path_size = size_t{ res.path_size };
                std::memcpy(paths_data + paths_offset, write_data.paths[idx].data(), write_data.paths[idx].size());
                paths_offset += res.path_size + 1;
                paths_data[paths_offset - 1] = '\0';
            }
        }

        // Write all custom chunks
        auto it = chunks.begin();
        auto const end = chunks.end();
        while(it != end && it->type == 0)
        {
            co_await writer.write_custom_chunk_data(write_data, *it);
            it += 1;
        }

        // Clear the final bytes required to be zeroed in the paths block
        std::memset(ptr_add(paths_data, paths_offset), 0, paths_info.size - paths_offset);

        // Write final memory information
        co_await writer.write_header(data_view(temp_paths_mem), offsets.paths_data);
        co_await writer.write_header(data_view(temp_resource_mem), offsets.resources);
        co_return writer.finalize();
    }

    auto write_cluster(
        hailstorm::v1::HailstormWriteParams const& params,
        hailstorm::v1::HailstormWriteData const& data
    ) noexcept -> hailstorm::Memory
    {
        uint32_t const count_ids = uint32_t(data.paths.size());
        assert(count_ids == data.data.size());
        assert(count_ids == data.metadata.size() || count_ids <= data.metadata_mapping.size());

        HailstormAsyncWriteParams async_params_empty{ .base_params = params };
        return write_cluster_internal<DataWriterMode::Synchronous>(params, async_params_empty, data).result_memory();
    }

    bool write_cluster_async(
        hailstorm::v1::HailstormAsyncWriteParams const& params,
        hailstorm::v1::HailstormWriteData const& data
    ) noexcept
    {
        uint32_t const count_ids = uint32_t(data.paths.size());
        assert(count_ids == data.data.size());
        assert(count_ids == data.metadata.size() || count_ids <= data.metadata_mapping.size());

        write_cluster_internal<DataWriterMode::Asynchronous>(params.base_params, params, data);
        return true;
    }

    auto prefixed_resource_paths_size(
        hailstorm::v1::HailstormPaths const& paths_info,
        uint32_t count_resources,
        std::string_view prefix
    ) noexcept -> size_t
    {
        size_t const extended_size = { count_resources * prefix.size() };
        return paths_info.size + extended_size;
    }

    bool prefix_resource_paths(
        hailstorm::v1::HailstormPaths const& paths_info,
        std::span<hailstorm::v1::HailstormResource> resources,
        hailstorm::Memory paths_data,
        std::string_view prefix
    ) noexcept
    {
        size_t const count_resources = resources.size();
        size_t const size_extended_paths = prefixed_resource_paths_size(
            paths_info, count_resources, prefix
        );
        if (size_extended_paths > paths_data.size)
        {
            return false;
        }

        size_t const size_prefix = prefix.size();
        size_t const size_extending = size_prefix * count_resources;

        // We find the first non '0' character. The .hsc ensures the last few bytes in the pats chunk are zeros.
        char* const paths_start = reinterpret_cast<char*>(paths_data.location);
        char* paths_end = paths_start + paths_info.size;
        char* ex_paths_end = paths_end + size_extending;
        while (paths_end[-1] == '\0')
        {
            paths_end -= 1;
            ex_paths_end -= 1;
        }

        // Iterate over each resource, update it's path (from last to first)
        uint32_t resource_idx;
        for (resource_idx = count_resources; ex_paths_end > paths_start && resource_idx > 0; --resource_idx)
        {
            v1::HailstormResource& res = resources[resource_idx - 1];
            *ex_paths_end = '\0';

            ex_paths_end -= res.path_size;
            std::memcpy(ex_paths_end, paths_start + res.path_offset, res.path_size);

            ex_paths_end -= size_prefix;
            std::memcpy(ex_paths_end, prefix.data(), size_prefix);

            // Update resource path information
            res.path_offset = ptr_distance<uint32_t>(paths_start, ex_paths_end);
            res.path_size += size_prefix;
            ex_paths_end -= 1;
        }

        // Ensure we finished at the proper location
        assert((ex_paths_end + 1) == paths_start);
        return resource_idx == 0 && (ex_paths_end + 1) == paths_start;
    }

} // namespace hailstorm::v1
