# Barcode Reader (C++) — branch webcam

🇬🇧 English version: [README.md](README.md)

> Questo è il branch `webcam`: il programma legge i codici **solo dal vivo da una webcam**.
> Immagini singole, cartelle (batch), output CSV e libcamera sono disponibili nel branch `main`.

## 1. Cosa fa il programma

`barcode_reader` apre una webcam, mostra il video e legge in tempo reale codici a barre 1D e QR code con [ZBar](https://github.com/mchehab/zbar) e [OpenCV](https://opencv.org/). Ogni nuovo codice letto viene stampato una volta sulla console:

```
OK | EAN13: 5901234123457 | quality=299 | pipeline=gray
```

Viene analizzata solo la regione di interesse (ROI). La ROI si indica in pixel (`--roi X Y W H`), si disegna col mouse sul primo frame (`--select-roi`), oppure è l'intero frame.

Se ZBar non riesce a leggere la ROI in scala di grigi, il programma prova elaborazioni via via più forti e si ferma alla prima che funziona:

| # | Nome passo | Operazione |
|---|------------|------------|
| 1 | `gray` | scala di grigi |
| 2 | `gray_clahe` | aumento del contrasto locale (CLAHE) |
| 3 | `denoise` | filtro bilaterale sul passo 2 |
| 4 | `thr_adapt` | soglia adattiva |
| 5 | `thr_otsu` | soglia di Otsu |
| 6 | `morph_open_horiz` | apertura morfologica orizzontale del passo 5 |
| 7–16 | `<passo>_x2`, `<passo>_x3` | passi 1–5 ingranditi 2× e 3× (saltati per ROI grandi) |

Per mantenere alto il frame rate, non tutti i frame vengono analizzati completamente:
- viene decodificato un frame ogni `--process-every`;
- una decodifica normale prova solo i primi `--quick-candidates` passi;
- ogni `--full-search-every` decodifiche vengono provati tutti i 16 passi.

I parametri dei passi sono in [config/barcode_config.hpp](config/barcode_config.hpp).

## 2. Struttura del progetto

```
HTA_barcode_reader_cpp/
├── CMakeLists.txt          script di compilazione
├── main.cpp                punto di ingresso: legge le opzioni, avvia il ciclo webcam
├── config/
│   └── barcode_config.hpp  costanti modificabili (pipeline, visualizzazione)
├── include/                header (uno per file sorgente)
│   ├── barcode_types.hpp   strutture condivise e valori di default delle opzioni
│   ├── cli_args.hpp
│   ├── decoder.hpp
│   ├── display.hpp
│   └── webcam_mode.hpp
└── src/
    ├── cli_args.cpp        lettura e controllo delle opzioni
    ├── decoder.cpp         interfaccia ZBar + pipeline di preprocessing
    ├── display.cpp         disegno, finestre, selezione ROI col mouse
    └── webcam_mode.cpp     ciclo dal vivo: acquisizione, decodifica, disegno, visualizzazione
```

## 3. Requisiti

| Componente | Versione | Note |
|------------|----------|------|
| Compilatore C++ | C++17 | GCC ≥ 9, Clang ≥ 9 oppure Visual Studio 2019/2022 |
| CMake | ≥ 3.16 | |
| OpenCV | 4.x | moduli `core imgproc highgui videoio` |
| ZBar | 0.23 | decodifica dei codici |
| Webcam | | qualsiasi camera apribile da OpenCV |

Compilato su Ubuntu 26.04 con GCC 15.2, CMake 4.4, OpenCV 4.10 e ZBar 0.23.93. Le istruzioni per Windows riportate sotto non sono state provate dagli autori di questo README.

## 4. Linux

### 4.1 Installare le librerie

Debian / Ubuntu / Raspberry Pi OS:
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libopencv-dev libzbar-dev
```

Fedora:
```bash
sudo dnf install -y gcc-c++ cmake pkgconf-pkg-config opencv-devel zbar-devel
```

### 4.2 Compilare

Dalla cartella del progetto:
```bash
cmake -B build -S .
cmake --build build -j
```
Il programma è `build/barcode_reader`. Il tipo di build predefinito è Release.

Opzioni di compilazione (da aggiungere al primo comando):

| Opzione | Effetto |
|---------|---------|
| `-DNATIVE_CPU=OFF` | eseguibile portabile (senza `-march=native`); da usare se il programma girerà su un altro computer |
| `-DCMAKE_BUILD_TYPE=Debug` | build di debug |

### 4.3 Eseguire

```bash
./build/barcode_reader --help
./build/barcode_reader                      # webcam predefinita, intero frame
./build/barcode_reader --select-roi         # ROI disegnata col mouse
./build/barcode_reader --camera-index 1 --roi 400 300 800 400
```

## 5. Windows

### 5.1 Installare strumenti e librerie

1. Installare **Visual Studio 2022** (basta la versione Community) con il carico di lavoro *Sviluppo di applicazioni desktop con C++*. CMake è incluso.
2. Installare [vcpkg](https://learn.microsoft.com/vcpkg/get_started/get-started) e le librerie. Nella *Developer PowerShell for VS 2022*:
   ```powershell
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   C:\vcpkg\vcpkg install opencv4:x64-windows mchehab-zbar:x64-windows
   ```
   In vcpkg il pacchetto ZBar si chiama `mchehab-zbar`. La prima compilazione di OpenCV richiede parecchio tempo.

### 5.2 Compilare

Dalla cartella del progetto, nella *Developer PowerShell for VS 2022*:
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```
Il programma è `build\Release\barcode_reader.exe`. vcpkg copia accanto all'eseguibile le DLL necessarie.

### 5.3 Eseguire

```powershell
.\build\Release\barcode_reader.exe --help
.\build\Release\barcode_reader.exe
.\build\Release\barcode_reader.exe --select-roi
.\build\Release\barcode_reader.exe --camera-index 1 --roi 400 300 800 400
```

## 6. Opzioni

| Opzione | Default | Descrizione |
|---------|---------|-------------|
| `--camera-index N` | 0 | numero della webcam |
| `--roi X Y W H` | intero frame | ROI in pixel (angolo in alto a sinistra, larghezza, altezza) |
| `--select-roi` | | ROI disegnata col mouse sul primo frame (INVIO/SPAZIO per confermare) |
| `--process-every N` | 2 | decodifica un frame ogni N |
| `--quick-candidates N` | 4 | numero di passi provati in una decodifica normale |
| `--full-search-every N` | 10 | ogni N decodifiche prova tutti i passi |
| `--debug` | disattivo | mostra anche l'immagine elaborata in una seconda finestra |
| `--help`, `-h` | | mostra le opzioni |

`--webcam` viene accettato e ignorato, così le righe di comando scritte per il branch `main` funzionano ancora.

Premere `q` nella finestra video per uscire.

## 7. Problemi frequenti

| Problema | Cosa controllare |
|----------|------------------|
| `ZBar not found` durante CMake | Linux: `libzbar-dev` e `pkg-config` installati. Windows: `mchehab-zbar` installato e `CMAKE_TOOLCHAIN_FILE` indicato. |
| `cannot open webcam` | provare `--camera-index 1`, `2`, …; su Linux elencare le camere con `ls /dev/video*` |
| Codice visibile ma non letto | stringere la ROI attorno al codice; usare `--debug` per vedere l'immagine elaborata; diminuire `--full-search-every` |
| Video troppo lento | aumentare `--process-every`, diminuire `--quick-candidates` |
