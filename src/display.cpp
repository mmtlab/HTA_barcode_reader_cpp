/**
 * @file display.cpp
 * @brief Implementazione delle utilità di visualizzazione.
 *        Implementation of display utilities.
 */

#include "display.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

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

// ============================================================================
//  get_screen_size
// ============================================================================

std::pair<int, int> get_screen_size() {
#ifdef _WIN32
    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    if (sw > 0 && sh > 0) return {sw, sh};
#endif
    // Fallback per Linux/macOS: risoluzione FHD.
    // Fallback for Linux/macOS: FHD resolution.
    return {1920, 1080};
}

// ============================================================================
//  resize_for_display
// ============================================================================

std::pair<cv::Mat, double> resize_for_display(const cv::Mat& img, int margin) {
    const int h = img.rows, w = img.cols;
    const auto [scr_w, scr_h] = get_screen_size();
    const int max_w = std::max(200, scr_w - margin);
    const int max_h = std::max(200, scr_h - margin);
    const double scale = std::min({(double)max_w / w, (double)max_h / h, 1.0});

    if (scale < 1.0) {
        cv::Mat resized;
        cv::resize(img, resized,
                   cv::Size(static_cast<int>(std::round(w * scale)),
                            static_cast<int>(std::round(h * scale))),
                   0, 0, cv::INTER_AREA);
        return {resized, scale};
    }
    // Nessun ridimensionamento necessario: clona per ownership sicura.
    // No resize needed: clone for safe ownership.
    return {img.clone(), 1.0};
}

// ============================================================================
//  ensure_bgr
// ============================================================================

cv::Mat ensure_bgr(const cv::Mat& img) {
    if (img.channels() == 1) {
        cv::Mat bgr;
        cv::cvtColor(img, bgr, cv::COLOR_GRAY2BGR);
        return bgr;
    }
    return img;  // già BGR / already BGR (no copy)
}

// ============================================================================
//  draw_result
// ============================================================================

cv::Mat draw_result(const cv::Mat& img_bgr,
                    const ROI& roi,
                    const std::vector<DecodedBarcode>& decoded,
                    const std::string& overlay_text,
                    int overlay_ok) {
    cv::Mat out = img_bgr.clone();

    // Rettangolo ROI (arancio) / ROI rectangle (orange)
    cv::rectangle(out,
                  {roi.x, roi.y},
                  {roi.x + roi.w, roi.y + roi.h},
                  {0, 200, 255}, 2);

    // Scelta testo e colore / Choose text and color
    std::string text;
    cv::Scalar text_color;

    if (overlay_text.empty()) {
        // Testo automatico dal vettore decoded / Auto text from decoded vector
        if (!decoded.empty()) {
            text       = decoded[0].data;
            text_color = {0, 255, 0};   // verde = OK / green = OK
        } else {
            text       = "Nessun barcode letto";
            text_color = {0, 0, 255};   // rosso = KO / red = KO
        }
    } else {
        text = overlay_text;
        text_color = (overlay_ok == 1)
                     ? cv::Scalar(0, 255, 0)   // verde / green
                     : cv::Scalar(0, 0, 255);  // rosso / red
    }

    cv::putText(out, text,
                {roi.x, std::max(40, roi.y - 10)},
                cv::FONT_HERSHEY_SIMPLEX, 1.5, text_color, 3, cv::LINE_AA);
    return out;
}

// ============================================================================
//  draw_live_overlay
// ============================================================================

cv::Mat draw_live_overlay(const cv::Mat& frame, const ROI& roi, const RealtimeState& st) {
    const std::string text = st.cached_text.empty() ? "Nessun barcode letto" : st.cached_text;
    return draw_result(frame, roi, {}, text, st.cached_ok ? 1 : 0);
}

// ============================================================================
//  make_debug_mosaic
// ============================================================================

cv::Mat make_debug_mosaic(const std::vector<std::pair<std::string, cv::Mat>>& candidates,
                           const std::string& winner_name,
                           int tile_w, int tile_h, int cols) {
    if (candidates.empty()) return {};

    // Costruisce i tile / Build tiles
    std::vector<cv::Mat> tiles;
    tiles.reserve(candidates.size());

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
        const auto& [name, img] = candidates[i];
        const bool is_winner = (name == winner_name);

        cv::Mat bgr = ensure_bgr(img);
        cv::Mat tile;
        cv::resize(bgr, tile, cv::Size(tile_w, tile_h), 0, 0, cv::INTER_AREA);

        // Bordo evidenziato per il vincente / Highlighted border for winner
        const cv::Scalar bcol = is_winner ? cv::Scalar(0, 255, 0)
                                          : cv::Scalar(100, 100, 100);
        cv::rectangle(tile, {0, 0}, {tile_w - 1, tile_h - 1}, bcol,
                      is_winner ? 3 : 1);

        // Etichetta / Label
        char label[128];
        std::snprintf(label, sizeof(label), "%02d | %s", i, name.c_str());
        cv::rectangle(tile, {0, tile_h - 26}, {tile_w - 1, tile_h - 1}, {0, 0, 0}, -1);
        cv::putText(tile, label, {8, tile_h - 8},
                    cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    is_winner ? cv::Scalar(0, 255, 0) : cv::Scalar(255, 255, 255),
                    1, cv::LINE_AA);
        tiles.push_back(std::move(tile));
    }

    // Assembla il mosaico / Assemble mosaic
    const int rows = static_cast<int>(std::ceil(static_cast<double>(tiles.size()) / cols));
    cv::Mat mosaic = cv::Mat::zeros(rows * tile_h, cols * tile_w, CV_8UC3);

    for (int i = 0; i < static_cast<int>(tiles.size()); ++i) {
        const int r = i / cols, c = i % cols;
        tiles[i].copyTo(mosaic(cv::Rect(c * tile_w, r * tile_h, tile_w, tile_h)));
    }

    // Intestazione con nome del vincente / Header with winner name
    const int hdr_h = 40;
    cv::Mat header = cv::Mat::zeros(hdr_h, mosaic.cols, CV_8UC3);
    const std::string wtxt = winner_name.empty() ? "nessuna" : winner_name;
    cv::putText(header, "Sequenza preprocessing (WIN=" + wtxt + ")",
                {10, 26}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {0, 255, 0}, 2, cv::LINE_AA);

    cv::Mat out;
    cv::vconcat(header, mosaic, out);
    return out;
}
