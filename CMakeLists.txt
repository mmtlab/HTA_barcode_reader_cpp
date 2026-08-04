cmake_minimum_required(VERSION 3.16)
project(barcode_reader CXX)

# ============================================================================
#  Standard C++17 richiesto per: std::filesystem, structured bindings,
#  std::optional, if-constexpr, std::async sliding-window.
#  C++17 required for: std::filesystem, structured bindings,
#  std::optional, if-constexpr, std::async sliding-window.
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # per clangd / for clangd

# ============================================================================
#  Dipendenze obbligatorie / Required dependencies
# ============================================================================

# ---------- OpenCV ----------
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui videoio)

# ---------- ZBar ----------
# Ricerca in ordine: vcpkg → pkg-config → find_library.
# Search order: vcpkg → pkg-config → find_library.
find_package(unofficial-zbar CONFIG QUIET)
if(unofficial-zbar_FOUND)
    set(ZBAR_TARGET unofficial::zbar::zbar)
    message(STATUS "ZBar found via vcpkg")
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ZBAR IMPORTED_TARGET zbar)
        if(ZBAR_FOUND)
            set(ZBAR_TARGET PkgConfig::ZBAR)
            message(STATUS "ZBar found via pkg-config")
        endif()
    endif()
    if(NOT DEFINED ZBAR_TARGET)
        find_library(ZBAR_LIBRARY NAMES zbar libzbar)
        find_path(ZBAR_INCLUDE_DIR NAMES zbar.h)
        if(ZBAR_LIBRARY AND ZBAR_INCLUDE_DIR)
            add_library(zbar_imported IMPORTED INTERFACE)
            set_target_properties(zbar_imported PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${ZBAR_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES      "${ZBAR_LIBRARY}")
            set(ZBAR_TARGET zbar_imported)
            message(STATUS "ZBar found via find_library: ${ZBAR_LIBRARY}")
        else()
            message(FATAL_ERROR
                "ZBar non trovata / ZBar not found.\n"
                "  Windows (vcpkg): vcpkg install zbar:x64-windows\n"
                "  Linux (apt):     sudo apt install libzbar-dev")
        endif()
    endif()
endif()

# ---------- Threads (std::async / std::future per batch parallelo) ----------
# Threads (std::async / std::future for parallel batch)
find_package(Threads REQUIRED)

# ============================================================================
#  Sorgenti / Sources
# ============================================================================
set(SOURCES
    main.cpp
    src/zbar_decode.cpp
    src/preprocessing.cpp
    src/display.cpp
    src/roi_utils.cpp
    src/cli_args.cpp
    src/modes/image_mode.cpp
    src/modes/batch_mode.cpp
    src/modes/realtime_mode.cpp
)

add_executable(barcode_reader ${SOURCES})

# Directory degli header / Header directory
target_include_directories(barcode_reader PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(barcode_reader PRIVATE
    ${OpenCV_LIBS}
    ${ZBAR_TARGET}
    Threads::Threads
)

# ============================================================================
#  Ottimizzazioni compilatore / Compiler optimizations
# ============================================================================
if(MSVC)
    # Windows: encoding UTF-8, disabilita macro min/max di Windows.
    # Windows: UTF-8 encoding, disable Windows min/max macros.
    target_compile_options(barcode_reader PRIVATE /utf-8 /O2)
    target_compile_definitions(barcode_reader PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    # Linux / macOS: ottimizzazione aggressiva + architettura nativa.
    # Linux / macOS: aggressive optimization + native architecture tuning.
    target_compile_options(barcode_reader PRIVATE
        $<$<CONFIG:Release>:-O3 -march=native -funroll-loops>
        $<$<CONFIG:Debug>:-O0 -g>
    )

    # Link-Time Optimization (LTO) per build Release.
    # Link-Time Optimization (LTO) for Release builds.
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_supported OUTPUT lto_output)
    if(lto_supported)
        set_property(TARGET barcode_reader PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        message(STATUS "LTO abilitato per Release / LTO enabled for Release")
    else()
        message(STATUS "LTO non supportato / LTO not supported: ${lto_output}")
    endif()
endif()

if(WIN32)
    target_link_libraries(barcode_reader PRIVATE user32)
endif()

# ============================================================================
#  Dipendenze opzionali / Optional dependencies
# ============================================================================

# ---------- Pylon SDK (camera Basler) ----------
option(USE_PYLON "Abilita supporto camera Basler via Pylon SDK / Enable Basler camera support" OFF)
if(USE_PYLON)
    find_package(pylon QUIET)
    if(pylon_FOUND)
        target_compile_definitions(barcode_reader PRIVATE HAS_PYLON)
        target_link_libraries(barcode_reader PRIVATE pylon::pylon)
        message(STATUS "Pylon SDK trovato - supporto Basler abilitato / Pylon SDK found - Basler support enabled")
    else()
        message(WARNING "Pylon SDK non trovato. Supporto Basler disabilitato. / Pylon SDK not found. Basler support disabled.")
    endif()
endif()

# ---------- libcamera (Arducam / RPi via GStreamer) ----------
option(USE_LIBCAMERA "Abilita supporto libcamera (Linux/RPi, richiede GStreamer in OpenCV) / Enable libcamera support" OFF)
if(USE_LIBCAMERA)
    target_compile_definitions(barcode_reader PRIVATE HAS_LIBCAMERA)
    message(STATUS "Supporto libcamera abilitato (richiede OpenCV con GStreamer) / libcamera support enabled (requires OpenCV with GStreamer)")
endif()

# ============================================================================
#  Riepilogo configurazione / Configuration summary
# ============================================================================
message(STATUS "")
message(STATUS "=== barcode_reader build configuration ===")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenCV:       ${OpenCV_VERSION}")
message(STATUS "  ZBar target:  ${ZBAR_TARGET}")
message(STATUS "  Pylon:        ${USE_PYLON}")
message(STATUS "  libcamera:    ${USE_LIBCAMERA}")
message(STATUS "==========================================")
message(STATUS "")
