# Build script of barcode_reader (CMake >= 3.16, C++17).
#
#   Linux  : cmake -B build -S . && cmake --build build -j
#   Windows: cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
#            cmake --build build --config Release
#
# Option: -DNATIVE_CPU=OFF (portable binary, without -march=native).
# See README.md for details.

cmake_minimum_required(VERSION 3.16)
project(barcode_reader CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for clangd / IDEs

# Release build unless the user chooses otherwise (single-config generators only).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(NATIVE_CPU "Optimise for this computer's CPU (GCC/Clang)"         ON)

# ---------------------------------------------------------------------------
# Required libraries
# ---------------------------------------------------------------------------
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio)

# ZBar has no CMake package: use pkg-config (Linux), otherwise search the
# library and header directly (Windows/vcpkg: the vcpkg toolchain adds its folders).
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ZBAR QUIET IMPORTED_TARGET zbar)
endif()
if(ZBAR_FOUND)
    set(ZBAR_TARGET PkgConfig::ZBAR)
else()
    find_library(ZBAR_LIBRARY NAMES zbar libzbar)
    find_path(ZBAR_INCLUDE_DIR NAMES zbar.h)
    if(NOT ZBAR_LIBRARY OR NOT ZBAR_INCLUDE_DIR)
        message(FATAL_ERROR "ZBar not found.\n"
                            "  Linux:   sudo apt install libzbar-dev\n"
                            "  Windows: vcpkg install mchehab-zbar:x64-windows")
    endif()
    add_library(zbar_library INTERFACE IMPORTED)
    set_target_properties(zbar_library PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ZBAR_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES      "${ZBAR_LIBRARY}")
    set(ZBAR_TARGET zbar_library)
endif()

# ---------------------------------------------------------------------------
# Program
# ---------------------------------------------------------------------------
add_executable(barcode_reader
    main.cpp
    src/cli_args.cpp
    src/decoder.cpp
    src/display.cpp
    src/webcam_mode.cpp
)
target_include_directories(barcode_reader PRIVATE include config ${OpenCV_INCLUDE_DIRS})
target_link_libraries(barcode_reader PRIVATE ${OpenCV_LIBS} ${ZBAR_TARGET})

if(MSVC)
    target_compile_options(barcode_reader PRIVATE /utf-8)  # source files are UTF-8
    target_compile_definitions(barcode_reader PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(NATIVE_CPU)
    target_compile_options(barcode_reader PRIVATE $<$<CONFIG:Release>:-march=native>)
endif()

# Link-time optimisation for Release builds, when the compiler supports it.
include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_MESSAGE)
if(LTO_SUPPORTED)
    set_property(TARGET barcode_reader PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

message(STATUS "barcode_reader: OpenCV ${OpenCV_VERSION}, ZBar ${ZBAR_TARGET}, "
               "LTO ${LTO_SUPPORTED}")
