/**
 * decoder.cpp
 *
 * decode_barcodes_with_zbar(): converts the image to 8-bit gray and scans it with ZBar.
 *
 * decode_with_preprocessing(): tries the steps below in order, from cheapest
 * to most expensive, and stops at the first one that ZBar can read:
 *    1. gray              plain grayscale
 *    2. gray_clahe        local contrast enhancement (CLAHE)
 *    3. denoise           bilateral filter on gray_clahe
 *    4. thr_adapt         adaptive (Gaussian) threshold
 *    5. thr_otsu          Otsu threshold
 *    6. morph_open_horiz  horizontal opening on thr_otsu (cleans the bars)
 *    7. <step>_x2/_x3     steps 1-5 enlarged 2x and 3x (small or blurred codes)
 * Parameters are in config/barcode_config.hpp.
 */
#include "decoder.hpp"

#include "barcode_config.hpp"

#include <opencv2/imgproc.hpp>
#include <zbar.h>

// Maps a ZBar symbol type to the name used in the output (same names as pyzbar).
static std::string zbar_type_name(zbar::zbar_symbol_type_t type) {
    switch (type) {
        case zbar::ZBAR_EAN2:        return "EAN2";
        case zbar::ZBAR_EAN5:        return "EAN5";
        case zbar::ZBAR_EAN8:        return "EAN8";
        case zbar::ZBAR_UPCE:        return "UPCE";
        case zbar::ZBAR_ISBN10:      return "ISBN10";
        case zbar::ZBAR_UPCA:        return "UPCA";
        case zbar::ZBAR_EAN13:       return "EAN13";
        case zbar::ZBAR_ISBN13:      return "ISBN13";
        case zbar::ZBAR_I25:         return "I25";
        case zbar::ZBAR_DATABAR:     return "DATABAR";
        case zbar::ZBAR_DATABAR_EXP: return "DATABAR_EXP";
        case zbar::ZBAR_CODABAR:     return "CODABAR";
        case zbar::ZBAR_CODE39:      return "CODE39";
        case zbar::ZBAR_PDF417:      return "PDF417";
        case zbar::ZBAR_QRCODE:      return "QRCODE";
        case zbar::ZBAR_CODE93:      return "CODE93";
        case zbar::ZBAR_CODE128:     return "CODE128";
        default:                     return "UNKNOWN";
    }
}

std::vector<DecodedBarcode> decode_barcodes_with_zbar(const cv::Mat& image) {
    // ZBar wants one contiguous 8-bit gray buffer ("Y800").
    cv::Mat gray = image;
    if (image.channels() == 3) cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    if (image.channels() == 4) cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    if (!gray.isContinuous()) gray = gray.clone();

    // A new scanner on every call costs little, is safe with several threads,
    // and the result does not depend on previously scanned images.
    zbar::ImageScanner scanner;
    scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 1);  // all symbologies

    zbar::Image zbar_image(gray.cols, gray.rows, "Y800", gray.data, gray.total());
    scanner.scan(zbar_image);

    std::vector<DecodedBarcode> found;
    for (auto symbol = zbar_image.symbol_begin(); symbol != zbar_image.symbol_end(); ++symbol) {
        if (symbol->get_type() == zbar::ZBAR_PARTIAL) continue;
        found.push_back({zbar_type_name(symbol->get_type()), symbol->get_data(),
                         symbol->get_quality()});
    }
    zbar_image.set_data(nullptr, 0);  // the buffer belongs to OpenCV, not to ZBar
    return found;
}

DecodeResult decode_with_preprocessing(const cv::Mat& roi_bgr,
                                       int max_decode_attempts,
                                       DebugImages debug_images) {
    // One CLAHE object per thread, created once (creating it is the slow part).
    static thread_local cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(
        config::kClaheClipLimit, cv::Size(config::kClaheTileSize, config::kClaheTileSize));

    DecodeResult result;
    const int attempt_limit = (max_decode_attempts > 0) ? max_decode_attempts : INT_MAX;
    int attempts = 0;

    // Tries one candidate image. Returns true when the pipeline can stop.
    auto try_candidate = [&](const std::string& name, const cv::Mat& candidate) {
        if (debug_images != DebugImages::None) result.attempted.emplace_back(name, candidate);

        if (result.decoded.empty() && attempts < attempt_limit) {
            ++attempts;
            result.decoded = decode_barcodes_with_zbar(candidate);
            if (!result.decoded.empty()) {
                result.pipeline_name = name;
                result.winning_image = candidate;
            }
        }
        const bool search_over = !result.decoded.empty() || attempts >= attempt_limit;
        return search_over && debug_images != DebugImages::All;
    };

    // 1. Grayscale
    cv::Mat gray;
    cv::cvtColor(roi_bgr, gray, cv::COLOR_BGR2GRAY);
    if (try_candidate("gray", gray)) return result;

    // 2. CLAHE: local contrast enhancement
    cv::Mat gray_clahe;
    clahe->apply(gray, gray_clahe);
    if (try_candidate("gray_clahe", gray_clahe)) return result;

    // 3. Bilateral filter: removes noise, keeps edges sharp
    cv::Mat denoised;
    cv::bilateralFilter(gray_clahe, denoised, config::kBilateralDiameter,
                        config::kBilateralSigmaColor, config::kBilateralSigmaSpace);
    if (try_candidate("denoise", denoised)) return result;

    // 4. Adaptive threshold: black/white with a local threshold (uneven lighting)
    cv::Mat thr_adapt;
    cv::adaptiveThreshold(denoised, thr_adapt, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY, config::kAdaptiveBlockSize, config::kAdaptiveOffset);
    if (try_candidate("thr_adapt", thr_adapt)) return result;

    // 5. Otsu threshold: black/white with one global threshold
    cv::Mat thr_otsu;
    cv::threshold(denoised, thr_otsu, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    if (try_candidate("thr_otsu", thr_otsu)) return result;

    // 6. Horizontal opening: removes small specks between the vertical bars
    cv::Mat morph_open;
    const cv::Mat kernel =
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(config::kMorphKernelWidth, 1));
    cv::morphologyEx(thr_otsu, morph_open, cv::MORPH_OPEN, kernel);
    if (try_candidate("morph_open_horiz", morph_open)) return result;

    // 7. Enlarged copies of steps 1-5 (skipped for large crops to save memory)
    const std::vector<std::pair<std::string, cv::Mat>> bases = {
        {"gray", gray}, {"gray_clahe", gray_clahe}, {"denoise", denoised},
        {"thr_adapt", thr_adapt}, {"thr_otsu", thr_otsu}};

    for (const auto& [name, base] : bases) {
        const int pixels = base.rows * base.cols;
        for (const int factor : {2, 3}) {
            const int max_pixels = (factor == 2) ? config::kMaxPixelsForUpscale2x
                                                 : config::kMaxPixelsForUpscale3x;
            if (pixels > max_pixels) continue;
            cv::Mat enlarged;
            cv::resize(base, enlarged, cv::Size(), factor, factor, cv::INTER_CUBIC);
            if (try_candidate(name + "_x" + std::to_string(factor), enlarged)) return result;
        }
    }
    return result;  // nothing decoded
}
