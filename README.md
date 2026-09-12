# intellistream-svgto3dprint

Turn an SVG logo into a multi-colour 3D-printable model. Written in C.

* Imports SVG paths, shapes, transforms, clip paths, strokes, gradients and `<text>`.
* Every fill colour becomes its own watertight mesh, up to **8 materials**.
  **Layered** mode makes one colour the body and the rest thin layers on top
  (raised, or inlaid flush).
* Optional base plate; live 3D preview with dimensions, grid, bed outline and
  a measuring tool.
* Exports **3MF** (per-colour parts, ready for multi-colour printing) and
  **STL**.
* **Split into pieces** for prints larger than your printer, by object or
  into plate-sized tiles. Pieces are turned or shrunk to fit and packed onto
  printer plates automatically.
* **Pieces lock together without glue**: jigsaw dovetail tabs, plus optional
  *sliding dovetail keys* — small printed bars that slide into slots on the
  underside and hold neighbouring plates down to each other. Slide a key back
  out to take the pieces apart.
* Headless command-line mode for scripting.

The GUI is built on SDL3 + OpenGL 3.2 and Nuklear, and runs on Linux, Windows
and macOS.

![intellistream-svgto3dprint GUI showing a multi-colour logo model](docs/screenshot.png)

The same logo at 1000 mm width, split into 14 letter-sized pieces:

![intellistream-svgto3dprint pieces view with the logo split into 14 pieces](docs/screenshot-pieces.png)

## Building

Dependencies: a C99 compiler, CMake ≥ 3.16 (or GNU make), and SDL3. Nuklear
and libtess2 are vendored in `third_party/`.

### Linux

    sudo apt install build-essential cmake libsdl3-dev      # Debian / Ubuntu (≥ 25.04)
    sudo dnf install gcc cmake SDL3-devel                   # Fedora

If your distribution has no SDL3 package, CMake fetches and builds it for
you; on Ubuntu 24.04 install its build dependencies first:

    sudo apt install build-essential cmake git libx11-dev libxext-dev \
        libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev \
        libxtst-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
        libgl-dev libegl-dev libdbus-1-dev

A missing X11 extension package just disables that SDL feature instead of
failing the build; install it and re-run `cmake` to turn it back on.

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    ./build/intellistream-svgto3dprint examples/simple.svg

With SDL3 installed system-wide, the plain Makefile also works:
`make -j && ./intellistream-svgto3dprint examples/simple.svg`.

### macOS

    brew install cmake sdl3
    cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j

### Windows

Use CMake with Visual Studio or MinGW; SDL3 is fetched automatically
(`vcpkg install sdl3` also works).

### CPU baseline

x86-64 builds use FMA, which needs Haswell (2013) or newer. For older
machines: `cmake -B build -DLOGO3D_X86_FMA=OFF` (or `make X86_FMA=0`). This
changes how the tessellator triangulates a shape but not the geometry itself,
which is what `tests/compare_info.py` checks.

## Using the GUI

    intellistream-svgto3dprint [logo.svg]

* **File**: open an SVG (or drop one onto the window), export STL or 3MF.
  With several pieces, the default writes every piece and key into one 3MF
  so the slicer's *Arrange* can spread them over its plates; one file per
  piece or per printer plate are also available.
* **Size**: model width or height in mm, mirroring for face-down printing,
  curve tolerance.
* **Split into pieces**: *By object* keeps letters/symbols together;
  *Plate-sized tiles* cuts a grid whose cells fill the bed (outer margin and
  joints included). Pieces too large for the plate are turned,
  shrunk, tiled or flagged, per the *Oversize* setting. A *Pieces* tab shows
  every piece on its own plate (orbit/pan/zoom, double-click to open).
  With a base plate, *Connected plates* joins each row into one strip with
  jigsaw dovetail tabs; the logo on top is cut to the same outline (tabs
  carry the artwork, sockets cut it away). *Tab spacing* sets the distance
  between tab centres along a seam (60 mm by default), *Tab width* the width
  of each tab at its widest (0 sizes every tab to its seam); tabs keep their
  dovetail shape, reach at most 12 mm into the neighbour, and a seam too
  short for the chosen spacing or width gets fewer or smaller tabs.
  *Tab offset* slides the tabs along their seam (positive up or right) to move
  one off a thin part of the artwork; each seam gives only what its end tabs
  can spare, so tabs stay clear of the corners, and a tab and its socket
  always move together. Add
  *sliding dovetail keys* to also lock pieces together vertically — a
  separate printed bar slides into an underside slot across each seam after
  assembly, and slides back out to separate them.
  Keys need a base at least 3 mm thick and are exported alongside the pieces.
  `--export-test` writes a small two-plate coupon to dial in the clearance
  before committing to a full print.
* **Base plate**: thickness, margin, corner radius, colour.
* **Colours**: swatch, area share, print toggle, merge, and height per slot.
  *Layered* stacks a body colour with thinner layers on top (or inlaid
  flush); untick it for colours side by side.
* **Build plate**: plate size (presets included), grid, and the plate padding:
  the space kept free on the plate, half on each side. Pieces are cut to the
  plate minus the padding; a one-piece model is refitted to it. A dashed
  outline on the plate shows the area inside the padding.
* **View**: camera presets (`0/1/3/7/9` iso/front/right/top/bottom — bottom
  shows the key slots), perspective/orthographic toggle, dimensions,
  outlines, and a two-point measuring tool.

Mouse: left drag orbits, right/middle drag pans, wheel zooms, `F` fits.
`P` toggles perspective, `M` the measure tool, `Ctrl+O` / `Cmd+O` opens,
`Ctrl+E` / `Cmd+E` exports 3MF. Side-panel edits undo with `Ctrl+Z` /
`Cmd+Z` and redo with `Ctrl+Shift+Z` / `Ctrl+Y` / `Cmd+Shift+Z`.

Colours are quantised to at most 8 materials (base plate included), and
painter's order is respected: a shape drawn on top cuts a hole in whatever
is beneath it, so colours never overlap in the print.

## Command line

    intellistream-svgto3dprint --info logo.svg
    intellistream-svgto3dprint --export logo.3mf --width 150 --base 2 --margin 3 logo.svg
    intellistream-svgto3dprint --export big.3mf --width 2000 --split objects --plate 250x250 --padding 20 logo.svg
    intellistream-svgto3dprint --export big.3mf --width 2000 --split objects --joints keys logo.svg
    intellistream-svgto3dprint --export big.3mf --width 1200 --split tiles --joint-width 16 --joint-spacing 40 logo.svg
    intellistream-svgto3dprint --export big.3mf --width 800 --split objects --joint-offset 25 logo.svg
    intellistream-svgto3dprint --export-test fit.3mf --joints keys --joint-clearance 0.1

`intellistream-svgto3dprint --help` lists every option: sizes, per-slot
heights, colour merge threshold, material limit, base plate colour,
mirroring, and the split/joints/export flags above.

## Slicer notes

* Each 3MF object is an assembly of one part per colour
  (`base_RRGGBB`, `color1_RRGGBB`, ...), so a slicer's "too small, scale
  from inches?" check looks at the whole object and never misfires.
* **OrcaSlicer / Bambu Studio**: opens with one part per colour and the
  extruder of each already set; a colour-mapping prompt may still appear.
* **PrusaSlicer / SuperSlicer**: answer *Yes* to loading as one object with
  multiple parts, *No* to importing print settings.
* **Several pieces in Orca/Bambu**: those slicers build plates only from
  their own project files. Import the pieces (or the all-in-one 3MF) and
  press **Arrange** to spread them over new plates; `--per-plate` writes one
  file per plate instead, for any slicer.
* **Sliding dovetail keys** print wide face down, no support, same material
  and clearance as the pieces. Fit depends mostly on the printer, not the
  filament — print `--export-test` first and adjust `--joint-clearance`
  (0.15 mm default; 0.1 for a snug fit, 0.2–0.25 for ABS/ASA).
* STL exports one merged, single-colour file by default. Use 3MF for
  multi-colour printing; `--per-color` writes one STL per colour for slicers
  that need it (answer *No* if asked to scale a thin layer to millimetres).
* `<text>` renders with the closest system font; pick an exact one with
  "Text font..." or `--font FILE`.

## Windows and macOS builds

The window/exe icon is compiled in from `assets/intellistream-svgto3dprint.ico`
(regenerate `src/icon_data.h` with `tools/mkicon.py` after changing it).

The code is portable C on SDL3; `CMakeLists.txt` builds with Visual Studio,
MinGW or Xcode/clang. `.github/workflows/build.yml` builds Linux, Windows and
a universal macOS executable on every push and publishes them as artifacts.

### Cross-compiling the Windows build on Linux

    sudo apt install mingw-w64      # or: sudo dnf install mingw64-gcc
    cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release
    cmake --build build-win -j

`build-win/intellistream-svgto3dprint.exe` runs under Wine or on real
Windows; `tests/run_tests.sh` needs one of those to run there too.

## Releases

    make release VERSION=v1.2.3

Tags and pushes the release (working tree must be clean); the tag triggers
`.github/workflows/release.yml`, which builds and publishes binaries for
Linux, Windows and macOS to a GitHub Release. To redo one, delete the tag
(`git tag -d v1.2.3 && git push origin :refs/tags/v1.2.3`) and its release
from GitHub first.

## Tests

    make test            # or: ctest --test-dir build

* `tests/test_region.c`: unit tests for the polygon operations in
  `src/region.c` — the booleans that keep every extruded mesh closed.
* `tests/test_ui.c`: headless checks of the Nuklear widgets.
* `tests/run_tests.sh`: converts every example through the CLI in its main
  modes, validates every exported mesh (closed shells, 3MF part/material
  ranges, no overlaps on a plate), and compares `--info` output with the
  recordings in `tests/expected/`. Needs python3 for the mesh checks.

After a deliberate change in what the program produces, re-record the
expectations and review the diff:

    UPDATE=1 sh tests/run_tests.sh ./intellistream-svgto3dprint

## Licence

Licensed under the [Apache License, Version 2.0](LICENSE).

Third-party components keep their own licences; see `THIRD_PARTY.md`.
