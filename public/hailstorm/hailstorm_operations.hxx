/// Copyright 2023 - 2023, Dandielo <dandielo@iceshard.net>
/// SPDX-License-Identifier: MIT

#pragma once
#include <hailstorm/hailstorm_types.hxx>
#include <hailstorm/hailstorm.hxx>

namespace hailstorm
{

    namespace v1
    {

        //! \brief Reads hailstorm cluser form the given input containing at least the whole header data.
        //! \note If successful the passed HailstormData struct will have all field updated.
        //!
        //! \param [in] header_data Containing the whole hailstorm header description.
        //! \param [out] out_hailstorm Hailstorm object that will have fields updated to access header data easily.
        //! \return 'Res::Success' if the cluster was valid and version was supported, otherwise an error with the load error.
        auto read_header(
            hailstorm::Data header_data,
            hailstorm::v1::HailstormData& out_hailstorm
        ) noexcept -> hailstorm::Result;

        //! \brief Creates a new Hailstorm cluster based on the write params and provided resource information.
        //!
        //! \note Because HS format is quite complex when it comes to writing the creation is handled internally,
        //!   however chunk selection and data transforms are defined using the HailstormWriteParams struct.
        //!   This allows for the main routine to stay stable and handle all boilter plate regarding resource
        //!   iterations, but flexible enough to have full control on how resources are stored in the final cluster.
        //!
        //! \pre All three lists describing resource information are of the same size.
        //!
        //! \param [in] params Write params containing logic and detailed information on how to create a final HS cluster.
        //! \param [in] data A struct containg the data describing all resources to be stored in this cluster.
        //!
        //! \return Allocated memory ready to be written to a file. Returns an empty block if the operation fails.
        auto write_cluster(
            hailstorm::v1::HailstormWriteParams const& params,
            hailstorm::v1::HailstormWriteData const& data
        ) noexcept -> hailstorm::Memory;

        //! \brief Creates a new Hailstorm cluster based on the write params and provided resource information.
        //!
        //! \note This function requires the user to set all async functions to properly handle writing data.
        //!   Additionally there are no guarantees that write requests are in order. Always use the offset to write
        //!   data into it's expected location.
        //! \note Because HS format is quite complex when it comes to writing the creation is handled internally,
        //!   however chunk selection and data writing are defined using the HailstormAsyncWriteParams struct.
        //!   This allows for the main routine to stay stable and handle all boilter plate regarding resource
        //!   iterations, but flexible enough to have full control on how data is written.
        //!
        //! \pre All three lists describing resource information are of the same size.
        //!
        //! \param [in] params Write params containing logic and detailed information on how to create a final HS cluster.
        //! \param [in] data A struct containg the data describing all resources to be stored in this cluster.
        //!
        //! \return Allocated memory ready to be written to a file. Returns an empty block if the operation fails.
        bool write_cluster_async(
            hailstorm::v1::HailstormAsyncWriteParams const& params,
            hailstorm::v1::HailstormWriteData const& data
        ) noexcept;

        //! \brief Returns the total size necessary to store all path data with an prefix appended to each entry.
        //! \param [in] paths_info Path information coming from a hailstorm header.
        //! \param [in] resource_count Number of resources this prefix will be appended to.
        //! \param [in] prefix The prefix to be appended to each path.
        //! \return Size in bytes required for the path buffer to store all entries with the given prefix. \see prefix_resource_paths.
        auto prefixed_resource_paths_size(
            hailstorm::v1::HailstormPaths const& paths_info,
            uint32_t resource_count,
            std::string_view prefix
        ) noexcept -> size_t;

        //! \brief Updates resource paths stored in memory with enough with a given prefix and updates the given resource list.
        //! \note The memory needs to be big enoguh to store all paths with the appended prefix, \see prefixed_resource_paths_size.
        //!
        //! \warning The passed buffer is required to have paths data at the start.
        //! \warning The operation will update the buffer contents and resource information.
        //! \warning It's is REQUIRED that this function works on the entire resource list.
        //!
        //! \param [in] paths_info Path information coming from a hailstorm header.
        //! \param [in] resources List of ALL resources that are part of the paths buffer.
        //! \param [in] resources The memory block containing path data and additional space to contain all prefixed entries.
        //! \param [in] prefix The prefix to be appended to each path.
        //! \return 'true' If the update was successful and all data could be updated.
        bool prefix_resource_paths(
            hailstorm::v1::HailstormPaths const& paths_info,
            std::span<hailstorm::v1::HailstormResource> resources,
            hailstorm::Memory paths_data,
            std::string_view prefix
        ) noexcept;

        //! \brief The data to be provided when writing a hailstorm cluster.
        struct HailstormWriteData
        {
            //! \brief A list of 'paths' for each entry in the `data` array.
            //! \note A path can be any string identifier for resources. However it's recommended to follow the URI format.
            std::span<std::string_view const> paths;

            //! \brief A list of data blocks to be written to the hailstorm cluster.
            //! \note This list is required to be the size of 'ids'.
            std::span<hailstorm::Data const> data;

            //! \brief A list of metadata entries to be writted to the hailstorm cluster.
            //! \note If 'metadata_mapping' is empty this list is required to be the size of 'ids'.
            //!
            //! \details This list may be smaller than the number of resources, if and only if, the 'metadata_mapping' is
            //!   provided and is equal to the number of written resources. This allows to store a single metadata entry
            //!   for multiple resources to reduce the total size of the file and runtime footprint.
            std::span<hailstorm::Data const> metadata;

            //! \brief A list of indices referencing one of the Metadata objects in the 'metadata' list.
            //! \note If provided, this list is required to be the size of 'ids'.
            std::span<uint32_t const> metadata_mapping;

            //! \brief Application custom values.
            uint32_t custom_values[4];
        };

        //! \brief Used to select chunks for resource metadata and data destinations.
        //! \note If metadata mapping is used, the value of 'meta_chunk' may be ignored in favor for the shared metadata object.
        struct HailstormWriteChunkRef
        {
            //! \brief Chunk index where data should be stored.
            uint16_t data_chunk;

            //! \brief Chunk index where data should be stored.
            uint16_t meta_chunk;

            //! \brief If 'true' a new chunk will be created and the 'data_chunk' value will be used as a base.
            bool data_create = false;

            //! \brief If 'true' a new chunk will be created and the 'meta_chunk' value will be used as a base.
            bool meta_create = false;
        };

        //! \brief A description of the 'write' operation for a Hailstorm cluster. Allows to partially control how
        //!   the resulting hailstorm cluster looks.
        //! \attention Please make sure you properly fill 'required' members or use default values.
        struct HailstormWriteParams
        {
            //! \brief Function signature for a chunk selection heuristic. The returned structure will be used
            //!   to select or create a chunk for the given resource information.
            //!
            //! \attention If creation of new chunks was requested, the resource will be checked again.
            //!   In addition, the returned chunk indices will be used to serve as 'base_chunk' for the create heuristic.
            //!
            //! \param [in] resource_meta Metadata associated with the given resource.
            //! \param [in] resource_data Object data associated with the given resource.
            //! \param [in] chunks List of already existing chunks.
            //! \param [in] userdata Value passed by the user using the 'HailstormWriteParams' struct.
            //! \return Indices for the data and metadata pair given.
            using ChunkSelectFn = auto(
                hailstorm::Data resource_meta,
                hailstorm::Data resource_data,
                std::span<hailstorm::v1::HailstormChunk const> chunks,
                void* userdata
            ) noexcept -> hailstorm::v1::HailstormWriteChunkRef;

            //! \brief Function signature for chunk creation heuristic. The returned chunk definition will be used
            //!   to create a new chunk.
            //!
            //! \attention If the writing started without a list of pre-defined chunks, this function is called once
            //!   to define the first chunk. The 'base_chunk' variable in such a case is entierly empty.
            //!
            //! \param [in] resource_meta Metadata associated with the given resource.
            //! \param [in] resource_data Object data associated with the given resource.
            //! \param [in] base_chunk The base definition for the new chunk, based on the index returned from the 'select' function.
            //! \param [in] userdata Value passed by the user using the 'HailstormWriteParams' struct.
            //! \return A chunk definition that will be used to start a new chunk in the cluster.
            using ChunkCreateFn = auto(
                hailstorm::Data resource_meta,
                hailstorm::Data resource_data,
                hailstorm::v1::HailstormChunk base_chunk,
                void* userdata
            ) noexcept -> hailstorm::v1::HailstormChunk;

            //! \brief Function signature for writing resource data to a chunk.
            //!
            //! \note This function is called for each resource in the 'HailstormWriteData' struct.
            //!
            //! \param [in] write_data Data passed to the write procedure for easy access.
            //! \param [in] resource_index Index of the resource requested to be written.
            //! \param [in] memory Memory block where the resource data should be written.
            //! \param [in] userdata Value passed by the user using the 'HailstormWriteParams' struct.
            //! \return 'true' if the write was successful, otherwise 'false'.
            using ResouceWriteFn = auto(
                hailstorm::v1::HailstormWriteData const& write_data,
                uint32_t resource_index,
                hailstorm::Memory memory,
                void* userdata
            ) noexcept -> bool;

            //! \brief Function signature for writing custom chunk data.
            //!
            //! \note Called for each chunk of type '0' in the 'HailstormWriteData' struct.
            //! \note Called after all resources have been written.
            //!
            //! \param [in] write_data Data passed to the write procedure for easy access.
            //! \param [in] chunk Chunk information for the chunk requested to be written.
            //! \param [in] memory Memory block where the chunk data should be written.
            //! \param [in] userdata Value passed by the user using the 'HailstormWriteParams' struct.
            //! \return 'true' if the write was successful, otherwise 'false'.
            using CustomChunkWriteFn = auto(
                hailstorm::v1::HailstormWriteData const& write_data,
                hailstorm::v1::HailstormChunk const& chunk,
                hailstorm::Memory memory,
                void* userdata
            ) noexcept -> bool;

            //! \brief Allocator object used to handle various temporary allocations.
            hailstorm::Allocator& temp_alloc;

            //! \brief Allocator object used to allocate the final memory for writing.
            //! \note Unused during if using asynchronous write.
            hailstorm::Allocator& cluster_alloc;

            //! \brief List of initial chunks to be part of the cluster.
            //! \attention This list is not curated and empty chunks may end up in the cluster.
            //! \note This is the only allowed way to provide AppSpecific chunks where chunk type is '0'.
            std::span<hailstorm::v1::HailstormChunk const> initial_chunks;

            //! \brief Estimated number of chunks in the final cluster, allows to minimize temporary allocations.
            uint32_t estimated_chunk_count = 0;

            //! \brief Please see documentation of ChunkSelectFn.
            ChunkSelectFn* fn_select_chunk;

            //! \brief Please see documentation of ChunkCreateFn.
            ChunkCreateFn* fn_create_chunk;

            //! \brief Please see documentation of ResouceWriteFn.
            ResouceWriteFn* fn_resource_write;

            //! \brief Please see documentation of CustomChunkWriteFn.
            //! \note This function is only called if there are chunks of type '0' in the cluster.
            CustomChunkWriteFn* fn_custom_chunk_write;

            //! \brief User provided value, can be anything, passed to function routines.
            void* userdata;
        };

        //! \brief A description of a async write operation for a Hailstorm cluster. Allows to partially control how
        //!   the resulting hailstorm cluster looks.
        //! \note This description is an extension of the regular write params description.
        //! \note All async function calls need to be provided by the user.
        struct HailstormAsyncWriteParams
        {
            HailstormWriteParams base_params;

            using AsyncOpenFn = auto(
                size_t final_cluster_size,
                void* userdata
            ) noexcept -> bool;

            using AsyncWriteHeaderFn = auto(
                hailstorm::Data header_data,
                size_t write_offset,
                void* userdata
            ) noexcept -> bool;

            using AsyncWriteDataFn = auto(
                hailstorm::v1::HailstormWriteData const& write_data,
                uint32_t resource_index,
                size_t write_offset,
                void* userdata
            ) noexcept -> bool;

            using AsyncwriteCustomChunkFn = auto(
                hailstorm::v1::HailstormWriteData const& write_data,
                hailstorm::v1::HailstormChunk const& chunk,
                size_t write_offset,
                void* userdata
            ) noexcept -> bool;

            using AsyncCloseFn = auto(
                void* userdata
            ) noexcept -> bool;

            AsyncOpenFn* fn_async_open;
            AsyncWriteHeaderFn* fn_async_write_header;
            AsyncWriteDataFn* fn_async_write_metadata;
            AsyncWriteDataFn* fn_async_write_resource;
            AsyncwriteCustomChunkFn* fn_async_write_custom_chunk;
            AsyncCloseFn* fn_async_close;

            //! \brief User provided value, can be anything, passed to function routines.
            void* async_userdata;
        };

        //! \brief Default heuristic for creating chunks.
        //! \note This function is suboptimal, it always returns Mixed chunk types with Regular persitance strategy.
        //!   Each chunk is at most 32_MiB big and files bigger than that will be stored in exclusive chunks.
        inline auto default_chunk_create_logic(
            hailstorm::Data resource_meta,
            hailstorm::Data resource_data,
            hailstorm::v1::HailstormChunk base_chunk_info,
            void* /*userdata*/
        ) noexcept -> hailstorm::v1::HailstormChunk
        {
            // If empty, set the first chunk so later it can be used as the base chunk
            if (base_chunk_info.size == 0)
            {
                base_chunk_info.align = 8;
                base_chunk_info.is_compressed = false;
                base_chunk_info.is_encrypted = false;
                base_chunk_info.persistance = 1; // Persistance is regular by default
                base_chunk_info.type = 3; // Type is mixed by default
                base_chunk_info.size = 32 * Constant_1MiB; // By default chunks should be at max 32MiB
            }

            // Calculate chunk size (meta + data)
            // TODO: Assert(resource_data.align <= 8)
            uint64_t const final_size = resource_meta.size + resource_data.size;

            // Base chunk should always be 32_MiB unless the resources requires more
            if (final_size > (32 * Constant_1MiB))
            {
                base_chunk_info.size = final_size;
                base_chunk_info.align = (uint32_t) resource_data.align;
            }
            return base_chunk_info;
        }

        //! \note This function is suboptimal, it assumes all chunks are mixed and always assigns both
        //!   data and metadata to the last chunk. New chunks will be created if the selected chunk is to small.
        inline auto default_chunk_select_logic(
            hailstorm::Data const& /*resource_meta*/,
            hailstorm::Data /*resource_data*/,
            std::span<hailstorm::v1::HailstormChunk const> chunks,
            void* /*userdata*/
        ) noexcept -> hailstorm::v1::HailstormWriteChunkRef
        {
            // Always pick last chunk, if it's too small a 'create' chunk will be called and the select logic will be repeated.
            uint16_t const last_chunk = uint16_t(chunks.size() - 1);

            // Default selection only supports mixed chunks.
            // TODO: assert(chunks[last_chunk].type == 3);
            return {
                .data_chunk = last_chunk,
                .meta_chunk = last_chunk
            };
        }

    } // namespace v1

} // namespace hailstorm
