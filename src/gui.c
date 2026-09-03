/* SDL3 + OpenGL + Nuklear user interface. */
#include "app.h"
#include "export.h"
#include "render.h"
#include "glapi.h"
#include "nk_sdl_gl3.h"
#include "icon_data.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_VERTEX_MEMORY (1024 * 1024)
#define MAX_ELEMENT_MEMORY (512 * 1024)

typedef struct {
    app_state *app;
    SDL_Window *win;
    SDL_GLContext gl;
    struct nk_context *ctx;
    render_t *ren;
    camera_t cam;
    view_opts view;
    float ui;               /* UI scale (window units per logical pixel) */
    float px;               /* framebuffer pixels per window unit */
    int panel_w;
    /* 3D view interaction */
    int drag_button;        /* 0 none, 1 orbit, 2 pan */
    float last_mx, last_my;
    float press_mx, press_my;
    int measure_mode;
    int nmeasure;
    double mpts[2][3];
    int hover_valid;
    double hover_pt[3];
    int hover_slot;
    /* overlay toggles */
    int show_dims;
    int show_slot_dims;
    int show_triad;
    /* rebuild scheduling */
    model_params built;
    int built_valid;
    Uint64 dirty_since;
    int dirty;
    /* async dialog results */
    SDL_Mutex *lock;
    char dialog_path[1024];
    int dialog_kind;        /* 0 none, 1 open, 2 stl, 3 stl per colour, 4 3mf, -1 error */
    char dialog_err[256];
    /* text fields */
    char path_buf[1024];
    char export_buf[1200];
    int export_mode;        /* 0 all pieces in one file, 1 one file per piece, 2 one file per printer plate */
    int dialogs_failed;     /* native file dialogs unavailable: show path fields */
    float stagger_first, stagger_step;
    float same_height;
    char status[256];
    Uint64 status_until;
    int show_help;
    const char *screenshot_path;
    int frame;
    /* tabs: 0 = model view, 1 = pieces grid, 2 = one piece */
    int tab;
    int sel_piece;
    int tab_first;          /* first piece number shown in the tab strip */
    int last_view_tab, last_view_sel;
    float grid_zoom;
    int last_nchunks;
    int tab_h;              /* window units, 0 when no tab bar */
    Uint64 last_click_ms;
    int last_click_piece;
} gui_t;

#define GRID_GAP 6

/* ------------------------------------------------------------------ */

static void set_status(gui_t *g, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->status, sizeof(g->status), fmt, ap);
    va_end(ap);
    g->status_until = SDL_GetTicks() + 6000;
}

static struct nk_color rgb_col(unsigned rgb)
{
    return nk_rgb((int)((rgb >> 16) & 255), (int)((rgb >> 8) & 255), (int)(rgb & 255));
}

static unsigned col_rgb(struct nk_color c)
{
    return ((unsigned)c.r << 16) | ((unsigned)c.g << 8) | (unsigned)c.b;
}

static void model_changed(gui_t *g)
{
    render_set_model(g->ren, &g->app->model, &g->app->params);
    g->built = g->app->params;
    g->built_valid = 1;
    g->nmeasure = 0;
}

static void load_file(gui_t *g, const char *path)
{
    if (!path || !path[0]) return;
    /* a new logo starts from defaults; printer settings (plate, grid) are kept */
    model_params_default(&g->app->params);
    g->app->params.chunk_max_w = g->view.bed_w - 4;
    g->app->params.chunk_max_d = g->view.bed_d - 4;
    g->same_height = 1.0f;
    g->stagger_first = 0.6f;
    g->stagger_step = 0.2f;
    g->export_mode = 1;
    g->measure_mode = 0;
    g->nmeasure = 0;
    g->tab = 0;
    g->sel_piece = -1;
    g->grid_zoom = 1.0f;
    g->export_buf[0] = 0;
    if (app_load_svg(g->app, path)) {
        char title[1200];
        const svg_doc *d = g->app->doc;
        model_changed(g);
        camera_fit(&g->cam, &g->app->model);
        snprintf(g->path_buf, sizeof(g->path_buf), "%s", path);
        snprintf(title, sizeof(title), "logo3dprint - %s", path);
        SDL_SetWindowTitle(g->win, title);
        if (d->n_text_skipped) set_status(g, "Loaded. %d <text> element(s) skipped: no usable font (choose one under File).", d->n_text_skipped);
        else if (g->app->model.colors_before_merge > g->app->model.nslots)
            set_status(g, "Loaded. %d colours merged into %d slots.", g->app->model.colors_before_merge, g->app->model.nslots);
        else set_status(g, "Loaded %s", path);
    } else {
        set_status(g, "Error: %s", g->app->last_error);
    }
}

static void do_export(gui_t *g, int kind, const char *path)
{
    char err[256];
    char fixed[1100];
    size_t len = strlen(path);
    int files;
    const char *ext = kind == 4 ? ".3mf" : ".stl";
    if (!g->app->model.meshes_valid) { set_status(g, "Nothing to export: load an SVG first."); return; }
    if (!path[0]) { set_status(g, "Enter an export path first."); return; }
    if (len < 4 || SDL_strcasecmp(path + len - 4, ext) != 0) snprintf(fixed, sizeof(fixed), "%s%s", path, ext);
    else snprintf(fixed, sizeof(fixed), "%s", path);
    files = export_model(&g->app->model, &g->app->params, kind == 4 ? 2 : (kind == 3 ? 1 : 0), g->export_mode, fixed, err, sizeof(err));
    if (files > 0) {
        snprintf(g->export_buf, sizeof(g->export_buf), "%s", fixed);
        if (files == 1) set_status(g, "Exported %s", fixed);
        else set_status(g, "Exported %d files (%s ...)", files, fixed);
    } else set_status(g, "Export failed: %s", err);
}

/* ---- SDL file dialogs (callbacks may run on another thread) -------- */

static void SDLCALL dialog_cb(void *userdata, const char *const *filelist, int filter)
{
    gui_t *g = (gui_t *)userdata;
    (void)filter;
    SDL_LockMutex(g->lock);
    if (!filelist) {
        snprintf(g->dialog_err, sizeof(g->dialog_err), "%s", SDL_GetError());
        g->dialog_kind = -g->dialog_kind;
        if (g->dialog_kind == 0) g->dialog_kind = -1;
    } else if (!filelist[0]) {
        g->dialog_kind = 0;         /* cancelled */
    } else {
        snprintf(g->dialog_path, sizeof(g->dialog_path), "%s", filelist[0]);
        g->dialog_kind += 100;      /* result ready */
    }
    SDL_UnlockMutex(g->lock);
}

static void start_open_dialog(gui_t *g)
{
    static const SDL_DialogFileFilter filters[] = {{"SVG images", "svg"}, {"All files", "*"}};
    SDL_LockMutex(g->lock);
    g->dialog_kind = 1;
    SDL_UnlockMutex(g->lock);
    SDL_ShowOpenFileDialog(dialog_cb, g, g->win, filters, 2, g->path_buf[0] ? g->path_buf : NULL, false);
}

static void start_font_dialog(gui_t *g)
{
    static const SDL_DialogFileFilter filters[] = {{"Fonts", "ttf;otf;ttc"}, {"All files", "*"}};
    SDL_LockMutex(g->lock);
    g->dialog_kind = 5;
    SDL_UnlockMutex(g->lock);
    SDL_ShowOpenFileDialog(dialog_cb, g, g->win, filters, 2, NULL, false);
}

static void start_save_dialog(gui_t *g, int kind)
{
    static const SDL_DialogFileFilter stl_f[] = {{"STL mesh", "stl"}};
    static const SDL_DialogFileFilter mf_f[] = {{"3MF model", "3mf"}};
    char def[1100];
    const char *base = g->app->svg_path[0] ? g->app->svg_path : "logo.svg";
    size_t n = strlen(base);
    if (n > 4 && SDL_strcasecmp(base + n - 4, ".svg") == 0) n -= 4;
    if (n > sizeof(def) - 8) n = sizeof(def) - 8;
    memcpy(def, base, n);
    def[n] = 0;
    strcat(def, kind == 4 ? ".3mf" : ".stl");
    SDL_LockMutex(g->lock);
    g->dialog_kind = kind;
    SDL_UnlockMutex(g->lock);
    SDL_ShowSaveFileDialog(dialog_cb, g, g->win, kind == 4 ? mf_f : stl_f, 1, def);
}

static void poll_dialogs(gui_t *g)
{
    int kind;
    char path[1024], err[256];
    SDL_LockMutex(g->lock);
    kind = g->dialog_kind;
    snprintf(path, sizeof(path), "%s", g->dialog_path);
    snprintf(err, sizeof(err), "%s", g->dialog_err);
    if (kind >= 100 || kind < 0) g->dialog_kind = 0;
    SDL_UnlockMutex(g->lock);
    if (kind >= 100 && kind - 100 == 5) {
        /* font chosen: re-parse the SVG with it */
        snprintf(g->app->text_font, sizeof(g->app->text_font), "%s", path);
        if (g->app->svg_path[0]) {
            if (app_reload_svg(g->app)) { model_changed(g); set_status(g, "Text rendered with %s", path); }
            else set_status(g, "Error: %s", g->app->last_error);
        }
        return;
    }
    if (kind < 0) {
        g->dialogs_failed = 1;
        set_status(g, "File dialog unavailable (%s). Use the path fields, drag and drop, or the command line.", err[0] ? err : "no dialog backend");
    } else if (kind >= 100) {
        kind -= 100;
        if (kind == 1) load_file(g, path);
        else do_export(g, kind, path);
    }
}

/* ---- rebuild scheduling --------------------------------------------- */

/* Settings that only matter at export time do not change the model, so
 * toggling them must not trigger a rebuild. */
static int params_differ(const model_params *a, const model_params *b)
{
    model_params x, y;
    memcpy(&x, a, sizeof x);
    memcpy(&y, b, sizeof y);
    x.export_color_objects = y.export_color_objects = 0;
    return memcmp(&x, &y, sizeof x) != 0;
}

static int params_need_layout(const model_params *a, const model_params *b)
{
    int i;
    if (a->width_mm != b->width_mm || a->height_mm != b->height_mm || a->fit_by_height != b->fit_by_height) return 1;
    if (a->mirror_x != b->mirror_x || a->curve_tol_mm != b->curve_tol_mm || a->merge_threshold != b->merge_threshold) return 1;
    if (a->max_colors != b->max_colors) return 1;
    if ((a->base_enabled && a->base_color_slot < 0) != (b->base_enabled && b->base_color_slot < 0)) return 1;
    if (a->base_enabled != b->base_enabled || a->base_margin != b->base_margin ||
        (a->base_thickness > 0) != (b->base_thickness > 0)) return 1;
    for (i = 0; i < MAX_SLOTS; i++) if (a->slot_merge_into[i] != b->slot_merge_into[i]) return 1;
    return 0;
}

static int params_need_meshes(const model_params *a, const model_params *b)
{
    int i;
    if (a->base_enabled != b->base_enabled || a->base_thickness != b->base_thickness) return 1;
    if (a->base_margin != b->base_margin || a->base_radius != b->base_radius) return 1;
    for (i = 0; i < MAX_SLOTS; i++)
        if (a->slot_height[i] != b->slot_height[i] || a->slot_visible[i] != b->slot_visible[i]) return 1;
    if (a->layered != b->layered || a->layered_flush != b->layered_flush || a->body_slot != b->body_slot || a->body_height != b->body_height) return 1;
    if (a->chunk_mode != b->chunk_mode || a->chunk_join_pct != b->chunk_join_pct || a->chunk_oversize != b->chunk_oversize) return 1;
    if (a->chunk_max_w != b->chunk_max_w || a->chunk_max_d != b->chunk_max_d) return 1;
    if (a->chunk_joints != b->chunk_joints || a->joint_clearance != b->joint_clearance) return 1;
    return 0;
}

static int params_need_view(const model_params *a, const model_params *b)
{
    return a->chunk_spacing != b->chunk_spacing || a->chunk_view != b->chunk_view;
}

static void schedule_rebuild(gui_t *g)
{
    Uint64 now = SDL_GetTicks();
    if (!g->built_valid || !g->app->doc) return;
    if (params_need_layout(&g->built, &g->app->params) || params_need_meshes(&g->built, &g->app->params)) {
        if (!g->dirty) { g->dirty = 1; g->dirty_since = now; }
    } else if (params_need_view(&g->built, &g->app->params)) {
        /* cheap: only the preview arrangement changes */
        app_rebuild_view(g->app);
        model_changed(g);
        g->dirty = 0;
        return;
    } else {
        /* colour-only changes are cheap */
        if (params_differ(&g->built, &g->app->params)) {
            render_set_colors(g->ren, &g->app->model, &g->app->params);
            g->built = g->app->params;
        }
        g->dirty = 0;
    }
    if (g->dirty && now - g->dirty_since > 150) {
        int ok;
        if (params_need_layout(&g->built, &g->app->params)) ok = app_rebuild(g->app);
        else ok = app_rebuild_meshes(g->app);
        if (!ok) set_status(g, "Error: %s", g->app->last_error);
        model_changed(g);
        g->dirty = 0;
    }
}

/* ---- pieces grid ----------------------------------------------------- */

static void grid_layout(const gui_t *g, int vw, int vh, int *cols, int *rows)
{
    int n = g->app->model.nchunks;
    float aspect = vh > 0 ? (float)vw / (float)vh : 1.0f;
    int c = (int)ceilf(sqrtf((float)n * aspect));
    if (c < 1) c = 1;
    if (c > n) c = n;
    *cols = c;
    *rows = (n + c - 1) / c;
}

/* Cell rectangle (window units, relative to the 3D area) of piece i. */
static void grid_cell(const gui_t *g, int vw, int vh, int i, float *x, float *y, float *w, float *h)
{
    int cols, rows;
    float cw, ch, gap = GRID_GAP * g->ui;
    grid_layout(g, vw, vh, &cols, &rows);
    cw = (float)vw / cols;
    ch = (float)vh / rows;
    *x = (i % cols) * cw + gap / 2;
    *y = (i / cols) * ch + gap / 2;
    *w = cw - gap;
    *h = ch - gap;
}

static int grid_hit(const gui_t *g, int vw, int vh, float px, float py)
{
    int i;
    for (i = 0; i < g->app->model.nchunks; i++) {
        float x, y, w, h;
        grid_cell(g, vw, vh, i, &x, &y, &w, &h);
        if (px >= x && px < x + w && py >= y && py < y + h) return i;
    }
    return -1;
}

/* Camera for a grid cell: shared orientation, fitted to the plate (or the piece). */
static void grid_camera(const gui_t *g, int i, int cw, int ch, camera_t *cam)
{
    double mn[3], mx[3];
    const model_params *p = &g->app->params;
    *cam = g->cam;
    if (!render_chunk_bbox(g->ren, i, mn, mx)) { mn[0] = mn[1] = mn[2] = 0; mx[0] = mx[1] = mx[2] = 1; }
    if (g->view.show_bed) {
        double hw = p->chunk_max_w / 2 + 2, hd = p->chunk_max_d / 2 + 2;
        if (-hw < mn[0]) mn[0] = -hw;
        if (hw > mx[0]) mx[0] = hw;
        if (-hd < mn[1]) mn[1] = -hd;
        if (hd > mx[1]) mx[1] = hd;
    }
    camera_fit_bbox(cam, mn, mx);
    /* fit uses the bounding sphere; tighten for wide cells */
    cam->dist *= 0.85f / (g->grid_zoom > 0.05f ? g->grid_zoom : 0.05f);
    (void)cw; (void)ch;
}

/* ---- overlay drawing helpers ------------------------------------------ */

typedef struct {
    struct nk_command_buffer *canvas;
    const struct nk_user_font *font;
    float ox, oy;           /* viewport origin in window units */
    int vw, vh;             /* viewport size in window units */
    float ui;
} overlay_t;

static float text_width(const overlay_t *o, const char *s)
{
    return o->font->width(o->font->userdata, o->font->height, s, nk_strlen(s));
}

static void ov_label(const overlay_t *o, float x, float y, const char *s, struct nk_color fg, struct nk_color bg, int centered)
{
    float w = text_width(o, s) + 8 * o->ui, h = o->font->height + 4 * o->ui;
    struct nk_rect r;
    if (centered) { x -= w / 2; y -= h / 2; }
    r = nk_rect(x, y, w, h);
    nk_fill_rect(o->canvas, r, 3 * o->ui, bg);
    nk_draw_text(o->canvas, nk_rect(x + 4 * o->ui, y + 2 * o->ui, w, h), s, nk_strlen(s), o->font, bg, fg);
}

static void ov_arrow(const overlay_t *o, float x, float y, float dx, float dy, struct nk_color c)
{
    float len = 9 * o->ui, wid = 3.5f * o->ui;
    float bx = x - dx * len, by = y - dy * len;
    nk_fill_triangle(o->canvas, x, y, bx - dy * wid, by + dx * wid, bx + dy * wid, by - dx * wid, c);
}

static int ov_project(const overlay_t *o, const camera_t *cam, const double *p, float *sx, float *sy)
{
    if (!camera_project(cam, o->vw, o->vh, p, sx, sy)) return 0;
    *sx += o->ox;
    *sy += o->oy;
    return 1;
}

/* Dimension between world points a and b, drawn offset by `off` (world). */
static void ov_dimension(const overlay_t *o, const camera_t *cam, const double *a, const double *b, const double *off,
                         const char *label, struct nk_color col)
{
    double a2[3], b2[3];
    float ax, ay, bx, by, ax2, ay2, bx2, by2, dx, dy, len, ext = 6 * o->ui;
    int i;
    for (i = 0; i < 3; i++) { a2[i] = a[i] + off[i]; b2[i] = b[i] + off[i]; }
    if (!ov_project(o, cam, a, &ax, &ay) || !ov_project(o, cam, b, &bx, &by)) return;
    if (!ov_project(o, cam, a2, &ax2, &ay2) || !ov_project(o, cam, b2, &bx2, &by2)) return;
    /* extension lines, slightly past the dimension line */
    {
        float ex = ax2 - ax, ey = ay2 - ay, el = sqrtf(ex * ex + ey * ey);
        if (el > 0) { ex /= el; ey /= el; }
        nk_stroke_line(o->canvas, ax, ay, ax2 + ex * ext, ay2 + ey * ext, 1.0f * o->ui, col);
        ex = bx2 - bx; ey = by2 - by; el = sqrtf(ex * ex + ey * ey);
        if (el > 0) { ex /= el; ey /= el; }
        nk_stroke_line(o->canvas, bx, by, bx2 + ex * ext, by2 + ey * ext, 1.0f * o->ui, col);
    }
    dx = bx2 - ax2; dy = by2 - ay2;
    len = sqrtf(dx * dx + dy * dy);
    if (len < 1) return;
    dx /= len; dy /= len;
    nk_stroke_line(o->canvas, ax2, ay2, bx2, by2, 1.2f * o->ui, col);
    if (len > 24 * o->ui) {
        ov_arrow(o, ax2, ay2, -dx, -dy, col);
        ov_arrow(o, bx2, by2, dx, dy, col);
    }
    ov_label(o, (ax2 + bx2) / 2, (ay2 + by2) / 2, label, nk_rgb(20, 20, 24), nk_rgba(250, 250, 250, 225), 1);
}

static void draw_pieces_overlay(gui_t *g, const overlay_t *o)
{
    const model_t *m = &g->app->model;
    char buf[160];
    int i;
    for (i = 0; i < m->nchunks; i++) {
        const chunk_t *c = &m->chunks[i];
        float x, y, w, h;
        double cw, cd;
        struct nk_color border = (i == g->sel_piece) ? nk_rgb(255, 200, 80) : (c->fits ? nk_rgba(120, 130, 150, 160) : nk_rgb(220, 70, 60));
        grid_cell(g, o->vw, o->vh, i, &x, &y, &w, &h);
        nk_stroke_rect(o->canvas, nk_rect(o->ox + x, o->oy + y, w, h), 4 * o->ui, (i == g->sel_piece) ? 2.5f * o->ui : 1.0f * o->ui, border);
        model_chunk_size(m, i, &cw, &cd);
        if (m->nplates > 1) snprintf(buf, sizeof(buf), "%d: %.0f x %.0f mm, plate %d", i + 1, cw, cd, c->on_plate + 1);
        else snprintf(buf, sizeof(buf), "%d: %.0f x %.0f mm", i + 1, cw, cd);
        if (text_width(o, buf) + 12 * o->ui > w) snprintf(buf, sizeof(buf), "%d: %.0f x %.0f mm", i + 1, cw, cd);
        if (text_width(o, buf) + 12 * o->ui > w) snprintf(buf, sizeof(buf), "%d", i + 1);
        ov_label(o, o->ox + x + 6 * o->ui, o->oy + y + 6 * o->ui, buf, nk_rgb(240, 240, 245),
                 c->fits ? nk_rgba(0, 0, 0, 130) : nk_rgba(160, 40, 30, 200), 0);
        {
            /* extra lines: what happens to the piece on export */
            const char *lines[3];
            char l1[32], l2[32];
            int nl = 0, k;
            if (!c->fits) lines[nl++] = "does not fit";
            if (c->fits && c->scale < 0.995) { snprintf(l1, sizeof(l1), "scaled %.0f%%", c->scale * 100); lines[nl++] = l1; }
            if (c->fits && c->rot != 0) { snprintf(l2, sizeof(l2), "turned %.0f deg", c->rot); lines[nl++] = l2; }
            for (k = 0; k < nl; k++) {
                if (text_width(o, lines[k]) + 12 * o->ui > w) continue;
                ov_label(o, o->ox + x + 6 * o->ui, o->oy + y + 6 * o->ui + (k + 1) * (o->font->height + 6 * o->ui), lines[k],
                         c->fits ? nk_rgb(255, 210, 120) : nk_rgb(255, 200, 190), c->fits ? nk_rgba(0, 0, 0, 130) : nk_rgba(160, 40, 30, 200), 0);
            }
        }
    }
    {
        float y = o->oy + o->vh - o->font->height - 8 * o->ui;
        if (m->nplates > 1)
            snprintf(buf, sizeof(buf), "%d pieces on %d printer plates   Drag: orbit all   Wheel: zoom   Click: select   Double-click: open in Model view", m->nchunks, m->nplates);
        else
            snprintf(buf, sizeof(buf), "%d pieces   Drag: orbit all   Wheel: zoom   Click: select   Double-click: open in Model view", m->nchunks);
        ov_label(o, o->ox + 92 * o->ui, y, SDL_GetTicks() < g->status_until && g->status[0] ? g->status : buf,
                 nk_rgba(235, 235, 240, 220), nk_rgba(0, 0, 0, 110), 0);
    }
}

static void draw_overlay(gui_t *g, const overlay_t *o)
{
    const model_t *m = &g->app->model;
    const model_params *p = &g->app->params;
    struct nk_color dimc = nk_rgb(255, 200, 80);
    char buf[128];
    int i;

    if (g->tab == 1) { draw_pieces_overlay(g, o); return; }
    if (g->tab == 2 && m->meshes_valid && g->sel_piece >= 0 && g->sel_piece < m->nchunks) {
        const chunk_t *c = &m->chunks[g->sel_piece];
        double cw, cd;
        char extra[96] = "";
        model_chunk_size(m, g->sel_piece, &cw, &cd);
        if (c->scale < 0.995 && c->rot != 0) snprintf(extra, sizeof(extra), ", scaled to %.0f%%, turned %.0f deg", c->scale * 100, c->rot);
        else if (c->scale < 0.995) snprintf(extra, sizeof(extra), ", scaled to %.0f%%", c->scale * 100);
        else if (c->rot != 0) snprintf(extra, sizeof(extra), ", turned %.0f deg", c->rot);
        snprintf(buf, sizeof(buf), "Piece %d of %d:  %.1f x %.1f mm%s%s", g->sel_piece + 1, m->nchunks, cw, cd, extra, c->fits ? "" : "  (does not fit the plate)");
        ov_label(o, o->ox + 10 * o->ui, o->oy + 8 * o->ui, buf, nk_rgb(240, 240, 245), c->fits ? nk_rgba(30, 110, 200, 200) : nk_rgba(160, 40, 30, 220), 0);
    }

    /* axis triad in the lower-left corner */
    if (g->show_triad) {
        float cx = o->ox + 46 * o->ui, cy = o->oy + o->vh - 46 * o->ui, len = 30 * o->ui;
        const float dirs[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        const struct nk_color cols[3] = {nk_rgb(220, 70, 70), nk_rgb(70, 190, 70), nk_rgb(80, 130, 240)};
        const char *names[3] = {"X", "Y", "Z"};
        float order[3][3];
        int idx[3] = {0, 1, 2}, k;
        for (i = 0; i < 3; i++) camera_screen_dir(&g->cam, dirs[i], &order[i][0], &order[i][1], &order[i][2]);
        /* draw farthest first */
        for (i = 0; i < 3; i++) for (k = i + 1; k < 3; k++) if (order[idx[k]][2] < order[idx[i]][2]) { int t = idx[i]; idx[i] = idx[k]; idx[k] = t; }
        nk_fill_circle(o->canvas, nk_rect(cx - 40 * o->ui, cy - 40 * o->ui, 80 * o->ui, 80 * o->ui), nk_rgba(0, 0, 0, 60));
        for (i = 0; i < 3; i++) {
            int a = idx[i];
            float ex = cx + order[a][0] * len, ey = cy + order[a][1] * len;
            nk_stroke_line(o->canvas, cx, cy, ex, ey, 2.5f * o->ui, cols[a]);
            nk_fill_circle(o->canvas, nk_rect(ex - 7 * o->ui, ey - 7 * o->ui, 14 * o->ui, 14 * o->ui), cols[a]);
            nk_draw_text(o->canvas, nk_rect(ex - 4 * o->ui, ey - o->font->height / 2, 14 * o->ui, o->font->height),
                         names[a], 1, o->font, cols[a], nk_rgb(255, 255, 255));
        }
    }

    if (m->meshes_valid) {
        double mn[3], mx[3], size, off;
        memcpy(mn, m->bbox_min, sizeof(mn));
        memcpy(mx, m->bbox_max, sizeof(mx));
        size = mx[0] - mn[0];
        if (mx[1] - mn[1] > size) size = mx[1] - mn[1];
        off = size * 0.09 + 2.0;
        if (g->show_dims) {
            double a[3], b[3], o3[3];
            /* width along X, in front of the model */
            a[0] = mn[0]; a[1] = mn[1]; a[2] = mn[2];
            b[0] = mx[0]; b[1] = mn[1]; b[2] = mn[2];
            o3[0] = 0; o3[1] = -off; o3[2] = 0;
            snprintf(buf, sizeof(buf), "%.2f mm", mx[0] - mn[0]);
            ov_dimension(o, &g->cam, a, b, o3, buf, dimc);
            /* depth along Y, right of the model */
            a[0] = mx[0]; a[1] = mn[1]; a[2] = mn[2];
            b[0] = mx[0]; b[1] = mx[1]; b[2] = mn[2];
            o3[0] = off; o3[1] = 0; o3[2] = 0;
            snprintf(buf, sizeof(buf), "%.2f mm", mx[1] - mn[1]);
            ov_dimension(o, &g->cam, a, b, o3, buf, dimc);
            /* height along Z at the front-right corner */
            a[0] = mx[0]; a[1] = mn[1]; a[2] = mn[2];
            b[0] = mx[0]; b[1] = mn[1]; b[2] = mx[2];
            o3[0] = off * 0.7; o3[1] = -off * 0.7; o3[2] = 0;
            snprintf(buf, sizeof(buf), "%.2f mm", mx[2] - mn[2]);
            ov_dimension(o, &g->cam, a, b, o3, buf, dimc);
        }
        if (g->show_slot_dims) {
            /* one vertical dimension per part along the left side */
            int k = 0;
            double a[3], b[3], o3[3];
            if (p->base_enabled && p->base_thickness > 0) {
                a[0] = mn[0]; a[1] = mn[1]; a[2] = 0;
                b[0] = mn[0]; b[1] = mn[1]; b[2] = p->base_thickness;
                o3[0] = -off * 0.7; o3[1] = -off * 0.7; o3[2] = 0;
                snprintf(buf, sizeof(buf), "base %.2f mm", p->base_thickness);
                ov_dimension(o, &g->cam, a, b, o3, buf, rgb_col(model_base_rgb(m, p)));
                k++;
            }
            for (i = 0; i < m->nslots; i++) {
                double zlo, zhi;
                if (!model_slot_active(m, p, i)) continue;
                model_slot_zrange(m, p, i, &zlo, &zhi);
                a[0] = mn[0]; a[1] = mn[1]; a[2] = zlo;
                b[0] = mn[0]; b[1] = mn[1]; b[2] = zhi;
                o3[0] = -off * (0.7 + 0.55 * k); o3[1] = -off * (0.7 + 0.55 * k); o3[2] = 0;
                snprintf(buf, sizeof(buf), "C%d %.2f mm", i + 1, p->slot_height[i]);
                ov_dimension(o, &g->cam, a, b, o3, buf, rgb_col(model_slot_rgb(m, p, i)));
                k++;
            }
        }
    }

    /* piece numbers when the logo is split */
    if (m->meshes_valid && m->nchunks > 1 && p->chunk_view == 0) {
        for (i = 0; i < m->nchunks; i++) {
            const chunk_t *c = &m->chunks[i];
            double pt[3];
            float sx, sy;
            pt[0] = (c->bbox_min[0] + c->bbox_max[0]) / 2 + c->place[0];
            pt[1] = c->bbox_max[1] + c->place[1];
            pt[2] = c->bbox_max[2];
            if (ov_project(o, &g->cam, pt, &sx, &sy)) {
                snprintf(buf, sizeof(buf), "%d", i + 1);
                ov_label(o, sx, sy - 10 * o->ui, buf, nk_rgb(255, 255, 255), c->fits ? nk_rgba(30, 110, 200, 220) : nk_rgba(200, 50, 40, 230), 1);
            }
        }
    }

    /* measurement */
    if (g->nmeasure > 0) {
        float ax, ay, bx, by;
        struct nk_color mc = nk_rgb(90, 220, 255);
        if (ov_project(o, &g->cam, g->mpts[0], &ax, &ay)) {
            nk_fill_circle(o->canvas, nk_rect(ax - 4 * o->ui, ay - 4 * o->ui, 8 * o->ui, 8 * o->ui), mc);
            if (g->nmeasure == 2 && ov_project(o, &g->cam, g->mpts[1], &bx, &by)) {
                double dx = g->mpts[1][0] - g->mpts[0][0], dy = g->mpts[1][1] - g->mpts[0][1], dz = g->mpts[1][2] - g->mpts[0][2];
                double d = sqrt(dx * dx + dy * dy + dz * dz);
                nk_stroke_line(o->canvas, ax, ay, bx, by, 1.5f * o->ui, mc);
                nk_fill_circle(o->canvas, nk_rect(bx - 4 * o->ui, by - 4 * o->ui, 8 * o->ui, 8 * o->ui), mc);
                snprintf(buf, sizeof(buf), "%.2f mm  (dX %.2f  dY %.2f  dZ %.2f)", d, fabs(dx), fabs(dy), fabs(dz));
                ov_label(o, (ax + bx) / 2, (ay + by) / 2 - 14 * o->ui, buf, nk_rgb(10, 30, 40), nk_rgba(200, 240, 255, 235), 1);
            } else if (g->hover_valid) {
                double dx = g->hover_pt[0] - g->mpts[0][0], dy = g->hover_pt[1] - g->mpts[0][1], dz = g->hover_pt[2] - g->mpts[0][2];
                if (ov_project(o, &g->cam, g->hover_pt, &bx, &by)) {
                    nk_stroke_line(o->canvas, ax, ay, bx, by, 1.0f * o->ui, mc);
                    snprintf(buf, sizeof(buf), "%.2f mm", sqrt(dx * dx + dy * dy + dz * dz));
                    ov_label(o, (ax + bx) / 2, (ay + by) / 2 - 14 * o->ui, buf, nk_rgb(10, 30, 40), nk_rgba(200, 240, 255, 200), 1);
                }
            }
        }
    }

    /* status bar */
    {
        float y = o->oy + o->vh - o->font->height - 8 * o->ui;
        float x = o->ox + 92 * o->ui;
        struct nk_color fg = nk_rgb(235, 235, 240);
        if (g->hover_valid) {
            const char *part = g->hover_slot < 0 ? "base" : "colour";
            char piece[64] = "";
            if (m->nchunks > 1) {
                int ci = model_chunk_at(m, p, g->hover_pt[0], g->hover_pt[1]);
                if (ci >= 0) {
                    double cw, cd;
                    model_chunk_size(m, ci, &cw, &cd);
                    if (m->chunks[ci].scale != 1) snprintf(piece, sizeof(piece), "   piece %d: %.1f x %.1f mm, scaled %.0f%% on export", ci + 1, cw, cd, m->chunks[ci].scale * 100);
                    else if (m->chunks[ci].rot != 0) snprintf(piece, sizeof(piece), "   piece %d: %.1f x %.1f mm, turned %.0f deg on export", ci + 1, cw, cd, m->chunks[ci].rot);
                    else snprintf(piece, sizeof(piece), "   piece %d: %.1f x %.1f mm%s", ci + 1, cw, cd, m->chunks[ci].fits ? "" : " (does not fit!)");
                }
            }
            if (g->hover_slot < 0) snprintf(buf, sizeof(buf), "X %.2f  Y %.2f  Z %.2f   (%s)%s", g->hover_pt[0], g->hover_pt[1], g->hover_pt[2], part, piece);
            else snprintf(buf, sizeof(buf), "X %.2f  Y %.2f  Z %.2f   (%s %d, #%06X)%s", g->hover_pt[0], g->hover_pt[1], g->hover_pt[2], part,
                          g->hover_slot + 1, model_slot_rgb(m, p, g->hover_slot), piece);
            ov_label(o, x, y, buf, fg, nk_rgba(0, 0, 0, 120), 0);
        } else if (g->measure_mode) {
            ov_label(o, x, y, g->nmeasure == 1 ? "Measure: click the second point (Esc to cancel)" : "Measure: click the first point on the model",
                     fg, nk_rgba(0, 0, 0, 120), 0);
        } else if (SDL_GetTicks() < g->status_until && g->status[0]) {
            ov_label(o, x, y, g->status, fg, nk_rgba(0, 0, 0, 120), 0);
        } else {
            ov_label(o, x, y, "Left drag: orbit   Right/middle drag: pan   Wheel: zoom   F: fit   1/3/7: front/right/top   Drop an SVG to load",
                     nk_rgba(235, 235, 240, 200), nk_rgba(0, 0, 0, 90), 0);
        }
        if (!m->meshes_valid) {
            const char *msg = "Open an SVG logo (File panel) or drop it onto the window";
            float w = text_width(o, msg);
            ov_label(o, o->ox + (o->vw - w) / 2, o->oy + o->vh / 2, msg, nk_rgb(230, 230, 235), nk_rgba(0, 0, 0, 110), 0);
        }
    }
}

/* ---- side panel ----------------------------------------------------- */

static void panel_colors(gui_t *g)
{
    struct nk_context *ctx = g->ctx;
    model_t *m = &g->app->model;
    model_params *p = &g->app->params;
    float ui = g->ui;
    int i;
    char buf[96];
    int mats = model_material_count(m, p);
    int logo_limit = p->max_colors - ((p->base_enabled && p->base_color_slot < 0) ? 1 : 0);

    nk_layout_row_dynamic(ctx, 22 * ui, 1);
    snprintf(buf, sizeof(buf), "Materials in use: %d / %d   (logo colours: %d, limit %d)", mats, p->max_colors, m->nslots, logo_limit);
    nk_label(ctx, buf, NK_TEXT_LEFT);
    if (m->colors_before_merge > m->nslots) {
        snprintf(buf, sizeof(buf), "%d SVG colours were merged into %d slots", m->colors_before_merge, m->nslots);
        nk_label_colored(ctx, buf, NK_TEXT_LEFT, nk_rgb(255, 200, 90));
    }
    {
        float t = (float)p->merge_threshold;
        nk_layout_row_dynamic(ctx, 24 * ui, 1);
        nk_property_float(ctx, "Merge similar colours (0-200)", 0, &t, 200, 1, 0.5f);
        p->merge_threshold = t;
    }
    {
        /* stacking: one colour forms the body, the others are thin layers on top */
        nk_bool b = p->layered != 0;
        nk_layout_row_dynamic(ctx, 22 * ui, 1);
        nk_checkbox_label(ctx, "Layered: other colours are thin layers on the body colour", &b);
        if (b && !p->layered) {
            /* switching on: thin layers again */
            int k;
            for (k = 0; k < MAX_SLOTS; k++) if (p->slot_height[k] > 0.2) p->slot_height[k] = 0.2;
        } else if (!b && p->layered) {
            /* switching off: every colour stands on its own, so give thin layers a real height */
            int k;
            for (k = 0; k < MAX_SLOTS; k++) if (p->slot_height[k] < 0.5) p->slot_height[k] = 1.0;
        }
        p->layered = b ? 1 : 0;
        if (p->layered) {
            const char *items[MAX_SLOTS + 1];
            char names[MAX_SLOTS + 1][40];
            int k, n = 0, sel = 0, map[MAX_SLOTS + 1], auto_body = model_body_slot(m, p);
            snprintf(names[0], sizeof(names[0]), "Body: largest colour (%d)", auto_body + 1);
            items[0] = names[0];
            map[0] = -1;
            n = 1;
            for (k = 0; k < m->nslots; k++) {
                if (m->slots[k].merged_into >= 0) continue;
                snprintf(names[n], sizeof(names[n]), "Body: colour %d #%06X", k + 1, model_slot_rgb(m, p, k));
                items[n] = names[n];
                map[n] = k;
                if (k == p->body_slot) sel = n;
                n++;
            }
            nk_layout_row_dynamic(ctx, 26 * ui, 1);
            sel = nk_combo(ctx, items, n, sel, (int)(24 * ui), nk_vec2(300 * ui, 220 * ui));
            p->body_slot = map[sel];
            {
                double bh = p->body_height;
                nk_layout_row_dynamic(ctx, 24 * ui, 1);
                nk_property_double(ctx, "#Body height (mm)", 0.2, &bh, 50.0, 0.1, 0.02f);
                p->body_height = bh;
            }
            b = p->layered_flush != 0;
            nk_layout_row_dynamic(ctx, 22 * ui, 1);
            nk_checkbox_label(ctx, "Inlaid: layers flush with the body top (pockets)", &b);
            p->layered_flush = b ? 1 : 0;
        }
    }
    for (i = 0; i < m->nslots; i++) {
        unsigned rgb = model_slot_rgb(m, p, i);
        float total = 0;
        int k;
        const char *items[MAX_SLOTS + 1];
        char names[MAX_SLOTS + 1][24];
        int sel = 0, nitems = 1;
        for (k = 0; k < m->nslots; k++) total += (float)m->slots[k].area;
        nk_layout_row_begin(ctx, NK_STATIC, 26 * ui, 4);
        nk_layout_row_push(ctx, 54 * ui);
        {
            enum nk_symbol_type saved_sym = ctx->style.combo.sym_normal, saved_hover = ctx->style.combo.sym_hover, saved_active = ctx->style.combo.sym_active;
            ctx->style.combo.sym_normal = ctx->style.combo.sym_hover = ctx->style.combo.sym_active = NK_SYMBOL_NONE;
            if (nk_combo_begin_color(ctx, rgb_col(rgb), nk_vec2(220 * ui, 330 * ui))) {
                struct nk_colorf cf = nk_color_cf(rgb_col(rgb));
                ctx->style.combo.sym_normal = saved_sym; ctx->style.combo.sym_hover = saved_hover; ctx->style.combo.sym_active = saved_active;
                nk_layout_row_dynamic(ctx, 150 * ui, 1);
                cf = nk_color_picker(ctx, cf, NK_RGB);
                nk_layout_row_dynamic(ctx, 24 * ui, 1);
                cf.r = nk_propertyf(ctx, "#R:", 0, cf.r, 1.0f, 0.01f, 0.005f);
                cf.g = nk_propertyf(ctx, "#G:", 0, cf.g, 1.0f, 0.01f, 0.005f);
                cf.b = nk_propertyf(ctx, "#B:", 0, cf.b, 1.0f, 0.01f, 0.005f);
                {
                    unsigned nrgb = col_rgb(nk_rgb_cf(cf));
                    if (nrgb != rgb) { p->slot_rgb[i] = nrgb; p->slot_rgb_override[i] = 1; }
                }
                if (nk_button_label(ctx, "Use SVG colour")) p->slot_rgb_override[i] = 0;
                nk_combo_end(ctx);
            }
            ctx->style.combo.sym_normal = saved_sym; ctx->style.combo.sym_hover = saved_hover; ctx->style.combo.sym_active = saved_active;
        }
        nk_layout_row_push(ctx, 96 * ui);
        if (m->slots[i].merged_into >= 0)
            snprintf(buf, sizeof(buf), "%d: #%06X -> %d", i + 1, m->slots[i].rgb, m->slots[i].merged_into + 1);
        else
            snprintf(buf, sizeof(buf), "%d: #%06X", i + 1, m->slots[i].rgb);
        nk_label(ctx, buf, NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 52 * ui);
        snprintf(buf, sizeof(buf), "%4.1f%%", total > 0 ? 100.0f * (float)m->slots[i].area / total : 0.0f);
        nk_label(ctx, buf, NK_TEXT_RIGHT);
        nk_layout_row_push(ctx, 62 * ui);
        {
            nk_bool vis = p->slot_visible[i] != 0;
            nk_checkbox_label(ctx, "print", &vis);
            p->slot_visible[i] = vis ? 1 : 0;
        }
        nk_layout_row_end(ctx);
        nk_layout_row_begin(ctx, NK_DYNAMIC, 24 * ui, 2);
        nk_layout_row_push(ctx, 0.62f);
        if (m->slots[i].merged_into < 0) {
            double h = p->slot_height[i];
            int body = model_body_slot(m, p);
            if (body >= 0 && i == body) {
                nk_label(ctx, "body (height set above)", NK_TEXT_LEFT);
            } else {
                if (body >= 0) snprintf(buf, sizeof(buf), "#Layer %d (mm)", i + 1);
                else snprintf(buf, sizeof(buf), "#Height %d (mm)", i + 1);
                nk_property_double(ctx, buf, 0.0, &h, 50.0, 0.1, 0.02f);
                p->slot_height[i] = h;
            }
        } else {
            nk_label(ctx, "merged", NK_TEXT_LEFT);
        }
        nk_layout_row_push(ctx, 0.38f);
        items[0] = "keep";
        for (k = 0; k < m->nslots; k++) {
            if (k == i || m->slots[k].merged_into >= 0) continue;
            snprintf(names[nitems], sizeof(names[nitems]), "merge -> %d", k + 1);
            items[nitems] = names[nitems];
            if (p->slot_merge_into[i] == k) sel = nitems;
            nitems++;
        }
        sel = nk_combo(ctx, items, nitems, sel, (int)(24 * ui), nk_vec2(130 * ui, 200 * ui));
        if (sel == 0) p->slot_merge_into[i] = -1;
        else {
            int cnt = 0;
            for (k = 0; k < m->nslots; k++) {
                if (k == i || m->slots[k].merged_into >= 0) continue;
                cnt++;
                if (cnt == sel) p->slot_merge_into[i] = k;
            }
        }
        nk_layout_row_end(ctx);
    }
    if (m->nslots > 0) {
        nk_layout_row_begin(ctx, NK_DYNAMIC, 24 * ui, 2);
        nk_layout_row_push(ctx, 0.62f);
        nk_property_float(ctx, "#Same height (mm)", 0.05f, &g->same_height, 50, 0.1f, 0.02f);
        nk_layout_row_push(ctx, 0.38f);
        if (nk_button_label(ctx, "Apply to all")) for (i = 0; i < MAX_SLOTS; i++) p->slot_height[i] = g->same_height;
        nk_layout_row_end(ctx);
        nk_layout_row_dynamic(ctx, 22 * ui, 1);
        nk_label(ctx, "Stagger: distinct heights per colour, fewer filament changes", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 24 * ui, 3);
        nk_property_float(ctx, "#First", 0.05f, &g->stagger_first, 50, 0.1f, 0.02f);
        nk_property_float(ctx, "#Step", 0.0f, &g->stagger_step, 10, 0.1f, 0.02f);
        if (nk_button_label(ctx, "Stagger")) app_stagger_heights(g->app, g->stagger_first, g->stagger_step);
    }
}

static void panel(gui_t *g, int x, int y, int w, int h)
{
    struct nk_context *ctx = g->ctx;
    model_t *m = &g->app->model;
    model_params *p = &g->app->params;
    float ui = g->ui;
    char buf[160];
    struct nk_rect bounds = nk_rect((float)x, (float)y, (float)w, (float)h);

    if (nk_window_find(ctx, "Panel")) nk_window_set_bounds(ctx, "Panel", bounds);
    if (nk_begin(ctx, "Panel", bounds, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR * 0)) {
        /* --- file --- */
        if (nk_tree_push(ctx, NK_TREE_TAB, "File", NK_MAXIMIZED)) {
            nk_layout_row_begin(ctx, NK_DYNAMIC, 26 * ui, 2);
            nk_layout_row_push(ctx, 0.36f);
            if (nk_button_label(ctx, "Open SVG...")) start_open_dialog(g);
            nk_layout_row_push(ctx, 0.64f);
            if (g->app->svg_path[0]) {
                const char *base = strrchr(g->app->svg_path, '/');
#ifdef _WIN32
                const char *bs = strrchr(g->app->svg_path, '\\');
                if (bs && (!base || bs > base)) base = bs;
#endif
                nk_label(ctx, base ? base + 1 : g->app->svg_path, NK_TEXT_LEFT);
            } else nk_label(ctx, "no file loaded (or drop an SVG here)", NK_TEXT_LEFT);
            nk_layout_row_end(ctx);
            if (g->dialogs_failed) {
                nk_layout_row_begin(ctx, NK_DYNAMIC, 26 * ui, 2);
                nk_layout_row_push(ctx, 0.78f);
                nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, g->path_buf, sizeof(g->path_buf), nk_filter_default);
                nk_layout_row_push(ctx, 0.22f);
                if (nk_button_label(ctx, "Load")) load_file(g, g->path_buf);
                nk_layout_row_end(ctx);
            }
            /* font used for <text> elements */
            nk_layout_row_begin(ctx, NK_DYNAMIC, 26 * ui, 3);
            nk_layout_row_push(ctx, 0.36f);
            if (nk_button_label(ctx, "Text font...")) start_font_dialog(g);
            nk_layout_row_push(ctx, 0.48f);
            {
                const char *fp = g->app->text_font[0] ? g->app->text_font : (g->app->doc && g->app->doc->font_used[0] ? g->app->doc->font_used : "");
                const char *base = fp[0] ? strrchr(fp, '/') : NULL;
                if (!fp[0]) nk_label(ctx, "auto (system font)", NK_TEXT_LEFT);
                else nk_label(ctx, base ? base + 1 : fp, NK_TEXT_LEFT);
            }
            nk_layout_row_push(ctx, 0.16f);
            if (nk_button_label(ctx, "Auto") && g->app->text_font[0]) {
                g->app->text_font[0] = 0;
                if (g->app->svg_path[0] && app_reload_svg(g->app)) model_changed(g);
            }
            nk_layout_row_end(ctx);
            nk_layout_row_dynamic(ctx, 26 * ui, 2);
            if (nk_button_label(ctx, "Export STL...")) start_save_dialog(g, 2);
            if (nk_button_label(ctx, "Export 3MF...")) start_save_dialog(g, 4);
            nk_layout_row_dynamic(ctx, 22 * ui, 1);
            nk_label(ctx, "3MF: one object, a part per colour (multi-colour printing).", NK_TEXT_LEFT);
            nk_label(ctx, "STL: one merged file, single colour (use 3MF for multi-colour).", NK_TEXT_LEFT);
            if (m->nchunks > 1) {
                static const char *modes[] = {"One file per piece", "One file per printer plate (pieces arranged)", "All pieces in one file"};
                int sel = g->export_mode == 1 ? 0 : (g->export_mode == 2 ? 1 : 2);
                nk_layout_row_dynamic(ctx, 26 * ui, 1);
                sel = nk_combo(ctx, modes, 3, sel, 24 * ui, nk_vec2(nk_widget_width(ctx), 110 * ui));
                g->export_mode = sel == 0 ? 1 : (sel == 1 ? 2 : 0);
            }
            if (g->dialogs_failed) {
                nk_layout_row_begin(ctx, NK_DYNAMIC, 26 * ui, 3);
                nk_layout_row_push(ctx, 0.56f);
                nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, g->export_buf, sizeof(g->export_buf), nk_filter_default);
                nk_layout_row_push(ctx, 0.22f);
                if (nk_button_label(ctx, "STL")) do_export(g, 2, g->export_buf);
                nk_layout_row_push(ctx, 0.22f);
                if (nk_button_label(ctx, "3MF")) do_export(g, 4, g->export_buf);
                nk_layout_row_end(ctx);
            }
            nk_tree_pop(ctx);
        }
        /* --- size --- */
        if (nk_tree_push(ctx, NK_TREE_TAB, "Size", NK_MAXIMIZED)) {
            /* overall footprint; the base plate margin is part of it */
            double aspect = (m->valid && m->logo_w > 0) ? m->logo_h / m->logo_w : 1.0;
            double mg = (p->base_enabled && p->base_thickness > 0 && p->base_margin > 0) ? 2.0 * p->base_margin : 0.0;
            double wv = p->fit_by_height ? ((p->height_mm - mg) / (aspect > 0 ? aspect : 1) + mg) : p->width_mm;
            double hv = p->fit_by_height ? p->height_mm : ((p->width_mm - mg) * aspect + mg);
            double w0 = wv, h0 = hv;
            nk_layout_row_dynamic(ctx, 24 * ui, 1);
            nk_property_double(ctx, "#Model width (mm)", 1, &wv, 10000, 1, 0.2f);
            nk_property_double(ctx, "#Model height (mm)", 1, &hv, 10000, 1, 0.2f);
            if (wv != w0) { p->width_mm = wv; p->fit_by_height = 0; }
            else if (hv != h0) { p->height_mm = hv; p->fit_by_height = 1; }
            nk_layout_row_dynamic(ctx, 26 * ui, 1);
            snprintf(buf, sizeof(buf), "Fit to plate (%.0f x %.0f minus %.0f mm padding)", g->view.bed_w, g->view.bed_d, p->plate_padding);
            if (nk_button_label(ctx, buf)) {
                double w = app_fit_whole_model(g->app, g->view.bed_w, g->view.bed_d);
                if (w > 0) set_status(g, "Resized to %.0f mm", w);
            }
            {
                nk_bool mir = p->mirror_x != 0;
                nk_checkbox_label(ctx, "Mirror (for printing face down)", &mir);
                p->mirror_x = mir ? 1 : 0;
            }
            {
                double t = p->curve_tol_mm;
                nk_property_double(ctx, "#Curve tolerance (mm)", 0.002, &t, 1.0, 0.005, 0.002f);
                p->curve_tol_mm = t;
            }
            nk_tree_pop(ctx);
        }
        /* --- build plate / grid --- */
        if (nk_tree_push(ctx, NK_TREE_TAB, "Build plate", NK_MAXIMIZED)) {
            static const struct { const char *name; float w, d; } presets[] = {
                {"Custom", 0, 0},
                {"250 x 250 (default)", 250, 250},
                {"180 x 180 (Prusa Mini)", 180, 180},
                {"220 x 220 (Ender 3)", 220, 220},
                {"235 x 235 (Ender 3 V3)", 235, 235},
                {"250 x 210 (Prusa MK3/MK4)", 250, 210},
                {"256 x 256 (Bambu X1/P1)", 256, 256},
                {"270 x 270 (Snapmaker U1)", 270, 270},
                {"300 x 300", 300, 300},
                {"350 x 350 (Bambu H2D)", 350, 350},
            };
            const int npresets = (int)(sizeof(presets) / sizeof(presets[0]));
            const char *names[16];
            int i, sel = 0;
            nk_bool b;
            for (i = 0; i < npresets; i++) {
                names[i] = presets[i].name;
                if (i > 0 && presets[i].w == g->view.bed_w && presets[i].d == g->view.bed_d) sel = i;
            }
            nk_layout_row_dynamic(ctx, 26 * ui, 1);
            i = nk_combo(ctx, names, npresets, sel, (int)(24 * ui), nk_vec2(300 * ui, 260 * ui));
            if (i != sel && i > 0) { g->view.bed_w = presets[i].w; g->view.bed_d = presets[i].d; }
            nk_layout_row_dynamic(ctx, 24 * ui, 1);
            nk_property_float(ctx, "#Plate width X (mm)", 50, &g->view.bed_w, 1000, 5, 0.5f);
            nk_property_float(ctx, "#Plate depth Y (mm)", 50, &g->view.bed_d, 1000, 5, 0.5f);
            nk_property_float(ctx, "#Grid step (mm)", 1, &g->view.grid_step, 100, 1, 0.2f);
            {
                float pad = (float)p->plate_padding;
                nk_property_float(ctx, "#One-piece padding (mm)", 0, &pad, 200, 5, 0.5f);
                p->plate_padding = pad;
            }
            nk_layout_row_dynamic(ctx, 22 * ui, 2);
            b = g->view.show_bed != 0; nk_checkbox_label(ctx, "Show plate", &b); g->view.show_bed = b;
            b = g->view.show_grid != 0; nk_checkbox_label(ctx, "Show grid", &b); g->view.show_grid = b;
            if (m->meshes_valid && p->chunk_mode == CHUNK_OFF) {
                double mw = m->bbox_max[0] - m->bbox_min[0], md = m->bbox_max[1] - m->bbox_min[1];
                double ang = model_fit_angle(mw, md, g->view.bed_w, g->view.bed_d);
                if (ang < 0) {
                    nk_layout_row_dynamic(ctx, 22 * ui, 1);
                    snprintf(buf, sizeof(buf), "Model (%.0f x %.0f mm) exceeds the plate at any angle!", mw, md);
                    nk_label_colored(ctx, buf, NK_TEXT_LEFT, nk_rgb(255, 110, 90));
                } else if (ang > 0) {
                    nk_layout_row_dynamic(ctx, 22 * ui, 1);
                    snprintf(buf, sizeof(buf), "Model (%.0f x %.0f mm) fits the plate turned %.0f deg", mw, md, ang);
                    nk_label_colored(ctx, buf, NK_TEXT_LEFT, nk_rgb(255, 200, 90));
                }
            }
            nk_tree_pop(ctx);
        }
        /* --- split into pieces --- */
        if (nk_tree_push(ctx, NK_TREE_TAB, "Split into pieces (large prints)", NK_MAXIMIZED)) {
            static const char *modes[] = {"Off: print as one piece", "By object (letters, symbols)", "Plate-sized tiles"};
            int mode = p->chunk_mode;
            nk_bool b;
            nk_layout_row_dynamic(ctx, 26 * ui, 1);
            mode = nk_combo(ctx, modes, 3, mode, (int)(24 * ui), nk_vec2(300 * ui, 120 * ui));
            if (mode != p->chunk_mode) {
                p->chunk_mode = mode;
                p->chunk_view = 0;
                if (mode == CHUNK_OFF) {
                    /* back to one piece: make it fit the plate again */
                    double w = app_fit_whole_model(g->app, g->view.bed_w, g->view.bed_d);
                    if (w > 0) set_status(g, "Resized to %.0f mm to fit the plate with %.0f mm padding", w, p->plate_padding);
                }
            }
            if (p->chunk_mode == CHUNK_OBJECTS) {
                float jp = (float)p->chunk_join_pct;
                (void)b;
                nk_layout_row_dynamic(ctx, 24 * ui, 1);
                nk_property_float(ctx, "#Join gap (% of logo height)", 0, &jp, 50, 0.5f, 0.1f);
                p->chunk_join_pct = jp;
                nk_layout_row_dynamic(ctx, 22 * ui, 1);
                snprintf(buf, sizeof(buf), "Side-by-side objects closer than %.1f mm join", p->chunk_join_pct / 100.0 * m->logo_h);
                nk_label(ctx, buf, NK_TEXT_LEFT);
                {
                    static const char *policies[] = {"Oversize: cut into tiles", "Oversize: shrink all pieces alike", "Oversize: shrink each piece", "Oversize: keep (warn)"};
                    nk_layout_row_dynamic(ctx, 26 * ui, 1);
                    p->chunk_oversize = nk_combo(ctx, policies, 4, p->chunk_oversize, (int)(24 * ui), nk_vec2(320 * ui, 140 * ui));
                }
                if (m->meshes_valid && m->chunk_uniform_scale < 0.9995) {
                    nk_layout_row_dynamic(ctx, 22 * ui, 1);
                    snprintf(buf, sizeof(buf), "All pieces scaled to %.0f%% (logo %.0f x %.0f mm)", m->chunk_uniform_scale * 100,
                             (m->logo_w) * m->chunk_uniform_scale, (m->logo_h) * m->chunk_uniform_scale);
                    nk_label_colored(ctx, buf, NK_TEXT_LEFT, nk_rgb(255, 200, 90));
                }
                if (m->meshes_valid && m->nchunks > 0 && m->chunk_fit_scale > 0 && fabs(m->chunk_fit_scale - 1.0) > 0.005) {
                    double mg = (p->base_enabled && p->base_thickness > 0 && p->base_margin > 0) ? 2.0 * p->base_margin : 0.0;
                    double curw = p->fit_by_height ? (m->logo_w + mg) : p->width_mm;
                    double neww = (curw - mg) * m->chunk_fit_scale + mg;
                    nk_layout_row_dynamic(ctx, 22 * ui, 1);
                    snprintf(buf, sizeof(buf), "All pieces fit uncut up to %.0f mm width", neww);
                    nk_label(ctx, buf, NK_TEXT_LEFT);
                    nk_layout_row_dynamic(ctx, 26 * ui, 1);
                    snprintf(buf, sizeof(buf), "Resize logo to %.0f mm (keeps proportions)", neww);
                    if (nk_button_label(ctx, buf)) { p->width_mm = neww; p->fit_by_height = 0; }
                }
            }
            if (p->chunk_mode != CHUNK_OFF) {
                float sp = (float)p->chunk_spacing;
                const char *items[130];
                char names[130][16];
                int i, n = m->nchunks < 128 ? m->nchunks : 128, sel = p->chunk_view;
                double maxw = 0, maxd = 0;
                int oversize = 0;
                nk_layout_row_dynamic(ctx, 24 * ui, 1);
                nk_property_float(ctx, "#Piece spacing (mm)", 0, &sp, 200, 1, 0.2f);
                p->chunk_spacing = sp;
                for (i = 0; i < m->nchunks; i++) {
                    double w, d;
                    model_chunk_size(m, i, &w, &d);
                    if (w > maxw) maxw = w;
                    if (d > maxd) maxd = d;
                    if (!m->chunks[i].fits) oversize++;
                }
                nk_layout_row_dynamic(ctx, 22 * ui, 1);
                if (m->nplates > 1)
                    snprintf(buf, sizeof(buf), "%d pieces on %d plates, largest %.0f x %.0f mm (plate %.0f x %.0f)", m->nchunks, m->nplates, maxw, maxd, p->chunk_max_w, p->chunk_max_d);
                else
                    snprintf(buf, sizeof(buf), "%d piece%s, largest %.0f x %.0f mm (plate %.0f x %.0f)", m->nchunks, m->nchunks == 1 ? "" : "s", maxw, maxd, p->chunk_max_w, p->chunk_max_d);
                nk_label(ctx, buf, NK_TEXT_LEFT);
                if (m->nchunks == 1 && p->chunk_mode == CHUNK_TILES) {
                    nk_label_colored(ctx, "The whole logo fits on one plate: nothing to split.", NK_TEXT_LEFT, nk_rgb(255, 200, 90));
                    nk_label_colored(ctx, "Increase the model width to get several tiles.", NK_TEXT_LEFT, nk_rgb(255, 200, 90));
                }
                if (oversize) {
                    snprintf(buf, sizeof(buf), "%d piece%s do%s not fit the plate!", oversize, oversize == 1 ? "" : "s", oversize == 1 ? "es" : "");
                    nk_label_colored(ctx, buf, NK_TEXT_LEFT, nk_rgb(255, 110, 90));
                } else {
                    int rotated = 0, scaled = 0;
                    for (i = 0; i < m->nchunks; i++) { if (m->chunks[i].rot != 0) rotated++; if (m->chunks[i].scale < 0.995) scaled++; }
                    if (rotated) {
                        snprintf(buf, sizeof(buf), "%d piece%s exported turned to fit the plate", rotated, rotated == 1 ? " is" : "s are");
                        nk_label(ctx, buf, NK_TEXT_LEFT);
                    }
                    if (scaled && p->chunk_oversize == 2) {
                        snprintf(buf, sizeof(buf), "%d piece%s shrunk to fit the plate", scaled, scaled == 1 ? " is" : "s are");
                        nk_label_colored(ctx, buf, NK_TEXT_LEFT, nk_rgb(255, 200, 90));
                    }
                }
                (void)items; (void)names; (void)n; (void)sel;
                if (p->base_enabled && p->base_thickness > 0) {
                    nk_bool jb = p->chunk_joints != 0;
                    float cl = (float)p->joint_clearance;
                    nk_layout_row_dynamic(ctx, 22 * ui, 1);
                    nk_checkbox_label(ctx, "Connected plates: one strip per row, dovetail joints", &jb);
                    p->chunk_joints = jb ? 1 : 0;
                    if (p->chunk_joints) {
                        nk_layout_row_dynamic(ctx, 24 * ui, 1);
                        nk_property_float(ctx, "#Joint clearance (mm)", 0, &cl, 1, 0.05f, 0.005f);
                        p->joint_clearance = cl;
                    }
                }
                nk_layout_row_dynamic(ctx, 22 * ui, 1);
                nk_label(ctx, "Each piece gets its own base plate and export file.", NK_TEXT_LEFT);
                nk_label(ctx, "Use the tabs above the 3D view to look at single pieces.", NK_TEXT_LEFT);
            }
            nk_tree_pop(ctx);
        }
        /* --- base plate --- */
        if (nk_tree_push(ctx, NK_TREE_TAB, "Base plate", NK_MAXIMIZED)) {
            nk_bool en = p->base_enabled != 0;
            nk_layout_row_dynamic(ctx, 22 * ui, 1);
            nk_checkbox_label(ctx, "Base plate under the logo", &en);
            p->base_enabled = en ? 1 : 0;
            if (p->base_enabled) {
                double t = p->base_thickness, mg = p->base_margin, rd = p->base_radius;
                const char *items[MAX_SLOTS + 1];
                char names[MAX_SLOTS + 1][24];
                int i, sel = 0;
                nk_layout_row_dynamic(ctx, 24 * ui, 1);
                nk_property_double(ctx, "#Thickness (mm)", 0.2, &t, 50, 0.2, 0.02f);
                nk_property_double(ctx, "#Margin (mm)", 0, &mg, 100, 0.5, 0.05f);
                nk_property_double(ctx, "#Corner radius (mm)", 0, &rd, 100, 0.5, 0.05f);
                p->base_thickness = t; p->base_margin = mg; p->base_radius = rd;
                items[0] = "Own colour";
                for (i = 0; i < m->nslots && i < MAX_SLOTS; i++) {
                    snprintf(names[i + 1], sizeof(names[i + 1]), "Same as colour %d", i + 1);
                    items[i + 1] = names[i + 1];
                }
                if (p->base_color_slot >= 0 && p->base_color_slot < m->nslots) sel = p->base_color_slot + 1;
                nk_layout_row_begin(ctx, NK_STATIC, 26 * ui, 2);
                nk_layout_row_push(ctx, 54 * ui);
                {
                enum nk_symbol_type saved_sym = ctx->style.combo.sym_normal, saved_hover = ctx->style.combo.sym_hover, saved_active = ctx->style.combo.sym_active;
                ctx->style.combo.sym_normal = ctx->style.combo.sym_hover = ctx->style.combo.sym_active = NK_SYMBOL_NONE;
                if (nk_combo_begin_color(ctx, rgb_col(model_base_rgb(m, p)), nk_vec2(220 * ui, 300 * ui))) {
                    struct nk_colorf cf = nk_color_cf(rgb_col(p->base_rgb));
                    ctx->style.combo.sym_normal = saved_sym; ctx->style.combo.sym_hover = saved_hover; ctx->style.combo.sym_active = saved_active;
                    nk_layout_row_dynamic(ctx, 150 * ui, 1);
                    cf = nk_color_picker(ctx, cf, NK_RGB);
                    nk_layout_row_dynamic(ctx, 24 * ui, 1);
                    cf.r = nk_propertyf(ctx, "#R:", 0, cf.r, 1.0f, 0.01f, 0.005f);
                    cf.g = nk_propertyf(ctx, "#G:", 0, cf.g, 1.0f, 0.01f, 0.005f);
                    cf.b = nk_propertyf(ctx, "#B:", 0, cf.b, 1.0f, 0.01f, 0.005f);
                    p->base_rgb = col_rgb(nk_rgb_cf(cf));
                    p->base_color_slot = -1;
                    nk_combo_end(ctx);
                }
                ctx->style.combo.sym_normal = saved_sym; ctx->style.combo.sym_hover = saved_hover; ctx->style.combo.sym_active = saved_active;
                }
                nk_layout_row_push(ctx, 200 * ui);
                sel = nk_combo(ctx, items, m->nslots + 1, sel, (int)(24 * ui), nk_vec2(200 * ui, 200 * ui));
                p->base_color_slot = sel - 1;
                nk_layout_row_end(ctx);
            }
            nk_tree_pop(ctx);
        }
        /* --- colours --- */
        snprintf(buf, sizeof(buf), "Colours (%d)", m->nslots);
        if (nk_tree_push(ctx, NK_TREE_TAB, buf, NK_MAXIMIZED)) {
            if (m->nslots == 0) { nk_layout_row_dynamic(ctx, 22 * ui, 1); nk_label(ctx, "No model loaded", NK_TEXT_LEFT); }
            else panel_colors(g);
            nk_tree_pop(ctx);
        }
        /* --- view --- */
        if (nk_tree_push(ctx, NK_TREE_TAB, "View", NK_MAXIMIZED)) {
            nk_bool b;
            nk_layout_row_dynamic(ctx, 26 * ui, 5);
            if (nk_button_label(ctx, "Fit")) camera_fit(&g->cam, m);
            if (nk_button_label(ctx, "Iso")) camera_preset(&g->cam, 0);
            if (nk_button_label(ctx, "Top")) camera_preset(&g->cam, 1);
            if (nk_button_label(ctx, "Front")) camera_preset(&g->cam, 2);
            if (nk_button_label(ctx, "Right")) camera_preset(&g->cam, 3);
            nk_layout_row_dynamic(ctx, 22 * ui, 2);
            b = !g->cam.ortho; nk_checkbox_label(ctx, "Perspective", &b); g->cam.ortho = !b;
            b = g->view.show_bbox != 0; nk_checkbox_label(ctx, "Bounding box", &b); g->view.show_bbox = b;
            b = g->show_dims != 0; nk_checkbox_label(ctx, "Dimensions", &b); g->show_dims = b;
            b = g->show_slot_dims != 0; nk_checkbox_label(ctx, "Colour heights", &b); g->show_slot_dims = b;
            b = g->view.show_outline != 0; nk_checkbox_label(ctx, "Outlines", &b); g->view.show_outline = b;
            b = g->show_triad != 0; nk_checkbox_label(ctx, "Axis triad", &b); g->show_triad = b;
            nk_layout_row_dynamic(ctx, 26 * ui, 2);
            b = g->measure_mode != 0;
            nk_checkbox_label(ctx, "Measure tool", &b);
            if (b != (g->measure_mode != 0)) { g->measure_mode = b; g->nmeasure = 0; }
            if (nk_button_label(ctx, "Clear measure")) g->nmeasure = 0;
            nk_tree_pop(ctx);
        }
        /* --- info --- */
        if (nk_tree_push(ctx, NK_TREE_TAB, "Model info", NK_MAXIMIZED)) {
            nk_layout_row_dynamic(ctx, 20 * ui, 1);
            if (m->meshes_valid) {
                int i;
                double vol = m->base_volume;
                for (i = 0; i < m->nslots; i++) vol += m->slot_volume[i];
                snprintf(buf, sizeof(buf), "Size: %.2f x %.2f x %.2f mm", m->bbox_max[0] - m->bbox_min[0], m->bbox_max[1] - m->bbox_min[1], m->bbox_max[2] - m->bbox_min[2]);
                nk_label(ctx, buf, NK_TEXT_LEFT);
                snprintf(buf, sizeof(buf), "Logo: %.2f x %.2f mm, %d shapes", m->logo_w, m->logo_h, m->nshapes);
                nk_label(ctx, buf, NK_TEXT_LEFT);
                snprintf(buf, sizeof(buf), "Triangles: %d   Volume: %.1f cm3", m->total_tris, vol / 1000.0);
                nk_label(ctx, buf, NK_TEXT_LEFT);
                if (p->base_enabled && p->base_thickness > 0) {
                    snprintf(buf, sizeof(buf), "Base: %.1f cm3", m->base_volume / 1000.0);
                    nk_label(ctx, buf, NK_TEXT_LEFT);
                }
                for (i = 0; i < m->nslots; i++) {
                    if (m->slots[i].merged_into >= 0) continue;
                    snprintf(buf, sizeof(buf), "Colour %d: %.1f mm2, %.2f cm3, %d tris", i + 1, m->slots[i].area, m->slot_volume[i] / 1000.0, m->slot_mesh[i].nt);
                    nk_label(ctx, buf, NK_TEXT_LEFT);
                }
                if (g->app->doc) {
                    if (g->app->doc->n_text) { snprintf(buf, sizeof(buf), "%d text element(s) rendered", g->app->doc->n_text); nk_label(ctx, buf, NK_TEXT_LEFT); }
                    if (g->app->doc->n_text_skipped) { snprintf(buf, sizeof(buf), "%d text element(s) skipped: no font", g->app->doc->n_text_skipped); nk_label_colored(ctx, buf, NK_TEXT_LEFT, nk_rgb(255, 170, 80)); }
                    if (g->app->doc->n_image) { snprintf(buf, sizeof(buf), "%d <image> skipped", g->app->doc->n_image); nk_label_colored(ctx, buf, NK_TEXT_LEFT, nk_rgb(255, 170, 80)); }
                    if (g->app->doc->n_gradients) { snprintf(buf, sizeof(buf), "%d gradient(s) replaced by average colour", g->app->doc->n_gradients); nk_label_colored(ctx, buf, NK_TEXT_LEFT, nk_rgb(255, 200, 90)); }
                }
            } else {
                nk_label(ctx, "No model", NK_TEXT_LEFT);
            }
            nk_tree_pop(ctx);
        }
    }
    nk_end(ctx);
}

/* ---- events ----------------------------------------------------------- */

static int in_viewport(const gui_t *g, float x, float y, int vw, int vh)
{
    return x >= 0 && y >= 0 && x < vw && y < vh;
}

static void pick_at(gui_t *g, float x, float y, int vw, int vh)
{
    double o[3], d[3], hit[3];
    int slot;
    g->hover_valid = 0;
    g->view.highlight_slot = -2;
    if (!g->app->model.meshes_valid) return;
    camera_ray(&g->cam, vw, vh, x, y, o, d);
    if (model_pick(&g->app->model, &g->app->params, o, d, hit, &slot, NULL)) {
        memcpy(g->hover_pt, hit, sizeof(hit));
        g->hover_slot = slot;
        g->hover_valid = 1;
        g->view.highlight_slot = slot;
    }
}

/* True while a text field or a property in edit mode has keyboard focus. */
static int text_field_active(gui_t *g)
{
    struct nk_window *w = nk_window_find(g->ctx, "Panel");
    return w && (w->edit.active || w->property.active);
}

static void handle_event(gui_t *g, const SDL_Event *e, int vw, int vh, int *running)
{
    int text_active = text_field_active(g);
    switch (e->type) {
    case SDL_EVENT_QUIT:
        *running = 0;
        break;
    case SDL_EVENT_DROP_FILE:
        if (e->drop.data) load_file(g, e->drop.data);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (in_viewport(g, e->button.x, e->button.y - g->tab_h, vw, vh)) {
            g->press_mx = g->last_mx = e->button.x;
            g->press_my = g->last_my = e->button.y;
            if (e->button.button == SDL_BUTTON_LEFT) g->drag_button = 1;
            else if (e->button.button == SDL_BUTTON_RIGHT || e->button.button == SDL_BUTTON_MIDDLE) g->drag_button = 2;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (g->drag_button) {
            float dx = e->button.x - g->press_mx, dy = e->button.y - g->press_my;
            int click = g->drag_button == 1 && fabsf(dx) < 3 && fabsf(dy) < 3;
            if (click && g->tab == 1) {
                int hit = grid_hit(g, vw, vh, e->button.x, e->button.y - g->tab_h);
                Uint64 now = SDL_GetTicks();
                if (hit >= 0 && hit == g->last_click_piece && now - g->last_click_ms < 400) {
                    g->tab = 2;
                    g->sel_piece = hit;
                    g->last_click_piece = -1;
                } else {
                    g->sel_piece = hit;
                    g->last_click_piece = hit;
                    g->last_click_ms = now;
                }
            } else if (click && g->measure_mode) {
                pick_at(g, e->button.x, e->button.y - g->tab_h, vw, vh);
                if (g->hover_valid) {
                    if (g->nmeasure >= 2) g->nmeasure = 0;
                    memcpy(g->mpts[g->nmeasure], g->hover_pt, sizeof(g->hover_pt));
                    g->nmeasure++;
                }
            }
            g->drag_button = 0;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (g->drag_button) {
            float dx = e->motion.x - g->last_mx, dy = e->motion.y - g->last_my;
            if (g->drag_button == 1) camera_orbit(&g->cam, dx, dy);
            else if (g->tab == 0) camera_pan(&g->cam, dx, dy, vh);
            g->last_mx = e->motion.x;
            g->last_my = e->motion.y;
        } else if (g->tab == 0 && in_viewport(g, e->motion.x, e->motion.y - g->tab_h, vw, vh)) {
            pick_at(g, e->motion.x, e->motion.y - g->tab_h, vw, vh);
        } else {
            g->hover_valid = 0;
            g->view.highlight_slot = -2;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL: {
        float mx, my;
        SDL_GetMouseState(&mx, &my);
        if (in_viewport(g, mx, my - g->tab_h, vw, vh)) {
            if (g->tab == 1) { g->grid_zoom *= powf(1.15f, e->wheel.y); if (g->grid_zoom < 0.2f) g->grid_zoom = 0.2f; if (g->grid_zoom > 8) g->grid_zoom = 8; }
            else camera_zoom(&g->cam, e->wheel.y);
        }
        break;
    }
    case SDL_EVENT_KEY_DOWN:
        if (text_active) break;
        switch (e->key.key) {
        case SDLK_F: camera_fit(&g->cam, &g->app->model); break;
        case SDLK_1: camera_preset(&g->cam, 2); break;
        case SDLK_3: camera_preset(&g->cam, 3); break;
        case SDLK_7: camera_preset(&g->cam, 1); break;
        case SDLK_0: camera_preset(&g->cam, 0); break;
        case SDLK_P: g->cam.ortho = !g->cam.ortho; break;
        case SDLK_M: g->measure_mode = !g->measure_mode; g->nmeasure = 0; break;
        case SDLK_ESCAPE: g->nmeasure = 0; g->measure_mode = 0; break;
        case SDLK_O: if (e->key.mod & SDL_KMOD_CTRL) start_open_dialog(g); break;
        case SDLK_E: if (e->key.mod & SDL_KMOD_CTRL) start_save_dialog(g, 4); break;
        default: break;
        }
        break;
    default:
        break;
    }
}

/* ---- fonts ------------------------------------------------------------ */

static void load_fonts(gui_t *g)
{
    struct nk_font_atlas *atlas;
    struct nk_font *font = NULL;
    static const char *candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        NULL
    };
    float size = floorf(15.0f * g->ui + 0.5f);
    /* Nuklear lays out and draws in window units, which on a Retina Mac cover
     * g->px framebuffer pixels each.  Everything but the text is solid-coloured
     * triangles and magnifies losslessly; glyphs come from a texture, so bake
     * the atlas at the size the pixels actually are and let nuklear scale the
     * quads back down (it divides by handle.height / info.height).  The atlas
     * then lands on the framebuffer one texel per pixel. */
    float baked = floorf(size * g->px + 0.5f);
    struct nk_font_config cfg = nk_font_config(baked);
    int i;
    /* glyphs rendered 1:1 on integer pixels stay sharp */
    cfg.oversample_h = 1;
    cfg.oversample_v = 1;
    cfg.pixel_snap = nk_true;
    nk_sdl_font_stash_begin(&atlas);
    for (i = 0; candidates[i] && !font; i++) {
        SDL_PathInfo info;
        if (SDL_GetPathInfo(candidates[i], &info)) font = nk_font_atlas_add_from_file(atlas, candidates[i], baked, &cfg);
    }
    if (!font) {
        size = floorf(13.0f * g->ui + 0.5f);
        baked = floorf(size * g->px + 0.5f);
        cfg = nk_font_config(baked);
        cfg.oversample_h = 1;
        cfg.oversample_v = 1;
        cfg.pixel_snap = nk_true;
        font = nk_font_atlas_add_default(atlas, baked, &cfg);
    }
    nk_sdl_font_stash_end();
    if (font) {
        font->handle.height = size;   /* draw the oversized atlas at its window-unit size */
        nk_style_set_font(g->ctx, &font->handle);
    }
    /* clearer check boxes: dark box, bright mark when checked */
    {
        struct nk_style *s = &g->ctx->style;
        s->checkbox.normal = nk_style_item_color(nk_rgb(48, 48, 54));
        s->checkbox.hover = nk_style_item_color(nk_rgb(64, 64, 72));
        s->checkbox.active = nk_style_item_color(nk_rgb(64, 64, 72));
        s->checkbox.cursor_normal = nk_style_item_color(nk_rgb(96, 176, 255));
        s->checkbox.cursor_hover = nk_style_item_color(nk_rgb(130, 196, 255));
        s->checkbox.border_color = nk_rgb(130, 130, 140);
        s->checkbox.border = 1.0f;
        s->checkbox.padding = nk_vec2(3 * g->ui, 3 * g->ui);
        s->option.normal = s->checkbox.normal;
        s->option.hover = s->checkbox.hover;
        s->option.active = s->checkbox.active;
        s->option.cursor_normal = s->checkbox.cursor_normal;
        s->option.cursor_hover = s->checkbox.cursor_hover;
        s->option.border_color = s->checkbox.border_color;
        s->option.border = 1.0f;
    }
}

/* ---- main ------------------------------------------------------------- */

/* The window (and dock / taskbar) icon, decoded from the run-length RGBA
 * stream in icon_data.h. SDL copies the surface, so nothing has to stay
 * alive afterwards; failures just leave the default icon. */
static void set_window_icon(SDL_Window *win)
{
    size_t npix = (size_t)ICON_W * ICON_H, o = 0, i;
    Uint8 *px = (Uint8 *)malloc(npix * 4);
    SDL_Surface *s;
    if (!px) return;
    for (i = 0; i + 5 <= sizeof icon_rle; i += 5) {
        int n = icon_rle[i];
        while (n-- > 0 && o < npix) {
            memcpy(px + o * 4, icon_rle + i + 1, 4);
            o++;
        }
    }
    if (o == npix) {
        s = SDL_CreateSurfaceFrom(ICON_W, ICON_H, SDL_PIXELFORMAT_RGBA32, px, ICON_W * 4);
        if (s) {
            SDL_SetWindowIcon(win, s);
            SDL_DestroySurface(s);
        }
    }
    free(px);
}

int gui_main(app_state *a)
{
    gui_t g;
    int running = 1;
    char err[256];
    const char *missing = NULL;

    memset(&g, 0, sizeof(g));
    g.app = a;
    g.screenshot_path = a->screenshot_path;
    g.sel_piece = -1;
    g.last_click_piece = -1;
    g.last_view_tab = -1;
    g.last_view_sel = -1;
    g.grid_zoom = 1.0f;
    g.last_nchunks = 1;
    g.same_height = 1.0f;
    g.stagger_first = 0.6f;
    g.stagger_step = 0.2f;
    g.export_mode = 1;
    g.show_dims = 1;
    g.show_triad = 1;
    g.view.show_grid = 1;
    g.view.show_bed = 1;
    g.view.show_outline = 1;
    g.view.grid_step = 10;
    g.view.bed_w = 250;     /* typical print plate today: 25 x 25 cm */
    g.view.bed_d = 250;
    g.view.highlight_slot = -2;
    g.view.bg[0] = 0.16f; g.view.bg[1] = 0.17f; g.view.bg[2] = 0.20f;
    g.cam.fov = 40;
    camera_preset(&g.cam, a->view_preset);
    g.cam.dist = 150;
    snprintf(g.path_buf, sizeof(g.path_buf), "%s", a->svg_path);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    {
        int ww = a->win_w > 0 ? a->win_w : 1280, wh = a->win_h > 0 ? a->win_h : 800;
        g.win = SDL_CreateWindow("logo3dprint", ww, wh, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    }
    if (g.win) g.gl = SDL_GL_CreateContext(g.win);
    if (!g.win || !g.gl) {
        /* retry without multisampling */
        if (g.win) SDL_DestroyWindow(g.win);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        g.win = SDL_CreateWindow("logo3dprint", a->win_w > 0 ? a->win_w : 1280, a->win_h > 0 ? a->win_h : 800,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        g.gl = g.win ? SDL_GL_CreateContext(g.win) : NULL;
        if (!g.win || !g.gl) {
            fprintf(stderr, "Could not create an OpenGL 3.2 window: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
    }
    SDL_SetWindowMinimumSize(g.win, 700, 480);
    set_window_icon(g.win);
    SDL_GL_MakeCurrent(g.win, g.gl);
    SDL_GL_SetSwapInterval(1);
    if (!glapi_load((void *(*)(const char *))SDL_GL_GetProcAddress, &missing)) {
        fprintf(stderr, "OpenGL function %s is missing; OpenGL 3.2 core is required.\n", missing ? missing : "?");
        SDL_GL_DestroyContext(g.gl);
        SDL_DestroyWindow(g.win);
        SDL_Quit();
        return 1;
    }
    {
        float ds = SDL_GetWindowDisplayScale(g.win), pd = SDL_GetWindowPixelDensity(g.win);
        if (ds <= 0) ds = 1;
        if (pd <= 0) pd = 1;
        g.ui = ds / pd;
        if (g.ui < 0.75f) g.ui = 0.75f;
        if (g.ui > 3.0f) g.ui = 3.0f;
        /* keep the scale on quarter steps so widget sizes land on whole pixels */
        g.ui = floorf(g.ui * 4.0f + 0.5f) / 4.0f;
        g.px = pd;
        fprintf(stderr, "display scale %.2f, pixel density %.2f, UI scale %.2f\n", ds, pd, g.ui);
    }
    g.lock = SDL_CreateMutex();
    g.ctx = nk_sdl_init(g.win);
    load_fonts(&g);
    g.ren = render_create(err, sizeof(err));
    if (!g.ren) {
        fprintf(stderr, "Renderer setup failed: %s\n", err);
        nk_sdl_shutdown();
        SDL_GL_DestroyContext(g.gl);
        SDL_DestroyWindow(g.win);
        SDL_Quit();
        return 1;
    }
    if (a->model.meshes_valid) {
        char title[1200];
        model_changed(&g);
        camera_fit(&g.cam, &a->model);
        if (a->open_piece > 0 && a->open_piece <= a->model.nchunks) {
            g.tab = 2;
            g.sel_piece = a->open_piece - 1;
            g.last_nchunks = a->model.nchunks;
        } else if (a->init_tab == 1) {
            g.tab = 0;
            g.last_nchunks = a->model.nchunks;
        } else if (a->init_tab == 2 && a->model.nchunks > 1) {
            g.tab = 1;
            g.last_nchunks = a->model.nchunks;
        }
        snprintf(title, sizeof(title), "logo3dprint - %s", a->svg_path);
        SDL_SetWindowTitle(g.win, title);
    } else if (a->last_error[0]) {
        set_status(&g, "Error: %s", a->last_error);
    }
    g.panel_w = (int)(370 * g.ui);

    while (running) {
        SDL_Event e;
        int ww, wh, pw, ph, vw, vh;
        SDL_GetWindowSize(g.win, &ww, &wh);
        SDL_GetWindowSizeInPixels(g.win, &pw, &ph);
        g.px = ww > 0 ? (float)pw / (float)ww : 1.0f;
        vw = ww - g.panel_w;
        vh = wh;
        if (vw < 100) vw = 100;

        /* tab bar only while the logo is split into pieces */
        {
            int nch = a->model.meshes_valid ? a->model.nchunks : 1;
            if (nch != g.last_nchunks) {
                g.tab = nch > 1 ? 1 : 0;
                g.sel_piece = -1;
                g.last_nchunks = nch;
            }
            g.tab_h = nch > 1 ? (int)(30 * g.ui) : 0;
            if (nch <= 1) g.tab = 0;
        }
        vh = wh - g.tab_h;

        nk_input_begin(g.ctx);
        while (SDL_PollEvent(&e)) {
            nk_sdl_handle_event(&e);
            handle_event(&g, &e, vw, vh, &running);
        }
        nk_input_end(g.ctx);

        /* pieces must fit the plate with a little clearance */
        a->params.chunk_max_w = g.view.bed_w - 4;
        a->params.chunk_max_d = g.view.bed_d - 4;
        /* the piece tabs drive the single-piece preview */
        if (g.tab == 2 && g.sel_piece >= 0 && g.sel_piece < a->model.nchunks) a->params.chunk_view = g.sel_piece + 1;
        else a->params.chunk_view = 0;
        poll_dialogs(&g);
        schedule_rebuild(&g);
        if (g.tab != g.last_view_tab || (g.tab == 2 && g.sel_piece != g.last_view_sel)) {
            if (g.tab != 1) camera_fit(&g.cam, &a->model);
            g.last_view_tab = g.tab;
            g.last_view_sel = g.sel_piece;
            g.nmeasure = 0;
        }

        /* tab bar */
        if (g.tab_h > 0) {
            struct nk_rect r = nk_rect(0, 0, (float)vw, (float)g.tab_h);
            char label[64];
            if (nk_window_find(g.ctx, "Tabs")) nk_window_set_bounds(g.ctx, "Tabs", r);
            if (nk_begin(g.ctx, "Tabs", r, NK_WINDOW_NO_SCROLLBAR)) {
                struct nk_style_button active = g.ctx->style.button, normal = g.ctx->style.button;
                int n = a->model.nchunks, i;
                float wide = 110 * g.ui, num = 34 * g.ui, arrow = 26 * g.ui, sp = g.ctx->style.window.spacing.x;
                int fit = (int)((vw - 2 * wide - 2 * arrow - 8 * sp - 12 * g.ui) / (num + sp));
                int need_arrows;
                if (fit < 1) fit = 1;
                need_arrows = n > fit;
                if (!need_arrows) g.tab_first = 0;
                if (g.tab_first > n - fit) g.tab_first = n - fit;
                if (g.tab_first < 0) g.tab_first = 0;
                if (g.tab == 2 && (g.sel_piece < g.tab_first || g.sel_piece >= g.tab_first + fit)) {
                    g.tab_first = g.sel_piece - fit / 2;
                    if (g.tab_first < 0) g.tab_first = 0;
                    if (g.tab_first > n - fit) g.tab_first = n - fit;
                }
                active.normal = nk_style_item_color(nk_rgb(60, 110, 170));
                active.hover = nk_style_item_color(nk_rgb(70, 125, 190));
                active.text_normal = nk_rgb(255, 255, 255);
                nk_layout_row_begin(g.ctx, NK_STATIC, 22 * g.ui, 4 + fit);
                nk_layout_row_push(g.ctx, wide);
                if (nk_button_label_styled(g.ctx, g.tab == 0 ? &active : &normal, "Model")) g.tab = 0;
                nk_layout_row_push(g.ctx, wide);
                snprintf(label, sizeof(label), "Pieces (%d)", n);
                if (nk_button_label_styled(g.ctx, g.tab == 1 ? &active : &normal, label)) g.tab = 1;
                nk_layout_row_push(g.ctx, arrow);
                if (need_arrows) { if (nk_button_symbol(g.ctx, NK_SYMBOL_TRIANGLE_LEFT) && g.tab_first > 0) g.tab_first--; }
                else nk_spacing(g.ctx, 1);
                for (i = g.tab_first; i < g.tab_first + fit && i < n; i++) {
                    nk_layout_row_push(g.ctx, num);
                    snprintf(label, sizeof(label), "%d", i + 1);
                    if (nk_button_label_styled(g.ctx, (g.tab == 2 && g.sel_piece == i) ? &active : &normal, label)) { g.tab = 2; g.sel_piece = i; }
                }
                nk_layout_row_push(g.ctx, arrow);
                if (need_arrows) { if (nk_button_symbol(g.ctx, NK_SYMBOL_TRIANGLE_RIGHT) && g.tab_first < n - fit) g.tab_first++; }
                nk_layout_row_end(g.ctx);
            }
            nk_end(g.ctx);
        }
        /* transparent overlay window over the viewport (no input) */
        {
            struct nk_style_item saved_bg = g.ctx->style.window.fixed_background;
            struct nk_vec2 saved_pad = g.ctx->style.window.padding;
            struct nk_rect r = nk_rect(0, (float)g.tab_h, (float)vw, (float)vh);
            g.ctx->style.window.fixed_background = nk_style_item_color(nk_rgba(0, 0, 0, 0));
            g.ctx->style.window.padding = nk_vec2(0, 0);
            if (nk_window_find(g.ctx, "Overlay")) nk_window_set_bounds(g.ctx, "Overlay", r);
            if (nk_begin(g.ctx, "Overlay", r, NK_WINDOW_NO_INPUT | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
                overlay_t o;
                o.canvas = nk_window_get_canvas(g.ctx);
                o.font = g.ctx->style.font;
                o.ox = 0; o.oy = (float)g.tab_h;
                o.vw = vw; o.vh = vh;
                o.ui = g.ui;
                draw_overlay(&g, &o);
            }
            nk_end(g.ctx);
            g.ctx->style.window.fixed_background = saved_bg;
            g.ctx->style.window.padding = saved_pad;
        }
        panel(&g, vw, 0, ww - vw, wh);

        /* render */
        glViewport(0, 0, pw, ph);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.11f, 0.11f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (g.tab == 1 && a->model.nchunks > 1) {
            int i;
            view_opts vo = g.view;
            vo.highlight_slot = -2;
            vo.bg[0] = 0.13f; vo.bg[1] = 0.14f; vo.bg[2] = 0.17f;
            vo.bed_w = a->params.chunk_max_w + 4;
            vo.bed_d = a->params.chunk_max_d + 4;
            for (i = 0; i < a->model.nchunks; i++) {
                float cx, cy, cw, ch;
                camera_t cam;
                int px0, py0, pw0, ph0;
                grid_cell(&g, vw, vh, i, &cx, &cy, &cw, &ch);
                grid_camera(&g, i, (int)cw, (int)ch, &cam);
                px0 = (int)(cx * g.px);
                pw0 = (int)(cw * g.px);
                ph0 = (int)(ch * g.px);
                py0 = ph - (int)((g.tab_h + cy) * g.px) - ph0;
                render_draw_chunk(g.ren, i, &cam, &vo, px0, py0, pw0, ph0);
            }
        } else {
            render_draw(g.ren, &g.cam, &g.view, 0, ph - (int)((g.tab_h + vh) * g.px), (int)(vw * g.px), (int)(vh * g.px));
        }
        nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
        if (g.screenshot_path && ++g.frame == 5) {
            /* dump the back buffer as a binary PPM and quit (used by tests) */
            unsigned char *pix = (unsigned char *)malloc((size_t)pw * (size_t)ph * 3);
            FILE *f = fopen(g.screenshot_path, "wb");
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, pw, ph, GL_RGB, GL_UNSIGNED_BYTE, pix);
            if (f) {
                int y;
                fprintf(f, "P6\n%d %d\n255\n", pw, ph);
                for (y = ph - 1; y >= 0; y--) fwrite(pix + (size_t)y * (size_t)pw * 3, 1, (size_t)pw * 3, f);
                fclose(f);
            }
            free(pix);
            running = 0;
        }
        SDL_GL_SwapWindow(g.win);
    }

    render_destroy(g.ren);
    nk_sdl_shutdown();
    SDL_DestroyMutex(g.lock);
    SDL_GL_DestroyContext(g.gl);
    SDL_DestroyWindow(g.win);
    SDL_Quit();
    return 0;
}
