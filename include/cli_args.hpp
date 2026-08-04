/**
 * @file cli_args.hpp
 * @brief Parser degli argomenti da linea di comando.
 *        Command-line argument parser.
 */
#pragma once

#include "barcode_types.hpp"

/**
 * @brief Stampa il messaggio di aiuto su stdout.
 *        Prints the help message to stdout.
 */
void print_help();

/**
 * @brief Effettua il parsing degli argomenti CLI e restituisce la struct Args.
 *        Parses CLI arguments and returns the Args struct.
 *
 * In caso di errore (argomento sconosciuto, modalità mancante, parametri
 * non validi) stampa un messaggio su stderr e chiama std::exit(1).
 * On error (unknown argument, missing mode, invalid parameters) prints
 * a message to stderr and calls std::exit(1).
 *
 * @param argc Numero di argomenti (da main). / Argument count (from main).
 * @param argv Vettore di stringhe argomento (da main). / Argument string array (from main).
 * @return     Struttura Args popolata. / Populated Args struct.
 */
Args parse_args(int argc, char* argv[]);
