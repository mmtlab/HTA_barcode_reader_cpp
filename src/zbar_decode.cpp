/**
 * @file zbar_decode.cpp
 * @brief Implementazione del wrapper ZBar per la decodifica barcode.
 *        Implementation of the ZBar wrapper for barcode decoding.
 *
 * Lo scanner ZBar è statico e condiviso tra tutte le chiamate per evitare
 * l'overhead di creazione/distruzione ad ogni frame.
 * The ZBar scanner is static and shared across all calls to avoid
 * creation/destruction overhead on every frame.
 */

#include "zbar_decode.hpp"

#include <opencv2/imgproc.hpp>
#include <zbar.h>

// ============================================================================
//  Funzioni interne (non esposte nell'header)
//  Internal functions (not exposed in the header)
// ============================================================================

/**
 * @brief Converte un tipo simbolo ZBar nella stringa leggibile corrispondente.
 *        Converts a ZBar symbol type to the corresponding readable string.
 *
 * I nomi sono compatibili con pyzbar (senza trattini, tutto maiuscolo).
 * Names are compatible with pyzbar (no hyphens, all uppercase).
 *
 * @param t Tipo simbolo ZBar. / ZBar symbol type.
 * @return  Nome del tipo. / Type name.
 */
static std::string barcode_type_name(zbar::zbar_symbol_type_t t) {
    switch (t) {
    case zbar::ZBAR_EAN2:        return "EAN2";
    case zbar::ZBAR_EAN5:        return "EAN5";
    case zbar::ZBAR_EAN8:        return "EAN8";
    case zbar::ZBAR_UPCE:        return "UPCE";
    case zbar::ZBAR_ISBN10:      return "ISBN10";
    case zbar::ZBAR_UPCA:        return "UPCA";
    case zbar::ZBAR_EAN13:       return "EAN13";
    case zbar::ZBAR_ISBN13:      return "ISBN13";
    case zbar::ZBAR_I25:         return "I25";
    case zbar::ZBAR_DATABAR:     return "DATABAR";
    case zbar::ZBAR_DATABAR_EXP: return "DATABAR_EXP";
    case zbar::ZBAR_CODABAR:     return "CODABAR";
    case zbar::ZBAR_CODE39:      return "CODE39";
    case zbar::ZBAR_PDF417:      return "PDF417";
    case zbar::ZBAR_QRCODE:      return "QRCODE";
    case zbar::ZBAR_CODE93:      return "CODE93";
    case zbar::ZBAR_CODE128:     return "CODE128";
    default:                     return "UNKNOWN";
    }
}

// ============================================================================
//  API pubblica / Public API
// ============================================================================

std::vector<DecodedBarcode> decode_zbar(const cv::Mat& img) {
    // Scanner statico: inizializzato una volta sola, riusato tra le chiamate.
    // Static scanner: initialized once, reused across calls.
    static zbar::ImageScanner scanner;
    static bool initialized = false;
    if (!initialized) {
        // Abilita tutti i tipi di simboli supportati.
        // Enable all supported symbol types.
        scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 1);
        initialized = true;
    }

    // --- Conversione in scala di grigi / Convert to grayscale ---
    // ZBar richiede immagini Y800 (8-bit grayscale).
    // ZBar requires Y800 (8-bit grayscale) images.
    cv::Mat gray;
    if (img.channels() >= 3) {
        cv::cvtColor(img, gray,
                     img.channels() == 4 ? cv::COLOR_BGRA2GRAY
                                         : cv::COLOR_BGR2GRAY);
    } else {
        // Già in grigio: usa direttamente se continua, altrimenti clona.
        // Already grayscale: use directly if continuous, otherwise clone.
        gray = img.isContinuous() ? img : img.clone();
    }

    // ZBar richiede dati contigui in memoria.
    // ZBar requires contiguous memory.
    if (!gray.isContinuous())
        gray = gray.clone();

    // --- Scan ZBar ---
    zbar::Image zimg(static_cast<unsigned>(gray.cols),
                     static_cast<unsigned>(gray.rows),
                     "Y800",
                     gray.data,
                     static_cast<unsigned long>(gray.total()));
    scanner.scan(zimg);

    // --- Raccolta risultati / Collect results ---
    std::vector<DecodedBarcode> results;
    for (auto it = zimg.symbol_begin(); it != zimg.symbol_end(); ++it) {
        if (it->get_type() == zbar::ZBAR_PARTIAL)
            continue;  // ignora simboli parziali / skip partial symbols
        results.push_back({
            barcode_type_name(it->get_type()),
            it->get_data(),
            it->get_quality()
        });
    }

    // Sgancia il buffer da ZBar prima che gray venga distrutta.
    // Detach buffer from ZBar before gray is destroyed.
    zimg.set_data(nullptr, 0);
    return results;
}
