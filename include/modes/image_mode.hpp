/**
 * @file image_mode.hpp
 * @brief Modalità immagine singola. / Single-image mode.
 */
#pragma once

#include "barcode_types.hpp"
#include <string>

/**
 * @brief Legge e decodifica un barcode da un file immagine.
 *        Reads and decodes a barcode from an image file.
 *
 * Carica l'immagine, applica la ROI, esegue la pipeline di preprocessing
 * e stampa il risultato su stdout. Opzionalmente visualizza le finestre
 * di risultato e debug.
 * Loads the image, applies the ROI, runs the preprocessing pipeline and
 * prints the result to stdout. Optionally displays result and debug windows.
 *
 * @param image_path Percorso file immagine. / Image file path.
 * @param roi        ROI da analizzare. / ROI to analyze.
 * @param show_debug Se true, mostra il mosaico di debug. / If true, shows debug mosaic.
 * @param no_show    Se true, non apre finestre GUI. / If true, no GUI windows are opened.
 */
void read_barcode_from_image(const std::string& image_path,
                              const ROI& roi,
                              bool show_debug,
                              bool no_show = false);
