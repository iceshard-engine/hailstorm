/// Copyright 2023 - 2023, Dandielo <dandielo@iceshard.net>
/// SPDX-License-Identifier: MIT

#pragma once
#include <hailstorm/hailstorm_types.hxx>

namespace hailstorm
{

    //! \brief A QWord value used to identify the Hailstorm format.
    static constexpr uint32_t Constant_HailstormMagic = 'ISHS';

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
            uint8_t _unused04b : 4;

            //! \brief Number of data chunks in this pack.
            uint32_t count_chunks;

            //! \brief Number of resources in this pack.
            uint32_t count_resources;

            //! \brief If greater than '0', the whole pack is sliced into chunks of aligned to the given size.
            //! \remarks The minimum value is at least 4KiB and needs to be a power of '2'. When an invalid value is specified writing will result with an error.
            //!
            //! \note This means that each chunk size is a multiple of the `pack_slice_alignment` value. The pack header and paths are also aligned to this value.
            //! \note The data layout:
            //!   * '[header]' - aligned to 'pack_slice_alignment'
            //!   * '[paths]' - aligned to 'pack_slice_alignment'
            //!   * '[chunks...]' - each chunk aligned to 'pack_slice_alignment'
            uint32_t pack_slice_alignment;

            //! \brief Unique pack identifier used for patch and extension packs.
            //! \details The identifier should be unique for base packs, however patch and expansion packs
            //!   need to set this ID to the base pack that is beeing updated.
            //! \remarks Each pack needs to have a unique ID. Values are application defined.
            uint32_t pack_id;

            //! \brief Integer value starting at '0' and increasing for each extension pack applied to a base pack.
            //! \note This value is '0' only for base packs.
            //! \example After applying two extension packs to a pack with id '1', a patch targeting this version
            //!     is requried to set 'pack_id' and 'pack_expansion_ver' to '1' and '2' respectively.
            //! \remarks Values need to be growing.
            uint16_t pack_expansion_ver;

            //! \brief Integer value starting at '0' and increasing for each patch pack applied to a base or extension pack.
            //! \note This value is '0' only for base and extension pack pairs.
            //! \example After applying two patches to (id:1, ext_ver:0) the 'patch_ver' is '2' for this pair, but the 'patch_ver' for (id:1, ext_ver:1) is still '0'.
            //! \remarks Values need to be growing.
            uint16_t pack_patch_ver;

            //! \brief Custom values available for application specific use.
            //! \note The array size may change in the future.
            uint32_t app_custom_values[2];
        };

        static_assert(sizeof(HailstormHeaderBase) == 16);
        static_assert(sizeof(HailstormHeader) - sizeof(HailstormHeaderBase) == 48);
        static_assert(sizeof(HailstormHeader) == 64);

        //! \brief Hailstorm path information. Optional, might not contain actuall data.
        //! \version HSC0-0.0.1
        struct HailstormPaths
        {
            uint32_t offset;
            uint32_t size;
        };

        static_assert(sizeof(HailstormPaths) == 8);
        static_assert(alignof(HailstormHeader) >= alignof(HailstormPaths));

        //! \brief Hailstorm chunk information used to optimize loading and keeping resources in memory.
        //! \version HSC0-0.0.1
        struct HailstormChunk
        {
            //! \brief Offset in file where chunk data is stored.
            uint64_t offset;

            //! \brief Total size of chunk data.
            uint64_t size;

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
            uint8_t persistance : 2;

            //! \brief Contains additional info in regards how data is stored and how it should be loaded.
            //! \note Flags
            //!   * 'None' - Regular independent chunk, load as a single block of data. (value: 0, default)
            //!   * 'Partial' - Data is part of a bigger resource spaning multiple chunks that needs to be loaded together. (value: 1)
            //!   * 'Streamed' - Data is part of a bigger resource spaning multiple chunks that can be loaded and unloaded independently. (value: 3, implies: 'Partial')
            //!   * '_Unused1' - (value: 4)
            //!   * '_Unused2' - (value: 8)
            uint8_t flags : 4;

            uint8_t _unused3B[3];

            //! \brief Custom value available for application specific use.
            //! \note This field may reused by the format in the future.
            uint32_t app_custom_value;

            //! \brief Number of entries stored in this chunk. Can be used to optimize runtime allocations.
            uint32_t count_entries;
        };

        static_assert(sizeof(HailstormChunk) == 32);

        // We ensure that the header alignment is at least the same as the chunk alignment
        //   after that we want to ensure that the paths struct will keep at least chunk alingment valid
        static_assert(alignof(HailstormHeader) >= alignof(HailstormChunk));
        static_assert((sizeof(HailstormPaths) % alignof(HailstormChunk)) == 0);

        //! \brief Hailstorm resource information, used to access resource related data.
        //! \version HSC0-0.0.1
        struct HailstormResource
        {
            //! \brief The chunk index at which resource data is stored.
            uint32_t chunk;

            //! \brief The chunk index at which resource metadata is stored.
            uint32_t meta_chunk;

            //! \brief The offset at which data is stored. This value is relative to the chunk it is stored in.
            uint32_t offset;

            //! \brief The size of the stored data.
            uint32_t size;

            //! \brief If compressed, this field contains the uncompressed size of the data. Otherwise it's the same as 'size'.
            uint32_t size_origin;

            //! \brief The offset at which metadata is stored. This value is relative to the meta_chunk it is stored in.
            uint32_t meta_offset;

            //! \brief The size of the stored metadata.
            uint32_t meta_size;

            //! \brief The offset at which path information is stored. This value is relative to the HailsormPaths offset member.
            //! \warning Only valid on 'base' and 'extension' packs.
            uint32_t path_offset;

            //! \brief The size of the path.
            //! \note A value of '0' is allowed in 'expansion' and 'patch' packs, which then requires a pack reader to use 'pack_resource_index' instead of
            //!   'path_offset' to reference the actual resource.
            uint16_t path_size;

            //! \brief Compression flags to determine the used algorithm. Some this value may be entirely app specific if the 'AppSpecific' flag is set.
            //! \note This library standarizes the following compression flags / algorithms.
            //!   * 'Uncompressed' - (value: 0)
            //!   * 'ZLib' - Common compression library for data-streams. (value: 1)
            //!   * 'Zstd' - Real-time compression algorithm developed by Facebook. (value: 2)
            //!   * 'QOI' - The data is compressed as "Quite OK Image Format" (value: 3)
            //!   * 'QOA' - The data is compressed as "Quite OK Audio Format" (value: 4)
            //!   * 'AppSpecific' (flag) - Application specific compression format. When this flag is set, the values between 1-15 are application specific. (value: 16)
            //!
            //! \remarks The 'standarized' formats where chosen based on my own needs and biases, but you are free to use the 'AppSpecific' flag and
            //!   do whatever you want!
            uint8_t compression_type : 5;

            //! \brief The compression level (if supported by the given format).
            uint8_t compression_level : 3;

            //! \brief Simple value param to be used when decompressing.
            //! \note This param is not used by any of the standarized compression formats. If you wish to pass values for any of the formats described under 'compresion_type' field (ZLib, Zstd, etc...)
            //!   you should use the 'AppSpecific' since it could be incompatible with the common defaults for each library.
            uint8_t compression_param;
        };

        static_assert(sizeof(HailstormResource) == 36);
        static_assert(alignof(HailstormChunk) >= alignof(HailstormResource));

        //! \brief Struct providing access to Hailstorm header data wrapped in a more accessible way.
        //! \note This struct can be filled using the hailstorm::read_header function.
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

} // namespace hailstorm
