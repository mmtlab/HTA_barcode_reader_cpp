/**
 * @file image_mode.cpp
 * @brief Implementazione della modalità immagine singola.
 *        Implementation of single-image mode.
 */

#include "modes/image_mode.hpp"
#include "display.hpp"
#include "preprocessing.hpp"
#include "roi_utils.hpp"

#include <iostream>
#include <stdexcept>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

void read_barcode_from_image(const std::string& image_path,
                              const ROI& roi,
                              bool show_debug,
                              bool no_show) {
    // Carica l'immagine da disco / Load image from disk
    const cv::Mat img = cv::imread(image_path);
    if (img.empty())
        throw std::runtime_error("Immagine non trovata o non leggibile: " + image_path);

    // Limita la ROI ai bordi dell'immagine / Clamp ROI to image boundaries
    const ROI r = clamp_roi(roi.x, roi.y, roi.w, roi.h, img.cols, img.rows);
    const cv::Mat roi_img = img(cv::Rect(r.x, r.y, r.w, r.h));

    // Esegue la pipeline di preprocessing e decode / Run preprocessing + decode pipeline
    const auto res = decode_with_preprocessing(roi_img, show_debug);

    // Stampa risultati su stdout / Print results to stdout
    if (!res.decoded.empty()) {
        for (const auto& item : res.decoded)
            std::cout << "OK | type=" << item.type
                      << " | data="     << item.data
                      << " | quality="  << item.quality
                      << " | pipeline=" << res.pipeline_name << "\n";
    } else {
        std::cout << "KO | Nessun barcode letto nella ROI\n";
    }

    if (no_show) return;

    // -------------------------------------------------------------------------
    //  Visualizzazione risultato / Result display
    // -------------------------------------------------------------------------
    const cv::Mat vis = draw_result(img, r, res.decoded);
    const auto [vd, _s] = resize_for_display(vis);
    cv::namedWindow("barcode_result", cv::WINDOW_NORMAL);
    cv::imshow("barcode_result", vd);

    // Visualizzazione mosaico debug / Debug mosaic display
    if (show_debug && !res.attempted.empty()) {
        const cv::Mat mosaic = make_debug_mosaic(res.attempted, res.pipeline_name);
        if (!mosaic.empty()) {
            const auto [md, _s2] = resize_for_display(mosaic, 120);
            cv::namedWindow("debug_mosaic", cv::WINDOW_NORMAL);
            cv::imshow("debug_mosaic", md);
        }
    }

    cv::waitKey(0);
    cv::destroyAllWindows();
}
