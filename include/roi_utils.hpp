/**
 * @file roi_utils.hpp
 * @brief Utilità per la gestione della Regione di Interesse (ROI).
 *        Utilities for Region of Interest (ROI) management.
 */
#pragma once

#include "barcode_types.hpp"
#include <opencv2/core.hpp>
#include <string>

/**
 * @brief Limita la ROI ai bordi dell'immagine.
 *        Clamps the ROI to the image boundaries.
 *
 * Garantisce che la ROI sia interamente contenuta nell'immagine
 * e che abbia dimensione minima 1×1.
 * Ensures the ROI is entirely contained within the image and has
 * a minimum size of 1×1.
 *
 * @param x, y, w, h Coordinate e dimensioni ROI da limitare.
 *                   ROI coordinates and dimensions to clamp.
 * @param img_w      Larghezza immagine. / Image width.
 * @param img_h      Altezza immagine. / Image height.
 * @return           ROI limitata ai bordi. / ROI clamped to boundaries.
 */
ROI clamp_roi(int x, int y, int w, int h, int img_w, int img_h);

/**
 * @brief Permette la selezione interattiva della ROI con il mouse.
 *        Allows interactive ROI selection with the mouse.
 *
 * Ridimensiona l'immagine per adattarla allo schermo, poi usa
 * cv::selectROI(). Le coordinate restituite sono nello spazio
 * dell'immagine originale, già limitate ai bordi.
 * Resizes the image to fit the screen, then uses cv::selectROI().
 * The returned coordinates are in original image space, already clamped.
 *
 * @param img  Immagine su cui selezionare la ROI. / Image to select ROI on.
 * @param win  Titolo della finestra. / Window title.
 * @return     ROI selezionata. / Selected ROI.
 * @throws std::runtime_error Se l'utente annulla (area nulla o ESC).
 *                            If user cancels (zero area or ESC).
 */
ROI select_roi_on_image(const cv::Mat& img,
                         const std::string& win = "Seleziona ROI e premi ENTER/SPACE");

/**
 * @brief Carica un'immagine dal disco e avvia la selezione interattiva ROI.
 *        Loads an image from disk and starts interactive ROI selection.
 *
 * @param image_path Percorso file immagine. / Image file path.
 * @return           ROI selezionata. / Selected ROI.
 * @throws std::runtime_error Se l'immagine non è leggibile o ROI nulla.
 *                            If the image is unreadable or ROI is zero-area.
 */
ROI select_roi_with_mouse(const std::string& image_path);
