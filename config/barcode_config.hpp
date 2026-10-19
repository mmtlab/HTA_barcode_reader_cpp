/**
 * barcode_config.hpp
 *
 * Compile-time settings of the barcode reader, collected in one place.
 * Change a value here and rebuild; no other file needs to be edited.
 * Command-line defaults (e.g. --process-every) are in include/barcode_types.hpp.
 */
#pragma once

#include <array>
#include <string_view>

namespace config {

// ---------------------------------------------------------------------------
// Preprocessing pipeline (see src/decoder.cpp)
// ---------------------------------------------------------------------------
constexpr double kClaheClipLimit        = 2.0;  // CLAHE contrast limit
constexpr int    kClaheTileSize         = 8;    // CLAHE grid: 8 x 8 tiles
constexpr int    kBilateralDiameter     = 7;    // bilateral filter neighbourhood [px]
constexpr double kBilateralSigmaColor   = 50.0;
constexpr double kBilateralSigmaSpace   = 50.0;
constexpr int    kAdaptiveBlockSize     = 31;   // adaptive threshold window [px], odd
constexpr double kAdaptiveOffset        = 5.0;  // constant subtracted from the local mean
constexpr int    kMorphKernelWidth      = 7;    // horizontal opening kernel: 7 x 1 px

// Upscaled candidates are skipped for large crops to limit memory use.
constexpr int kMaxPixelsForUpscale2x = 4 * 1024 * 1024;  // ROI <= 4 Mpx -> try x2
constexpr int kMaxPixelsForUpscale3x = 2 * 1024 * 1024;  // ROI <= 2 Mpx -> try x3

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
// Used on Linux/macOS, where the screen size is not queried.
constexpr int kFallbackScreenWidth  = 1920;  // [px]
constexpr int kFallbackScreenHeight = 1080;  // [px]

// Debug grid: one tile per preprocessing candidate.
constexpr int kDebugTileWidth   = 320;  // [px]
constexpr int kDebugTileHeight  = 180;  // [px]
constexpr int kDebugGridColumns = 4;

// ---------------------------------------------------------------------------
// Batch mode
// ---------------------------------------------------------------------------
// File extensions read by --folder (compared in lower case).
constexpr std::array<std::string_view, 7> kImageExtensions = {
    ".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".tif", ".webp"};

}  // namespace config
