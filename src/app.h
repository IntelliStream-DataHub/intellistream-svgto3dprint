/* Shared application state between the CLI and the GUI. */
#ifndef LOGO3D_APP_H
#define LOGO3D_APP_H

#include "svg.h"
#include "model.h"

typedef struct {
    char svg_path[1024];
    svg_doc *doc;
    model_params params;
    model_t model;
    /* the colour slots the per-slot params (heights, visibility, merges, base
     * colour) refer to: app_rebuild() remaps them by colour when a re-layout
     * changes the slots, and an undo in the GUI restores them with the params */
    int pslots_n;
    unsigned pslots_rgb[MAX_SLOTS];
    int width_from_cli;         /* the user gave an explicit size */
    char last_error[512];
    const char *screenshot_path; /* GUI: save a frame to this PPM file and exit */
    int view_preset;            /* GUI: initial camera preset (0 iso, 1 top, 2 front, 3 right) */
    int win_w, win_h;           /* GUI: initial window size (0 = default) */
    int open_piece;             /* GUI: start on the tab of this piece (1-based, 0 = none) */
    int init_tab;               /* GUI: 0 = auto (pieces grid when split), 1 = Model tab, 2 = Pieces grid */
    char text_font[1024];       /* font file for <text> ("" = pick system fonts) */
} app_state;

void app_init(app_state *a);
void app_free(app_state *a);
/* Load an SVG; sets a sensible default size when none was given. Returns 1 on success. */
int app_load_svg(app_state *a, const char *path);
/* Re-parse the current SVG (e.g. after a font change), keeping all settings. */
int app_reload_svg(app_state *a);
/* Recompute layout + meshes. Returns 1 on success. */
int app_rebuild(app_state *a);
/* Only rebuild meshes (heights / base changes). */
int app_rebuild_meshes(app_state *a);
/* Only rebuild the preview (piece selection / spacing). */
int app_rebuild_view(app_state *a);
/* Resize the whole (unsplit) model to the plate minus padding, keeping proportions.
 * Returns the new model width, or 0 when there is no model. */
double app_fit_whole_model(app_state *a, double plate_w, double plate_d);
/* Apply stagger heights: slot i gets base + i * step (in slot order). */
void app_stagger_heights(app_state *a, double first, double step);

/* Implemented by the GUI. Returns process exit code. */
int gui_main(app_state *a);

#endif
