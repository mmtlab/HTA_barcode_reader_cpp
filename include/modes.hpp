/**
 * modes.hpp
 *
 * Entry points of the operating modes, called by main.cpp.
 *   src/image_modes.cpp  : --image, --folder
 *   src/camera_modes.cpp : --webcam, --libcamera
 * Each function throws std::runtime_error on fatal problems (file or camera not available).
 */
#pragma once

#include "barcode_types.hpp"

void run_image_mode(const Args& args);   // one image file
void run_batch_mode(const Args& args);   // every image in a folder
void run_webcam_mode(const Args& args);  // OpenCV webcam

#ifdef HAS_LIBCAMERA
void run_libcamera_mode(const Args& args);  // libcamera / Arducam via GStreamer
#endif
