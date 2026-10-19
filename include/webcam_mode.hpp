/**
 * webcam_mode.hpp
 *
 * Live barcode reading from a webcam (implemented in src/webcam_mode.cpp).
 */
#pragma once

#include "barcode_types.hpp"

// Opens the webcam, reads barcodes until 'q' is pressed.
// Throws std::runtime_error if the webcam cannot be opened or read.
void run_webcam_mode(const Args& args);
