# AGENTS.md

Guidance for coding agents working in this repository. `README.md` is the
user manual (features, platform setup, GUI/CLI usage, slicer notes); this
file covers what you need to change the code safely.

## What this is

A C99 program that turns an SVG logo into multi-colour 3D-printable meshes
(3MF/STL), optionally split into jigsaw-jointed pieces for large prints. One
executable serves both a headless CLI (`--info`, `--export`) and an SDL3 +
OpenGL 3.2 + Nuklear GUI. No external dependencies besides SDL3; everything
else is vendored in `third_party/`.

## Repository map

```
src/            all application code (one flat directory)
  xml.[ch]        minimal XML DOM parser
  svg.[ch]        SVG -> flat list of filled/stroked paths with resolved colours
  textfont.[ch]   <text> support: system font lookup + glyph outlines (stb_truetype)
  region.[ch]     2D polygon sets with boolean ops on libtess2 (union/subtract/clip)
  model.[ch]      the core: colour slots, layered/side-by-side, chunking into
                  pieces, jigsaw tabs, sliding dovetail keys, plate packing, meshes
  export.h        exporter API; export_stl.c, export_3mf.c, zip.[ch] (3MF container)
  app.[ch]        app_state shared by CLI and GUI; load/rebuild orchestration
  cli.c           argument parsing, --info/--export; returns -1 to start the GUI
  main.c          entry point: CLI first, GUI if the CLI says so
  gui.c           the whole Nuklear UI, input handling, side panel, pieces tab
  panel_undo.h    undo/redo ring for side-panel edits (header-only, also tested headless)
  render.[ch]     OpenGL preview: camera, grid, bed, dimensions, picking
  glapi.[ch]      GENERATED minimal GL 3.3 core loader (never include system GL headers)
  nk_sdl_gl3.[ch] Nuklear SDL3/GL backend
  nk_impl.c       the single Nuklear implementation TU (built with -w)
  nk_config.h     shared Nuklear #defines, included by nk_impl.c and tests/test_ui.c
  icon_data.h     GENERATED from assets/*.ico by tools/mkicon.py
tests/          see "Testing" below
examples/       sample SVGs converted by the regression suite
tests/fixtures/ synthetic SVGs for plate-layout tests
third_party/    libtess2, nuklear.h, stb_truetype.h (with local patches, see THIRD_PARTY.md)
docs/           README screenshots
cmake/          MinGW cross-compile toolchain
.github/        build.yml (every push/PR, runs ctest on Linux+macOS), release.yml (tags)
```

Data flow: `xml` -> `svg` -> `model_layout` (stage A: regions per colour slot,
painter's-order cutting) -> `model_build_meshes` (stage B: chunks, joints,
keys, extruded meshes) -> `model_build_view` (preview placement, plate
packing) -> `export_*`. `app.c` is the only place that sequences these; both
front ends go through it.

## Build

CMake is the primary build. `build/` is gitignored.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release   # fetches and builds SDL3 if none is installed (slow once)
cmake --build build -j
./build/intellistream-svgto3dprint examples/simple.svg
```

The plain `Makefile` also works but only with a system SDL3 visible to
`pkg-config sdl3`. Either way the build must stay warning-free under
`-Wall -Wextra -Wno-unused-parameter` (only `nk_impl.c`, `textfont.c` and
libtess2 have warning exceptions, and those are deliberate).

Other options: `-DLOGO3D_X86_FMA=OFF` for pre-Haswell x86, `-DLOGO3D_FETCH_SDL=OFF`
to fail instead of downloading SDL3, `-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake`
to cross-compile for Windows.

## Testing

Run the full suite before you consider a change done:

```sh
ctest --test-dir build --output-on-failure    # or: make test
```

Four tests, all headless (no window needed):

| Test | What it covers |
|------|----------------|
| `test_region` (`tests/test_region.c`) | polygon booleans in `region.c`: areas, contour counts, clean output |
| `test_ui` (`tests/test_ui.c`) | Nuklear widgets and the panel undo ring, with hand-fed input |
| `test_plates` (`tests/test_plates.c`) | plate layout across shapes, widths, margins and plate sizes, tiles and objects, with and without joints: every piece fits unscaled, one plate contour, artwork conserved, rows aligned |
| `headless_exports` (`tests/run_tests.sh`) | converts every example through the CLI in its main modes, validates every 3MF/STL, diffs `--info` output against `tests/expected/*.info` |

`run_tests.sh` needs `python3` for the mesh validators (`check_3mf.py`,
`check_stl.py`) and the tolerant comparison (`compare_info.py`); without it
only exact comparisons run. Examples containing `<text>` are pinned to
Liberation Sans; where that font is not installed (macOS by default) their
`--info` comparison is skipped and only their meshes are checked. Outputs go
to `$TMPDIR/intellistream-svgto3dprint-tests/`.

Rules:

* **Every exported mesh must be closed.** The validators fail on any edge not
  matched by its reverse. If you touch `region.c` or mesh construction in
  `model.c`, run the suite; do not loosen the validators.
* **Geometry, not triangle counts, is the contract.** `compare_info.py` drops
  triangle counts and allows 0.1% / 0.05 mm on decimal numbers because FMA
  and compiler differences change triangulation but not shape. Integers
  (slot, piece, path counts) and text must match exactly.
* **Re-record expectations only for a deliberate output change**, then review
  the diff of `tests/expected/` and explain it in the commit:
  `UPDATE=1 sh tests/run_tests.sh ./build/intellistream-svgto3dprint`
* When you add a CLI mode or option that changes output, add a `run`/`convert`
  line to `run_tests.sh` and record its `.info` file. Layout rules that need
  assertions rather than snapshots belong in `test_plates.c`.
* `tests/tess_orient.c` is a manual probe of libtess2 orientation, not part
  of any build.

### Checking GUI changes without a display session

`test_ui` covers widget behaviour. For rendering and layout, the GUI can draw
one frame and exit:

```sh
./build/intellistream-svgto3dprint --screenshot out.ppm --window 1600x1000 --view iso examples/simple.svg
./build/intellistream-svgto3dprint --screenshot out.ppm --tab pieces --piece 3 --split tiles --plate 80x80 --width 400 examples/intellistream-logo.svg
```

`*.ppm` is gitignored. Convert with any image tool to look at it.

## Code conventions

Match the existing code; there is no formatter config, so consistency is by
hand.

* C99, 4-space indentation, no tabs. `/* */` comments only, never `//`.
* Function-opening brace on its own line; `if`/`for` braces K&R style.
  Short single-statement bodies stay on one line (`if (!r) return;`).
* Declare variables at the top of the block, C89 style.
* Header guards `LOGO3D_<FILE>_H`. Module prefix per file (`region_*`,
  `model_*`, `svg_*`, `export_*`, `zip_*`, `textfont_*`); the GL loader uses
  `l3d_gl*`.
* Structs pair `*_init` / `*_free`; `*_free` leaves the struct reusable.
* Failure convention: functions that can fail return 0/NULL and, where there
  is something to say, write into `char *err, size_t errlen`. No `exit()`
  from library code; only `cli.c` and `main.c` decide exit codes.
* Portable C only. Windows is a first-class target built by CI: guard
  platform code with `#ifdef _WIN32` (see `textfont.c`) and avoid POSIX-only
  functions such as `strcasecmp` (GUI code uses `SDL_strcasecmp`; core code
  writes its own helper).
* Floating point: the build uses FMA and fp-contraction. Never let slot
  ordering, merges or layout decisions turn on exact `double` comparisons;
  use a tolerance (see `area_cmp` in `model.c`).
* `MAX_SLOTS` is 8 materials including the base plate. Everything indexed by
  slot is a fixed array of that size.
* Comments explain *why* (slicer quirks, numerical reasons, print physics).
  The top of `export_3mf.c` and `CMakeLists.txt` are the model to follow.

## Generated and vendored files

* `src/glapi.[ch]`: generated loader. Add a GL entry point by extending both
  files by hand in the same pattern rather than including `<GL/gl.h>`.
* `src/icon_data.h`: regenerate with `python3 tools/mkicon.py` (needs Pillow)
  after changing `assets/intellistream-svgto3dprint.ico`. Do not hand-edit.
* `third_party/`: carries local patches (libtess2 uses `double`, Nuklear's
  colour picker tracks drags outside the widget). Never overwrite with an
  upstream copy without re-applying them, and update `THIRD_PARTY.md`
  (version, licence, modification list) with any change.
* Nuklear is compiled exactly once, in `nk_impl.c`. Change Nuklear options in
  `nk_config.h`, which `test_ui.c` shares, not in individual files.

## Adding a feature: the checklist

A user-visible option usually touches all of these; leaving one out is the
common mistake.

1. `model_params` in `model.h` and its default in `model_params_default()`.
2. `cli.c`: the `usage()` text and the argument parser.
3. `gui.c`: the side-panel widget (and `panel_undo.h` capture if it is undoable).
4. `run_tests.sh` + `tests/expected/`, or `test_plates.c` for layout rules.
5. `README.md`: the "Using the GUI" and/or "Command line" sections.

## Git and CI

* Branch from `master` with a kebab-case name; PRs target `master`.
* Commit subjects are imperative, sentence case, no trailing period
  ("Add side-panel undo and cut jigsaw plates around the artwork").
* `.github/workflows/build.yml` builds Linux, Windows and universal macOS on
  every push and PR, running `ctest` on Linux and macOS. A change that only
  passes locally on one platform is not done.
* Releases: `make release VERSION=vX.Y.Z` tags and pushes, which triggers
  `release.yml`. Only do this when explicitly asked; it needs a clean tree.
