/**
 * @file main.cpp
 * @brief Punto di ingresso del programma barcode_reader.
 *        Program entry point for barcode_reader.
 *
 * Il main si occupa esclusivamente di:
 * The main is solely responsible for:
 *   1. Inizializzazione opzionale del SDK Pylon (Basler).
 *      Optional Pylon SDK initialization (Basler).
 *   2. Parsing degli argomenti CLI.
 *      CLI argument parsing.
 *   3. Dispatch alla modalità operativa corretta.
 *      Dispatch to the correct operating mode.
 *   4. Gestione centralizzata delle eccezioni non catturate.
 *      Centralized handling of uncaught exceptions.
 */

#include "cli_args.hpp"
#include "modes/batch_mode.hpp"
#include "modes/image_mode.hpp"
#include "modes/realtime_mode.hpp"
#include "roi_utils.hpp"

#include <iostream>

#ifdef HAS_PYLON
#  include <pylon/PylonIncludes.h>
#endif

int main(int argc, char* argv[]) {
    // Inizializzazione Pylon (no-op se HAS_PYLON non definito).
    // Pylon initialization (no-op if HAS_PYLON not defined).
#ifdef HAS_PYLON
    Pylon::PylonAutoInitTerm pylonAutoInitTerm;
#endif

    // Parsing e validazione argomenti / Argument parsing and validation
    const Args args = parse_args(argc, argv);

    try {
        // -----------------------------------------------------------------
        //  Dispatch alla modalità operativa / Dispatch to operating mode
        // -----------------------------------------------------------------
        if (args.webcam) {
            run_webcam_mode(args);

        } else if (args.basler) {
#ifdef HAS_PYLON
            run_basler_mode(args);
#else
            std::cerr << "Supporto Basler non compilato.\n"
                         "Basler support not compiled.\n"
                         "Ricompila con / Rebuild with: cmake -DUSE_PYLON=ON ...\n";
            return 1;
#endif

        } else if (args.libcamera_mode) {
#ifdef HAS_LIBCAMERA
            run_libcamera_mode(args);
#else
            std::cerr << "Supporto libcamera non compilato.\n"
                         "libcamera support not compiled.\n"
                         "Ricompila con / Rebuild with: cmake -DUSE_LIBCAMERA=ON ...\n";
            return 1;
#endif

        } else if (!args.folder_path.empty()) {
            run_batch_mode(args);

        } else {
            // Modalità immagine singola / Single-image mode
            ROI roi = args.roi;
            if (args.select_roi) {
                roi = select_roi_with_mouse(args.image_path);
                std::cout << "ROI selezionata / Selected ROI: ("
                          << roi.x << ", " << roi.y << ", "
                          << roi.w << ", " << roi.h << ")\n";
            }
            read_barcode_from_image(args.image_path, roi, args.debug, args.no_show);
        }

    } catch (const std::exception& e) {
        std::cerr << "Errore fatale / Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
