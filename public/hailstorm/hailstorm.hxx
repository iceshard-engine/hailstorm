/// Copyright 2023 - 2023, Dandielo <dandielo@iceshard.net>
/// SPDX-License-Identifier: MIT

#pragma once
#include <hailstorm/hailstorm_types.hxx>

namespace hailstorm
{

    //! \brief A word value used to identify the Hailstorm format.
    static constexpr uint64_t Constant_HailstormMagic = 'ISHS';

    //! \brief A word value used to identify the Hailstorm format specification version.
    static constexpr uint32_t Constant_HailstormHeaderVersionV0 = 'HSC0';

    //! \brief A base header, always present in any Hailstorm version. Allows to properly select the API version
    //!   and the total size of header data.
    //! \note As long as the whole header size is loaded into memory, all header values, regardless of the version,
    //!   are accessible.
    struct HailstormHeaderBase
    {
        //! \brief Magic value, selected once and never changes for this format. Use for validation / format detection.
        uint32_t magic;

        //! \brief The current version of the data format. This may change but will rarely happen.
        //! \note Differences between header versions might be breaking so each version should be handled separately.
        uint32_t header_version;

        //! \brief The total size of header data. Allows to load only most important parts of a hailstorm file.
        //! \note The data size to be loaded allows to access all information about available resources.
        //! \note This does not include 'paths' data of resource.
        uint64_t header_size;
    };

    namespace v1
    {

        //! \brief Hailstorm header for version HSC0-X.Y.Z
        struct HailstormHeader : HailstormHeaderBase
        {
            //! \brief The next hailstorm header chunk. Contains the 'total size' for the whole pack defined by this header.
            //! \note There might be no data availble, meaning, this was the last header in the chain.
            //! \note The next header needs to be handled simillary to the original header. Which means reading the base header, and then the header size.
            uint64_t offset_next;

            //! \brief The offset at which actual file data and metadata values are stored.
            //! \note Using this value for 'header_size' instead allows to also load all 'paths' for resources.
            uint64_t offset_data;

            //! \brief The engine version on which this data was created. May introduce minor change to the header format, but is not allowed to change the main headers ABI.
            //! \note All other HS header types (chunk, resource, resource_id, paths) may introduce ABI breaking changes.
            uint8_t version[3];

            //! \brief ALL chunk data is encrypted separately. Load the whole file and decrypt it before reading.
            uint8_t is_encrypted : 1;

            //! \brief This is an expansion data pack and does only contain patched or additional game/program data.
            uint8_t is_expansion : 1;

            //! \brief This is an patch pack and does only contain updated versions of existing resources.
            //! \note Patch packs MAY not provide paths information, instead 'resource.path_offset' contains the resource
            //!   index that it replaces. The 'resource.path_size' field is unused and the value is undefined.
            uint8_t is_patch : 1;

            //! \brief The data stored in this pack is pre-baked and can be consumed directly by most engine systems.
            uint8_t is_baked : 1;

            //! \brief Reserved for future use.
            //! \version HSC0-0.0.1
            uint8_t _unused05b : 4;

            //! \brief Number of data chunks in this pack.
            uint16_t count_chunks;

            //! \brief Number of resources in this pack.
            uint16_t count_resources;

            //! \brief Unique pack identifier used for patch and extension packs.
            //! \details The identifier should be unique of base packs, however patch and expansion packs
            //!   need to set this ID to the base pack that is beeing updated.
            //! \remarks Each pack needs to have a unique ID. Values are application defined.
            uint32_t pack_id;

            //! \brief Integer value starting at '1' and increasing by one for each extension and patch pack applied to a base pack.
            //! \note This value is always '0' for base packs. The pack order for patch and extension packs is independent.
            //! \example After applying 1 extension pack and 4 patch packs, the next pack orders will be '2' and '5' respectively.
            //! \remarks Values need to be growing.
            uint32_t pack_order;

            //! \brief Custom values available for application specific use.
            uint32_t app_custom_values[4];
        };

        static_assert(sizeof(HailstormHeaderBase) == 16);
        static_assert(sizeof(HailstormHeader) - sizeof(HailstormHeaderBase) == 48);
        static_assert(sizeof(HailstormHeader) == 64);

        //! \brief Hailstorm path information. Optional, might not contain actuall data.
        //! \version HSC0-0.0.1
        struct HailstormPaths
        {
            uint64_t offset;
            uint64_t size;
        };

        static_assert(sizeof(HailstormPaths) == 16);
        static_assert(alignof(HailstormHeader) >= alignof(HailstormPaths));

        //! \brief Hailstorm chunk information used to optimize loading and keeping resources in memory.
        //! \version HSC0-0.0.1
        struct HailstormChunk
        {
            //! \brief Offset in file where chunk data is stored.
            uint64_t offset;

            //! \brief Total size of chunk data.
            uint64_t size;

            //! \brief The size of the stored data when uncompressed and/or decrypted. Use this value
            //!   to allocate the final runtime memory block.
            //! \note Value is equal to 'size' if 'is_compressed == 0' and 'is_encrypted == 0'
            uint64_t size_origin;

            //! \brief Alignment requirements of the data stored in the chunk.
            //! \note This requirements is applied to each resource, however loading the whole chunk with the given alignment
            //!   ensures all resources data is stored at the proper alignment.
            uint32_t align;

            //! \brief The type of data stored in this chunk. One of: AppSpecific/Undefined = 0, Metadata = 1, FileData = 2, Mixed = 3
            //! \note When a chunk has 'AppSpecific' data, it's contents is undefined by the format.
            uint8_t type : 2;

            //! \brief The preffered loading strategy. One of: Temporary = 0, Regular = 1, LoadIfPossible = 2, LoadAlways = 3
            //! \note Persistance details:
            //!   * 'temporary' - used for one-use files that can be released soon after. (value: 0)
            //!   * 'regular' - used for on-demand loading, but can be unloaded if necessary and unused. (value: 1)
            //!   * 'load if possible' - used for common resources files that allow to reduce loading times in various locations. (value: 2)
            //!   * 'force-load' - used for resources that are accessed all the time and/or should never be reloaded. (value: 3)
            uint8_t persistance : 4;

            //! \brief The chunk data is encrypted separately. Load the whole chunk and decrypt it before reading.
            //! \note If data was also compressed, this step is done BEFORE decompressing the data.
            uint8_t is_encrypted : 1;

            //! \brief The chunk data needs to be decompressed before reading.
            //! \note If data was also encrypted, this step is done AFTER decrypting the data.
            uint8_t is_compressed : 1;

            //! \brief Custom value available for application specific use.
            //! \note This field may reused by the format in the future.
            uint8_t app_custom_value;

            //! \brief Number of entries stored in this chunk. Can be used to optimize runtime allocations.
            uint16_t count_entries;
        };

        static_assert(sizeof(HailstormChunk) == 32);
        static_assert(alignof(HailstormPaths) >= alignof(HailstormChunk));

        //! \brief Hailstorm resource information, used to access resource related data.
        //! \version HSC0-0.0.1
        struct HailstormResource
        {
            //! \brief The chunk index at which resource data is stored.
            uint16_t chunk;

            //! \brief The chunk index at which resource metadata is stored.
            uint16_t meta_chunk;

            //! \brief The offset at which data is stored. This value is relative to the chunk it is stored in.
            uint32_t offset;

            //! \brief The size of the stored data.
            uint32_t size;

            //! \brief The offset at which metadata is stored. This value is relative to the meta_chunk it is stored in.
            uint32_t meta_offset;

            //! \brief The size of the stored metadata.
            uint32_t meta_size;

            union
            {
                //! \brief The offset at which path information is stored. This value is relative to the HailsormPaths offset member.
                //! \warning Only valid on 'base' and 'extension' packs.
                uint32_t path_offset;

                //! \brief An absolute resource index of the resource to be replaced.
                //! \note When a patch pack is created it requires access to all base and extensions packs with 'pack_id' and 'pack_order'
                //!   set properly to calculate this 'patch_resource_index' value.
                //! \example If we have a pack (res_count = 30) with two extensions (res_count = 7), a patch to the second resource
                //!   in the second extension pack would set 'patch_resource_index' to '38'. Indexing starts at '0' so '30 + 7 + 1'.
                //! \warning Only valid on 'patch' packs.
                uint32_t patch_resource_index;
            };

            //! \brief The size of the path.
            uint32_t path_size;
        };

        static_assert(sizeof(HailstormResource) == 28);
        static_assert(alignof(HailstormChunk) >= alignof(HailstormResource));

        //! \brief Struct providing access to Hailstorm header data wrapped in a more accessible way.
        //! \note This struct can be filled used some of the hailstorm functions.
        struct HailstormData
        {
            hailstorm::v1::HailstormHeader header;
            std::span<hailstorm::v1::HailstormChunk const> chunks;
            std::span<hailstorm::v1::HailstormResource const> resources;
            hailstorm::v1::HailstormPaths paths;
            hailstorm::Data paths_data;
        };

        struct HailstormReadParams;
        struct HailstormWriteParams;
        struct HailstormWriteData;
        struct HailstormAsyncWriteParams;

    } // namespace v1


    using HailstormHeader = v1::HailstormHeader;
    using HailstormPaths = v1::HailstormPaths;
    using HailstormChunk = v1::HailstormChunk;
    using HailstormResource = v1::HailstormResource;
    using HailstormData = v1::HailstormData;

} // namespace ice
