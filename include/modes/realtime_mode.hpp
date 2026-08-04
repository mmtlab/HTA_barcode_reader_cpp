/**
 * @file realtime_mode.hpp
 * @brief Modalità real-time: webcam, camera Basler (Pylon), libcamera (RPi).
 *        Real-time modes: webcam, Basler camera (Pylon), libcamera (RPi).
 */
#pragma once

#include "barcode_types.hpp"

/**
 * @brief Esegue un singolo passo di decodifica su un frame real-time.
 *        Executes a single decoding step on a real-time frame.
 *
 * Aggiorna lo stato (RealtimeState) con il risultato del decode e la
 * view di debug. Viene chiamato ad ogni frame dal loop della camera.
 * Updates the state (RealtimeState) with the decode result and debug view.
 * Called every frame by the camera loop.
 *
 * @param roi_img           Ritaglio BGR della ROI corrente. / BGR crop of the current ROI.
 * @param st                Stato real-time da aggiornare (in/out). / Real-time state to update (in/out).
 * @param show_debug        Se true, aggiorna la finestra debug. / If true, updates debug window.
 * @param process_every     Decodifica ogni N frame. / Decode every N frames.
 * @param quick_candidates  Max candidati preprocessing nei frame rapidi. / Max preprocessing candidates in quick frames.
 * @param full_search_every Ogni N decode esegue la ricerca completa. / Every N decodes runs full search.
 */
void run_decode_step(const cv::Mat& roi_img, RealtimeState& st,
                     bool show_debug,
                     int  process_every,
                     int  quick_candidates,
                     int  full_search_every);

/**
 * @brief Avvia il loop di lettura barcode da webcam.
 *        Starts the barcode reading loop from webcam.
 * @param args Argomenti CLI. / CLI arguments.
 */
void run_webcam_mode(const Args& args);

#ifdef HAS_PYLON
/**
 * @brief Avvia il loop di lettura barcode da camera Basler (richiede Pylon SDK).
 *        Starts the barcode reading loop from a Basler camera (requires Pylon SDK).
 * @param args Argomenti CLI. / CLI arguments.
 */
void run_basler_mode(const Args& args);
#endif

#ifdef HAS_LIBCAMERA
/**
 * @brief Avvia il loop di lettura barcode da libcamera / Arducam (richiede GStreamer).
 *        Starts the barcode reading loop from libcamera / Arducam (requires GStreamer).
 * @param args Argomenti CLI. / CLI arguments.
 */
void run_libcamera_mode(const Args& args);
#endif
