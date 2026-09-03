/* Command-line front end.  Returns -1 when the GUI should be started. */
#include "app.h"
#include "export.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *f)
{
    fprintf(f,
        "usage: logo3dprint [options] [logo.svg]\n"
        "\n"
        "Without --export or --info the GUI starts (with the SVG loaded when given).\n"
        "\n"
        "  --export FILE        write FILE (.stl or .3mf) and exit\n"
        "  --per-color          STL: one file per colour instead of one merged file\n"
        "                       (needs a manual multi-part merge in the slicer; use 3MF\n"
        "                       for multi-colour printing instead), FILE is the prefix\n"
        "  --info               print colour slots and model statistics, then exit\n"
        "  --width MM           model width in mm, base plate included (default 200)\n"
        "  --height MM          fit the model to this height instead of a width\n"
        "  --color-height MM    height of every colour (default: body 2.0, layers 0.2)\n"
        "  --slot-height N=MM   height of colour slot N (1-based)\n"
        "  --stagger FIRST,STEP colour slot i gets FIRST + i*STEP mm\n"
        "  --layered            (default) the body colour forms the whole logo, other colours\n"
        "                       are thin layers on top of it (their heights = layer thickness)\n"
        "  --no-layered         colours side by side instead, each with its own height\n"
        "  --flush              with --layered: layers inlaid flush with the body top\n"
        "  --body N             with --layered: colour slot N forms the body (default: largest colour)\n"
        "  --body-height MM     with --layered: height of the body (default 2)\n"
        "  --no-base            no base plate\n"
        "  --base MM            base plate thickness (default 2)\n"
        "  --margin MM          base plate margin around the logo (default 3)\n"
        "  --radius MM          base plate corner radius (default 3)\n"
        "  --base-color HEX     base plate colour (default 202020)\n"
        "  --base-slot N        base plate uses the colour of slot N\n"
        "  --mirror             mirror the logo (print face down)\n"
        "  --tolerance MM       curve flattening tolerance (default 0.02)\n"
        "  --merge D            merge colours closer than D (RGB distance, 0-441)\n"
        "  --max-colors N       material limit including the base plate (default 8, max 8)\n"
        "  --hide N             do not output colour slot N\n"
        "  --split objects|tiles  split the logo into pieces for a larger total print:\n"
        "                       by object (letters, symbols) or into plate-sized tiles\n"
        "  --join PCT           objects closer than PCT%% of the logo height form one piece (default 5)\n"
        "  --plate WxD          printer plate size in mm for the pieces (default 246x246)\n"
        "  --oversize cut|uniform|each|keep  pieces larger than the plate: cut into tiles,\n"
        "                       shrink all pieces alike (default), shrink each piece, or keep\n"
        "  --fit-plate          resize the logo to fit the plate: as one piece, plate minus\n"
        "                       padding; when splitting, the largest size with every piece uncut\n"
        "  --padding MM         padding around a one-piece model for --fit-plate (default 40)\n"
        "  --no-joints          pieces get separate rounded plates instead of a connected strip\n"
        "                       with dovetail joints\n"
        "  --joint-clearance MM play between dovetail tab and socket (default 0.15)\n"
        "  --single-file        export all pieces into one file instead of one file per piece\n"
        "  --per-plate          one file per printer plate, with the pieces arranged on it\n"
        "                       (--info shows the arrangement; spacing = --spacing)\n"
        "  --spacing MM         space between pieces, in the preview and on a plate (default 8)\n"

        "  --font FILE          TrueType/OpenType font for <text> (default: matching system font)\n"
        "  --screenshot FILE    GUI: render one frame to FILE (binary PPM) and exit\n"
        "  --view iso|top|front|right  GUI: initial camera\n"
        "  --window WxH         GUI: initial window size\n"
        "  --piece N            GUI: start on the tab of piece N\n"
        "  --tab model|pieces   GUI: initial tab (default: pieces grid when split)\n"
        "  -h, --help           this text\n");
}

static int parse_hex(const char *s, unsigned *rgb)
{
    char buf[16];
    if (s[0] == '#') s++;
    snprintf(buf, sizeof(buf), "#%s", s);
    return svg_parse_color(buf, rgb);
}

static void print_info(const app_state *a)
{
    const model_t *m = &a->model;
    const model_params *p = &a->params;
    int i;
    printf("file: %s\n", a->svg_path);
    printf("paths: %d  shapes: %d  colours in SVG: %d  slots: %d  materials: %d/%d\n",
           a->doc->npaths, m->nshapes, m->colors_before_merge, m->nslots, model_material_count(m, p), p->max_colors);
    if (a->doc->n_text) printf("text: %d element(s) rendered with %s\n", a->doc->n_text, a->doc->font_used);
    if (a->doc->n_text_skipped) printf("warning: %d <text> element(s) skipped: no usable font found (use --font)\n", a->doc->n_text_skipped);
    if (a->doc->n_image) printf("warning: %d <image> element(s) skipped\n", a->doc->n_image);
    if (a->doc->n_gradients) printf("note: %d gradient fill(s) replaced by their average colour\n", a->doc->n_gradients);
    if (m->colors_before_merge > m->nslots) printf("note: %d colours merged into %d slots\n", m->colors_before_merge, m->nslots);
    printf("logo size: %.2f x %.2f mm  (scale %.5f mm/unit)\n", m->logo_w, m->logo_h, m->scale);
    printf("model bbox: %.2f x %.2f x %.2f mm\n", m->bbox_max[0] - m->bbox_min[0], m->bbox_max[1] - m->bbox_min[1], m->bbox_max[2] - m->bbox_min[2]);
    printf("triangles: %d\n", m->total_tris);
    if (p->base_enabled && p->base_thickness > 0)
        printf("base: #%06X  %.1f mm thick  %.0f mm^3  %d tris\n", model_base_rgb(m, p), p->base_thickness, m->base_volume, m->base_mesh.nt);
    if (m->nchunks > 1) {
        printf("pieces: %d (plate %.0f x %.0f mm)", m->nchunks, p->chunk_max_w, p->chunk_max_d);
        if (p->chunk_mode == CHUNK_OBJECTS && m->chunk_fit_scale > 0) printf("  every piece fits uncut up to %.0f%% of this size", m->chunk_fit_scale * 100);
        if (m->chunk_uniform_scale < 0.9995) printf("  all pieces scaled to %.0f%%", m->chunk_uniform_scale * 100);
        printf("\n");
        printf("plates: %d (printer plate %.0f x %.0f mm, %.0f mm between pieces)\n", m->nplates, m->plate_w, m->plate_d, p->chunk_spacing);
        for (i = 0; i < m->nchunks; i++) {
            double w, d;
            model_chunk_size(m, i, &w, &d);
            printf("  %s: %.1f x %.1f mm, %d tris%s", m->chunks[i].name, w, d, m->chunks[i].ntris,
                   m->chunks[i].fits ? "" : "  (too large for the plate!)");
            if (m->chunks[i].rot != 0) printf("  turned %.0f deg on export", m->chunks[i].rot);
            if (m->chunks[i].scale != 1) printf("  scaled to %.0f%% on export", m->chunks[i].scale * 100);
            printf("  plate %d at %.1f,%.1f", m->chunks[i].on_plate + 1, m->chunks[i].plate_pos[0], m->chunks[i].plate_pos[1]);
            printf("\n");
        }
    }
    for (i = 0; i < m->nslots; i++) {
        printf("slot %d: #%06X", i + 1, m->slots[i].rgb);
        if (p->slot_rgb_override[i]) printf(" -> #%06X", p->slot_rgb[i]);
        if (m->slots[i].merged_into >= 0) { printf("  merged into slot %d\n", m->slots[i].merged_into + 1); continue; }
        {
            double zlo, zhi;
            model_slot_zrange(m, p, i, &zlo, &zhi);
            printf("  shapes %d  area %.1f mm^2  z %.2f-%.2f mm  volume %.1f mm^3  tris %d%s%s\n",
                   m->slots[i].nshapes, m->slots[i].area, zlo, zhi, m->slot_volume[i], m->slot_mesh[i].nt,
                   p->slot_visible[i] ? "" : "  (hidden)", (model_body_slot(m, p) == i) ? "  (body)" : "");
        }
    }
}

int cli_main(int argc, char **argv, app_state *a)
{
    const char *export_path = NULL;
    const char *input = NULL;
    int per_color = 0, info = 0, i;
    double color_height = -1;
    int slot_h_n[MAX_SLOTS], nslot_h = 0;
    double slot_h_v[MAX_SLOTS];
    double stagger_first = -1, stagger_step = 0;
    int hide[MAX_SLOTS], nhide = 0;
    int base_slot = -1;
    int single_file = 0, per_plate = 0;
    int fit_plate = 0;

    for (i = 1; i < argc; i++) {
        const char *s = argv[i];
        const char *next = (i + 1 < argc) ? argv[i + 1] : NULL;
#define NEED_ARG() do { if (!next) { fprintf(stderr, "%s needs an argument\n", s); return 2; } i++; } while (0)
        if (!strcmp(s, "-h") || !strcmp(s, "--help")) { usage(stdout); return 0; }
        else if (!strcmp(s, "--export")) { NEED_ARG(); export_path = next; }
        else if (!strcmp(s, "--per-color")) per_color = 1;
        else if (!strcmp(s, "--info")) info = 1;
        else if (!strcmp(s, "--width")) { NEED_ARG(); a->params.width_mm = atof(next); a->params.fit_by_height = 0; a->width_from_cli = 1; }
        else if (!strcmp(s, "--height")) { NEED_ARG(); a->params.height_mm = atof(next); a->params.fit_by_height = 1; a->width_from_cli = 1; }
        else if (!strcmp(s, "--color-height")) { NEED_ARG(); color_height = atof(next); }
        else if (!strcmp(s, "--slot-height")) {
            int n; double v;
            NEED_ARG();
            if (sscanf(next, "%d=%lf", &n, &v) != 2 || n < 1 || n > MAX_SLOTS) { fprintf(stderr, "bad --slot-height value '%s'\n", next); return 2; }
            if (nslot_h < MAX_SLOTS) { slot_h_n[nslot_h] = n - 1; slot_h_v[nslot_h] = v; nslot_h++; }
        }
        else if (!strcmp(s, "--stagger")) {
            NEED_ARG();
            if (sscanf(next, "%lf,%lf", &stagger_first, &stagger_step) != 2) { fprintf(stderr, "bad --stagger value '%s'\n", next); return 2; }
        }
        else if (!strcmp(s, "--layered")) a->params.layered = 1;
        else if (!strcmp(s, "--no-layered")) a->params.layered = 0;
        else if (!strcmp(s, "--flush")) a->params.layered_flush = 1;
        else if (!strcmp(s, "--body")) { NEED_ARG(); a->params.body_slot = atoi(next) - 1; }
        else if (!strcmp(s, "--body-height")) { NEED_ARG(); a->params.body_height = atof(next); }
        else if (!strcmp(s, "--no-base")) a->params.base_enabled = 0;
        else if (!strcmp(s, "--base")) { NEED_ARG(); a->params.base_thickness = atof(next); a->params.base_enabled = a->params.base_thickness > 0; }
        else if (!strcmp(s, "--margin")) { NEED_ARG(); a->params.base_margin = atof(next); }
        else if (!strcmp(s, "--radius")) { NEED_ARG(); a->params.base_radius = atof(next); }
        else if (!strcmp(s, "--base-color")) { NEED_ARG(); if (!parse_hex(next, &a->params.base_rgb)) { fprintf(stderr, "bad colour '%s'\n", next); return 2; } a->params.base_color_slot = -1; }
        else if (!strcmp(s, "--base-slot")) { NEED_ARG(); base_slot = atoi(next) - 1; }
        else if (!strcmp(s, "--mirror")) a->params.mirror_x = 1;
        else if (!strcmp(s, "--tolerance")) { NEED_ARG(); a->params.curve_tol_mm = atof(next); }
        else if (!strcmp(s, "--merge")) { NEED_ARG(); a->params.merge_threshold = atof(next); }
        else if (!strcmp(s, "--max-colors")) { NEED_ARG(); a->params.max_colors = atoi(next); if (a->params.max_colors < 1) a->params.max_colors = 1; if (a->params.max_colors > MAX_SLOTS) a->params.max_colors = MAX_SLOTS; }
        else if (!strcmp(s, "--hide")) { NEED_ARG(); if (nhide < MAX_SLOTS) hide[nhide++] = atoi(next) - 1; }
        else if (!strcmp(s, "--screenshot")) { NEED_ARG(); a->screenshot_path = next; }
        else if (!strcmp(s, "--font")) { NEED_ARG(); snprintf(a->text_font, sizeof(a->text_font), "%s", next); }
        else if (!strcmp(s, "--window")) {
            NEED_ARG();
            if (sscanf(next, "%dx%d", &a->win_w, &a->win_h) != 2 || a->win_w < 400 || a->win_h < 300) { fprintf(stderr, "bad --window value '%s' (e.g. 1600x1000)\n", next); return 2; }
        }
        else if (!strcmp(s, "--piece")) { NEED_ARG(); a->open_piece = atoi(next); }
        else if (!strcmp(s, "--tab")) {
            NEED_ARG();
            if (!strcmp(next, "model")) a->init_tab = 1;
            else if (!strcmp(next, "pieces")) a->init_tab = 2;
            else { fprintf(stderr, "bad --tab value '%s' (model, pieces)\n", next); return 2; }
        }
        else if (!strcmp(s, "--view")) {
            NEED_ARG();
            if (!strcmp(next, "top")) a->view_preset = 1;
            else if (!strcmp(next, "front")) a->view_preset = 2;
            else if (!strcmp(next, "right")) a->view_preset = 3;
            else a->view_preset = 0;
        }
        else if (!strcmp(s, "--split")) {
            NEED_ARG();
            if (!strcmp(next, "objects")) a->params.chunk_mode = CHUNK_OBJECTS;
            else if (!strcmp(next, "tiles")) a->params.chunk_mode = CHUNK_TILES;
            else if (!strcmp(next, "off")) a->params.chunk_mode = CHUNK_OFF;
            else { fprintf(stderr, "bad --split value '%s' (objects, tiles, off)\n", next); return 2; }
        }
        else if (!strcmp(s, "--join")) { NEED_ARG(); a->params.chunk_join_pct = atof(next); }
        else if (!strcmp(s, "--plate")) {
            double w, d;
            NEED_ARG();
            if (sscanf(next, "%lfx%lf", &w, &d) != 2 || w < 10 || d < 10) { fprintf(stderr, "bad --plate value '%s' (e.g. 250x250)\n", next); return 2; }
            a->params.chunk_max_w = w - 4; a->params.chunk_max_d = d - 4;
        }
        else if (!strcmp(s, "--no-cut")) a->params.chunk_oversize = 3;
        else if (!strcmp(s, "--oversize")) {
            NEED_ARG();
            if (!strcmp(next, "cut")) a->params.chunk_oversize = 0;
            else if (!strcmp(next, "uniform") || !strcmp(next, "scale")) a->params.chunk_oversize = 1;
            else if (!strcmp(next, "each")) a->params.chunk_oversize = 2;
            else if (!strcmp(next, "keep")) a->params.chunk_oversize = 3;
            else { fprintf(stderr, "bad --oversize value '%s'\n", next); return 2; }
        }
        else if (!strcmp(s, "--fit-plate")) fit_plate = 1;
        else if (!strcmp(s, "--padding")) { NEED_ARG(); a->params.plate_padding = atof(next); }
        else if (!strcmp(s, "--single-file")) single_file = 1;
        else if (!strcmp(s, "--per-plate")) per_plate = 1;
        else if (!strcmp(s, "--spacing")) { NEED_ARG(); a->params.chunk_spacing = atof(next); }
        else if (!strcmp(s, "--no-joints")) a->params.chunk_joints = 0;
        else if (!strcmp(s, "--joint-clearance")) { NEED_ARG(); a->params.joint_clearance = atof(next); }

        else if (s[0] == '-' && s[1]) { fprintf(stderr, "unknown option '%s'\n", s); usage(stderr); return 2; }
        else input = s;
#undef NEED_ARG
    }
    if (color_height > 0) for (i = 0; i < MAX_SLOTS; i++) a->params.slot_height[i] = color_height;
    for (i = 0; i < nslot_h; i++) a->params.slot_height[slot_h_n[i]] = slot_h_v[i];
    for (i = 0; i < nhide; i++) if (hide[i] >= 0 && hide[i] < MAX_SLOTS) a->params.slot_visible[hide[i]] = 0;
    if (base_slot >= 0 && base_slot < MAX_SLOTS) a->params.base_color_slot = base_slot;

    if (!export_path && !info) {
        if (input) {
            if (!app_load_svg(a, input)) fprintf(stderr, "warning: %s\n", a->last_error);
            else if (stagger_first >= 0) { app_stagger_heights(a, stagger_first, stagger_step); app_rebuild_meshes(a); }
        }
        return -1;
    }
    if (!input) {
        fprintf(stderr, "an input SVG is required\n");
        return 2;
    }
    if (!app_load_svg(a, input)) {
        fprintf(stderr, "error: %s\n", a->last_error);
        return 1;
    }
    if (stagger_first >= 0) {
        app_stagger_heights(a, stagger_first, stagger_step);
        app_rebuild_meshes(a);
    }
    if (fit_plate && a->params.chunk_mode == CHUNK_OFF) {
        double w = app_fit_whole_model(a, a->params.chunk_max_w + 4, a->params.chunk_max_d + 4);
        if (w > 0) { printf("resized to %.1f mm to fit the plate with %.0f mm padding\n", w, a->params.plate_padding); app_rebuild(a); }
    } else if (fit_plate && a->model.chunk_fit_scale > 0 && a->params.chunk_mode == CHUNK_OBJECTS) {
        double mg = (a->params.base_enabled && a->params.base_thickness > 0 && a->params.base_margin > 0) ? 2.0 * a->params.base_margin : 0.0;
        double curw = a->params.fit_by_height ? (a->model.logo_w + mg) : a->params.width_mm;
        a->params.width_mm = (curw - mg) * a->model.chunk_fit_scale + mg;
        a->params.fit_by_height = 0;
        printf("resized to %.1f mm so that every piece fits the plate\n", a->params.width_mm);
        app_rebuild(a);
    }
    if (info) print_info(a);
    if (export_path) {
        char err[256];
        size_t len = strlen(export_path);
        int kind, files;
        if (len > 4 && (!strcmp(export_path + len - 4, ".3mf") || !strcmp(export_path + len - 4, ".3MF"))) kind = 2;
        else kind = per_color ? 1 : 0;
        files = export_model(&a->model, &a->params, kind, single_file ? 0 : (per_plate ? 2 : 1), export_path, err, sizeof(err));
        if (!files) { fprintf(stderr, "export failed: %s\n", err); return 1; }
        if (files == 1) printf("wrote %s\n", export_path);
        else printf("wrote %d files (%s ...)\n", files, export_path);
    }
    return 0;
}
