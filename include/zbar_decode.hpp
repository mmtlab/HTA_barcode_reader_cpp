/**
 * @file zbar_decode.hpp
 * @brief Interfaccia pubblica del wrapper ZBar per la decodifica barcode.
 *        Public interface of the ZBar wrapper for barcode decoding.
 *
 * Espone un'unica funzione decode_zbar() che accetta qualsiasi immagine
 * OpenCV (BGR, BGRA o grigia) e restituisce i barcode trovati.
 * Exposes a single decode_zbar() function that accepts any OpenCV image
 * (BGR, BGRA or grayscale) and returns the found barcodes.
 */
#pragma once

#include "barcode_types.hpp"
#include <opencv2/core.hpp>
#include <vector>

/**
 * @brief Decodifica tutti i barcode in un'immagine usando ZBar.
 *        Decode all barcodes in an image using ZBar.
 *
 * La funzione mantiene uno scanner ZBar statico (riusato tra le chiamate)
 * e converte automaticamente l'immagine in scala di grigi se necessario.
 * The function keeps a static ZBar scanner (reused across calls) and
 * automatically converts the image to grayscale if needed.
 *
 * @param img Immagine di input (1, 3 o 4 canali). / Input image (1, 3 or 4 channels).
 * @return    Vettore di barcode decodificati (vuoto se nessuno trovato).
 *            Vector of decoded barcodes (empty if none found).
 */
std::vector<DecodedBarcode> decode_zbar(const cv::Mat& img);
