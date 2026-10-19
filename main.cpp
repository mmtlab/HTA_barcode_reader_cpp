/**
 * main.cpp
 *
 * Program entry point: reads the command line and starts the chosen mode.
 * Fatal errors are printed and the program exits with code 1.
 * Usage: see README.md or run "barcode_reader --help".
 */
#include "cli_args.hpp"
#include "modes.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    const Args args = parse_args(argc, argv);

    try {
        if (!args.image_path.empty()) {
            run_image_mode(args);
        } else if (!args.folder_path.empty()) {
            run_batch_mode(args);
        } else if (args.webcam) {
            run_webcam_mode(args);
        } else if (args.libcamera) {
#ifdef HAS_LIBCAMERA
            run_libcamera_mode(args);
#else
            std::cerr << "libcamera support not built. Rebuild with: cmake -DUSE_LIBCAMERA=ON ...\n";
            return 1;
#endif
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
