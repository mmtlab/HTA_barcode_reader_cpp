/**
 * main.cpp
 *
 * Program entry point: reads the command line and starts live reading from the webcam.
 * Fatal errors are printed and the program exits with code 1.
 * Usage: see README.md or run "barcode_reader --help".
 */
#include "cli_args.hpp"
#include "webcam_mode.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    const Args args = parse_args(argc, argv);

    try {
        run_webcam_mode(args);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
