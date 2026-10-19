/**
 * display.hpp
 *
 * Everything that draws or shows images: ROI handling, result overlay,
 * windows fitted to the screen and mouse ROI selection.
 */
#pragma once

#include "barcode_types.hpp"

// Text drawn on the image when no barcode is read (kept identical to the Python version).
inline const std::string kNoBarcodeText = "Nessun barcode letto";

// Restricts a ROI to the image area; the result is at least 1 x 1 px.
cv::Rect clamp_roi(const cv::Rect& roi, const cv::Size& image_size);

// Draws the ROI rectangle and a text above it (green if ok, red otherwise).
cv::Mat draw_roi_and_text(const cv::Mat& image_bgr, const cv::Rect& roi,
                          const std::string& text, bool ok);

// Shows an image in a resizable window, shrunk if it is larger than the screen.
void show_fitted(const std::string& window_name, const cv::Mat& image, int margin_px = 120);

// Lets the user draw a ROI with the mouse (ENTER/SPACE confirms).
// Returns the ROI in original image coordinates; throws std::runtime_error if cancelled.
cv::Rect select_roi_with_mouse(const cv::Mat& image, const std::string& window_title);
