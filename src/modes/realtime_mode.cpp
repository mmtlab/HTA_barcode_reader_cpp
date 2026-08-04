/**
 * @file realtime_mode.cpp
 * @brief Implementazione delle modalità real-time: webcam, Basler, libcamera.
 *        Implementation of real-time modes: webcam, Basler, libcamera.
 *
 * Il loop di decodifica usa una strategia a due velocità / The decode loop uses
 * a two-speed strategy:
 *   - Frame "rapidi" (ogni process_every): prova solo quick_candidates step.
 *     "Quick" frames (every process_every): try only quick_candidates steps.
 *   - Frame "completi" (ogni full_search_every decode): prova tutta la pipeline.
 *     "Full" frames (every full_search_every decodes): try the whole pipeline.
 *
 * Il risultato è cachato tra un frame e l'altro per garantire una visualizzazione
 * fluida anche quando il decode fallisce su un frame singolo.
 * The result is cached between frames to ensure smooth display even when
 * decoding fails on a single frame.
 */

#include "modes/realtime_mode.hpp"

#include "display.hpp"
#include "preprocessing.hpp"
#include "roi_utils.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#ifdef HAS_PYLON
#  include <pylon/PylonIncludes.h>
#endif

// ============================================================================
//  run_decode_step
// ============================================================================

void run_decode_step(const cv::Mat& roi_img, RealtimeState& st,
                     bool show_debug,
                     int process_every,
                     int quick_candidates,
                     int full_search_every) {
    st.frame_counter++;
    const bool should_process = (st.frame_counter % std::max(1, process_every)) == 0;

    if (should_process) {
        // Determina il numero massimo di candidati per questo frame.
        // Determine max candidates for this frame.
        const bool do_full = (st.frame_counter % std::max(1, full_search_every)) == 0;
        const int  max_c   = do_full ? -1 : std::max(1, quick_candidates);

        const auto res = decode_with_preprocessing(roi_img, show_debug, max_c);

        if (!res.decoded.empty()) {
            const std::string out = res.decoded[0].type + ": " + res.decoded[0].data;

            // Evita stampe ridondanti (dedup su stringa) / Avoid redundant prints (string dedup)
            if (out != st.last_printed_text) {
                std::cout << "OK | " << out
                          << " | quality="  << res.decoded[0].quality
                          << " | pipeline=" << res.pipeline_name << "\n";
                st.last_printed_text = out;
            }
            st.cached_text = out;
            st.cached_ok   = true;
            st.cached_age  = 0;
        } else {
            ++st.cached_age;
            // Invalida la cache dopo process_every frame senza decode.
            // Invalidate cache after process_every frames without decode.
            if (st.cached_age > process_every) {
                st.cached_text.clear();
                st.cached_ok = false;
            }
        }

        // Aggiorna la view di debug / Update debug view
        if (show_debug) {
            if (!res.winning_image.empty()) {
                st.last_debug_view  = ensure_bgr(res.winning_image);
                st.last_debug_title = "debug_live_win_" + res.pipeline_name;
            } else if (!res.attempted.empty()) {
                st.last_debug_view  = ensure_bgr(res.attempted.back().second);
                st.last_debug_title = "debug_live_last_" + res.attempted.back().first;
            }
        }
    } else {
        // Frame saltato: incrementa età cache / Skipped frame: increment cache age
        ++st.cached_age;
        if (st.cached_age > process_every) {
            st.cached_text.clear();
            st.cached_ok = false;
        }
    }

    // Mostra finestra debug (se abilitata e disponibile) / Show debug window (if enabled and available)
    if (show_debug && !st.last_debug_view.empty()) {
        const auto [dd, _s] = resize_for_display(st.last_debug_view, 180);
        cv::imshow("debug_live", dd);
        cv::setWindowTitle("debug_live", st.last_debug_title);
    }
}

// ============================================================================
//  Helper: testo HUD camera / Camera HUD text helper
// ============================================================================

/**
 * @brief Aggiunge il testo HUD standard al frame visualizzato.
 *        Adds standard HUD text to the displayed frame.
 *
 * @param vis           Frame con overlay barcode già applicato.
 *                      Frame with barcode overlay already applied.
 * @param process_every Parametro --process-every corrente.
 * @param quick         Parametro --quick-candidates corrente.
 * @param full_every    Parametro --full-search-every corrente.
 */
static void add_camera_hud(cv::Mat& vis, int process_every, int quick, int full_every) {
    cv::putText(vis, "Premi q per uscire / Press q to quit",
                {20, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {255, 255, 255}, 2, cv::LINE_AA);

    char info[256];
    std::snprintf(info, sizeof(info),
                  "decode ogni %d frame | quick=%d | full=%d",
                  process_every, quick, full_every);
    cv::putText(vis, info, {20, 60},
                cv::FONT_HERSHEY_SIMPLEX, 0.6, {255, 255, 255}, 1, cv::LINE_AA);
}

// ============================================================================
//  run_webcam_mode
// ============================================================================

void run_webcam_mode(const Args& args) {
    cv::VideoCapture cap(args.camera_index);
    if (!cap.isOpened())
        throw std::runtime_error("Impossibile aprire webcam con indice " +
                                 std::to_string(args.camera_index));

    // Legge il primo frame per la selezione ROI / Read first frame for ROI selection
    cv::Mat first;
    if (!cap.read(first) || first.empty()) {
        cap.release();
        throw std::runtime_error("Impossibile leggere il primo frame dalla webcam");
    }

    // Risolve la ROI / Resolve ROI
    ROI sel;
    if (args.select_roi)
        sel = select_roi_on_image(first, "Seleziona ROI webcam");
    else if (args.roi_set)
        sel = clamp_roi(args.roi.x, args.roi.y, args.roi.w, args.roi.h,
                        first.cols, first.rows);
    else
        sel = {0, 0, first.cols, first.rows};

    cv::namedWindow("barcode_webcam", cv::WINDOW_NORMAL);
    if (args.debug)
        cv::namedWindow("debug_live", cv::WINDOW_NORMAL);

    RealtimeState state;
    cv::Mat frame;

    while (cap.read(frame) && !frame.empty()) {
        const ROI r = clamp_roi(sel.x, sel.y, sel.w, sel.h, frame.cols, frame.rows);
        const cv::Mat roi_img = frame(cv::Rect(r.x, r.y, r.w, r.h));

        run_decode_step(roi_img, state, args.debug,
                        args.process_every, args.quick_candidates, args.full_search_every);

        cv::Mat vis = draw_live_overlay(frame, r, state);
        add_camera_hud(vis, args.process_every, args.quick_candidates, args.full_search_every);

        const auto [vd, _s] = resize_for_display(vis);
        cv::imshow("barcode_webcam", vd);

        if ((cv::waitKey(1) & 0xFF) == 'q') break;
    }

    cap.release();
    cv::destroyAllWindows();
}

// ============================================================================
//  run_basler_mode  (opzionale / optional - richiede Pylon SDK)
// ============================================================================

#ifdef HAS_PYLON
void run_basler_mode(const Args& args) {
    Pylon::CTlFactory& factory = Pylon::CTlFactory::GetInstance();
    Pylon::DeviceInfoList_t devices;
    factory.EnumerateDevices(devices);

    if (devices.empty())
        throw std::runtime_error("Nessuna camera Basler trovata / No Basler camera found");

    // Selezione device per seriale o indice / Select device by serial or index
    int dev_idx = -1;
    if (!args.basler_serial.empty()) {
        for (int i = 0; i < static_cast<int>(devices.size()); ++i) {
            if (std::string(devices[i].GetSerialNumber().c_str()) == args.basler_serial) {
                dev_idx = i; break;
            }
        }
        if (dev_idx < 0) {
            std::string avail;
            for (const auto& d : devices) {
                if (!avail.empty()) avail += ", ";
                avail += d.GetSerialNumber().c_str();
            }
            throw std::runtime_error("Seriale Basler '" + args.basler_serial +
                                     "' non trovato. Disponibili / Available: " + avail);
        }
    } else {
        if (args.device_index < 0 || args.device_index >= static_cast<int>(devices.size()))
            throw std::runtime_error("Indice Basler non valido / Invalid Basler index: " +
                                     std::to_string(args.device_index) + ". Trovate / Found: " +
                                     std::to_string(devices.size()) + " camere");
        dev_idx = args.device_index;
    }

    Pylon::CInstantCamera camera(factory.CreateDevice(devices[dev_idx]));
    Pylon::CImageFormatConverter converter;
    converter.OutputPixelFormat  = Pylon::PixelType_BGR8packed;
    converter.OutputBitAlignment = Pylon::OutputBitAlignment_MsbAligned;

    camera.Open();
    camera.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly);

    try {
        // Acquisisce il primo frame per la ROI / Grab first frame for ROI
        Pylon::CGrabResultPtr grab;
        camera.RetrieveResult(2000, grab, Pylon::TimeoutHandling_ThrowException);
        if (!grab->GrabSucceeded())
            throw std::runtime_error("Impossibile leggere il primo frame dalla Basler / "
                                     "Cannot read first frame from Basler");

        Pylon::CPylonImage pimg;
        converter.Convert(pimg, grab);
        cv::Mat first(static_cast<int>(pimg.GetHeight()),
                      static_cast<int>(pimg.GetWidth()),
                      CV_8UC3, pimg.GetBuffer());
        first = first.clone();  // copia sicura fuori dal buffer Pylon / safe copy out of Pylon buffer

        ROI sel;
        if (args.select_roi)
            sel = select_roi_on_image(first, "Seleziona ROI Basler");
        else if (args.roi_set)
            sel = clamp_roi(args.roi.x, args.roi.y, args.roi.w, args.roi.h,
                            first.cols, first.rows);
        else
            sel = {0, 0, first.cols, first.rows};

        cv::namedWindow("barcode_basler", cv::WINDOW_NORMAL);
        if (args.debug)
            cv::namedWindow("debug_live", cv::WINDOW_NORMAL);

        RealtimeState state;

        while (camera.IsGrabbing()) {
            camera.RetrieveResult(2000, grab, Pylon::TimeoutHandling_ThrowException);
            if (!grab->GrabSucceeded()) continue;

            Pylon::CPylonImage pi;
            converter.Convert(pi, grab);
            cv::Mat frame(static_cast<int>(pi.GetHeight()),
                          static_cast<int>(pi.GetWidth()),
                          CV_8UC3, pi.GetBuffer());
            frame = frame.clone();  // copia sicura / safe copy

            const ROI r = clamp_roi(sel.x, sel.y, sel.w, sel.h, frame.cols, frame.rows);
            const cv::Mat roi_img = frame(cv::Rect(r.x, r.y, r.w, r.h));

            run_decode_step(roi_img, state, args.debug,
                            args.process_every, args.quick_candidates, args.full_search_every);

            cv::Mat vis = draw_live_overlay(frame, r, state);
            add_camera_hud(vis, args.process_every, args.quick_candidates, args.full_search_every);

            const auto [vd, _s] = resize_for_display(vis);
            cv::imshow("barcode_basler", vd);

            if ((cv::waitKey(1) & 0xFF) == 'q') break;
        }
    } catch (...) {
        // Cleanup garantito anche in caso di eccezione / Guaranteed cleanup on exception
        if (camera.IsGrabbing())  camera.StopGrabbing();
        if (camera.IsOpen())      camera.Close();
        cv::destroyAllWindows();
        throw;
    }

    if (camera.IsGrabbing())  camera.StopGrabbing();
    if (camera.IsOpen())      camera.Close();
    cv::destroyAllWindows();
}
#endif // HAS_PYLON

// ============================================================================
//  run_libcamera_mode  (opzionale / optional - richiede GStreamer)
// ============================================================================

#ifdef HAS_LIBCAMERA
void run_libcamera_mode(const Args& args) {
    /*
     * picamera2 (Python) non ha equivalente diretto in C++.
     * Usiamo una pipeline GStreamer con libcamerasrc, che richiede
     * OpenCV compilato con GStreamer e libcamera installata.
     *
     * picamera2 (Python) has no direct C++ equivalent.
     * We use a GStreamer pipeline with libcamerasrc, requiring
     * OpenCV built with GStreamer support and libcamera installed.
     */
    const std::string pipeline =
        "libcamerasrc ! video/x-raw,width=" + std::to_string(args.libcamera_width) +
        ",height=" + std::to_string(args.libcamera_height) +
        ",format=BGR ! videoconvert ! video/x-raw,format=BGR ! appsink";

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened())
        throw std::runtime_error(
            "Impossibile aprire camera libcamera.\n"
            "Verifica che GStreamer e libcamera siano installati e che OpenCV "
            "sia compilato con supporto GStreamer.\n"
            "Cannot open libcamera. Check GStreamer + libcamera installation "
            "and that OpenCV was built with GStreamer support.");

    cv::Mat first;
    if (!cap.read(first) || first.empty()) {
        cap.release();
        throw std::runtime_error("Impossibile leggere il primo frame dalla camera libcamera / "
                                 "Cannot read first frame from libcamera");
    }

    // Normalizza canali / Normalize channels
    auto normalize_channels = [](cv::Mat& m) {
        if (m.channels() == 1) cv::cvtColor(m, m, cv::COLOR_GRAY2BGR);
        else if (m.channels() == 4) cv::cvtColor(m, m, cv::COLOR_BGRA2BGR);
    };
    normalize_channels(first);

    ROI sel;
    if (args.select_roi)
        sel = select_roi_on_image(first, "Seleziona ROI libcamera");
    else if (args.roi_set)
        sel = clamp_roi(args.roi.x, args.roi.y, args.roi.w, args.roi.h,
                        first.cols, first.rows);
    else
        sel = {0, 0, first.cols, first.rows};

    cv::namedWindow("barcode_libcamera", cv::WINDOW_NORMAL);
    if (args.debug)
        cv::namedWindow("debug_live", cv::WINDOW_NORMAL);

    RealtimeState state;
    cv::Mat frame;

    while (cap.read(frame) && !frame.empty()) {
        normalize_channels(frame);

        const ROI r = clamp_roi(sel.x, sel.y, sel.w, sel.h, frame.cols, frame.rows);
        const cv::Mat roi_img = frame(cv::Rect(r.x, r.y, r.w, r.h));

        run_decode_step(roi_img, state, args.debug,
                        args.process_every, args.quick_candidates, args.full_search_every);

        cv::Mat vis = draw_live_overlay(frame, r, state);
        add_camera_hud(vis, args.process_every, args.quick_candidates, args.full_search_every);

        const auto [vd, _s] = resize_for_display(vis);
        cv::imshow("barcode_libcamera", vd);

        if ((cv::waitKey(1) & 0xFF) == 'q') break;
    }

    cap.release();
    cv::destroyAllWindows();
}
#endif // HAS_LIBCAMERA
