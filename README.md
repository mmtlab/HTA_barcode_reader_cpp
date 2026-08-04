# Barcode Reader — C++

Lettore di barcode ad alte prestazioni in C++17, con pipeline di preprocessing multi-stadio, parallelismo batch e supporto per webcam, camera Basler e libcamera (RPi/Arducam).

> Traduzione C++ di `barcode_reader.py`. Output e interfaccia CLI identici alla versione Python.

---

## Indice

1. [Funzionalità](#funzionalità)
2. [Architettura](#architettura)
3. [Dipendenze](#dipendenze)
4. [Installazione librerie](#installazione-librerie)
5. [Compilazione](#compilazione)
6. [Utilizzo](#utilizzo)
7. [Output e formato CSV](#output-e-formato-csv)
8. [Pipeline di preprocessing](#pipeline-di-preprocessing)
9. [Parallelismo batch](#parallelismo-batch)
10. [Tuning real-time](#tuning-real-time)
11. [Troubleshooting](#troubleshooting)
12. [Confronto Python ↔ C++](#confronto-python--c)

---

## Funzionalità

| Modalità | Comando |
|----------|---------|
| **Immagine singola** | `--image <path>` |
| **Batch su cartella** | `--folder <path>` |
| **Webcam real-time** | `--webcam` |
| **Camera Basler (Pylon)** | `--basler` |
| **libcamera / Arducam (RPi)** | `--libcamera` |

- **ROI** selezionabile col mouse o specificata a mano in pixel
- **Pipeline adattiva**: da scala di grigi a upscaling 3×, con early-exit al primo decode
- **Batch parallelo**: `--threads N` per elaborare N immagini contemporaneamente
- **Export CSV** con tipo, dato, qualità, pipeline e coordinate ROI
- **Modalità headless** (`--no-show`) per esecuzione su server/CI
- **Debug visuale** con mosaico di tutti i candidati preprocessing

---

## Architettura

```
barcode/
├── CMakeLists.txt              Build system (CMake 3.16+)
├── main.cpp                    Entry point (≈55 righe / lines)
│
├── include/                    Header pubblici / Public headers
│   ├── barcode_types.hpp       Struct condivise (DecodedBarcode, ROI, Args, …)
│   ├── zbar_decode.hpp         API wrapper ZBar
│   ├── preprocessing.hpp       API pipeline preprocessing
│   ├── display.hpp             API utilità visualizzazione
│   ├── roi_utils.hpp           API gestione ROI
│   ├── cli_args.hpp            API parser CLI
│   └── modes/
│       ├── image_mode.hpp      Modalità immagine singola
│       ├── batch_mode.hpp      Modalità batch + parallelismo
│       └── realtime_mode.hpp   Modalità webcam / Basler / libcamera
│
└── src/                        Implementazioni / Implementations
    ├── zbar_decode.cpp
    ├── preprocessing.cpp       CLAHE statico, early-exit ottimizzato
    ├── display.cpp
    ├── roi_utils.cpp
    ├── cli_args.cpp
    └── modes/
        ├── image_mode.cpp
        ├── batch_mode.cpp      std::async sliding-window per parallelismo
        └── realtime_mode.cpp   Loop two-speed (quick / full)
```

---

## Dipendenze

### Obbligatorie

| Libreria | Versione | Scopo | Nota |
|----------|----------|-------|------|
| **OpenCV** | 4.x | I/O immagini, preprocessing, GUI | `core imgproc imgcodecs highgui videoio` |
| **ZBar** | qualsiasi | Decodifica barcode | Wrapper: `zbar_decode.cpp` |
| **C++17** | — | `std::filesystem`, `std::async`, structured bindings | — |

### Opzionali

| Libreria | Scopo | Flag CMake | Macro |
|----------|-------|------------|-------|
| **Pylon SDK** | Camera Basler | `-DUSE_PYLON=ON` | `HAS_PYLON` |
| **GStreamer + libcamera** | Arducam / RPi | `-DUSE_LIBCAMERA=ON` | `HAS_LIBCAMERA` |

---

## Installazione librerie

### Linux (Ubuntu / Debian)

```bash
sudo apt update
sudo apt install -y libopencv-dev libzbar-dev
```

### Windows (vcpkg)

```powershell
# Installa vcpkg se non presente
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

# Installa le librerie (la prima volta richiede diversi minuti per OpenCV)
C:\vcpkg\vcpkg install opencv4:x64-windows zbar:x64-windows
```

### Raspberry Pi / Arducam (libcamera)

```bash
sudo apt install -y libopencv-dev libzbar-dev libcamera-dev libgstreamer1.0-dev
# OpenCV deve essere compilato con GStreamer per la modalità --libcamera
```

### Basler (Pylon SDK)

Scarica e installa il [Pylon SDK](https://www.baslerweb.com/en/downloads/software-downloads/) dal sito Basler.  
CMake lo individua automaticamente tramite `find_package(pylon)`.

---

## Compilazione

### Linux — build Release ottimizzata (consigliata)

```bash
cd /path/to/c++/barcode
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

L'eseguibile si trova in `build/barcode_reader`.

> **Nota per Google Drive (FUSE/rclone)**: se la cartella è montata come drive remoto,
> il filesystem FUSE non preserva il bit eseguibile. Copia il binary in locale prima di eseguirlo:
> ```bash
> cp build/barcode_reader /usr/local/bin/barcode_reader
> # oppure / or
> cp build/barcode_reader ~/barcode_reader && ~/barcode_reader --help
> ```

### Linux — build Debug

```bash
cmake -B build_debug -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug -j$(nproc)
```

### Windows (vcpkg)

```powershell
cd "G:\Shared drives\HTA\Software\c++\barcode"
cmake -B build -S . `
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
    -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

L'eseguibile si trova in `build\Release\barcode_reader.exe`.

### Con opzioni aggiuntive

```bash
# Supporto Basler
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DUSE_PYLON=ON

# Supporto libcamera (RPi)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DUSE_LIBCAMERA=ON

# Entrambi
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DUSE_PYLON=ON -DUSE_LIBCAMERA=ON
```

> **Nota ottimizzazioni**: in modalità Release vengono abilitati automaticamente  
> `-O3 -march=native -funroll-loops` e la Link-Time Optimization (LTO) se supportata dal compilatore.

---

## Utilizzo

### Immagine singola

```bash
# ROI selezionata col mouse
./build/barcode_reader --image foto.jpg --select-roi

# ROI fissa + debug
./build/barcode_reader --image foto.jpg --roi 1617 1402 830 459 --debug

# ROI fissa, nessuna finestra GUI
./build/barcode_reader --image foto.jpg --roi 1617 1402 830 459 --no-show
```

### Batch su cartella

```bash
# Batch base (intera immagine come ROI)
./build/barcode_reader --folder /percorso/cartella

# Batch con ROI fissa
./build/barcode_reader --folder /percorso/cartella --roi 1617 1402 830 459

# Batch con selezione ROI col mouse (prima immagine)
./build/barcode_reader --folder /percorso/cartella --select-roi

# Batch headless + CSV
./build/barcode_reader --folder /percorso/cartella \
    --roi 1617 1402 830 459 --no-show --output risultati.csv

# Batch parallelo su 8 core (richiede --no-show e ROI fissa)
./build/barcode_reader --folder /percorso/cartella \
    --roi 1617 1402 830 459 --no-show --threads 8 --output risultati.csv

# Batch con debug visuale
./build/barcode_reader --folder /percorso/cartella --select-roi --debug
```

**Navigazione durante il batch:**

| Tasto | Azione |
|-------|--------|
| `SPAZIO` / `ENTER` | Immagine successiva |
| `q` / `ESC` | Interrompi batch |
| `r` | Riseleziona ROI (solo con `--select-roi`) |

#### Tutte le opzioni batch

| Opzione | Default | Descrizione |
|---------|---------|-------------|
| `--folder <path>` | — | **Obbligatorio** — cartella con le immagini |
| `--roi X Y W H` | — | ROI fissa in pixel |
| `--select-roi` | — | Selezione ROI col mouse (prima immagine) |
| `--output <file.csv>` | — | Salva i risultati in CSV |
| `--no-show` | false | Niente finestre GUI (headless) |
| `--threads N` | 1 | Worker paralleli (richiede `--no-show` e ROI fissa) |
| `--debug` | false | Mosaico preprocessing |

### Webcam real-time

```bash
# Webcam default (indice 0)
./build/barcode_reader --webcam --select-roi

# Webcam specifica
./build/barcode_reader --webcam --camera-index 1 --select-roi --debug

# Tuning velocità
./build/barcode_reader --webcam --select-roi \
    --process-every 1 --quick-candidates 3 --full-search-every 5
```

### Camera Basler (richiede Pylon SDK)

```bash
./build/barcode_reader --basler --select-roi
./build/barcode_reader --basler --basler-serial "12345678" --roi 0 0 1920 1080
./build/barcode_reader --basler --device-index 0 --select-roi --debug
```

### Arducam / libcamera (richiede GStreamer)

```bash
./build/barcode_reader --libcamera --select-roi
./build/barcode_reader --libcamera --libcamera-width 1280 --libcamera-height 720 --select-roi
```

### Tutte le opzioni real-time

| Opzione | Default | Descrizione |
|---------|---------|-------------|
| `--camera-index N` | 0 | Indice webcam |
| `--device-index N` | 0 | Indice camera Basler |
| `--basler-serial SN` | — | Seriale Basler (priorità su `--device-index`) |
| `--libcamera-width W` | 1920 | Larghezza frame libcamera |
| `--libcamera-height H` | 1080 | Altezza frame libcamera |
| `--process-every N` | 2 | Decode ogni N frame |
| `--quick-candidates N` | 4 | Step preprocessing nei frame "rapidi" |
| `--full-search-every N` | 10 | Ogni N decode esegue la pipeline completa |

---

## Output e formato CSV

### Output console

```
OK | type=CODE128 | data=1234567890 | quality=60 | pipeline=gray
KO | Nessun barcode letto nella ROI
```

### Schema CSV (`--output risultati.csv`)

| Colonna | Tipo | Descrizione |
|---------|------|-------------|
| `filename` | string | Nome file immagine |
| `status` | `OK` / `KO` / `SKIP` | Esito decode |
| `type` | string | Tipo barcode (es. `CODE128`, `QRCODE`) |
| `data` | string | Payload decodificato |
| `quality` | int | Punteggio qualità ZBar (0–100) |
| `pipeline` | string | Step preprocessing vincente |
| `roi_x` | int | Coordinata X ROI usata |
| `roi_y` | int | Coordinata Y ROI usata |
| `roi_w` | int | Larghezza ROI usata |
| `roi_h` | int | Altezza ROI usata |

---

## Pipeline di preprocessing

La decodifica tenta i seguenti step **in ordine**, fermandosi al primo decode riuscito (**early-exit**):

| # | Nome | Trasformazione |
|---|------|----------------|
| 1 | `gray` | Scala di grigi diretta |
| 2 | `gray_clahe` | CLAHE (contrasto locale, `clipLimit=2.0`, `tileSize=8×8`) |
| 3 | `denoise` | Filtro bilaterale su CLAHE (`d=7`, `σColor=50`, `σSpace=50`) |
| 4 | `thr_adapt` | Sogliatura adattiva Gaussian (`blockSize=31`, `C=5`) |
| 5 | `thr_otsu` | Sogliatura Otsu |
| 6 | `morph_open_horiz` | Apertura morfologica orizzontale (`7×1`, enfatizza barre) |
| 7–16 | `*_x2` / `*_x3` | Versioni upscalate 2× e 3× (INTER_CUBIC) di tutti i candidati base |

**Ottimizzazioni implementate:**
- L'oggetto `CLAHE` è **statico**: allocato una sola volta, riusato tra tutte le chiamate
- Lo scanner ZBar è **statico**: evita overhead di inizializzazione a ogni frame
- In modalità real-time, i frame "rapidi" provano solo i primi `--quick-candidates` step
- L'upscaling avviene solo se i candidati base hanno fallito

---

## Parallelismo batch

Con `--threads N` (N > 1, richiede `--no-show` e ROI fissa), il batch usa una **finestra scorrevole di `std::future`**:

```
[Thread 1] img_001 ──────────────► OK
[Thread 2] img_002 ──────────────► KO
[Thread 3] img_003 ──────────────► OK
[Thread 1] img_004 ──────────────► OK   (riusato dopo img_001)
...
```

- I risultati vengono raccolti **in ordine** (FIFO) per garantire ordinamento deterministico nel CSV
- La memoria usata è proporzionale a N (al massimo N immagini tenute in RAM contemporaneamente)
- La GUI non è disponibile in modalità parallela (OpenCV windows non sono thread-safe)

**Esempio di speedup tipico su 8 core (200 immagini):**

| Threads | Tempo (s) |
|---------|-----------|
| 1 | ~40 s |
| 4 | ~12 s |
| 8 | ~7 s |

> I valori dipendono dall'hardware, dalla dimensione delle immagini e dal numero di step necessari.

---

## Tuning real-time

Il loop di decodifica usa una strategia **two-speed** per bilanciare latenza e throughput:

```
Frame 1:  skip
Frame 2:  decode quick (quick_candidates step)
Frame 3:  skip
Frame 4:  decode quick
...
Frame N:  decode FULL (tutti gli step, ogni full_search_every decode)
```

**Configurazioni tipiche:**

| Scenario | `--process-every` | `--quick-candidates` | `--full-search-every` |
|----------|-------------------|----------------------|-----------------------|
| Massima velocità (30 fps, barcode nitido) | 3 | 2 | 5 |
| Bilanciato (default) | 2 | 4 | 10 |
| Barcode difficili / bassa illuminazione | 1 | 6 | 3 |

---

## Troubleshooting

**`ZBar non trovata`**
```bash
sudo apt install libzbar-dev        # Linux
vcpkg install zbar:x64-windows      # Windows
```

**`Impossibile aprire webcam`**
- Verifica che l'indice sia corretto: prova `--camera-index 1`, `2`, ecc.
- Su Linux: `ls /dev/video*`

**`Supporto Basler non compilato`**
```bash
cmake -B build -S . -DUSE_PYLON=ON
```

**`Supporto libcamera non compilato`**
```bash
cmake -B build -S . -DUSE_LIBCAMERA=ON
```

**Barcode non letto (KO)**
- Usa `--debug` per vedere quale step della pipeline viene tentato
- Prova a ridurre la ROI per escludere sfondo
- In real-time: riduci `--process-every` e aumenta `--quick-candidates`

**Nessun file nel batch**
- Controlla le estensioni supportate: `.jpg .jpeg .png .bmp .tiff .tif .webp`

---

## Confronto Python ↔ C++

| Aspetto | Python (`barcode_reader.py`) | C++ (`barcode_reader`) |
|---------|------------------------------|------------------------|
| Barcode decode | `pyzbar` (wrapper CPython) | ZBar C++ diretto |
| Visione | `opencv-python` | OpenCV C++ |
| Basler | `pypylon` | Pylon C++ SDK |
| libcamera | `picamera2` | GStreamer pipeline |
| Argomenti | `argparse` | Parser manuale (`cli_args.cpp`) |
| Build | `pip install` | CMake + vcpkg |
| Parallelismo batch | — | `std::async` N thread |
| Ottimizzazioni | — | `-O3 -march=native` + LTO |
| Output | Identico | Identico |
