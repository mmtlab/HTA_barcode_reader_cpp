# Barcode Reader (C++) — webcam branch

🇮🇹 Versione italiana: [README_ITA.md](README_ITA.md)

> This is the `webcam` branch: the program only reads barcodes **live from a webcam**.
> Single images, folders (batch), CSV output and libcamera are available on the `main` branch.

## 1. What the program does

`barcode_reader` opens a webcam, shows the video and reads 1D barcodes and QR codes in real time with [ZBar](https://github.com/mchehab/zbar) and [OpenCV](https://opencv.org/). Each new code read is printed once on the console:

```
OK | EAN13: 5901234123457 | quality=299 | pipeline=gray
```

Only the region of interest (ROI) is analysed. The ROI is given in pixels (`--roi X Y W H`), drawn with the mouse on the first frame (`--select-roi`), or is the whole frame.

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

To keep the frame rate high, not every frame is fully analysed:
- one frame every `--process-every` is decoded;
- a normal decode tries only the first `--quick-candidates` steps;
- every `--full-search-every` decodes, all 16 steps are tried.

The parameters of the steps are in [config/barcode_config.hpp](config/barcode_config.hpp).

## 2. Project layout

```
HTA_barcode_reader_cpp/
├── CMakeLists.txt          build script
├── main.cpp                entry point: reads the options, starts the webcam loop
├── config/
│   └── barcode_config.hpp  tunable constants (pipeline, display)
├── include/                headers (one per source file)
│   ├── barcode_types.hpp   shared structs and command-line defaults
│   ├── cli_args.hpp
│   ├── decoder.hpp
│   ├── display.hpp
│   └── webcam_mode.hpp
└── src/
    ├── cli_args.cpp        command-line parsing and checks
    ├── decoder.cpp         ZBar wrapper + preprocessing pipeline
    ├── display.cpp         drawing, windows, mouse ROI selection
    └── webcam_mode.cpp     live loop: grab, decode, draw, show
```

## 3. Requirements

| Component | Version | Notes |
|-----------|---------|-------|
| C++ compiler | C++17 | GCC ≥ 9, Clang ≥ 9 or Visual Studio 2019/2022 |
| CMake | ≥ 3.16 | |
| OpenCV | 4.x | modules `core imgproc highgui videoio` |
| ZBar | 0.23 | barcode decoder |
| Webcam | | any camera that OpenCV can open |

Built on Ubuntu 26.04 with GCC 15.2, CMake 4.4, OpenCV 4.10 and ZBar 0.23.93. The Windows instructions below have not been tested by the authors of this README.

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
| `-DNATIVE_CPU=OFF` | portable binary (no `-march=native`); use it if the program will run on another computer |
| `-DCMAKE_BUILD_TYPE=Debug` | debug build |

### 4.3 Run

```bash
./build/barcode_reader --help
./build/barcode_reader                      # default webcam, whole frame
./build/barcode_reader --select-roi         # draw the ROI with the mouse
./build/barcode_reader --camera-index 1 --roi 400 300 800 400
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
.\build\Release\barcode_reader.exe
.\build\Release\barcode_reader.exe --select-roi
.\build\Release\barcode_reader.exe --camera-index 1 --roi 400 300 800 400
```

## 6. Options

| Option | Default | Description |
|--------|---------|-------------|
| `--camera-index N` | 0 | webcam number |
| `--roi X Y W H` | whole frame | ROI in pixels (top-left corner, width, height) |
| `--select-roi` | | draw the ROI with the mouse on the first frame (ENTER/SPACE to confirm) |
| `--process-every N` | 2 | decode one frame every N |
| `--quick-candidates N` | 4 | number of steps tried on a normal decode |
| `--full-search-every N` | 10 | every N decodes, try all steps |
| `--debug` | off | also show the preprocessing image in a second window |
| `--help`, `-h` | | show the options |

`--webcam` is accepted and ignored, so command lines written for the `main` branch still work.

Press `q` in the video window to quit.

## 7. Troubleshooting

| Problem | What to check |
|---------|---------------|
| `ZBar not found` while running CMake | Linux: `libzbar-dev` and `pkg-config` installed. Windows: `mchehab-zbar` installed and `CMAKE_TOOLCHAIN_FILE` given. |
| `cannot open webcam` | try `--camera-index 1`, `2`, …; on Linux list cameras with `ls /dev/video*` |
| Code visible but not read | make the ROI tighter around the code; use `--debug` to see the processed image; decrease `--full-search-every` |
| Video too slow | increase `--process-every`, decrease `--quick-candidates` |
