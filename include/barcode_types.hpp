/**
 * @file barcode_types.hpp
 * @brief Strutture dati condivise tra tutti i moduli.
 *        Shared data structures used across all modules.
 *
 * Questo header non ha dipendenze su librerie esterne (solo STL e OpenCV core)
 * ed è incluso da tutti gli altri moduli.
 * This header has no external library dependencies (STL + OpenCV core only)
 * and is included by every other module.
 */
#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <utility>  // std::pair
#include <vector>

// ============================================================================
//  DecodedBarcode
// ============================================================================

/**
 * @brief Barcode decodificato da ZBar. / Barcode decoded by ZBar.
 *
 * Contiene tipo, dato grezzo e punteggio qualità restituiti dallo scanner.
 * Contains the symbol type, raw payload and quality score returned by the
 * scanner.
 */
struct DecodedBarcode {
    std::string type;   ///< Tipo barcode (es. "CODE128") / Barcode type
    std::string data;   ///< Payload decodificato / Decoded payload
    int quality = 0;    ///< Punteggio qualità ZBar (>0 = valido) / ZBar quality score (>0 = valid)
};

// ============================================================================
//  ROI
// ============================================================================

/**
 * @brief Regione di interesse rettangolare (coordinate pixel).
 *        Axis-aligned Region of Interest (pixel coordinates).
 */
struct ROI {
    int x = 0; ///< Coordinata X angolo superiore sinistro / Top-left X
    int y = 0; ///< Coordinata Y angolo superiore sinistro / Top-left Y
    int w = 0; ///< Larghezza in pixel / Width in pixels
    int h = 0; ///< Altezza in pixel / Height in pixels
};

// ============================================================================
//  DecodeResult
// ============================================================================

/**
 * @brief Risultato completo della pipeline preprocessing + decode.
 *        Full result of the preprocessing + decode pipeline.
 *
 * Contiene i simboli trovati, il nome dello step vincente, l'immagine
 * che ha prodotto il decode e (in modalità debug) tutti i candidati tentati.
 * Contains the found symbols, the winning step name, the image that yielded
 * the decode, and (in debug mode) all attempted candidates.
 */
struct DecodeResult {
    std::vector<DecodedBarcode> decoded;                           ///< Simboli trovati / Found symbols
    std::string pipeline_name;                                     ///< Step vincente / Winning step name
    cv::Mat winning_image;                                         ///< Immagine vincente / Winning image
    std::vector<std::pair<std::string, cv::Mat>> attempted;        ///< Tutti i tentativi (debug) / All attempts (debug)
};

// ============================================================================
//  RealtimeState
// ============================================================================

/**
 * @brief Stato del loop di decodifica real-time (webcam / camera).
 *        State for the real-time decoding loop (webcam / camera).
 *
 * Accumula il contatore frame, il testo cachato e le view di debug tra un
 * frame e l'altro, evitando stampe e aggiornamenti ridondanti.
 * Accumulates the frame counter, cached text and debug views between frames,
 * avoiding redundant prints and updates.
 */
struct RealtimeState {
    int frame_counter           = 0;         ///< Frame elaborati / Processed frames
    std::string last_printed_text;           ///< Ultimo testo stampato (dedup) / Last printed text (dedup)
    std::string cached_text;                 ///< Testo corrente da visualizzare / Current display text
    bool cached_ok              = false;     ///< true = barcode trovato / true = barcode found
    int  cached_age             = 999999;    ///< Frame dall'ultimo decode / Frames since last decode
    cv::Mat last_debug_view;                 ///< Ultima immagine debug / Last debug image
    std::string last_debug_title = "debug_live"; ///< Titolo finestra debug / Debug window title
};

// ============================================================================
//  Args
// ============================================================================

/**
 * @brief Argomenti da linea di comando, dopo il parsing.
 *        Command-line arguments after parsing.
 *
 * Raccoglie tutti i parametri configurabili dal CLI in un'unica struttura
 * passata alle funzioni di modalità (batch, immagine, realtime).
 * Collects all CLI-configurable parameters into a single structure passed
 * to the mode functions (batch, image, realtime).
 */
struct Args {
    // --- Modalità operative / Operating modes ---
    std::string image_path;          ///< --image
    std::string folder_path;         ///< --folder (batch)
    std::string output_csv;          ///< --output (CSV risultati / CSV results)
    bool no_show        = false;     ///< --no-show (headless)
    bool webcam         = false;     ///< --webcam
    bool basler         = false;     ///< --basler
    bool libcamera_mode = false;     ///< --libcamera

    // --- Parametri camera / Camera parameters ---
    int  camera_index   = 0;         ///< --camera-index
    int  device_index   = 0;         ///< --device-index (Basler)
    std::string basler_serial;       ///< --basler-serial
    int  libcamera_width  = 1920;    ///< --libcamera-width
    int  libcamera_height = 1080;    ///< --libcamera-height

    // --- ROI ---
    ROI  roi;                        ///< Coordinate ROI / ROI coordinates
    bool roi_set        = false;     ///< true = ROI fornita da CLI / ROI provided via CLI
    bool select_roi     = false;     ///< --select-roi (mouse)

    // --- Debug & diagnostica / Debug & diagnostics ---
    bool debug          = false;     ///< --debug

    // --- Tuning real-time ---
    int  process_every     = 2;      ///< --process-every N  (decode ogni N frame / decode every N frames)
    int  quick_candidates  = 4;      ///< --quick-candidates N (step rapidi / quick steps)
    int  full_search_every = 10;     ///< --full-search-every N (ricerca completa / full search interval)

    // --- Parallelismo batch / Batch parallelism ---
    int  num_threads    = 1;         ///< --threads N (worker paralleli, solo --no-show / parallel workers, --no-show only)
};
