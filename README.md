# logo3dprint

Turn an SVG logo into a multi-colour 3D-printable model. Written in C.

* Imports SVG files (paths, basic shapes, transforms, CSS classes, `<use>`,
  clip paths, strokes, gradients as flat colours, and `<text>` rendered with
  a matching system font or a font file of your choice).
* Every fill colour becomes a separate, watertight mesh that is extruded from a
  common base. Up to **8 materials** (colours) per model; extra colours are
  merged automatically, or by hand in the UI.
* Each colour has its **own height**, so the slicer can print flush multi-colour
  faces or staggered levels with fewer filament changes. **Layered** mode
  makes one colour the full body of the logo and puts the other colours on top
  of it as thin layers (raised, or inlaid flush with the body's top).
* Optional base plate with margin and rounded corners.
* Live 3D preview with CAD-style helpers: overall dimensions, per-colour
  height dimensions, printer bed outline, grid, bounding box, axis triad,
  cursor coordinate read-out and a two-point measuring tool.
* Exports **3MF** (one object with one part per colour, extruder assignment
  and colours embedded) and **STL** (one file per colour).
* **Split into pieces** for prints larger than the printer: the logo is cut
  into letters/objects or plate-sized tiles, each piece gets its own base plate
  and file, pieces that only fit diagonally are turned automatically, and a
  *Pieces* tab shows every piece on its own plate. A 2 m wide logo becomes a
  set of 25 cm prints.
* Headless command-line mode for scripting.

The GUI is built on SDL3 + OpenGL 3.2 and the single-header Nuklear toolkit, so
it runs on Linux, Windows and macOS.

## Building

Dependencies: a C99 compiler, CMake ≥ 3.16 (or GNU make), and SDL3.
Nuklear and libtess2 are vendored in `third_party/`.

### Linux

If your distribution ships SDL3 (Ubuntu ≥ 25.04, Debian 13, Fedora 41, Arch):

    sudo apt install build-essential cmake libsdl3-dev      # Debian / Ubuntu
    sudo dnf install gcc cmake SDL3-devel                   # Fedora

Otherwise CMake downloads and builds SDL3 for you. That needs SDL's own build
dependencies, on Ubuntu 24.04 for example:

    sudo apt install build-essential cmake git libx11-dev libxext-dev \
        libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev \
        libxtst-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
        libgl-dev libegl-dev libdbus-1-dev

Then:

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    ./build/logo3dprint examples/simple.svg

With SDL3 installed system-wide you can also use the plain Makefile:

    make -j
    ./logo3dprint examples/simple.svg

### macOS

    brew install cmake sdl3
    cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j

### Windows

Use CMake with Visual Studio or MinGW. SDL3 is fetched automatically when it is
not found (`vcpkg install sdl3` also works).

## Using the GUI

    logo3dprint [logo.svg]

* **File**: open an SVG (or drop one onto the window), export STL / 3MF.
  Opening a file resets all model settings; the plate settings are kept. If no
  native file dialog is available, path fields appear as a fallback.
* **Size**: model width or height in millimetres, base plate included (the
  other follows the aspect ratio; default 200 mm wide), mirroring for face-down
  printing, curve tolerance.
* **Split into pieces**: *By object* separates letters and symbols (parts that
  belong together, such as the dot of an i or nested chevrons, stay joined;
  the *join gap* joins side-by-side objects closer than a percentage of the
  logo height), *Plate-sized tiles* cuts the logo into a grid. Pieces are
  turned to fit the plate when that helps. Pieces that still do not fit are
  handled by the *Oversize* policy: shrink all pieces by the same factor
  (default, keeps the logo's proportions), shrink each piece on its own, cut
  into tiles, or keep and warn. "Resize logo to N mm" sets the model size to
  the largest one at which every piece fits unshrunk. The tab bar above the
  3D view then offers a *Pieces* grid (one viewport per piece: drag to orbit
  all, wheel to zoom, double-click to open) and one tab per piece. Every piece
  is exported centred, turned and scaled exactly as shown, one file per piece.
  With a base plate, *Connected plates* gives every row of pieces one
  continuous strip of plate: neighbouring plates meet halfway between the
  pieces with square edges and dovetail tabs/sockets (a few per joint,
  adjustable clearance), only the two ends of a row keep rounded corners, so
  the pieces plug together into one plaque. Plate-sized tiles get joints on
  all four sides along the cut lines; tile sizes leave room for the tabs.
* **Base plate**: thickness, margin, corner radius, colour (own colour or the
  same material as one of the logo colours).
* **Colours**: one row per colour slot with a colour swatch (click to change the
  print colour), area share, *print* toggle, *merge* into another slot and the
  extrusion height. *Layered* (on by default) stacks the colours: the chosen
  body colour (by default the colour with the largest visible area) covers the
  whole logo footprint at its body height (2 mm), and every
  other colour becomes a layer of its own thickness (default 0.2 mm, one
  print layer) on top of the body; *Inlaid* sinks those layers into pockets
  so the top stays flush.
  Untick it for colours side by side, each with its own height.
* **Split into pieces** also splits a piece that is too large for the plate
  between its objects when that gives upright parts (the four chevrons of the
  IntelliStream logo become two pieces of two), before falling back to
  turning, shrinking or tiling. Every fit check tries rotations. "Apply to all" sets one height; "Stagger" gives each colour a
  different height (first + n·step) so upper layers need fewer filament changes.
  "Merge similar colours" folds near-identical shades together.
* **Build plate**: plate size (presets for common printers including the
  Snapmaker U1, default 250 x 250 mm), grid step, plate/grid visibility, and
  the padding used when fitting a one-piece model (default 40 mm); warns when
  the model does not fit at any angle. Switching the split mode back to one
  piece resizes the model to the plate minus that padding; the Size section
  has a "Fit to plate" button for the same.
* **View**: camera presets, perspective/orthographic, bounding box,
  dimensions, per-colour heights, outlines, axis triad and the measure tool (click two points on the model).

Mouse: left drag orbits, right/middle drag pans, wheel zooms.
Keys: `F` fit, `0` iso, `1` front, `3` right, `7` top, `P` perspective toggle,
`M` measure tool, `Esc` clear, `Ctrl+O` open, `Ctrl+E` export 3MF.

Colours are quantised so that the whole model uses at most 8 materials
(base plate included). The painter's order of the SVG is respected: a shape
drawn on top cuts a matching hole in the shapes below, so colours never
overlap in the print.

## Command line

    logo3dprint --info logo.svg
    logo3dprint --export logo.3mf --width 150 --base 2 --margin 3 logo.svg
    logo3dprint --export logo.stl --per-color --stagger 0.6,0.2 --no-base logo.svg
    logo3dprint --export big.3mf --width 2000 --split objects --plate 250x250 logo.svg   # big_chunk01.3mf ...
    logo3dprint --export big.3mf --width 2000 --split objects --fit-plate logo.svg      # resize so nothing is shrunk
    logo3dprint --export logo.3mf --no-layered --stagger 0.6,0.2 logo.svg              # colours side by side, staggered heights

`logo3dprint --help` lists every option (sizes, per-slot heights, colour merge
threshold, material limit, base plate colour, mirroring).

## Slicer notes

* The 3MF holds **one object per piece with a part per colour**
  (`base_RRGGBB`, `color1_RRGGBB`, ...), written as an assembly of component
  objects. Parts keep their relative position, which matters for the base
  plate and the layered colours, and the slicer's "object too small, scale
  from inches?" check never fires because it looks at the whole object.
* **OrcaSlicer / Bambu Studio**: the object opens with one part per colour and
  the extruder of every part already set (extruder 1 = base plate, then the
  colours), read from the `model_settings.config` inside the file. The file
  also carries per-triangle colours, so a colour-mapping prompt may appear.
* **PrusaSlicer / SuperSlicer**: the component objects arrive with their
  extruders set; when asked whether to load the file as a single object with
  multiple parts, answer Yes, and No to importing print settings.
* Other slicers get a standard 3MF with `basematerials` colours; assign the
  filaments per object manually if needed.
* STL exports are one file per colour; import them together as one object
  with multiple parts. If the slicer asks whether a thin colour layer "is too
  small, scale to millimetres?", answer No.
* Text is rendered with the closest system font (Helvetica/Arial map to
  Liberation Sans, Arimo or DejaVu Sans on Linux). Pick a specific font file
  with "Text font..." or `--font FILE` for an exact match; draw.io exports
  with multi-line labels are handled.

## Windows and macOS builds

The code is portable C on SDL3. `CMakeLists.txt` builds with Visual Studio,
MinGW or Xcode/clang and fetches SDL3 automatically. The GitHub Actions
workflow in `.github/workflows/build.yml` builds Linux, Windows and a universal
macOS executable on every push and publishes them as artifacts; push the
repository to GitHub to get downloadable binaries without a local toolchain.

## Releases

    make release VERSION=v1.2.3

tags and pushes the release (the working tree must be clean). Pushing a
version tag triggers `.github/workflows/release.yml`, which builds Linux,
Windows and macOS binaries and publishes them to a GitHub Release with notes
generated from the commits since the previous tag.

Delete a tag (`git tag -d v1.2.3 && git push origin :refs/tags/v1.2.3`) and
its release from GitHub to redo a botched one before re-tagging.

## Tests

    make test            # or: ctest --test-dir build

Converts every example in `examples/` and checks that all exported meshes are
closed (each edge used exactly twice).

## Licence

Licensed under the [Apache License, Version 2.0](LICENSE).

Third-party components keep their own licences; see `THIRD_PARTY.md`.
