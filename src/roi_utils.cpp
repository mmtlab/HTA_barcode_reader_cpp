/**
 * @file roi_utils.cpp
 * @brief Implementazione delle utilità per la gestione della ROI.
 *        Implementation of ROI management utilities.
 */

#include "roi_utils.hpp"
#include "display.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

// ============================================================================
//  clamp_roi
// ============================================================================

ROI clamp_roi(int x, int y, int w, int h, int img_w, int img_h) {
    // Limita x e y all'interno dell'immagine / Clamp x and y within image bounds
    x = std::max(0, std::min(x, img_w - 1));
    y = std::max(0, std::min(y, img_h - 1));
    // Assicura dimensione minima 1×1 / Ensure minimum size 1×1
    w = std::max(1, std::min(w, img_w - x));
    h = std::max(1, std::min(h, img_h - y));
    return {x, y, w, h};
}

// ============================================================================
//  select_roi_on_image
// ============================================================================

ROI select_roi_on_image(const cv::Mat& img, const std::string& win) {
    const int img_w = img.cols, img_h = img.rows;

    // Ridimensiona per adattarsi allo schermo prima della selezione.
    // Resize to fit the screen before selection.
    const auto [disp, scale] = resize_for_display(img);

    cv::namedWindow(win, cv::WINDOW_NORMAL);
    cv::imshow(win, disp);
    // Scarica eventi tastiera pendenti (es. ENTER premuto nel terminale).
    // Flush pending keyboard events (e.g. ENTER pressed in terminal).
    cv::waitKey(200);

    const cv::Rect r = cv::selectROI(win, disp, false, false);
    cv::destroyWindow(win);

    if (r.width <= 0 || r.height <= 0)
        throw std::runtime_error("ROI non valida: selezione annullata o area nulla");

    // Riscala le coordinate allo spazio dell'immagine originale.
    // Rescale coordinates back to original image space.
    int rx = r.x, ry = r.y, rw = r.width, rh = r.height;
    if (scale < 1.0) {
        rx = static_cast<int>(std::round(rx / scale));
        ry = static_cast<int>(std::round(ry / scale));
        rw = static_cast<int>(std::round(rw / scale));
        rh = static_cast<int>(std::round(rh / scale));
    }
    return clamp_roi(rx, ry, rw, rh, img_w, img_h);
}

// ============================================================================
//  select_roi_with_mouse
// ============================================================================

ROI select_roi_with_mouse(const std::string& image_path) {
    const cv::Mat img = cv::imread(image_path);
    if (img.empty())
        throw std::runtime_error("Immagine non trovata o non leggibile: " + image_path);
    return select_roi_on_image(img);
}
