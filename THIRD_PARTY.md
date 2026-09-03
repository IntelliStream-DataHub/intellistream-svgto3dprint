# Third-party components

| Component | Location | Version | Licence |
|-----------|----------|---------|---------|
| Nuklear (immediate-mode UI) | `third_party/nuklear/nuklear.h` | 4.13.3 (commit a1856105f45fa0da552872cca17d5c9b5950d582) | Public domain / MIT |
| libtess2 (polygon tessellation) | `third_party/libtess2/` | commit 8dbd6483e920311a58c9af10a10beb278efebc36 | SGI Free Software License B (see `LICENSE.txt` there) |
| SDL3 (window, input, OpenGL context, file dialogs) | external dependency, fetched by CMake when absent | 3.4.16 | zlib |
| stb_truetype (font parsing for SVG text) | `third_party/stb/stb_truetype.h` | 1.26 (commit 2c980bb59875b0d32144a71867fbdebb2f77cd20) | Public domain / MIT |

Local modifications:

* `third_party/libtess2/tesselator.h`: `TESSreal` changed from `float` to `double`
  and `TESS_MAX_VALID_INPUT_VALUE` raised accordingly, so millimetre geometry keeps
  full precision through repeated boolean operations.
* `third_party/libtess2/*.c`: include path of `tesselator.h` flattened.
* `third_party/nuklear/nuklear.h`: `nk_color_picker_behavior()` keeps tracking
  a drag that started on the colour matrix or a bar after the mouse leaves it
  (clamped), so pure white and other extremes can be reached by dragging past
  the edge. Guarded by `tests/test_ui.c`.
