/**
 * @file batch_mode.hpp
 * @brief Modalità batch su cartella, con parallelismo opzionale.
 *        Folder batch mode, with optional parallelism.
 *
 * Con --threads N e --no-show, le immagini vengono elaborate in parallelo
 * su N worker thread, riducendo il tempo totale di elaborazione.
 * With --threads N and --no-show, images are processed in parallel
 * across N worker threads, reducing total processing time.
 */
#pragma once

#include "barcode_types.hpp"

/**
 * @brief Elabora in batch tutte le immagini in una cartella.
 *        Processes in batch all images in a folder.
 *
 * - Raccoglie le immagini, ordinate per nome.
 * - Seleziona (o riusa) la ROI.
 * - Decodifica ogni immagine con la pipeline di preprocessing.
 * - Stampa i risultati su stdout e, se richiesto, li salva in CSV.
 * - Con args.num_threads > 1 e args.no_show, elabora in parallelo.
 *
 * - Collects images, sorted by name.
 * - Selects (or reuses) the ROI.
 * - Decodes each image with the preprocessing pipeline.
 * - Prints results to stdout and, if requested, saves to CSV.
 * - With args.num_threads > 1 and args.no_show, processes in parallel.
 *
 * @param args Argomenti CLI già validati. / Already-validated CLI arguments.
 */
void run_batch_mode(const Args& args);
