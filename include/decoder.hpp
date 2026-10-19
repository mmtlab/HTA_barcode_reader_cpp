/**
 * decoder.hpp
 *
 * Barcode decoding: a thin ZBar wrapper plus the preprocessing pipeline that
 * tries progressively stronger image transformations until a code is read.
 */
#pragma once

#include "barcode_types.hpp"

// Decodes all barcodes in an image (grayscale, BGR or BGRA) with ZBar.
std::vector<DecodedBarcode> decode_barcodes_with_zbar(const cv::Mat& image);

// Runs the preprocessing pipeline on a BGR crop and stops at the first decode.
//   max_decode_attempts : stop after this many steps (-1 = try all steps)
//   keep_attempted      : also return the images tried (DecodeResult::attempted)
DecodeResult decode_with_preprocessing(const cv::Mat& roi_bgr,
                                       int max_decode_attempts = -1,
                                       bool keep_attempted = false);
