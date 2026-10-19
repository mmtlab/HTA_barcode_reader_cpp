/**
 * cli_args.cpp
 *
 * print_help(): option list (defaults are read from Args, not repeated here).
 * parse_args(): reads argv into an Args struct, then validates the combination.
 */
#include "cli_args.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

void print_help() {
    const Args defaults;
    std::cout <<
        "Barcode reader: reads barcodes from images, folders or cameras.\n"
        "\n"
        "Mode (choose one):\n"
        "  --image <file>            single image (needs --roi or --select-roi)\n"
        "  --folder <dir>            all images in a folder (batch)\n"
        "  --webcam                  live from a webcam\n"
        "  --libcamera               live from libcamera / Arducam (build with -DUSE_LIBCAMERA=ON)\n"
        "\n"
        "Region of interest (default: whole image):\n"
        "  --roi X Y W H             fixed ROI in pixels\n"
        "  --select-roi              draw the ROI with the mouse (batch: on the first image)\n"
        "\n"
        "Output (--image and --folder):\n"
        "  --output <file.csv>       save results to CSV (--folder only)\n"
        "  --save-overlay <dir>      save each image with ROI and result drawn on it\n"
        "  --save-debug <dir>        save a grid with all preprocessing steps of each image\n"
        "  --no-show                 no windows (headless)\n"
        "  --threads N               parallel workers for --folder, needs --no-show [default: "
              << defaults.num_threads << "]\n"
        "  --debug                   also show the preprocessing steps in a window\n"
        "\n"
        "Cameras:\n"
        "  --camera-index N          webcam index [default: " << defaults.camera_index << "]\n"
        "  --libcamera-width W       libcamera frame width [default: " << defaults.libcamera_width << "]\n"
        "  --libcamera-height H      libcamera frame height [default: " << defaults.libcamera_height << "]\n"
        "\n"
        "Live tuning:\n"
        "  --process-every N         decode one frame every N [default: " << defaults.process_every << "]\n"
        "  --quick-candidates N      steps tried on a quick decode [default: " << defaults.quick_candidates << "]\n"
        "  --full-search-every N     every N decodes try all steps [default: " << defaults.full_search_every << "]\n"
        "\n"
        "  --help, -h                show this message\n"
        "\n"
        "Keys in windows: SPACE/ENTER = next image, r = reselect ROI (with --select-roi),\n"
        "                 q/ESC = quit.\n";
}

// Prints an error message and stops the program.
[[noreturn]] static void exit_with_error(const std::string& message) {
    std::cerr << "Error: " << message << "\nUse --help to see the options.\n";
    std::exit(1);
}

// Creates an output folder if needed; stops the program if that is not possible.
static void create_output_folder(const std::string& folder, const std::string& option) {
    if (folder.empty()) return;
    std::error_code error;
    fs::create_directories(folder, error);
    if (error || !fs::is_directory(folder))
        exit_with_error(option + ": cannot create folder '" + folder + "'");
}

Args parse_args(int argc, char* argv[]) {
    Args a;

    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];

        // Read the value that follows the current option.
        auto text_value = [&]() -> std::string {
            if (i + 1 >= argc) exit_with_error("missing value after " + option);
            return argv[++i];
        };
        auto int_value = [&]() -> int {
            const std::string text = text_value();
            try {
                std::size_t used = 0;
                const int value = std::stoi(text, &used);
                if (used == text.size()) return value;
            } catch (const std::exception&) {}
            exit_with_error(option + ": '" + text + "' is not an integer");
        };

        if      (option == "--help" || option == "-h") { print_help(); std::exit(0); }
        else if (option == "--image")             a.image_path = text_value();
        else if (option == "--folder")            a.folder_path = text_value();
        else if (option == "--webcam")            a.webcam = true;
        else if (option == "--libcamera")         a.libcamera = true;
        else if (option == "--select-roi")        a.select_roi = true;
        else if (option == "--output")            a.output_csv = text_value();
        else if (option == "--save-overlay")      a.save_overlay_dir = text_value();
        else if (option == "--save-debug")        a.save_debug_dir = text_value();
        else if (option == "--no-show")           a.no_show = true;
        else if (option == "--debug")             a.debug = true;
        else if (option == "--threads")           a.num_threads = int_value();
        else if (option == "--camera-index")      a.camera_index = int_value();
        else if (option == "--libcamera-width")   a.libcamera_width = int_value();
        else if (option == "--libcamera-height")  a.libcamera_height = int_value();
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

    // ---- Check the combination of options ----
    const int modes = !a.image_path.empty() + !a.folder_path.empty() + a.webcam + a.libcamera;
    if (modes != 1)
        exit_with_error("choose exactly one of --image, --folder, --webcam, --libcamera");

    if (!a.image_path.empty() && !a.roi_set && !a.select_roi)
        exit_with_error("--image needs --roi X Y W H or --select-roi");

    if (!a.folder_path.empty() && !fs::is_directory(a.folder_path))
        exit_with_error("--folder: '" + a.folder_path + "' is not a folder");

    if (a.roi_set && (a.roi.width < 1 || a.roi.height < 1))
        exit_with_error("--roi: width and height must be >= 1");

    if (a.num_threads < 1 || a.process_every < 1 || a.quick_candidates < 1 || a.full_search_every < 1)
        exit_with_error("--threads, --process-every, --quick-candidates and --full-search-every must be >= 1");

    if (!a.folder_path.empty() && !a.roi_set && !a.select_roi)
        std::cerr << "Warning: no ROI given, the whole image is used.\n";

    if (a.num_threads > 1 && (!a.no_show || a.select_roi)) {
        std::cerr << "Warning: --threads needs --no-show and no --select-roi; using 1 thread.\n";
        a.num_threads = 1;
    }

    create_output_folder(a.save_overlay_dir, "--save-overlay");
    create_output_folder(a.save_debug_dir, "--save-debug");
    return a;
}
