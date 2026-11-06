# Wuffs, a fast and memory-safe image decoder (Apache-2.0 OR MIT), used here for PNG
# https://github.com/google/wuffs/blob/v0.4.0-alpha.12/release/c/wuffs-v0.4.c
add_library(wuffs STATIC wuffs.cpp)
target_include_directories(wuffs SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
# Only the modules the PNG decoder and the C++ image API need
target_compile_definitions(wuffs PUBLIC
    WUFFS_CONFIG__MODULES
    WUFFS_CONFIG__MODULE__ADLER32
    WUFFS_CONFIG__MODULE__AUX__BASE
    WUFFS_CONFIG__MODULE__AUX__IMAGE
    WUFFS_CONFIG__MODULE__BASE
    WUFFS_CONFIG__MODULE__CRC32
    WUFFS_CONFIG__MODULE__DEFLATE
    WUFFS_CONFIG__MODULE__PNG
    WUFFS_CONFIG__MODULE__ZLIB
)
target_compile_options(wuffs PRIVATE -w -O3)
set_target_properties(wuffs PROPERTIES POSITION_INDEPENDENT_CODE ON)
