/**
 * @file batch_mode.cpp
 * @brief Implementazione della modalità batch su cartella.
 *        Implementation of folder batch mode.
 *
 * Architettura parallela / Parallel architecture:
 * ─────────────────────────────────────────────
 * Quando args.num_threads > 1 e args.no_show è true, le immagini vengono
 * elaborate in parallelo tramite una finestra scorrevole di std::future
 * (max N future in volo contemporaneamente). I risultati vengono raccolti
 * in ordine FIFO, garantendo ordinamento nel CSV e output console deterministico.
 *
 * When args.num_threads > 1 and args.no_show is true, images are processed
 * in parallel via a sliding window of std::future (max N futures in flight
 * simultaneously). Results are collected in FIFO order, guaranteeing
 * CSV ordering and deterministic console output.
 *
 * In modalità GUI (no_show=false), l'elaborazione è strettamente sequenziale
 * perché le finestre OpenCV non sono thread-safe.
 * In GUI mode (no_show=false), processing is strictly sequential because
 * OpenCV windows are not thread-safe.
 */

#include "modes/batch_mode.hpp"

#include "display.hpp"
#include "preprocessing.hpp"
#include "roi_utils.hpp"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;

// ============================================================================
//  Tipi interni / Internal types
// ============================================================================

/**
 * @brief Risultato dell'elaborazione di una singola immagine nel batch.
 *        Result of processing a single image in the batch.
 */
struct BatchResult {
    std::string img_name;         ///< Nome file (basename) / Filename (basename)
    bool skipped   = false;       ///< true = immagine non leggibile / unreadable image
    bool ok        = false;       ///< true = barcode trovato / barcode found
    DecodedBarcode bc;            ///< Barcode decodificato (valido solo se ok) / Decoded barcode (valid only if ok)
    std::string pipeline_name;    ///< Pipeline vincente / Winning pipeline
    ROI r;                        ///< ROI usata (dopo clamp) / ROI used (after clamp)
};

// ============================================================================
//  Funzioni interne / Internal functions
// ============================================================================

/**
 * @brief Verifica se un file ha un'estensione immagine supportata.
 *        Checks if a file has a supported image extension.
 */
static bool is_image_file(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    static const std::vector<std::string> exts = {
        ".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".tif", ".webp"
    };
    return std::find(exts.cbegin(), exts.cend(), ext) != exts.cend();
}

/**
 * @brief Elabora una singola immagine: carica, applica ROI, decodifica.
 *        Processes a single image: load, apply ROI, decode.
 *
 * Funzione pura, thread-safe (nessuna variabile globale mutabile).
 * Pure function, thread-safe (no mutable global state).
 *
 * @param img_path  Percorso completo dell'immagine. / Full image path.
 * @param roi       ROI da applicare. / ROI to apply.
 * @param do_debug  Se true, raccoglie i candidati debug (non thread-safe per GUI). / If true, collects debug candidates.
 * @return          BatchResult con il risultato del decode. / BatchResult with decode result.
 */
static BatchResult process_image(const fs::path& img_path, const ROI& roi, bool do_debug) {
    BatchResult res;
    res.img_name = img_path.filename().string();

    // Lettura immagine / Load image
    const cv::Mat img = cv::imread(img_path.string());
    if (img.empty()) {
        res.skipped = true;
        return res;
    }

    // ROI limitata ai bordi / ROI clamped to image boundaries
    res.r = clamp_roi(roi.x, roi.y, roi.w, roi.h, img.cols, img.rows);
    const cv::Mat roi_img = img(cv::Rect(res.r.x, res.r.y, res.r.w, res.r.h));

    // Decodifica con pipeline di preprocessing / Decode with preprocessing pipeline
    const auto dr = decode_with_preprocessing(roi_img, do_debug);
    if (!dr.decoded.empty()) {
        res.ok            = true;
        res.bc            = dr.decoded[0];
        res.pipeline_name = dr.pipeline_name;
    }
    return res;
}

/**
 * @brief Stampa un BatchResult su console e (opzionalmente) CSV.
 *        Prints a BatchResult to console and (optionally) CSV.
 *
 * @param res      Risultato da stampare. / Result to print.
 * @param idx      Indice immagine (1-based). / Image index (1-based).
 * @param total    Totale immagini nel batch. / Total images in batch.
 * @param csv_out  Stream CSV aperto (o non aperto se non richiesto). / Open CSV stream (or not open if not requested).
 */
static void print_result(const BatchResult& res, size_t idx, size_t total,
                          std::ofstream& csv_out) {
    // Prefisso con indice / Index prefix
    std::cout << "[" << idx << "/" << total << "] " << res.img_name << " ... ";

    if (res.skipped) {
        std::cout << "SKIP (immagine non leggibile)\n";
        if (csv_out.is_open())
            csv_out << res.img_name << ",SKIP,,,,,,,\n";
        return;
    }

    if (res.ok) {
        std::cout << "OK | type=" << res.bc.type
                  << " | data=" << res.bc.data
                  << " | quality=" << res.bc.quality
                  << " | pipeline=" << res.pipeline_name << "\n";
        if (csv_out.is_open())
            csv_out << res.img_name << ",OK,"
                    << res.bc.type << "," << res.bc.data << ","
                    << res.bc.quality << "," << res.pipeline_name << ","
                    << res.r.x << "," << res.r.y << ","
                    << res.r.w << "," << res.r.h << "\n";
    } else {
        std::cout << "KO | Nessun barcode letto\n";
        if (csv_out.is_open())
            csv_out << res.img_name << ",KO,,,,,,"
                    << res.r.x << "," << res.r.y << ","
                    << res.r.w << "," << res.r.h << "\n";
    }
}

// ============================================================================
//  Implementazione pubblica / Public implementation
// ============================================================================

void run_batch_mode(const Args& args) {
    // -------------------------------------------------------------------------
    //  Raccolta immagini ordinate / Collect sorted image list
    // -------------------------------------------------------------------------
    std::vector<fs::path> images;
    for (const auto& entry : fs::directory_iterator(args.folder_path)) {
        if (entry.is_regular_file() && is_image_file(entry.path()))
            images.push_back(entry.path());
    }
    std::sort(images.begin(), images.end());

    if (images.empty()) {
        std::cerr << "Nessuna immagine trovata in: " << args.folder_path << "\n";
        return;
    }
    std::cout << "Trovate " << images.size() << " immagini in " << args.folder_path << "\n";

    // -------------------------------------------------------------------------
    //  Apertura CSV (se richiesto) / Open CSV (if requested)
    // -------------------------------------------------------------------------
    std::ofstream csv_out;
    if (!args.output_csv.empty()) {
        csv_out.open(args.output_csv);
        if (!csv_out.is_open())
            throw std::runtime_error("Impossibile aprire il file CSV: " + args.output_csv);
        csv_out << "filename,status,type,data,quality,pipeline,roi_x,roi_y,roi_w,roi_h\n";
    }

    // -------------------------------------------------------------------------
    //  Risoluzione ROI / ROI resolution
    // -------------------------------------------------------------------------
    ROI current_roi;
    bool roi_ready = false;

    if (args.roi_set) {
        current_roi = args.roi;
        roi_ready   = true;
    }
    // Se --select-roi, la selezione avverrà sulla prima immagine (modalità sequenziale).
    // If --select-roi, selection will happen on the first image (sequential mode).

    // =========================================================================
    //  MODALITÀ PARALLELA / PARALLEL MODE
    //  Attivata quando: num_threads > 1 AND no_show=true AND ROI già nota.
    //  Activated when: num_threads > 1 AND no_show=true AND ROI already known.
    // =========================================================================
    const bool parallel_mode = (args.num_threads > 1)
                             && args.no_show
                             && (args.roi_set && !args.select_roi);

    if (parallel_mode) {
        std::cout << "Modalita parallela: " << args.num_threads << " thread\n"
                     "Parallel mode: " << args.num_threads << " threads\n";

        // Finestra scorrevole di future / Sliding window of futures
        // Mantiene al massimo num_threads future in volo contemporaneamente.
        // Keeps at most num_threads futures in flight simultaneously.
        using FutureResult = std::future<BatchResult>;
        std::deque<std::pair<size_t, FutureResult>> in_flight;

        auto flush_one = [&]() {
            // Attende il risultato più vecchio e lo stampa / Wait for oldest result and print it
            auto& [idx, fut] = in_flight.front();
            const auto res = fut.get();
            print_result(res, idx + 1, images.size(), csv_out);
            in_flight.pop_front();
        };

        for (size_t i = 0; i < images.size(); ++i) {
            // Attende se la finestra è piena / Wait if window is full
            while (static_cast<int>(in_flight.size()) >= args.num_threads)
                flush_one();

            // Lancia il worker asincrono / Launch async worker
            in_flight.push_back({
                i,
                std::async(std::launch::async, process_image,
                           images[i], current_roi, false /* no debug in parallel */)
            });
        }

        // Raccoglie tutti i risultati rimanenti / Collect remaining results
        while (!in_flight.empty())
            flush_one();

    } else {
        // =====================================================================
        //  MODALITÀ SEQUENZIALE / SEQUENTIAL MODE
        //  Usata quando: no_show=false (GUI) o select_roi o num_threads==1.
        //  Used when: no_show=false (GUI) or select_roi or num_threads==1.
        // =====================================================================

        for (size_t idx = 0; idx < images.size(); ++idx) {
            const std::string img_path = images[idx].string();
            const std::string img_name = images[idx].filename().string();

            std::cout << "[" << (idx + 1) << "/" << images.size() << "] "
                      << img_name << " ... " << std::flush;

            cv::Mat img = cv::imread(img_path);
            if (img.empty()) {
                std::cout << "SKIP (immagine non leggibile)\n";
                if (csv_out.is_open())
                    csv_out << img_name << ",SKIP,,,,,,,\n";
                continue;
            }

            // -----------------------------------------------------------------
            //  Selezione ROI interattiva (prima immagine) / Interactive ROI selection (first image)
            // -----------------------------------------------------------------
            if (args.select_roi && !roi_ready) {
                std::cout << "\n  Seleziona la ROI e premi ENTER/SPACE. "
                             "Premi ESC per usare l'intera immagine.\n";
                try {
                    current_roi = select_roi_on_image(
                        img, "ROI [" + img_name + "] - ENTER=conferma  ESC=intera immagine");
                    roi_ready = true;
                    std::cout << "  ROI: (" << current_roi.x << ", " << current_roi.y
                              << ", " << current_roi.w << ", " << current_roi.h << ")\n";
                } catch (const std::exception&) {
                    // ESC: usa l'intera immagine / ESC: use full image
                    current_roi = {0, 0, img.cols, img.rows};
                    roi_ready   = true;
                    std::cout << "  ROI non selezionata: uso dell'intera immagine.\n";
                }
            }

            if (!roi_ready) {
                // Nessuna ROI specificata: usa l'intera immagine / No ROI: use full image
                current_roi = {0, 0, img.cols, img.rows};
                roi_ready   = true;
            }

            // -----------------------------------------------------------------
            //  Decode / Decode
            // -----------------------------------------------------------------
            ROI r = clamp_roi(current_roi.x, current_roi.y,
                               current_roi.w, current_roi.h,
                               img.cols, img.rows);
            cv::Mat roi_img = img(cv::Rect(r.x, r.y, r.w, r.h));
            auto res = decode_with_preprocessing(roi_img, args.debug);

            // -----------------------------------------------------------------
            //  Output console e CSV / Console and CSV output
            // -----------------------------------------------------------------
            if (!res.decoded.empty()) {
                const auto& bc = res.decoded[0];
                std::cout << "OK | type=" << bc.type
                          << " | data=" << bc.data
                          << " | quality=" << bc.quality
                          << " | pipeline=" << res.pipeline_name << "\n";
                if (csv_out.is_open())
                    csv_out << img_name << ",OK," << bc.type << "," << bc.data << ","
                            << bc.quality << "," << res.pipeline_name << ","
                            << r.x << "," << r.y << "," << r.w << "," << r.h << "\n";
            } else {
                std::cout << "KO | Nessun barcode letto\n";
                if (csv_out.is_open())
                    csv_out << img_name << ",KO,,,,,,"
                            << r.x << "," << r.y << "," << r.w << "," << r.h << "\n";
            }

            // -----------------------------------------------------------------
            //  Visualizzazione GUI / GUI display
            // -----------------------------------------------------------------
            if (!args.no_show) {
                const std::string hint = args.select_roi
                    ? "ENTER/SPAZIO=avanti  r=riseleziona ROI  q=esci"
                    : "ENTER/SPAZIO=avanti  q=esci";

                cv::Mat vis = draw_result(img, r, res.decoded);
                char info[256];
                std::snprintf(info, sizeof(info), "%s [%zu/%zu]",
                              img_name.c_str(), idx + 1, images.size());

                cv::putText(vis, hint, {10, std::max(30, r.y - 50)},
                            cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 200, 0}, 2, cv::LINE_AA);
                cv::putText(vis, info, {10, img.rows - 20},
                            cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 255, 255}, 2, cv::LINE_AA);

                auto [vd, _s] = resize_for_display(vis);
                cv::namedWindow("batch_barcode", cv::WINDOW_NORMAL);
                cv::imshow("batch_barcode", vd);

                // Mosaico debug / Debug mosaic
                if (args.debug && !res.attempted.empty()) {
                    const cv::Mat mosaic = make_debug_mosaic(res.attempted, res.pipeline_name);
                    if (!mosaic.empty()) {
                        const auto [md, _s2] = resize_for_display(mosaic, 120);
                        cv::namedWindow("debug_mosaic", cv::WINDOW_NORMAL);
                        cv::imshow("debug_mosaic", md);
                    }
                }

                // Loop tasti / Key loop
                bool advance = false;
                while (!advance) {
                    const int key = cv::waitKey(0) & 0xFF;

                    if (key == 'q' || key == 27) {
                        // ESC / q = interrompi batch / quit batch
                        cv::destroyAllWindows();
                        std::cout << "\nBatch interrotto dall'utente. / Batch interrupted by user.\n";
                        return;

                    } else if (key == 13 || key == 32) {
                        advance = true;  // avanti / next

                    } else if ((key == 'r' || key == 'R') && args.select_roi) {
                        // Riseleziona ROI per questa immagine / Reselect ROI for this image
                        std::cout << "  Riseleziona ROI per " << img_name << "\n";
                        try {
                            current_roi = select_roi_on_image(
                                img, "Nuova ROI [" + img_name + "] - ENTER=conferma");
                            std::cout << "  Nuova ROI: (" << current_roi.x << ", "
                                      << current_roi.y << ", "
                                      << current_roi.w << ", " << current_roi.h << ")\n";

                            // Rielabora con la nuova ROI / Reprocess with new ROI
                            r       = clamp_roi(current_roi.x, current_roi.y,
                                                current_roi.w, current_roi.h,
                                                img.cols, img.rows);
                            roi_img = img(cv::Rect(r.x, r.y, r.w, r.h));
                            res     = decode_with_preprocessing(roi_img, args.debug);

                            // Aggiorna output / Update output
                            if (!res.decoded.empty()) {
                                const auto& bc2 = res.decoded[0];
                                std::cout << "  OK | type=" << bc2.type
                                          << " | data=" << bc2.data
                                          << " | quality=" << bc2.quality
                                          << " | pipeline=" << res.pipeline_name << "\n";
                                if (csv_out.is_open())
                                    csv_out << img_name << ",OK," << bc2.type << "," << bc2.data
                                            << "," << bc2.quality << "," << res.pipeline_name
                                            << "," << r.x << "," << r.y << ","
                                            << r.w << "," << r.h << "\n";
                            } else {
                                std::cout << "  KO | Nessun barcode letto\n";
                                if (csv_out.is_open())
                                    csv_out << img_name << ",KO,,,,,,"
                                            << r.x << "," << r.y << ","
                                            << r.w << "," << r.h << "\n";
                            }

                            // Aggiorna visualizzazione / Update display
                            vis = draw_result(img, r, res.decoded);
                            cv::putText(vis, hint, {10, std::max(30, r.y - 50)},
                                        cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 200, 0}, 2, cv::LINE_AA);
                            cv::putText(vis, info, {10, img.rows - 20},
                                        cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 255, 255}, 2, cv::LINE_AA);
                            const auto [vd2, _s3] = resize_for_display(vis);
                            cv::imshow("batch_barcode", vd2);

                        } catch (const std::exception&) {
                            std::cout << "  ROI non modificata. / ROI unchanged.\n";
                        }
                    }
                } // key loop
            } // if !no_show
        } // for images
    } // sequential mode

    cv::destroyAllWindows();

    std::cout << "\nBatch completato. " << images.size() << " immagini elaborate.\n"
                 "Batch complete. " << images.size() << " images processed.\n";
    if (!args.output_csv.empty())
        std::cout << "Risultati salvati in / Results saved to: " << args.output_csv << "\n";
}
