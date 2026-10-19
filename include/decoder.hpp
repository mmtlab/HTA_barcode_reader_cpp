/**
 * decoder.hpp
 *
 * Barcode decoding: a thin ZBar wrapper plus the preprocessing pipeline that
 * tries progressively stronger image transformations until a code is read.
 */
#pragma once

#include "barcode_types.hpp"

// Which preprocessing images to keep in DecodeResult::attempted.
enum class DebugImages {
    None,       // keep nothing (fastest)
    Attempted,  // keep the images that were actually tried
    All,        // build every step, even after a successful decode (for --save-debug)
};

// Decodes all barcodes in an image (grayscale, BGR or BGRA) with ZBar.
std::vector<DecodedBarcode> decode_barcodes_with_zbar(const cv::Mat& image);

// Runs the preprocessing pipeline on a BGR crop and stops at the first decode.
//   max_decode_attempts : stop after this many steps (-1 = try all steps)
//   debug_images        : which intermediate images to return
DecodeResult decode_with_preprocessing(const cv::Mat& roi_bgr,
                                       int max_decode_attempts = -1,
                                       DebugImages debug_images = DebugImages::None);
