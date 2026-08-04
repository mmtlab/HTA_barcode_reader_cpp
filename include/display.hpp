/**
 * @file display.hpp
 * @brief Utilità di visualizzazione: ridimensionamento, overlay, mosaico debug.
 *        Display utilities: resize, overlay, debug mosaic.
 */
#pragma once

#include "barcode_types.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <utility>  // std::pair
#include <vector>

/**
 * @brief Restituisce la risoluzione dello schermo.
 *        Returns the screen resolution.
 * @return Coppia (width, height) in pixel. / Pair (width, height) in pixels.
 */
std::pair<int, int> get_screen_size();

/**
 * @brief Ridimensiona un'immagine per adattarla allo schermo (solo se necessario).
 *        Resizes an image to fit the screen (only if needed).
 *
 * @param img    Immagine sorgente. / Source image.
 * @param margin Margine lasciato attorno all'immagine (px). / Margin left around the image (px).
 * @return       Coppia (immagine ridimensionata, fattore di scala usato).
 *               Pair (resized image, scale factor used).
 */
std::pair<cv::Mat, double> resize_for_display(const cv::Mat& img, int margin = 120);

/**
 * @brief Converte un'immagine a scala di grigi in BGR se necessario.
 *        Converts a grayscale image to BGR if needed.
 * @param img Immagine di input (1 o 3 canali). / Input image (1 or 3 channels).
 * @return    Immagine BGR. / BGR image.
 */
cv::Mat ensure_bgr(const cv::Mat& img);

/**
 * @brief Disegna il rettangolo ROI e il risultato del decode sull'immagine.
 *        Draws the ROI rectangle and the decode result on the image.
 *
 * @param img_bgr      Immagine BGR originale. / Original BGR image.
 * @param roi          ROI da evidenziare. / ROI to highlight.
 * @param decoded      Vettore di barcode decodificati (può essere vuoto).
 *                     Vector of decoded barcodes (may be empty).
 * @param overlay_text Testo personalizzato da mostrare (se vuoto usa decoded[0].data).
 *                     Custom text to display (if empty uses decoded[0].data).
 * @param overlay_ok   1=verde, 0=rosso, -1=auto dal vettore decoded.
 *                     1=green, 0=red, -1=auto from decoded vector.
 * @return             Immagine con overlay. / Image with overlay.
 */
cv::Mat draw_result(const cv::Mat& img_bgr, const ROI& roi,
                    const std::vector<DecodedBarcode>& decoded = {},
                    const std::string& overlay_text = "",
                    int overlay_ok = -1);

/**
 * @brief Disegna l'overlay real-time (testo cachato) sul frame.
 *        Draws the real-time overlay (cached text) on the frame.
 * @param frame Frame corrente. / Current frame.
 * @param roi   ROI attiva. / Active ROI.
 * @param st    Stato real-time corrente. / Current real-time state.
 * @return      Frame con overlay. / Frame with overlay.
 */
cv::Mat draw_live_overlay(const cv::Mat& frame, const ROI& roi, const RealtimeState& st);

/**
 * @brief Crea un mosaico di debug con tutti i candidati preprocessing.
 *        Creates a debug mosaic with all preprocessing candidates.
 *
 * @param candidates Vettore (nome, immagine) di tutti i candidati.
 *                   Vector (name, image) of all candidates.
 * @param winner_name Nome del candidato vincente (evidenziato in verde).
 *                    Name of the winning candidate (highlighted in green).
 * @param tile_w     Larghezza tile in pixel. / Tile width in pixels.
 * @param tile_h     Altezza tile in pixel. / Tile height in pixels.
 * @param cols       Numero di colonne nel mosaico. / Number of columns in the mosaic.
 * @return           Immagine mosaico BGR. / BGR mosaic image.
 */
cv::Mat make_debug_mosaic(const std::vector<std::pair<std::string, cv::Mat>>& candidates,
                           const std::string& winner_name,
                           int tile_w = 320, int tile_h = 180, int cols = 4);
