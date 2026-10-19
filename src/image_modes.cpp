/**
 * image_modes.cpp
 *
 * --image : decodes one file, prints the result and optionally shows it.
 * --folder: decodes every image in a folder (sorted by name). Results go to the
 *           console and optionally to a CSV file, overlay images and debug grids.
 *           With --threads N (headless) up to N images are decoded at the same
 *           time; results are still printed in file order.
 *
 * Console lines are kept identical to the Python version:
 *   OK | type=EAN13 | data=5901234123457 | quality=299 | pipeline=gray
 *   KO | Nessun barcode letto
 */
#include "modes.hpp"

#include "barcode_config.hpp"
#include "decoder.hpp"
#include "display.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;

// ===========================================================================
//  Helpers shared by --image and --folder
// ===========================================================================

// Result of one image file.
struct ImageResult {
    std::string file_name;    // name without folder, also used for saved images
    bool unreadable = false;  // the file could not be opened as an image
    cv::Rect roi;             // ROI actually used (clamped to the image)
    DecodeResult decode;
};

// Decodes the barcode inside `roi` of an already loaded image.
static ImageResult decode_image(const cv::Mat& image, const std::string& file_name,
                                const cv::Rect& roi, const Args& args) {
    // --save-debug needs every step; --debug only the steps actually tried.
    const DebugImages debug_images = !args.save_debug_dir.empty() ? DebugImages::All
                                   : args.debug                   ? DebugImages::Attempted
                                                                  : DebugImages::None;
    ImageResult result;
    result.file_name = file_name;
    result.roi = clamp_roi(roi, image.size());
    result.decode = decode_with_preprocessing(image(result.roi), -1, debug_images);
    return result;
}

// Writes the overlay image and/or the debug grid, if those options were given.
static void save_result_images(const cv::Mat& image, const ImageResult& result, const Args& args) {
    auto save = [](const std::string& folder, const std::string& file_name, const cv::Mat& picture) {
        const fs::path path = fs::path(folder) / file_name;
        if (picture.empty() || !cv::imwrite(path.string(), picture))
            std::cerr << "Warning: cannot write " << path.string() << "\n";
    };
    if (!args.save_overlay_dir.empty())
        save(args.save_overlay_dir, result.file_name,
             draw_decode_result(image, result.roi, result.decode.decoded));
    if (!args.save_debug_dir.empty())
        save(args.save_debug_dir, result.file_name,
             make_debug_grid(result.decode.attempted, result.decode.pipeline_name));
}

// One console line for one barcode, e.g. "OK | type=EAN13 | data=... | quality=.. | pipeline=gray".
static std::string barcode_line(const DecodedBarcode& barcode, const std::string& pipeline) {
    return "OK | type=" + barcode.type + " | data=" + barcode.data +
           " | quality=" + std::to_string(barcode.quality) + " | pipeline=" + pipeline;
}

// ===========================================================================
//  --image
// ===========================================================================

void run_image_mode(const Args& args) {
    const cv::Mat image = cv::imread(args.image_path);
    if (image.empty())
        throw std::runtime_error("cannot read image " + args.image_path);

    cv::Rect roi = args.roi;
    if (args.select_roi) {
        roi = select_roi_with_mouse(image, "Select ROI and press ENTER/SPACE");
        std::cout << "Selected ROI: " << roi.x << " " << roi.y << " "
                  << roi.width << " " << roi.height << "\n";
    }

    const std::string file_name = fs::path(args.image_path).filename().string();
    const ImageResult result = decode_image(image, file_name, roi, args);

    for (const auto& barcode : result.decode.decoded)
        std::cout << barcode_line(barcode, result.decode.pipeline_name) << "\n";
    if (result.decode.decoded.empty())
        std::cout << "KO | " << kNoBarcodeText << " nella ROI\n";

    save_result_images(image, result, args);
    if (args.no_show) return;

    show_fitted("barcode_result", draw_decode_result(image, result.roi, result.decode.decoded));
    if (args.debug)
        show_fitted("debug_grid", make_debug_grid(result.decode.attempted, result.decode.pipeline_name));
    cv::waitKey(0);
    cv::destroyAllWindows();
}

// ===========================================================================
//  --folder
// ===========================================================================

// Image files of a folder, sorted by name.
static std::vector<fs::path> list_image_files(const std::string& folder) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(folder)) {
        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        const auto& known = config::kImageExtensions;
        if (entry.is_regular_file() && std::find(known.begin(), known.end(), extension) != known.end())
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

// Quotes a CSV field if it contains a comma, a quote or a line break.
static std::string csv_field(const std::string& text) {
    if (text.find_first_of(",\"\n\r") == std::string::npos) return text;
    std::string quoted = "\"";
    for (const char c : text) quoted += (c == '"') ? std::string("\"\"") : std::string(1, c);
    return quoted + "\"";
}

// Appends one row: filename,status,type,data,quality,pipeline,roi_x,roi_y,roi_w,roi_h
static void write_csv_row(std::ofstream& csv, const ImageResult& result) {
    if (!csv.is_open()) return;
    csv << csv_field(result.file_name) << ",";
    if (result.unreadable) {
        csv << "SKIP,,,,,,,,\n";
        return;
    }
    if (result.decode.decoded.empty()) {
        csv << "KO,,,,";
    } else {
        const DecodedBarcode& barcode = result.decode.decoded[0];
        csv << "OK," << csv_field(barcode.type) << "," << csv_field(barcode.data) << ","
            << barcode.quality << "," << result.decode.pipeline_name;
    }
    const cv::Rect& r = result.roi;
    csv << "," << r.x << "," << r.y << "," << r.width << "," << r.height << "\n";
}

// Console text after "[i/n] name ... ": the result of the first barcode, KO or SKIP.
static std::string summary(const ImageResult& result) {
    if (result.unreadable) return "SKIP (immagine non leggibile)";
    if (result.decode.decoded.empty()) return "KO | " + kNoBarcodeText;
    return barcode_line(result.decode.decoded[0], result.decode.pipeline_name);
}

// Loads, decodes and saves images for one file. Runs in a worker thread with --threads.
static ImageResult process_file(const fs::path& path, const cv::Rect& roi, const Args& args) {
    const cv::Mat image = cv::imread(path.string());
    if (image.empty()) {
        ImageResult result;
        result.file_name = path.filename().string();
        result.unreadable = true;
        return result;
    }
    ImageResult result = decode_image(image, path.filename().string(), roi, args);
    save_result_images(image, result, args);
    result.decode.attempted.clear();  // free the debug images early
    return result;
}

// Parallel batch: args.num_threads workers each take the next file not yet
// started, so a slow image never keeps the other workers waiting. The main
// thread prints the results in file order as soon as each one is ready.
static void run_batch_parallel(const std::vector<fs::path>& files, const Args& args, std::ofstream& csv) {
    std::cout << "Parallel mode: " << args.num_threads << " threads\n";

    std::vector<std::promise<ImageResult>> promises(files.size());  // filled by the workers
    std::vector<std::future<ImageResult>> results;                  // read by the main thread
    for (auto& promise : promises) results.push_back(promise.get_future());

    std::atomic<std::size_t> next_file{0};
    auto worker = [&]() {
        for (std::size_t i; (i = next_file++) < files.size();) {
            try {
                promises[i].set_value(process_file(files[i], args.roi, args));
            } catch (...) {
                promises[i].set_exception(std::current_exception());  // re-thrown by get() below
            }
        }
    };
    std::vector<std::thread> workers;
    for (int t = 0; t < args.num_threads; ++t) workers.emplace_back(worker);

    try {
        for (std::size_t i = 0; i < files.size(); ++i) {
            const ImageResult result = results[i].get();  // waits for file i
            std::cout << "[" << i + 1 << "/" << files.size() << "] " << result.file_name
                      << " ... " << summary(result) << "\n";
            write_csv_row(csv, result);
        }
    } catch (...) {
        next_file = files.size();  // stop the workers, then report the error
        for (auto& w : workers) w.join();
        throw;
    }
    for (auto& w : workers) w.join();
}

// What the user asked in the batch window.
enum class UserChoice { Next, ReselectRoi, Quit };

// Shows one batch result and waits for a key.
static UserChoice show_batch_result(const cv::Mat& image, const ImageResult& result,
                                    std::size_t index, std::size_t total, const Args& args) {
    cv::Mat view = draw_decode_result(image, result.roi, result.decode.decoded);
    const std::string keys = args.select_roi ? "ENTER/SPAZIO=avanti  r=riseleziona ROI  q=esci"
                                             : "ENTER/SPAZIO=avanti  q=esci";
    const std::string position = result.file_name + " [" + std::to_string(index) + "/" +
                                 std::to_string(total) + "]";
    cv::putText(view, keys, {10, std::max(30, result.roi.y - 50)}, cv::FONT_HERSHEY_SIMPLEX, 1.0,
                {255, 200, 0}, 2, cv::LINE_AA);
    cv::putText(view, position, {10, image.rows - 20}, cv::FONT_HERSHEY_SIMPLEX, 1.0,
                {255, 255, 255}, 2, cv::LINE_AA);
    show_fitted("batch_barcode", view);
    if (args.debug)
        show_fitted("debug_grid", make_debug_grid(result.decode.attempted, result.decode.pipeline_name));

    while (true) {
        const int key = cv::waitKey(0) & 0xFF;
        if (key == 'q' || key == 27) return UserChoice::Quit;          // 27 = ESC
        if (key == 13 || key == 32) return UserChoice::Next;           // ENTER, SPACE
        if ((key == 'r' || key == 'R') && args.select_roi) return UserChoice::ReselectRoi;
    }
}

// Sequential batch: one image at a time, with optional windows and ROI selection.
// Returns false if the user quit before the end.
static bool run_batch_sequential(const std::vector<fs::path>& files, const Args& args, std::ofstream& csv) {
    cv::Rect roi = args.roi;                 // kWholeImageRoi unless --roi was given
    bool roi_to_select = args.select_roi;    // ask on the first readable image

    for (std::size_t i = 0; i < files.size(); ++i) {
        const std::string file_name = files[i].filename().string();
        const cv::Mat image = cv::imread(files[i].string());
        if (image.empty()) {
            ImageResult skipped;
            skipped.file_name = file_name;
            skipped.unreadable = true;
            std::cout << "[" << i + 1 << "/" << files.size() << "] " << file_name << " ... "
                      << summary(skipped) << "\n";
            write_csv_row(csv, skipped);
            continue;
        }

        if (roi_to_select) {
            try {
                roi = select_roi_with_mouse(image, "ROI [" + file_name + "] - ENTER=confirm  ESC=whole image");
            } catch (const std::exception&) {
                roi = kWholeImageRoi;  // ESC: use the whole image
            }
            roi_to_select = false;
        }

        ImageResult result = decode_image(image, file_name, roi, args);
        std::cout << "[" << i + 1 << "/" << files.size() << "] " << file_name << " ... "
                  << summary(result) << std::endl;

        // Window: the user may reselect the ROI (kept for the next images too).
        UserChoice choice = UserChoice::Next;
        while (!args.no_show) {
            choice = show_batch_result(image, result, i + 1, files.size(), args);
            if (choice != UserChoice::ReselectRoi) break;
            try {
                roi = select_roi_with_mouse(image, "New ROI [" + file_name + "] - ENTER=confirm");
                result = decode_image(image, file_name, roi, args);
                std::cout << "  new ROI -> " << summary(result) << "\n";
            } catch (const std::exception&) {
                std::cout << "  ROI unchanged\n";
            }
        }

        // The final result of this image (after any ROI change) is the one saved.
        write_csv_row(csv, result);
        save_result_images(image, result, args);
        if (choice == UserChoice::Quit) return false;
    }
    return true;
}

void run_batch_mode(const Args& args) {
    const std::vector<fs::path> files = list_image_files(args.folder_path);
    if (files.empty())
        throw std::runtime_error("no images found in " + args.folder_path);
    std::cout << "Found " << files.size() << " images in " << args.folder_path << "\n";

    std::ofstream csv;
    if (!args.output_csv.empty()) {
        csv.open(args.output_csv);
        if (!csv.is_open())
            throw std::runtime_error("cannot open CSV file " + args.output_csv);
        csv << "filename,status,type,data,quality,pipeline,roi_x,roi_y,roi_w,roi_h\n";
    }

    bool completed = true;
    if (args.num_threads > 1)
        run_batch_parallel(files, args, csv);
    else
        completed = run_batch_sequential(files, args, csv);
    cv::destroyAllWindows();

    std::cout << (completed ? "\nBatch complete.\n" : "\nBatch stopped by the user.\n");
    if (!args.output_csv.empty())       std::cout << "CSV: " << args.output_csv << "\n";
    if (!args.save_overlay_dir.empty()) std::cout << "Overlay images: " << args.save_overlay_dir << "\n";
    if (!args.save_debug_dir.empty())   std::cout << "Debug grids: " << args.save_debug_dir << "\n";
}
