/**
 * barcode_types.hpp
 *
 * Data structures shared by all modules:
 *   - DecodedBarcode : one symbol found by ZBar
 *   - DecodeResult   : output of the preprocessing + decoding pipeline
 *   - Args           : command-line options (with their default values)
 */
#pragma once

#include <climits>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

// One barcode found by ZBar.
struct DecodedBarcode {
    std::string type;  // symbology, e.g. "CODE128", "EAN13", "QRCODE"
    std::string data;  // decoded text
    int quality = 0;   // ZBar quality score
};

// Output of decode_with_preprocessing().
struct DecodeResult {
    std::vector<DecodedBarcode> decoded;  // empty if nothing was read
    std::string pipeline_name;            // preprocessing step that worked, e.g. "gray_clahe"
    cv::Mat winning_image;                // image produced by that step
    std::vector<std::pair<std::string, cv::Mat>> attempted;  // (name, image) per step, debug only
};

// ROI meaning "the whole frame": it is clamped to the real frame size before use.
inline const cv::Rect kWholeImageRoi(0, 0, INT_MAX, INT_MAX);

// Command-line options. The values below are the defaults.
struct Args {
    int camera_index = 0;           // --camera-index

    // Region of interest
    cv::Rect roi = kWholeImageRoi;  // --roi X Y W H
    bool roi_set    = false;        // true if --roi was given
    bool select_roi = false;        // --select-roi

    bool debug = false;             // --debug: show the preprocessing image

    // Real-time tuning
    int process_every     = 2;      // --process-every: decode one frame every N
    int quick_candidates  = 4;      // --quick-candidates: steps tried on a quick decode
    int full_search_every = 10;     // --full-search-every: every N decodes try all steps
};
