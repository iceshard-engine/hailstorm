# About
![TeamCity build status](https://teamcity.iceshard.net/app/rest/builds/buildType:id:IceShard_HailStormBuild/statusIcon.svg)

An attempt to make a resource package format that allows to efficiently load resources at runtime with additional information on how to load and keep such resources in memory.

Provides an API that allows to build packages synchronously or asynchronosuly.
> The Asynchronous API requires the user to properly handle write requests.

# License

This library is provided under the MIT license.
See [LICENSE](/LICENSE) file for details.

# Overview

The package format is separated into multiple sections allowing to quickly load and process available resource information without loading actual resource data or keeping the whole file in memory.

Streaming data from it should be the preffered way of usage. However, some data chunks might request to be always loaded in memory. In such cases, it's assumed that the data is crucial and accessed frequently.

## Format concepts

Provides high-level information on specific format sections.

### BaseHeader
> **Tags:** Stable, Base

Contains the formats `MAGIC` and `Version` and `HeaderSize` fields. The ABI should never change, even for major versions.

This header should be used to properly determine the ABI version of the data and the size of the version-specific header section.

### Header
> **Tags:** Overview, Version Specific,

Contains general information about the package and it's contents. The information can be grouped into the following:
* __Pack info:__
  * Is Expansion, Is Patch
  * Next Pack Offset (Total Size)
    * Multiple packs can be present in a single file. This field points to where the next header would be stored. This also provides the packs total-size
* __General data info:__
  * Num Chunks, Num Resources, Is Baked, Data Start Offset
* __Other:__
  * App Version
    * App version the pack was created with. This value shouldn't be used for picking logic paths at runtime, so apps are allowed to reuse it at their own discretion.
  * App Custom Values
    * App specific values that can contain additional pack specific information

### Chunk
> **Tags:** Data, Metadata, Version Specific

Contains information about a chunk of data that may contain resource data, resource metadata or app-specific data. Chunks should be used to group resource data that can be loaded and unloaded together to allow faster access at runtime.

For example, when loading a material it may consist of a shader and some textures. Storing them in the same chunk allows to load everything at once, and just point at the proper location during runtime. Once loaded into GPU memory the chunk can be released all at once.

> The `offset` and `size` fields of a chunk are using the decompressed / decrypted representation.

When loading a chunk the memory allocated needs to be aligned to the chunks preffered `align` value to avoid issues at runtime.

Other information available in chunks:
* __Chunk Type__ - Type of data does the chunk stores.
  * `AppSpecific/Undefined` - Data that cannot be defined or is specific to the application writing / readingt the pack.
  * `Data` - Raw resource data
  * `Metadata` - Key-Value like structure containing additional information about the resource.
  * `Mixed` - Both resource raw data and metadata are stored in this chunk.
* __Persistance__ - Suggested way of memory management at runtime
  * `File Temporary` - Only load the resource in question, Once unloaded data should be freed immediately.
  * `File Regular` - Only load the resource in question, keep loaded unless memory needs to be reclaimed.
  * `Chunk Load If Possible` - Load the whole chunk and keep in memory if possible.
  * `Chunk Force Load` - Load the whole chunk and keep in memory. Data in this chunks is crucial and accessed frequently.
* __Count Entries__ - How many entries are stored in this chunk.
* __App Custom Value__ - Similar to pack custom values, this is a application defined value and can be used for anything.

### Entry
> **Tags:** Resource, Version Specific

Entries contain information about a single resource and where the name, data and metadata is stored. Accessing that data requires access to specific chunks or paths info.

### PathsInfo
> **Tags:** Resource Name, Version Specific

A single block of data containing names of each stored resource. Each `Entry` has an offset and size pointing into this block that defines it's actual name.

# Quick API examples

The following section provides some basic examples on how to read and write Hailstorm packages.

## Reading package

```cpp

// Reads 'size' bytes of data into 'memory' from a file starting at 'file_offset'.
void read_from_file(FILE* file, void* memory, size_t size, size_t file_offset) { ... }

bool process_pack(FILE* file)
{
    hailstorm::HailstormHeaderBase base_header;
    read_from_file(file, &base_header, sizeof(base_header), 0);

    if (base_header.magic != hailstorm::Constant_HailstormMagic)
    {
        return false;
    }

    if (base_header.version != hailstorm::Constant_HailstormHeaderVersionV0)
    {
        return false;
    }

    void* memory_full_header = malloc(base_header.header_size);

    // Read again from file offset '0'
    read_from_file(file, memory_full_header, base_header.header_size, 0);

    hailstorm::Data const file_data{
        .location = memory_full_header,
        .size = base_header.size,
        .align = 8 /* 64bit assumed allignment, just an example */
    };

    // Read the file data and fill in a helper struct.
    hailstorm::HailstormData out_hailstorm_data;
    hailstorm::Result const result =

    // This function does NOT allocate, the 'out_hailstorm_data' struct is only valid as long as the backing memory 'file_data' is not released.
    hailstorm::read_header(file_data, out_hailstorm_data);

    if (result != hailstorm::Result::Success)
    {
        return false;
    }

    // Process the hailstorm header information...

    // Release the header data. Invalidates 'out_hailstorm_data' struct.
    free(memory_full_header);
}
```

## Writing package synchronously

Since writing a package is a bit more complex, even for the synchronous API's, it's not currently showcased in this repository.
