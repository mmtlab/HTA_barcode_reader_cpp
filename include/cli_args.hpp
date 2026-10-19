/**
 * cli_args.hpp
 *
 * Command-line parsing. Run the program with --help for the list of options.
 */
#pragma once

#include "barcode_types.hpp"

// Prints the list of options to stdout.
void print_help();

// Reads and checks the options. On an error it prints a message and exits with code 1.
Args parse_args(int argc, char* argv[]);
