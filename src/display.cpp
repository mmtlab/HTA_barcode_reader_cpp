/**
 * display.cpp
 *
 * Drawing and window helpers used by all modes (see include/display.hpp).
 * Colours are OpenCV BGR triplets.
 */
#include "display.hpp"

#include "barcode_config.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

static const cv::Scalar kGreen(0, 255, 0), kRed(0, 0, 255), kOrange(0, 200, 255);

// Screen size in pixels: queried on Windows, fixed fallback elsewhere.
static cv::Size screen_size() {
#ifdef _WIN32
    const int width = GetSystemMetrics(SM_CXSCREEN), height = GetSystemMetrics(SM_CYSCREEN);
    if (width > 0 && height > 0) return {width, height};
#endif
    return {config::kFallbackScreenWidth, config::kFallbackScreenHeight};
}

// Shrinks an image so that it fits on screen. Returns the scale used (1.0 = unchanged).
static double fit_to_screen(const cv::Mat& image, cv::Mat& fitted, int margin_px) {
    const cv::Size screen = screen_size();
    const double max_w = std::max(200, screen.width - margin_px);
    const double max_h = std::max(200, screen.height - margin_px);
    const double scale = std::min({max_w / image.cols, max_h / image.rows, 1.0});

    if (scale < 1.0)
        cv::resize(image, fitted, cv::Size(), scale, scale, cv::INTER_AREA);
    else
        fitted = image;
    return scale;
}

cv::Rect clamp_roi(const cv::Rect& roi, const cv::Size& image_size) {
    const int x = std::clamp(roi.x, 0, image_size.width - 1);
    const int y = std::clamp(roi.y, 0, image_size.height - 1);
    const int w = std::clamp(roi.width, 1, image_size.width - x);
    const int h = std::clamp(roi.height, 1, image_size.height - y);
    return {x, y, w, h};
}

cv::Mat draw_roi_and_text(const cv::Mat& image_bgr, const cv::Rect& roi,
                          const std::string& text, bool ok) {
    cv::Mat out = image_bgr.clone();
    cv::rectangle(out, roi.tl(), roi.br(), kOrange, 2);
    cv::putText(out, text, {roi.x, std::max(40, roi.y - 10)}, cv::FONT_HERSHEY_SIMPLEX, 1.5,
                ok ? kGreen : kRed, 3, cv::LINE_AA);
    return out;
}

void show_fitted(const std::string& window_name, const cv::Mat& image, int margin_px) {
    cv::Mat fitted;
    fit_to_screen(image, fitted, margin_px);
    cv::namedWindow(window_name, cv::WINDOW_NORMAL);
    cv::imshow(window_name, fitted);
}

cv::Rect select_roi_with_mouse(const cv::Mat& image, const std::string& window_title) {
    cv::Mat fitted;
    const double scale = fit_to_screen(image, fitted, 120);

    cv::namedWindow(window_title, cv::WINDOW_NORMAL);
    cv::imshow(window_title, fitted);
    cv::waitKey(200);  // discard key presses still pending (e.g. ENTER typed in the terminal)
    const cv::Rect selected = cv::selectROI(window_title, fitted, false, false);
    cv::destroyWindow(window_title);

    if (selected.area() <= 0)
        throw std::runtime_error("ROI selection cancelled or empty");

    // Convert back from screen coordinates to original image coordinates.
    const cv::Rect roi(static_cast<int>(std::round(selected.x / scale)),
                       static_cast<int>(std::round(selected.y / scale)),
                       static_cast<int>(std::round(selected.width / scale)),
                       static_cast<int>(std::round(selected.height / scale)));
    return clamp_roi(roi, image.size());
}
