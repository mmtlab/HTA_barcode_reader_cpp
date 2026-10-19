/**
 * cli_args.cpp
 *
 * print_help(): option list (defaults are read from Args, not repeated here).
 * parse_args(): reads argv into an Args struct, then checks the values.
 */
#include "cli_args.hpp"

#include <iostream>
#include <string>

void print_help() {
    const Args defaults;
    std::cout <<
        "Barcode reader: reads barcodes live from a webcam.\n"
        "\n"
        "  --camera-index N          webcam index [default: " << defaults.camera_index << "]\n"
        "\n"
        "Region of interest (default: whole frame):\n"
        "  --roi X Y W H             fixed ROI in pixels\n"
        "  --select-roi              draw the ROI with the mouse on the first frame\n"
        "\n"
        "Live tuning:\n"
        "  --process-every N         decode one frame every N [default: " << defaults.process_every << "]\n"
        "  --quick-candidates N      steps tried on a quick decode [default: " << defaults.quick_candidates << "]\n"
        "  --full-search-every N     every N decodes try all steps [default: " << defaults.full_search_every << "]\n"
        "\n"
        "  --debug                   also show the preprocessing image in a window\n"
        "  --help, -h                show this message\n"
        "\n"
        "Press q in the video window to quit.\n";
}

// Prints an error message and stops the program.
[[noreturn]] static void exit_with_error(const std::string& message) {
    std::cerr << "Error: " << message << "\nUse --help to see the options.\n";
    std::exit(1);
}

Args parse_args(int argc, char* argv[]) {
    Args a;

    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];

        // Reads the integer that follows the current option.
        auto int_value = [&]() -> int {
            if (i + 1 >= argc) exit_with_error("missing value after " + option);
            const std::string text = argv[++i];
            try {
                std::size_t used = 0;
                const int value = std::stoi(text, &used);
                if (used == text.size()) return value;
            } catch (const std::exception&) {}
            exit_with_error(option + ": '" + text + "' is not an integer");
        };

        if      (option == "--help" || option == "-h") { print_help(); std::exit(0); }
        else if (option == "--webcam")            {}  // accepted for compatibility with the main branch
        else if (option == "--camera-index")      a.camera_index = int_value();
        else if (option == "--select-roi")        a.select_roi = true;
        else if (option == "--debug")             a.debug = true;
        else if (option == "--process-every")     a.process_every = int_value();
        else if (option == "--quick-candidates")  a.quick_candidates = int_value();
        else if (option == "--full-search-every") a.full_search_every = int_value();
        else if (option == "--roi") {
            a.roi.x = int_value();
            a.roi.y = int_value();
            a.roi.width = int_value();
            a.roi.height = int_value();
            a.roi_set = true;
        }
        else exit_with_error("unknown option " + option);
    }

    // ---- Check the values ----
    if (a.roi_set && (a.roi.width < 1 || a.roi.height < 1))
        exit_with_error("--roi: width and height must be >= 1");

    if (a.process_every < 1 || a.quick_candidates < 1 || a.full_search_every < 1)
        exit_with_error("--process-every, --quick-candidates and --full-search-every must be >= 1");

    return a;
}
