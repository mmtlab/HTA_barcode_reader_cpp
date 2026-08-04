/**
 * @file preprocessing.hpp
 * @brief Pipeline di preprocessing immagine + decodifica barcode.
 *        Image preprocessing pipeline + barcode decoding.
 *
 * La pipeline applica in sequenza una serie di trasformazioni
 * (scala di grigi, CLAHE, filtro bilaterale, sogliatura adattiva, Otsu,
 * operazioni morfologiche, upscaling) e si ferma al primo decode riuscito.
 * The pipeline sequentially applies a series of transformations
 * (grayscale, CLAHE, bilateral filter, adaptive threshold, Otsu,
 * morphological ops, upscaling) and stops at the first successful decode.
 */
#pragma once

#include "barcode_types.hpp"
#include <opencv2/core.hpp>

/**
 * @brief Esegue la pipeline di preprocessing e decodifica sulla ROI.
 *        Runs the preprocessing and decoding pipeline on the ROI.
 *
 * Se `max_candidates` > 0, la ricerca si ferma dopo aver provato quel
 * numero di candidati (utile per i frame "rapidi" in modalità real-time).
 * If `max_candidates` > 0, the search stops after trying that many
 * candidates (useful for "quick" frames in real-time mode).
 *
 * @param roi_bgr       Ritaglio BGR della ROI. / BGR crop of the ROI.
 * @param collect_debug Se true, popola DecodeResult::attempted con tutti i
 *                      candidati tentati. / If true, fills DecodeResult::attempted
 *                      with all tried candidates.
 * @param max_candidates Numero massimo di candidati da provare (-1 = tutti).
 *                       Max candidates to try (-1 = all).
 * @return              Risultato con barcode trovati e metadati pipeline.
 *                      Result with found barcodes and pipeline metadata.
 */
DecodeResult decode_with_preprocessing(const cv::Mat& roi_bgr,
                                       bool collect_debug  = false,
                                       int  max_candidates = -1);
