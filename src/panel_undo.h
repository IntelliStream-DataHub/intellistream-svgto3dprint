/* Undo/redo stack for the right-hand settings panel.
 *
 * History stores snapshots of every panel-editable setting.  Consecutive
 * changes made by one gesture (a slider drag, a property being typed into)
 * coalesce into one step. */
#ifndef LOGO3D_PANEL_UNDO_H
#define LOGO3D_PANEL_UNDO_H

#include "model.h"
#include <string.h>

#define PANEL_UNDO_MAX 64

typedef struct {
    model_params params;
    int nslots;                     /* the colour slots params refer to (app_state.pslots_*) */
    unsigned slot_rgb[MAX_SLOTS];
    float bed_w, bed_d, grid_step;
    int show_bed, show_grid, show_bbox, show_outline;
    int show_dims, show_slot_dims, show_triad, cam_ortho, export_mode;
    float same_height, stagger_first, stagger_step;
} panel_undo_state;

typedef struct {
    panel_undo_state hist[PANEL_UNDO_MAX];
    int n, cur;
    unsigned gesture;       /* the gesture the newest step belongs to, 0 when it is over */
} panel_undo;

/* 1 = undo, 2 = redo, 0 = neither.
 * `primary` is Command on macOS and Control elsewhere. */
static int panel_undo_hotkey(int primary, int shift, int alt, int key)
{
    if (!primary || alt) return 0;
    if (key == 'z' && !shift) return 1;
    if (key == 'z' && shift) return 2;
    if (key == 'y' && !shift) return 2;
    return 0;
}

static int panel_undo_equal(const panel_undo_state *a, const panel_undo_state *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

static void panel_undo_reset(panel_undo *u, const panel_undo_state *now)
{
    memset(u, 0, sizeof(*u));
    if (now) {
        u->hist[0] = *now;
        u->n = 1;
        u->cur = 0;
    }
}

/* Record the state at the end of a frame.  `gesture` names the edit in
 * progress (non-zero while the mouse is held on a widget or a field is being
 * typed into): changes within one gesture rewrite the newest step, a change
 * under a different gesture, or none, starts a new one. */
static void panel_undo_record(panel_undo *u, const panel_undo_state *now, unsigned gesture)
{
    if (u->n <= 0) {
        u->hist[0] = *now;
        u->n = 1;
        u->cur = 0;
        u->gesture = 0;
        return;
    }
    if (panel_undo_equal(&u->hist[u->cur], now)) {
        if (!gesture) u->gesture = 0;
        return;
    }
    if (gesture && gesture == u->gesture) {
        u->hist[u->cur] = *now;
        return;
    }
    if (u->cur + 1 < PANEL_UNDO_MAX) u->cur++;
    else memmove(&u->hist[0], &u->hist[1], sizeof(u->hist[0]) * (PANEL_UNDO_MAX - 1));
    u->hist[u->cur] = *now;
    u->n = u->cur + 1;
    u->gesture = gesture;
}

static int panel_undo_can_undo(const panel_undo *u) { return u->cur > 0; }
static int panel_undo_can_redo(const panel_undo *u) { return u->cur + 1 < u->n; }

static const panel_undo_state *panel_undo_undo(panel_undo *u)
{
    if (!panel_undo_can_undo(u)) return NULL;
    u->gesture = 0;
    return &u->hist[--u->cur];
}

static const panel_undo_state *panel_undo_redo(panel_undo *u)
{
    if (!panel_undo_can_redo(u)) return NULL;
    u->gesture = 0;
    return &u->hist[++u->cur];
}

#endif
