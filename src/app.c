#include "app.h"
#include "textfont.h"

#include <stdio.h>
#include <string.h>

void app_init(app_state *a)
{
    memset(a, 0, sizeof(*a));
    model_params_default(&a->params);
    model_init(&a->model);
}

void app_free(app_state *a)
{
    svg_free(a->doc);
    a->doc = NULL;
    model_free(&a->model);
    textfont_cleanup();
}

int app_load_svg(app_state *a, const char *path)
{
    char err[256];
    svg_doc *doc = svg_parse_file(path, a->text_font[0] ? a->text_font : NULL, err, sizeof(err));
    if (!doc) {
        snprintf(a->last_error, sizeof(a->last_error), "%s", err[0] ? err : "could not parse SVG");
        return 0;
    }
    svg_free(a->doc);
    a->doc = doc;
    snprintf(a->svg_path, sizeof(a->svg_path), "%s", path);
    a->last_error[0] = 0;
    /* the size parameters are kept (default: 200 mm wide model) */
    return app_rebuild(a);
}

int app_reload_svg(app_state *a)
{
    char err[256];
    svg_doc *doc;
    if (!a->svg_path[0]) return 0;
    doc = svg_parse_file(a->svg_path, a->text_font[0] ? a->text_font : NULL, err, sizeof(err));
    if (!doc) {
        snprintf(a->last_error, sizeof(a->last_error), "%s", err[0] ? err : "could not parse SVG");
        return 0;
    }
    svg_free(a->doc);
    a->doc = doc;
    return app_rebuild(a);
}

int app_rebuild(app_state *a)
{
    char err[256];
    /* remember previous slot settings by colour, so they survive a re-layout */
    unsigned old_rgb[MAX_SLOTS];
    double old_h[MAX_SLOTS];
    unsigned old_ov_rgb[MAX_SLOTS];
    int old_ov[MAX_SLOTS], old_vis[MAX_SLOTS], old_merge[MAX_SLOTS], old_n = a->model.nslots, i, j;
    for (i = 0; i < old_n; i++) {
        old_rgb[i] = a->model.slots[i].rgb;
        old_h[i] = a->params.slot_height[i];
        old_ov[i] = a->params.slot_rgb_override[i];
        old_ov_rgb[i] = a->params.slot_rgb[i];
        old_vis[i] = a->params.slot_visible[i];
        old_merge[i] = a->params.slot_merge_into[i];
    }
    if (!a->doc) return 0;
    if (!model_layout(&a->model, a->doc, &a->params, err, sizeof(err))) {
        snprintf(a->last_error, sizeof(a->last_error), "%s", err[0] ? err : "layout failed");
        return 0;
    }
    if (old_n > 0) {
        int changed = 0;
        for (i = 0; i < a->model.nslots; i++)
            if (i >= old_n || old_rgb[i] != a->model.slots[i].rgb) changed = 1;
        if (changed || old_n != a->model.nslots) {
            /* remap per-slot settings by colour */
            model_params np = a->params;
            for (i = 0; i < a->model.nslots; i++) {
                np.slot_height[i] = 1.0;
                np.slot_rgb_override[i] = 0;
                np.slot_visible[i] = 1;
                np.slot_merge_into[i] = -1;
                for (j = 0; j < old_n; j++) {
                    if (old_rgb[j] == a->model.slots[i].rgb) {
                        np.slot_height[i] = old_h[j];
                        np.slot_rgb_override[i] = old_ov[j];
                        np.slot_rgb[i] = old_ov_rgb[j];
                        np.slot_visible[i] = old_vis[j];
                        if (old_merge[j] >= 0 && old_merge[j] < old_n) {
                            int k;
                            for (k = 0; k < a->model.nslots; k++)
                                if (a->model.slots[k].rgb == old_rgb[old_merge[j]] && k != i) np.slot_merge_into[i] = k;
                        }
                        break;
                    }
                }
            }
            if (memcmp(&np, &a->params, sizeof(np)) != 0) {
                a->params = np;
                if (!model_layout(&a->model, a->doc, &a->params, err, sizeof(err))) {
                    snprintf(a->last_error, sizeof(a->last_error), "%s", err[0] ? err : "layout failed");
                    return 0;
                }
            }
        }
    }
    return app_rebuild_meshes(a);
}

int app_rebuild_meshes(app_state *a)
{
    if (!a->model.valid) return 0;
    if (!model_build_meshes(&a->model, &a->params)) {
        snprintf(a->last_error, sizeof(a->last_error), "mesh generation failed");
        return 0;
    }
    return 1;
}

double app_fit_whole_model(app_state *a, double plate_w, double plate_d)
{
    model_params *p = &a->params;
    double mg = (p->base_enabled && p->base_thickness > 0 && p->base_margin > 0) ? 2.0 * p->base_margin : 0.0;
    double avail_w = plate_w - p->plate_padding - mg, avail_d = plate_d - p->plate_padding - mg;
    double lw = a->model.logo_w, lh = a->model.logo_h, s;
    if (!a->model.valid || lw <= 0 || lh <= 0) return 0;
    if (avail_w < 5) avail_w = 5;
    if (avail_d < 5) avail_d = 5;
    s = avail_w / lw;
    if (avail_d / lh < s) s = avail_d / lh;
    p->width_mm = lw * s + mg;
    p->fit_by_height = 0;
    return p->width_mm;
}

int app_rebuild_view(app_state *a)
{
    if (!a->model.valid || !a->model.meshes_valid) return 0;
    return model_build_view(&a->model, &a->params);
}

void app_stagger_heights(app_state *a, double first, double step)
{
    int i, k = 0;
    for (i = 0; i < a->model.nslots; i++) {
        if (a->model.slots[i].merged_into >= 0) continue;
        a->params.slot_height[i] = first + step * k;
        k++;
    }
}
