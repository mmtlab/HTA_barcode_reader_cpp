/**
 * camera_modes.cpp
 *
 * Live modes: webcam and libcamera (GStreamer).
 * Each mode only opens its camera and provides a "grab next frame" function;
 * the common loop run_live_loop() does the rest:
 *   1. ROI from --roi, from the mouse (--select-roi) or the whole frame;
 *   2. for each frame: decode (see update_live_decode), draw, show;
 *   3. stop when 'q' is pressed or the camera stops.
 *
 * To keep the frame rate high, not every frame is decoded:
 *   - one frame every --process-every is decoded;
 *   - a normal decode tries only the first --quick-candidates pipeline steps;
 *   - every --full-search-every decodes, all steps are tried.
 * The last text read stays on screen for --process-every frames, so it does not flicker.
 */
#include "modes.hpp"

#include "decoder.hpp"
#include "display.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

// Fills the frame with the next camera image; returns false when no more frames are available.
using FrameGrabber = std::function<bool(cv::Mat& frame)>;

// What the live loop remembers between frames.
struct LiveState {
    int frame_count  = 0;
    int decode_count = 0;
    int frames_since_last_read = 0;
    std::string shown_text;      // text drawn on the frame ("" = nothing read recently)
    std::string printed_text;    // last text printed, to avoid printing it again every frame
    cv::Mat debug_view;          // last preprocessing image (--debug)
};

// Decodes the ROI if this frame is due, and updates the state.
static void update_live_decode(const cv::Mat& roi_image, LiveState& state, const Args& args) {
    ++state.frame_count;

    if (state.frame_count % args.process_every == 0) {
        ++state.decode_count;
        const bool full_search = (state.decode_count % args.full_search_every == 0);
        const DecodeResult result = decode_with_preprocessing(
            roi_image, full_search ? -1 : args.quick_candidates,
            args.debug ? DebugImages::Attempted : DebugImages::None);

        if (args.debug && !result.attempted.empty())
            state.debug_view = result.decoded.empty() ? result.attempted.back().second
                                                      : result.winning_image;

        if (!result.decoded.empty()) {
            const DecodedBarcode& barcode = result.decoded[0];
            const std::string text = barcode.type + ": " + barcode.data;
            if (text != state.printed_text) {
                std::cout << "OK | " << text << " | quality=" << barcode.quality
                          << " | pipeline=" << result.pipeline_name << "\n";
                state.printed_text = text;
            }
            state.shown_text = text;
            state.frames_since_last_read = 0;
            return;
        }
    }
    if (++state.frames_since_last_read > args.process_every) state.shown_text.clear();
}

// Common loop of all live modes (see top of file).
static void run_live_loop(const std::string& window_name, const FrameGrabber& grab_frame,
                          const Args& args) {
    cv::Mat frame;
    if (!grab_frame(frame))
        throw std::runtime_error("cannot read the first frame from the camera");

    const cv::Rect roi = args.select_roi ? select_roi_with_mouse(frame, "Select ROI and press ENTER/SPACE")
                                         : args.roi;  // kWholeImageRoi unless --roi was given
    LiveState state;
    do {
        const cv::Rect frame_roi = clamp_roi(roi, frame.size());
        update_live_decode(frame(frame_roi), state, args);

        const bool ok = !state.shown_text.empty();
        cv::Mat view = draw_roi_and_text(frame, frame_roi, ok ? state.shown_text : kNoBarcodeText, ok);
        cv::putText(view, "Press q to quit", {20, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.7,
                    {255, 255, 255}, 2, cv::LINE_AA);
        cv::putText(view,
                    "decode every " + std::to_string(args.process_every) + " frames | quick=" +
                        std::to_string(args.quick_candidates) + " | full every " +
                        std::to_string(args.full_search_every),
                    {20, 60}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {255, 255, 255}, 1, cv::LINE_AA);
        show_fitted(window_name, view);
        if (args.debug && !state.debug_view.empty()) show_fitted("debug_live", state.debug_view, 180);
    } while ((cv::waitKey(1) & 0xFF) != 'q' && grab_frame(frame));

    cv::destroyAllWindows();
}

// ===========================================================================
//  Webcam (any camera supported by OpenCV)
// ===========================================================================

void run_webcam_mode(const Args& args) {
    cv::VideoCapture camera(args.camera_index);
    if (!camera.isOpened())
        throw std::runtime_error("cannot open webcam " + std::to_string(args.camera_index));

    run_live_loop("barcode_webcam",
                  [&camera](cv::Mat& frame) { return camera.read(frame) && !frame.empty(); }, args);
}

// ===========================================================================
//  libcamera / Arducam on Raspberry Pi (needs OpenCV with GStreamer,
//  build with -DUSE_LIBCAMERA=ON)
// ===========================================================================

#ifdef HAS_LIBCAMERA
void run_libcamera_mode(const Args& args) {
    const std::string pipeline =
        "libcamerasrc ! video/x-raw,width=" + std::to_string(args.libcamera_width) +
        ",height=" + std::to_string(args.libcamera_height) +
        ",format=BGR ! videoconvert ! video/x-raw,format=BGR ! appsink";

    cv::VideoCapture camera(pipeline, cv::CAP_GSTREAMER);
    if (!camera.isOpened())
        throw std::runtime_error("cannot open libcamera. Check that GStreamer and libcamera are "
                                 "installed and that OpenCV was built with GStreamer support.");

    auto grab_frame = [&camera](cv::Mat& frame) {
        if (!camera.read(frame) || frame.empty()) return false;
        if (frame.channels() == 1) cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR);
        if (frame.channels() == 4) cv::cvtColor(frame, frame, cv::COLOR_BGRA2BGR);
        return true;
    };
    run_live_loop("barcode_libcamera", grab_frame, args);
}
#endif  // HAS_LIBCAMERA
