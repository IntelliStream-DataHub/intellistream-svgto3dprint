/* Undo/redo stack for the right-hand settings panel.
 *
 * History stores snapshots of every panel-editable setting.  Consecutive
 * changes while the user is still dragging a slider or typing in a field
 * coalesce into one step. */
#ifndef LOGO3D_PANEL_UNDO_H
#define LOGO3D_PANEL_UNDO_H

#include "model.h"
#include <string.h>

#define PANEL_UNDO_MAX 64

typedef struct {
    model_params params;
    float bed_w, bed_d, grid_step;
    int show_bed, show_grid, show_bbox, show_outline;
    int show_dims, show_slot_dims, show_triad, cam_ortho, export_mode;
    float same_height, stagger_first, stagger_step;
} panel_undo_state;

typedef struct {
    panel_undo_state hist[PANEL_UNDO_MAX];
    int n, cur;
    int group;              /* 1 while coalescing an in-progress gesture */
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

static void panel_undo_record(panel_undo *u, const panel_undo_state *now, int coalesce)
{
    if (u->n <= 0) {
        u->hist[0] = *now;
        u->n = 1;
        u->cur = 0;
        u->group = 0;
        return;
    }
    if (panel_undo_equal(&u->hist[u->cur], now)) {
        if (!coalesce) u->group = 0;
        return;
    }
    if (coalesce && u->group) {
        u->hist[u->cur] = *now;
        return;
    }
    if (u->cur + 1 < PANEL_UNDO_MAX) u->cur++;
    else memmove(&u->hist[0], &u->hist[1], sizeof(u->hist[0]) * (PANEL_UNDO_MAX - 1));
    u->hist[u->cur] = *now;
    u->n = u->cur + 1;
    u->group = coalesce ? 1 : 0;
}

static int panel_undo_can_undo(const panel_undo *u) { return u->cur > 0; }
static int panel_undo_can_redo(const panel_undo *u) { return u->cur + 1 < u->n; }

static const panel_undo_state *panel_undo_undo(panel_undo *u)
{
    if (!panel_undo_can_undo(u)) return NULL;
    u->group = 0;
    return &u->hist[--u->cur];
}

static const panel_undo_state *panel_undo_redo(panel_undo *u)
{
    if (!panel_undo_can_redo(u)) return NULL;
    u->group = 0;
    return &u->hist[++u->cur];
}

#endif
