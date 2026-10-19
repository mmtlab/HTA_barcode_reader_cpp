# Barcode Reader (C++)

🇬🇧 English version: [README.md](README.md)

## 1. Cosa fa il programma

`barcode_reader` legge codici a barre 1D e QR code con [ZBar](https://github.com/mchehab/zbar) e [OpenCV](https://opencv.org/). Funziona con:

| Modalità | Opzione | Ingresso |
|----------|---------|----------|
| Immagine singola | `--image <file>` | un file immagine |
| Batch | `--folder <cartella>` | tutte le immagini di una cartella (`.jpg .jpeg .png .bmp .tif .tiff .webp`) |
| Webcam | `--webcam` | qualsiasi camera apribile da OpenCV |
| libcamera | `--libcamera` | camera Raspberry Pi / Arducam (GStreamer, opzionale) |

Viene analizzata solo la regione di interesse (ROI). La ROI si indica in pixel (`--roi X Y W H`), si disegna col mouse (`--select-roi`), oppure è l'intera immagine.

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

I parametri dei passi sono in [config/barcode_config.hpp](config/barcode_config.hpp).

## 2. Struttura del progetto

```
HTA_barcode_reader_cpp/
├── CMakeLists.txt          script di compilazione
├── main.cpp                punto di ingresso: legge le opzioni, avvia la modalità
├── config/
│   └── barcode_config.hpp  costanti modificabili (pipeline, visualizzazione, tipi di file)
├── include/                header (uno per file sorgente)
│   ├── barcode_types.hpp   strutture condivise e valori di default delle opzioni
│   ├── cli_args.hpp
│   ├── decoder.hpp
│   ├── display.hpp
│   └── modes.hpp
└── src/
    ├── cli_args.cpp        lettura e controllo delle opzioni
    ├── decoder.cpp         interfaccia ZBar + pipeline di preprocessing
    ├── display.cpp         disegno, griglia di debug, finestre, selezione ROI col mouse
    ├── image_modes.cpp     --image e --folder (sequenziale o parallelo)
    └── camera_modes.cpp    --webcam, --libcamera (un unico ciclo comune)
```

## 3. Requisiti

| Componente | Versione | Note |
|------------|----------|------|
| Compilatore C++ | C++17 | GCC ≥ 9, Clang ≥ 9 oppure Visual Studio 2019/2022 |
| CMake | ≥ 3.16 | |
| OpenCV | 4.x | moduli `core imgproc imgcodecs highgui videoio` |
| ZBar | 0.23 | decodifica dei codici |
| GStreamer + libcamera | opzionale | solo per `--libcamera`; OpenCV deve essere compilato con GStreamer |

Provato su Ubuntu 26.04 con GCC 15.2, CMake 4.4, OpenCV 4.10 e ZBar 0.23.93. Le istruzioni per Windows riportate sotto non sono state provate dagli autori di questo README.

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

Opzionale, per `--libcamera` su Raspberry Pi:
```bash
sudo apt install -y libcamera-dev gstreamer1.0-libcamera libgstreamer1.0-dev
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
| `-DUSE_LIBCAMERA=ON` | supporto libcamera |
| `-DNATIVE_CPU=OFF` | eseguibile portabile (senza `-march=native`); da usare se il programma girerà su un altro computer |
| `-DCMAKE_BUILD_TYPE=Debug` | build di debug |

### 4.3 Eseguire

```bash
./build/barcode_reader --help
./build/barcode_reader --image foto.jpg --select-roi
./build/barcode_reader --folder immagini --roi 1617 1402 830 459 --no-show --threads 8 --output risultati.csv
./build/barcode_reader --webcam --select-roi
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
.\build\Release\barcode_reader.exe --image foto.jpg --select-roi
.\build\Release\barcode_reader.exe --folder immagini --roi 1617 1402 830 459 --no-show --threads 8 --output risultati.csv
.\build\Release\barcode_reader.exe --webcam --select-roi
```

## 6. Opzioni

| Opzione | Default | Descrizione |
|---------|---------|-------------|
| `--image <file>` | | immagine singola; richiede `--roi` o `--select-roi` |
| `--folder <cartella>` | | tutte le immagini di una cartella |
| `--webcam` / `--libcamera` | | modalità con camera dal vivo |
| `--roi X Y W H` | intera immagine | ROI in pixel (angolo in alto a sinistra, larghezza, altezza) |
| `--select-roi` | | ROI disegnata col mouse; in batch sulla prima immagine, poi riusata |
| `--output <file.csv>` | | salva i risultati del batch in CSV |
| `--save-overlay <cartella>` | | salva ogni immagine con ROI e risultato disegnati |
| `--save-debug <cartella>` | | salva una griglia con tutti i 16 passi di preprocessing di ogni immagine (più lento) |
| `--no-show` | disattivo | nessuna finestra |
| `--threads N` | 1 | thread paralleli per `--folder`; richiede `--no-show` e niente `--select-roi` |
| `--debug` | disattivo | mostra i passi di preprocessing in una finestra |
| `--camera-index N` | 0 | numero della webcam |
| `--libcamera-width W` / `--libcamera-height H` | 1920 / 1080 | dimensione del frame libcamera [px] |
| `--process-every N` | 2 | dal vivo: decodifica un frame ogni N |
| `--quick-candidates N` | 4 | dal vivo: numero di passi provati in una decodifica normale |
| `--full-search-every N` | 10 | dal vivo: ogni N decodifiche prova tutti i passi |

Tasti nelle finestre: `SPAZIO`/`INVIO` immagine successiva, `r` riseleziona la ROI (con `--select-roi`), `q`/`ESC` esci.

## 7. Output

Console, una riga per immagine:
```
[1/18] a.png ... OK | type=EAN13 | data=5901234123457 | quality=299 | pipeline=gray
[6/18] b.png ... KO | Nessun barcode letto
[7/18] c.jpg ... SKIP (immagine non leggibile)
```
Le stringhe dei risultati sono identiche alla precedente versione Python, quindi gli script che le leggono continuano a funzionare.

CSV (`--output`), una riga per immagine:

| Colonna | Contenuto |
|---------|-----------|
| `filename` | nome del file immagine |
| `status` | `OK`, `KO` (nessun codice letto) o `SKIP` (file non leggibile) |
| `type`, `data`, `quality` | simbologia, testo decodificato, punteggio di qualità ZBar |
| `pipeline` | passo di preprocessing che ha funzionato |
| `roi_x`, `roi_y`, `roi_w`, `roi_h` | ROI effettivamente usata [px] |

## 8. Problemi frequenti

| Problema | Cosa controllare |
|----------|------------------|
| `ZBar not found` durante CMake | Linux: `libzbar-dev` e `pkg-config` installati. Windows: `mchehab-zbar` installato e `CMAKE_TOOLCHAIN_FILE` indicato. |
| `cannot open webcam` | provare `--camera-index 1`, `2`, …; su Linux elencare le camere con `ls /dev/video*` |
| `libcamera support not built` | ricompilare con `-DUSE_LIBCAMERA=ON` |
| `KO` su immagini che contengono un codice | stringere la ROI attorno al codice; usare `--debug` o `--save-debug` per vedere i passi |
| Modalità dal vivo troppo lenta | aumentare `--process-every`, diminuire `--quick-candidates` |
