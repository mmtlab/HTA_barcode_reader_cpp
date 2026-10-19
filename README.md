# Barcode Reader (C++)

🇮🇹 Versione italiana: [README_ITA.md](README_ITA.md)

## 1. What the program does

`barcode_reader` reads 1D barcodes and QR codes with [ZBar](https://github.com/mchehab/zbar) and [OpenCV](https://opencv.org/). It works on:

| Mode | Option | Input |
|------|--------|-------|
| Single image | `--image <file>` | one image file |
| Batch | `--folder <dir>` | every image in a folder (`.jpg .jpeg .png .bmp .tif .tiff .webp`) |
| Webcam | `--webcam` | any camera that OpenCV can open |
| libcamera | `--libcamera` | Raspberry Pi / Arducam camera (GStreamer, optional) |

Only the region of interest (ROI) is analysed. The ROI is given in pixels (`--roi X Y W H`), drawn with the mouse (`--select-roi`), or is the whole image.

If ZBar cannot read the plain grayscale ROI, the program tries progressively stronger image processing steps and stops at the first one that works:

| # | Step name | Operation |
|---|-----------|-----------|
| 1 | `gray` | grayscale |
| 2 | `gray_clahe` | local contrast enhancement (CLAHE) |
| 3 | `denoise` | bilateral filter on step 2 |
| 4 | `thr_adapt` | adaptive threshold |
| 5 | `thr_otsu` | Otsu threshold |
| 6 | `morph_open_horiz` | horizontal morphological opening of step 5 |
| 7–16 | `<step>_x2`, `<step>_x3` | steps 1–5 enlarged 2× and 3× (skipped for large ROIs) |

The parameters of these steps are in [config/barcode_config.hpp](config/barcode_config.hpp).

## 2. Project layout

```
HTA_barcode_reader_cpp/
├── CMakeLists.txt          build script
├── main.cpp                entry point: reads the options, starts the mode
├── config/
│   └── barcode_config.hpp  tunable constants (pipeline, display, file types)
├── include/                headers (one per source file)
│   ├── barcode_types.hpp   shared structs and command-line defaults
│   ├── cli_args.hpp
│   ├── decoder.hpp
│   ├── display.hpp
│   └── modes.hpp
└── src/
    ├── cli_args.cpp        command-line parsing and checks
    ├── decoder.cpp         ZBar wrapper + preprocessing pipeline
    ├── display.cpp         drawing, debug grid, windows, mouse ROI selection
    ├── image_modes.cpp     --image and --folder (sequential or parallel)
    └── camera_modes.cpp    --webcam, --libcamera (one shared loop)
```

## 3. Requirements

| Component | Version | Notes |
|-----------|---------|-------|
| C++ compiler | C++17 | GCC ≥ 9, Clang ≥ 9 or Visual Studio 2019/2022 |
| CMake | ≥ 3.16 | |
| OpenCV | 4.x | modules `core imgproc imgcodecs highgui videoio` |
| ZBar | 0.23 | barcode decoder |
| GStreamer + libcamera | optional | only for `--libcamera`; OpenCV must be built with GStreamer |

Tested on Ubuntu 26.04 with GCC 15.2, CMake 4.4, OpenCV 4.10 and ZBar 0.23.93. The Windows instructions below have not been tested by the authors of this README.

## 4. Linux

### 4.1 Install the libraries

Debian / Ubuntu / Raspberry Pi OS:
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libopencv-dev libzbar-dev
```

Fedora:
```bash
sudo dnf install -y gcc-c++ cmake pkgconf-pkg-config opencv-devel zbar-devel
```

Optional, for `--libcamera` on Raspberry Pi:
```bash
sudo apt install -y libcamera-dev gstreamer1.0-libcamera libgstreamer1.0-dev
```

### 4.2 Build

From the project folder:
```bash
cmake -B build -S .
cmake --build build -j
```
The program is `build/barcode_reader`. The default build type is Release.

Build options (add them to the first command):

| Option | Effect |
|--------|--------|
| `-DUSE_LIBCAMERA=ON` | libcamera support |
| `-DNATIVE_CPU=OFF` | portable binary (no `-march=native`); use it if the program will run on another computer |
| `-DCMAKE_BUILD_TYPE=Debug` | debug build |

### 4.3 Run

```bash
./build/barcode_reader --help
./build/barcode_reader --image photo.jpg --select-roi
./build/barcode_reader --folder images --roi 1617 1402 830 459 --no-show --threads 8 --output results.csv
./build/barcode_reader --webcam --select-roi
```

## 5. Windows

### 5.1 Install the tools and libraries

1. Install **Visual Studio 2022** (Community is enough) with the workload *Desktop development with C++*. It includes CMake.
2. Install [vcpkg](https://learn.microsoft.com/vcpkg/get_started/get-started) and the libraries. In *Developer PowerShell for VS 2022*:
   ```powershell
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   C:\vcpkg\vcpkg install opencv4:x64-windows mchehab-zbar:x64-windows
   ```
   In vcpkg the ZBar package is called `mchehab-zbar`. The first OpenCV build takes a long time.

### 5.2 Build

From the project folder, in *Developer PowerShell for VS 2022*:
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```
The program is `build\Release\barcode_reader.exe`. vcpkg copies the needed DLLs next to it.

### 5.3 Run

```powershell
.\build\Release\barcode_reader.exe --help
.\build\Release\barcode_reader.exe --image photo.jpg --select-roi
.\build\Release\barcode_reader.exe --folder images --roi 1617 1402 830 459 --no-show --threads 8 --output results.csv
.\build\Release\barcode_reader.exe --webcam --select-roi
```

## 6. Options

| Option | Default | Description |
|--------|---------|-------------|
| `--image <file>` | | single image; needs `--roi` or `--select-roi` |
| `--folder <dir>` | | all images in a folder |
| `--webcam` / `--libcamera` | | live camera modes |
| `--roi X Y W H` | whole image | ROI in pixels (top-left corner, width, height) |
| `--select-roi` | | draw the ROI with the mouse; in batch mode on the first image, then reused |
| `--output <file.csv>` | | write batch results to CSV |
| `--save-overlay <dir>` | | save each image with ROI and result drawn on it |
| `--save-debug <dir>` | | save a grid with all 16 preprocessing steps of each image (slower) |
| `--no-show` | off | no windows |
| `--threads N` | 1 | parallel workers for `--folder`; needs `--no-show` and no `--select-roi` |
| `--debug` | off | show the preprocessing steps in a window |
| `--camera-index N` | 0 | webcam number |
| `--libcamera-width W` / `--libcamera-height H` | 1920 / 1080 | libcamera frame size [px] |
| `--process-every N` | 2 | live: decode one frame every N |
| `--quick-candidates N` | 4 | live: number of steps tried on a normal decode |
| `--full-search-every N` | 10 | live: every N decodes, try all steps |

Keys in the windows: `SPACE`/`ENTER` next image, `r` reselect ROI (with `--select-roi`), `q`/`ESC` quit.

## 7. Output

Console, one line per image:
```
[1/18] a.png ... OK | type=EAN13 | data=5901234123457 | quality=299 | pipeline=gray
[6/18] b.png ... KO | Nessun barcode letto
[7/18] c.jpg ... SKIP (immagine non leggibile)
```
The result strings are kept identical to the earlier Python version, so existing scripts that parse them still work.

CSV (`--output`), one row per image:

| Column | Content |
|--------|---------|
| `filename` | image file name |
| `status` | `OK`, `KO` (nothing read) or `SKIP` (file not readable) |
| `type`, `data`, `quality` | symbology, decoded text, ZBar quality score |
| `pipeline` | preprocessing step that worked |
| `roi_x`, `roi_y`, `roi_w`, `roi_h` | ROI actually used [px] |

## 8. Troubleshooting

| Problem | What to check |
|---------|---------------|
| `ZBar not found` while running CMake | Linux: `libzbar-dev` and `pkg-config` installed. Windows: `mchehab-zbar` installed and `CMAKE_TOOLCHAIN_FILE` given. |
| `cannot open webcam` | try `--camera-index 1`, `2`, …; on Linux list cameras with `ls /dev/video*` |
| `libcamera support not built` | rebuild with `-DUSE_LIBCAMERA=ON` |
| `KO` on images that contain a code | make the ROI tighter around the code; use `--debug` or `--save-debug` to see the steps |
| Live mode too slow | increase `--process-every`, decrease `--quick-candidates` |
