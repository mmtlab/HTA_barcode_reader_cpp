/**
 * @file cli_args.cpp
 * @brief Implementazione del parser degli argomenti CLI.
 *        Implementation of the CLI argument parser.
 */

#include "cli_args.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

// ============================================================================
//  print_help
// ============================================================================

void print_help() {
    std::cout <<
R"(Lettura barcode da immagine o camera (C++)
Barcode reading from image or camera (C++)

Modalita / Modes (sceglierne una / pick one):
  --image  <path>          Immagine singola / Single image
  --folder <path>          Batch: tutte le immagini in una cartella / All images in a folder
  --webcam                 Lettura real-time da webcam / Real-time from webcam
  --basler                 Lettura real-time da camera Basler (Pylon) / Real-time from Basler camera
  --libcamera              Lettura real-time da Arducam/libcamera (RPi) / Real-time from libcamera

Opzioni ROI / ROI options:
  --roi X Y W H            ROI fissa in pixel / Fixed ROI in pixels
  --select-roi             Selezione ROI col mouse / Mouse ROI selection
                           In --folder: selezione sulla 1a immagine, poi riusata.
                           In --folder: selection on 1st image, then reused.
                           Premi 'r' per riselezionare / Press 'r' to reselect.

Opzioni batch / Batch options (--folder):
  --output  <file.csv>     Salva i risultati in CSV / Save results to CSV
  --no-show                Niente GUI (headless) / No GUI (headless)
  --threads N              Worker paralleli [default: 1, richiede --no-show]
                           Parallel workers [default: 1, requires --no-show]

Opzioni camera / Camera options:
  --camera-index  N        Indice webcam [default: 0] / Webcam index
  --device-index  N        Indice Basler [default: 0] / Basler device index
  --basler-serial SN       Seriale Basler (priorita su --device-index)
                           Basler serial (takes precedence over --device-index)
  --libcamera-width  W     Larghezza frame libcamera [default: 1920]
  --libcamera-height H     Altezza frame libcamera [default: 1080]

Opzioni real-time / Real-time options:
  --process-every    N     Decode ogni N frame [default: 2] / Decode every N frames
  --quick-candidates N     Step preprocessing rapidi [default: 4] / Quick preprocessing steps
  --full-search-every N    Ogni N decode usa la ricerca completa [default: 10]
                           Every N decodes runs the full search

Altro / Misc:
  --debug                  Mostra immagine preprocessata / mosaico debug
                           Shows preprocessed image / debug mosaic
  --help, -h               Mostra questo messaggio / Show this message

Navigazione batch / Batch navigation:
  SPAZIO / ENTER  = immagine successiva / next image
  q / ESC         = interrompi / quit
  r               = riseleziona ROI (solo --select-roi) / reselect ROI
)";
}

// ============================================================================
//  parse_args
// ============================================================================

Args parse_args(int argc, char* argv[]) {
    Args a;
    bool has_mode = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        // --- Aiuto / Help ---
        if (arg == "--help" || arg == "-h") {
            print_help();
            std::exit(0);
        }

        // --- Modalità operative / Operating modes ---
        else if (arg == "--image" && i + 1 < argc) {
            a.image_path = argv[++i]; has_mode = true;
        } else if (arg == "--folder" && i + 1 < argc) {
            a.folder_path = argv[++i]; has_mode = true;
        } else if (arg == "--webcam") {
            a.webcam = true; has_mode = true;
        } else if (arg == "--basler") {
            a.basler = true; has_mode = true;
        } else if (arg == "--libcamera") {
            a.libcamera_mode = true; has_mode = true;
        }

        // --- Output batch / Batch output ---
        else if (arg == "--output" && i + 1 < argc) {
            a.output_csv = argv[++i];
        } else if (arg == "--no-show") {
            a.no_show = true;
        } else if (arg == "--threads" && i + 1 < argc) {
            a.num_threads = std::stoi(argv[++i]);
        }

        // --- Parametri camera / Camera parameters ---
        else if (arg == "--camera-index" && i + 1 < argc) {
            a.camera_index = std::stoi(argv[++i]);
        } else if (arg == "--device-index" && i + 1 < argc) {
            a.device_index = std::stoi(argv[++i]);
        } else if (arg == "--basler-serial" && i + 1 < argc) {
            a.basler_serial = argv[++i];
        } else if (arg == "--libcamera-width" && i + 1 < argc) {
            a.libcamera_width = std::stoi(argv[++i]);
        } else if (arg == "--libcamera-height" && i + 1 < argc) {
            a.libcamera_height = std::stoi(argv[++i]);
        }

        // --- ROI ---
        else if (arg == "--roi" && i + 4 < argc) {
            a.roi.x   = std::stoi(argv[++i]);
            a.roi.y   = std::stoi(argv[++i]);
            a.roi.w   = std::stoi(argv[++i]);
            a.roi.h   = std::stoi(argv[++i]);
            a.roi_set = true;
        } else if (arg == "--select-roi") {
            a.select_roi = true;
        }

        // --- Debug & tuning real-time ---
        else if (arg == "--debug") {
            a.debug = true;
        } else if (arg == "--process-every" && i + 1 < argc) {
            a.process_every = std::stoi(argv[++i]);
        } else if (arg == "--quick-candidates" && i + 1 < argc) {
            a.quick_candidates = std::stoi(argv[++i]);
        } else if (arg == "--full-search-every" && i + 1 < argc) {
            a.full_search_every = std::stoi(argv[++i]);
        }

        // --- Argomento sconosciuto / Unknown argument ---
        else {
            std::cerr << "Argomento sconosciuto / Unknown argument: " << arg << "\n";
            print_help();
            std::exit(1);
        }
    }

    // =========================================================================
    //  Validazione / Validation
    // =========================================================================

    if (!has_mode) {
        std::cerr << "Errore: specificare --image, --folder, --webcam, --basler o --libcamera\n"
                     "Error: specify --image, --folder, --webcam, --basler or --libcamera\n";
        print_help();
        std::exit(1);
    }

    if (!a.image_path.empty() && !a.select_roi && !a.roi_set) {
        std::cerr << "Errore: con --image specificare --roi X Y W H oppure --select-roi\n"
                     "Error: with --image specify --roi X Y W H or --select-roi\n";
        std::exit(1);
    }

    if (!a.folder_path.empty() && !fs::is_directory(a.folder_path)) {
        std::cerr << "Errore: percorso --folder non valido o non e' una cartella: "
                  << a.folder_path << "\n"
                     "Error: --folder path is invalid or not a directory: "
                  << a.folder_path << "\n";
        std::exit(1);
    }

    if (!a.folder_path.empty() && !a.select_roi && !a.roi_set) {
        std::cerr << "Avviso / Warning: nessuna ROI specificata in --folder. "
                     "Verra' usata l'intera immagine.\n"
                     "No ROI specified for --folder. The entire image will be used.\n"
                     "Usa --roi X Y W H o --select-roi / Use --roi X Y W H or --select-roi\n";
    }

    if (a.num_threads < 1) {
        std::cerr << "Errore: --threads deve essere >= 1 / Error: --threads must be >= 1\n";
        std::exit(1);
    }

    if (a.num_threads > 1 && !a.no_show) {
        std::cerr << "Avviso / Warning: --threads > 1 richiede --no-show. "
                     "Il parallelismo verra' disabilitato.\n"
                     "--threads > 1 requires --no-show. Parallelism will be disabled.\n";
        a.num_threads = 1;  // fallback sicuro / safe fallback
    }

    if (a.process_every < 1) {
        std::cerr << "Errore: --process-every deve essere >= 1 / Error: --process-every must be >= 1\n";
        std::exit(1);
    }
    if (a.quick_candidates < 1) {
        std::cerr << "Errore: --quick-candidates deve essere >= 1 / Error: --quick-candidates must be >= 1\n";
        std::exit(1);
    }
    if (a.full_search_every < 1) {
        std::cerr << "Errore: --full-search-every deve essere >= 1 / Error: --full-search-every must be >= 1\n";
        std::exit(1);
    }

    return a;
}
