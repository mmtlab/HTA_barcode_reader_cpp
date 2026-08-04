/**
 * @file preprocessing.cpp
 * @brief Pipeline di preprocessing immagine + decodifica barcode.
 *        Image preprocessing pipeline + barcode decoding.
 *
 * La pipeline applica trasformazioni progressive in ordine crescente di costo
 * computazionale e si ferma al primo decode riuscito (early-exit).
 *
 * Ordine candidati:
 *   1. gray            – scala di grigi diretta
 *   2. gray_clahe      – CLAHE (contrasto locale, istanza statica)
 *   3. denoise         – filtro bilaterale su gray_clahe
 *   4. thr_adapt       – sogliatura adattiva (Gaussian)
 *   5. thr_otsu        – sogliatura Otsu
 *   6. morph_open_horiz– apertura morfologica orizzontale (enfatizza barre)
 *   7. *_x2, *_x3      – versioni 2× e 3× di tutti i candidati base
 *
 * The pipeline applies progressive transformations in increasing computational
 * cost order and stops at the first successful decode (early-exit).
 *
 * Candidate order:
 *   1. gray             – direct grayscale
 *   2. gray_clahe       – CLAHE (local contrast, static instance)
 *   3. denoise          – bilateral filter on gray_clahe
 *   4. thr_adapt        – adaptive threshold (Gaussian)
 *   5. thr_otsu         – Otsu threshold
 *   6. morph_open_horiz – horizontal morphological opening (emphasizes bars)
 *   7. *_x2, *_x3       – 2× and 3× upscaled versions of all base candidates
 */

#include "preprocessing.hpp"
#include "zbar_decode.hpp"

#include <climits>
#include <opencv2/imgproc.hpp>

// ============================================================================
//  Oggetti statici (allocati una volta sola) / Static objects (allocated once)
// ============================================================================

/**
 * @brief Istanza CLAHE condivisa tra tutte le chiamate.
 *        CLAHE instance shared across all calls.
 *
 * Evita la riallocazione a ogni frame, che è il costo maggiore di CLAHE.
 * Avoids reallocation every frame, which is the main cost of CLAHE.
 */
static cv::Ptr<cv::CLAHE> g_clahe = cv::createCLAHE(2.0, cv::Size(8, 8));

// ============================================================================
//  Implementazione / Implementation
// ============================================================================

DecodeResult decode_with_preprocessing(const cv::Mat& roi_bgr,
                                        bool collect_debug,
                                        int  max_candidates) {
    DecodeResult result;
    const int limit = (max_candidates > 0) ? max_candidates : INT_MAX;
    int idx = 0;

    /**
     * @brief Tenta un singolo candidato preprocessing.
     *        Tries a single preprocessing candidate.
     *
     * Aggiunge il candidato al log di debug se richiesto, poi prova a
     * decodificarlo. Restituisce true se il decode ha avuto successo oppure
     * se il limite di candidati è stato raggiunto.
     * Adds the candidate to the debug log if requested, then tries to decode
     * it. Returns true if decoding succeeded or the candidate limit is reached.
     */
    auto try_candidate = [&](const std::string& name, const cv::Mat& cand) -> bool {
        if (idx >= limit) return true;   // limite raggiunto / limit reached
        ++idx;
        if (collect_debug)
            result.attempted.push_back({name, cand});

        auto found = decode_zbar(cand);
        if (!found.empty()) {
            result.decoded       = std::move(found);
            result.pipeline_name = name;
            result.winning_image = cand;
            return true;  // early-exit
        }
        return false;
    };

    // -------------------------------------------------------------------------
    //  1. Scala di grigi / Grayscale
    // -------------------------------------------------------------------------
    cv::Mat gray;
    cv::cvtColor(roi_bgr, gray, cv::COLOR_BGR2GRAY);
    if (try_candidate("gray", gray)) return result;

    // -------------------------------------------------------------------------
    //  2. CLAHE – miglioramento contrasto locale / local contrast enhancement
    //     Usa l'istanza statica per evitare riallocazioni.
    //     Uses the static instance to avoid reallocations.
    // -------------------------------------------------------------------------
    cv::Mat gray_clahe;
    g_clahe->apply(gray, gray_clahe);
    if (try_candidate("gray_clahe", gray_clahe)) return result;

    // -------------------------------------------------------------------------
    //  3. Filtro bilaterale – riduzione rumore preservando i bordi.
    //     Bilateral filter – noise reduction while preserving edges.
    // -------------------------------------------------------------------------
    cv::Mat den;
    cv::bilateralFilter(gray_clahe, den, 7, 50, 50);
    if (try_candidate("denoise", den)) return result;

    // -------------------------------------------------------------------------
    //  4. Sogliatura adattiva (Gaussian) / Adaptive threshold (Gaussian)
    // -------------------------------------------------------------------------
    cv::Mat thr_adapt;
    cv::adaptiveThreshold(den, thr_adapt, 255,
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY, 31, 5);
    if (try_candidate("thr_adapt", thr_adapt)) return result;

    // -------------------------------------------------------------------------
    //  5. Sogliatura Otsu / Otsu threshold
    // -------------------------------------------------------------------------
    cv::Mat thr_otsu;
    cv::threshold(den, thr_otsu, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    if (try_candidate("thr_otsu", thr_otsu)) return result;

    // -------------------------------------------------------------------------
    //  6. Apertura morfologica orizzontale – enfatizza le barre verticali.
    //     Horizontal morphological opening – emphasises vertical bars.
    // -------------------------------------------------------------------------
    {
        const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 1));
        cv::Mat morph_open;
        cv::morphologyEx(thr_otsu, morph_open, cv::MORPH_OPEN, kernel);
        if (try_candidate("morph_open_horiz", morph_open)) return result;
    }

    // -------------------------------------------------------------------------
    //  7. Versioni upscalate (2× e 3×) di tutti i candidati base.
    //     Upscaled versions (2× and 3×) of all base candidates.
    //     Utili per barcode piccoli o sfocati. / Useful for small or blurry barcodes.
    //
    //  GUARDIA MEMORIA: se l'immagine base supera la soglia di pixel, lo
    //  scaling è saltato per evitare std::bad_alloc su frame ad alta risoluzione.
    //  MEMORY GUARD: if the base image exceeds the pixel threshold, scaling is
    //  skipped to avoid std::bad_alloc on high-resolution frames.
    // -------------------------------------------------------------------------

    // Soglia: 4 Mpixel. Un'immagine 2× di 2 Mpixel = 8 Mpixel ≈ 24 MB (BGR),
    // considerato accettabile. Un'immagine 3× di 2 Mpixel = 18 Mpixel ≈ 54 MB,
    // potenzialmente problematico su sistemi con poca RAM.
    // Threshold: 4 Mpixels. A 2× of 2 Mpixel = 8 Mpixel ≈ 24 MB (BGR),
    // considered acceptable. A 3× of 2 Mpixel = 18 Mpixel ≈ 54 MB, potentially
    // problematic on low-RAM systems.
    constexpr int kMaxPixelsForUpscale2x = 4 * 1024 * 1024;  // 4 Mpixel
    constexpr int kMaxPixelsForUpscale3x = 2 * 1024 * 1024;  // 2 Mpixel

    struct NamedImg { const std::string& n; const cv::Mat& m; };
    // Riferimenti ai candidati base già calcolati (nessuna copia).
    // References to already-computed base candidates (no copies).
    const std::vector<NamedImg> bases = {
        {"gray",             gray},
        {"gray_clahe",       gray_clahe},
        {"denoise",          den},
        {"thr_adapt",        thr_adapt},
        {"thr_otsu",         thr_otsu},
    };

    for (const auto& b : bases) {
        const int base_pixels = b.m.rows * b.m.cols;

        if (base_pixels <= kMaxPixelsForUpscale2x) {
            cv::Mat up2;
            cv::resize(b.m, up2, cv::Size(), 2.0, 2.0, cv::INTER_CUBIC);
            if (try_candidate(b.n + "_x2", up2)) return result;
        }

        if (base_pixels <= kMaxPixelsForUpscale3x) {
            cv::Mat up3;
            cv::resize(b.m, up3, cv::Size(), 3.0, 3.0, cv::INTER_CUBIC);
            if (try_candidate(b.n + "_x3", up3)) return result;
        }
    }

    return result;  // nessun barcode trovato / no barcode found
}
