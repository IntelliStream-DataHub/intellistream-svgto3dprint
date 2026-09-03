#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Mesh with vertex welding                                            */

void mesh_init(mesh_t *m)
{
    memset(m, 0, sizeof(*m));
}

void mesh_free(mesh_t *m)
{
    free(m->v);
    free(m->t);
    free(m->hash);
    mesh_init(m);
}

static uint64_t hash_coords(int64_t x, int64_t y, int64_t z)
{
    uint64_t h = (uint64_t)x * 0x9E3779B97F4A7C15ULL;
    h ^= (uint64_t)y * 0xC2B2AE3D27D4EB4FULL;
    h ^= (uint64_t)z * 0x165667B19E3779F9ULL;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 32;
    return h;
}

static int64_t quant(double v)
{
    return (int64_t)floor(v * 1e6 + 0.5);
}

static void mesh_rehash(mesh_t *m, int cap)
{
    int i;
    int *nh = (int *)malloc(sizeof(int) * cap);
    for (i = 0; i < cap; i++) nh[i] = -1;
    for (i = 0; i < m->nv; i++) {
        uint64_t h = hash_coords(quant(m->v[3 * i]), quant(m->v[3 * i + 1]), quant(m->v[3 * i + 2]));
        int slot = (int)(h & (uint64_t)(cap - 1));
        while (nh[slot] != -1) slot = (slot + 1) & (cap - 1);
        nh[slot] = i;
    }
    free(m->hash);
    m->hash = nh;
    m->hcap = cap;
}

unsigned mesh_add_vertex(mesh_t *m, double x, double y, double z)
{
    int64_t qx = quant(x), qy = quant(y), qz = quant(z);
    uint64_t h;
    int slot;
    if (m->hcap == 0 || m->nv * 2 >= m->hcap) mesh_rehash(m, m->hcap ? m->hcap * 2 : 1024);
    h = hash_coords(qx, qy, qz);
    slot = (int)(h & (uint64_t)(m->hcap - 1));
    while (m->hash[slot] != -1) {
        int i = m->hash[slot];
        if (quant(m->v[3 * i]) == qx && quant(m->v[3 * i + 1]) == qy && quant(m->v[3 * i + 2]) == qz) return (unsigned)i;
        slot = (slot + 1) & (m->hcap - 1);
    }
    if (m->nv == m->cv) {
        m->cv = m->cv ? m->cv * 2 : 1024;
        m->v = (double *)realloc(m->v, sizeof(double) * 3 * m->cv);
    }
    m->v[3 * m->nv] = (double)qx / 1e6;
    m->v[3 * m->nv + 1] = (double)qy / 1e6;
    m->v[3 * m->nv + 2] = (double)qz / 1e6;
    m->hash[slot] = m->nv;
    return (unsigned)m->nv++;
}

void mesh_add_tri(mesh_t *m, unsigned a, unsigned b, unsigned c)
{
    if (a == b || b == c || a == c) return;
    if (m->nt == m->ct) {
        m->ct = m->ct ? m->ct * 2 : 1024;
        m->t = (unsigned *)realloc(m->t, sizeof(unsigned) * 3 * m->ct);
    }
    m->t[3 * m->nt] = a;
    m->t[3 * m->nt + 1] = b;
    m->t[3 * m->nt + 2] = c;
    m->nt++;
}

double mesh_volume(const mesh_t *m)
{
    double vol = 0;
    int i;
    for (i = 0; i < m->nt; i++) {
        const double *a = &m->v[3 * m->t[3 * i]], *b = &m->v[3 * m->t[3 * i + 1]], *c = &m->v[3 * m->t[3 * i + 2]];
        vol += a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
    }
    return fabs(vol) / 6.0;
}

/* ------------------------------------------------------------------ */
/* Parameters                                                          */

void model_params_default(model_params *p)
{
    int i;
    memset(p, 0, sizeof(*p));
    p->width_mm = 200;      /* overall model width, fits a typical 250 mm plate */
    p->height_mm = 200;
    p->fit_by_height = 0;
    p->mirror_x = 0;
    p->curve_tol_mm = 0.02;
    p->merge_threshold = 0;
    p->max_colors = MAX_SLOTS;
    p->base_enabled = 1;
    p->base_thickness = 2.0;
    p->base_margin = 3.0;
    p->base_radius = 3.0;
    p->base_color_slot = -1;
    p->base_rgb = 0x202020;
    for (i = 0; i < MAX_SLOTS; i++) {
        p->slot_height[i] = 0.2;    /* layered mode: one print layer per colour on top of the body */
        p->slot_rgb[i] = 0;
        p->slot_rgb_override[i] = 0;
        p->slot_visible[i] = 1;
        p->slot_merge_into[i] = -1;
    }
    p->chunk_mode = CHUNK_OFF;
    p->chunk_join_pct = 3.0;
    p->chunk_oversize = 1;
    p->chunk_max_w = 240;
    p->chunk_max_d = 240;
    p->chunk_spacing = 8.0;
    p->chunk_view = 0;
    p->plate_padding = 40;
    p->chunk_joints = 1;
    p->joint_clearance = 0.15;
    p->export_color_objects = 0;   /* parts of one object keep their stacking in every slicer */
    p->layered = 1;             /* the main colour forms the body, other colours are layers on top */
    p->layered_flush = 0;
    p->body_slot = -1;          /* auto: the colour with the largest visible area */
    p->body_height = 2.0;
}

int model_body_slot(const model_t *m, const model_params *p)
{
    int b, i;
    if (!p->layered || m->nslots == 0) return -1;
    b = p->body_slot;
    if (b < 0 || b >= m->nslots || m->slots[b].merged_into >= 0) {
        /* auto: the colour covering the largest visible area */
        double best = -1;
        b = -1;
        for (i = 0; i < m->nslots; i++) {
            if (m->slots[i].merged_into >= 0 || !p->slot_visible[i]) continue;
            if (m->slots[i].area > best) { best = m->slots[i].area; b = i; }
        }
        if (b < 0) return -1;
    }
    if (m->slot_region[b].n == 0 || !p->slot_visible[b]) return -1;
    return b;
}

/* Height used for a slot: the body height for the body, the slot height otherwise. */
static double slot_h(const model_t *m, const model_params *p, int slot)
{
    return (model_body_slot(m, p) == slot) ? p->body_height : p->slot_height[slot];
}

void model_slot_zrange(const model_t *m, const model_params *p, int slot, double *zlo, double *zhi)
{
    int body = model_body_slot(m, p);
    double z0 = m->z_logo_bottom;
    if (body < 0 || slot == body) { *zlo = z0; *zhi = z0 + slot_h(m, p, slot); return; }
    if (p->layered_flush) { *zhi = z0 + p->body_height; *zlo = *zhi - p->slot_height[slot]; if (*zlo < z0) *zlo = z0; }
    else { *zlo = z0 + p->body_height; *zhi = *zlo + p->slot_height[slot]; }
}

static void chunk_init(chunk_t *c)
{
    int i;
    memset(c, 0, sizeof(*c));
    region_init(&c->clip);
    for (i = 0; i < MAX_SLOTS; i++) { region_init(&c->slot_region[i]); mesh_init(&c->slot_mesh[i]); }
    region_init(&c->body_region);
    region_init(&c->base_region);
    mesh_init(&c->base_mesh);
}

static void chunk_free(chunk_t *c)
{
    int i;
    region_free(&c->clip);
    for (i = 0; i < MAX_SLOTS; i++) { region_free(&c->slot_region[i]); mesh_free(&c->slot_mesh[i]); }
    region_free(&c->body_region);
    region_free(&c->base_region);
    mesh_free(&c->base_mesh);
}

static void model_free_chunks(model_t *m)
{
    int i;
    for (i = 0; i < m->nchunks; i++) chunk_free(&m->chunks[i]);
    free(m->chunks);
    m->chunks = NULL;
    m->nchunks = 0;
    m->chunks_valid = 0;
}

static void model_free_view(model_t *m)
{
    int i;
    for (i = 0; i < MAX_SLOTS; i++) { region_free(&m->view_slot_region[i]); mesh_free(&m->slot_mesh[i]); }
    region_free(&m->view_base_region);
    mesh_free(&m->base_mesh);
}

void model_init(model_t *m)
{
    int i;
    memset(m, 0, sizeof(*m));
    for (i = 0; i < MAX_SLOTS; i++) {
        region_init(&m->slot_region[i]);
        region_init(&m->view_slot_region[i]);
        mesh_init(&m->slot_mesh[i]);
    }
    region_init(&m->base_region);
    region_init(&m->footprint);
    region_init(&m->view_base_region);
    mesh_init(&m->base_mesh);
}

void model_free(model_t *m)
{
    int i;
    for (i = 0; i < MAX_SLOTS; i++) region_free(&m->slot_region[i]);
    region_free(&m->base_region);
    region_free(&m->footprint);
    model_free_view(m);
    model_free_chunks(m);
    model_init(m);
}

static double slot_h(const model_t *m, const model_params *p, int slot);

unsigned model_slot_rgb(const model_t *m, const model_params *p, int slot)
{
    if (slot < 0 || slot >= m->nslots) return 0x808080;
    return p->slot_rgb_override[slot] ? p->slot_rgb[slot] : m->slots[slot].rgb;
}

unsigned model_base_rgb(const model_t *m, const model_params *p)
{
    if (p->base_color_slot >= 0 && p->base_color_slot < m->nslots) return model_slot_rgb(m, p, p->base_color_slot);
    return p->base_rgb;
}

int model_slot_active(const model_t *m, const model_params *p, int slot)
{
    if (slot < 0 || slot >= m->nslots) return 0;
    if (m->slots[slot].merged_into >= 0) return 0;
    if (!p->slot_visible[slot]) return 0;
    if (slot_h(m, p, slot) <= 0) return 0;
    return m->slot_region[slot].n > 0;
}

int model_material_count(const model_t *m, const model_params *p)
{
    unsigned cols[MAX_SLOTS + 1];
    int n = 0, i, j;
    for (i = 0; i < m->nslots; i++) {
        unsigned c;
        if (!model_slot_active(m, p, i)) continue;
        c = model_slot_rgb(m, p, i);
        for (j = 0; j < n; j++) if (cols[j] == c) break;
        if (j == n) cols[n++] = c;
    }
    if (p->base_enabled && p->base_thickness > 0) {
        unsigned c = model_base_rgb(m, p);
        for (j = 0; j < n; j++) if (cols[j] == c) break;
        if (j == n) cols[n++] = c;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Flattening                                                          */

typedef struct {
    double *pts;
    int n, cap;
} ptbuf;

static void ptbuf_push(ptbuf *b, double x, double y)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 64;
        b->pts = (double *)realloc(b->pts, sizeof(double) * 2 * b->cap);
    }
    b->pts[2 * b->n] = x;
    b->pts[2 * b->n + 1] = y;
    b->n++;
}

static void flatten_cubic(ptbuf *b, double x0, double y0, double x1, double y1, double x2, double y2,
                          double x3, double y3, double tol, int depth)
{
    /* flatness test: distance of the control points from the chord */
    double dx = x3 - x0, dy = y3 - y0;
    double dd = dx * dx + dy * dy;
    int flat;
    if (dd > 1e-30) {
        double d1 = fabs((x1 - x3) * dy - (y1 - y3) * dx);
        double d2 = fabs((x2 - x3) * dy - (y2 - y3) * dx);
        flat = (d1 + d2) * (d1 + d2) < tol * tol * dd;
    } else {
        double e1 = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
        double e2 = (x2 - x0) * (x2 - x0) + (y2 - y0) * (y2 - y0);
        flat = e1 < tol * tol && e2 < tol * tol;
    }
    if (flat || depth >= 16) {
        ptbuf_push(b, x3, y3);
        return;
    }
    {
        double x01 = (x0 + x1) / 2, y01 = (y0 + y1) / 2;
        double x12 = (x1 + x2) / 2, y12 = (y1 + y2) / 2;
        double x23 = (x2 + x3) / 2, y23 = (y2 + y3) / 2;
        double x012 = (x01 + x12) / 2, y012 = (y01 + y12) / 2;
        double x123 = (x12 + x23) / 2, y123 = (y12 + y23) / 2;
        double xm = (x012 + x123) / 2, ym = (y012 + y123) / 2;
        flatten_cubic(b, x0, y0, x01, y01, x012, y012, xm, ym, tol, depth + 1);
        flatten_cubic(b, xm, ym, x123, y123, x23, y23, x3, y3, tol, depth + 1);
    }
}

typedef struct {
    ptbuf pts;
    int closed;
} polyline_t;

typedef struct {
    polyline_t *pl;
    int n, cap;
} polylist;

static polyline_t *polylist_new(polylist *l)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->pl = (polyline_t *)realloc(l->pl, sizeof(polyline_t) * l->cap);
    }
    memset(&l->pl[l->n], 0, sizeof(polyline_t));
    return &l->pl[l->n++];
}

static void polylist_free(polylist *l)
{
    int i;
    for (i = 0; i < l->n; i++) free(l->pl[i].pts.pts);
    free(l->pl);
    memset(l, 0, sizeof(*l));
}

/* Flatten an SVG path into polylines using a coordinate transform
 * (x,y) -> (sx*x + tx, sy*y + ty). */
static void flatten_path(const svg_path *p, double sx, double sy, double tx, double ty, double tol, polylist *out)
{
    polyline_t *cur = NULL;
    double cx = 0, cy = 0, startx = 0, starty = 0;
    int i;
    for (i = 0; i < p->nsegs; i++) {
        const svg_seg *s = &p->segs[i];
        switch (s->type) {
        case SVG_SEG_MOVE:
            cur = polylist_new(out);
            cx = startx = s->x[0] * sx + tx;
            cy = starty = s->y[0] * sy + ty;
            ptbuf_push(&cur->pts, cx, cy);
            break;
        case SVG_SEG_LINE:
            if (!cur) { cur = polylist_new(out); ptbuf_push(&cur->pts, cx, cy); }
            cx = s->x[0] * sx + tx;
            cy = s->y[0] * sy + ty;
            ptbuf_push(&cur->pts, cx, cy);
            break;
        case SVG_SEG_CUBIC: {
            double x1 = s->x[0] * sx + tx, y1 = s->y[0] * sy + ty;
            double x2 = s->x[1] * sx + tx, y2 = s->y[1] * sy + ty;
            double x3 = s->x[2] * sx + tx, y3 = s->y[2] * sy + ty;
            if (!cur) { cur = polylist_new(out); ptbuf_push(&cur->pts, cx, cy); }
            flatten_cubic(&cur->pts, cx, cy, x1, y1, x2, y2, x3, y3, tol, 0);
            cx = x3; cy = y3;
            break;
        }
        case SVG_SEG_CLOSE:
            if (cur) cur->closed = 1;
            cur = NULL;
            cx = startx; cy = starty;
            break;
        }
    }
}

/* Bounding box of a path's control polygon (cheap, conservative). */
static void path_control_bbox(const svg_path *p, double *bb)
{
    int i, k;
    for (i = 0; i < p->nsegs; i++) {
        const svg_seg *s = &p->segs[i];
        int np = s->type == SVG_SEG_CUBIC ? 3 : (s->type == SVG_SEG_CLOSE ? 0 : 1);
        for (k = 0; k < np; k++) {
            if (s->x[k] < bb[0]) bb[0] = s->x[k];
            if (s->y[k] < bb[1]) bb[1] = s->y[k];
            if (s->x[k] > bb[2]) bb[2] = s->x[k];
            if (s->y[k] > bb[3]) bb[3] = s->y[k];
        }
    }
}

/* ------------------------------------------------------------------ */
/* Stroking                                                            */

static void add_piece(region_t *r, double *pts, int n)
{
    contour_t c;
    c.pts = pts;
    c.n = n;
    if (contour_area(&c) < 0) contour_reverse(&c);
    region_add_contour(r, pts, n);
}

static void add_disk(region_t *r, double cx, double cy, double rad, double tol)
{
    int n, i;
    double *pts;
    double a = rad > tol ? acos(1 - tol / rad) : M_PI / 4;
    n = (int)ceil(M_PI / a);
    if (n < 8) n = 8;
    if (n > 128) n = 128;
    pts = (double *)malloc(sizeof(double) * 2 * n);
    for (i = 0; i < n; i++) {
        double t = 2 * M_PI * i / n;
        pts[2 * i] = cx + rad * cos(t);
        pts[2 * i + 1] = cy + rad * sin(t);
    }
    region_add_contour(r, pts, n);
    free(pts);
}

enum { JOIN_EXT, JOIN_WEDGE, JOIN_DISK, JOIN_BEVEL, JOIN_NONE };

/* Stroke a polyline as a union of overlapping pieces.  Every segment becomes
 * a rectangle; at joins with a small turn angle the rectangle is simply
 * extended to the miter point (exact for miter joins up to 90 degrees and
 * within tolerance for round/bevel joins), which avoids needle-thin join
 * polygons.  Larger turns get an explicit join piece. */
static void stroke_polyline(region_t *r, const polyline_t *pl, double hw, svg_linecap cap, svg_linejoin join,
                            double miterlimit, double tol)
{
    int n = pl->pts.n, i, m = 0;
    double *p = (double *)malloc(sizeof(double) * 2 * (n + 1));
    double *ext;
    int *mode;
    int closed = pl->closed;
    int nseg;
    double theta_small;

    /* remove duplicates */
    for (i = 0; i < n; i++) {
        double x = pl->pts.pts[2 * i], y = pl->pts.pts[2 * i + 1];
        if (m && fabs(p[2 * (m - 1)] - x) < 1e-9 && fabs(p[2 * (m - 1) + 1] - y) < 1e-9) continue;
        p[2 * m] = x; p[2 * m + 1] = y; m++;
    }
    if (closed && m > 1 && fabs(p[0] - p[2 * (m - 1)]) < 1e-9 && fabs(p[1] - p[2 * (m - 1) + 1]) < 1e-9) m--;
    if (m == 1) {
        if (cap == SVG_CAP_ROUND) add_disk(r, p[0], p[1], hw, tol);
        else if (cap == SVG_CAP_SQUARE) {
            double q[8] = {p[0] - hw, p[1] - hw, p[0] + hw, p[1] - hw, p[0] + hw, p[1] + hw, p[0] - hw, p[1] + hw};
            add_piece(r, q, 4);
        }
        free(p);
        return;
    }
    if (m < 2) { free(p); return; }
    if (closed && m < 3) closed = 0;
    nseg = closed ? m : m - 1;

    /* turn angle below which a miter extension is within tolerance of any join style */
    theta_small = 2.0 * acos(1.0 / (1.0 + (tol > 1e-6 ? tol : 1e-6) / hw));
    if (theta_small < 10.0 * M_PI / 180.0) theta_small = 10.0 * M_PI / 180.0;
    if (theta_small > 30.0 * M_PI / 180.0) theta_small = 30.0 * M_PI / 180.0;

    ext = (double *)calloc((size_t)m, sizeof(double));
    mode = (int *)malloc(sizeof(int) * (size_t)m);
    for (i = 0; i < m; i++) mode[i] = JOIN_NONE;

    /* classify joins */
    for (i = 0; i < m; i++) {
        int prev = i - 1, next = i + 1;
        double d1x, d1y, d2x, d2y, l1, l2, cross, dot, theta, ratio;
        if (!closed && (i == 0 || i == m - 1)) continue;
        if (prev < 0) prev = m - 1;
        if (next >= m) next = 0;
        d1x = p[2 * i] - p[2 * prev]; d1y = p[2 * i + 1] - p[2 * prev + 1];
        d2x = p[2 * next] - p[2 * i]; d2y = p[2 * next + 1] - p[2 * i + 1];
        l1 = sqrt(d1x * d1x + d1y * d1y); l2 = sqrt(d2x * d2x + d2y * d2y);
        if (l1 < 1e-12 || l2 < 1e-12) continue;
        d1x /= l1; d1y /= l1; d2x /= l2; d2y /= l2;
        cross = d1x * d2y - d1y * d2x;
        dot = d1x * d2x + d1y * d2y;
        theta = atan2(fabs(cross), dot);          /* 0 = straight, pi = reversal */
        if (theta < 1e-6) continue;
        if (theta > M_PI - 1e-3) continue;         /* reversal: rectangles overlap fully */
        ratio = 1.0 / cos(theta / 2);
        if (theta < theta_small) {
            mode[i] = JOIN_EXT;
            ext[i] = hw * tan(theta / 2);
        } else if (join == SVG_JOIN_MITER && ratio <= miterlimit && theta <= M_PI / 2 + 1e-9) {
            mode[i] = JOIN_EXT;
            ext[i] = hw * tan(theta / 2);
        } else if (join == SVG_JOIN_MITER && ratio <= miterlimit) {
            mode[i] = JOIN_WEDGE;
        } else if (join == SVG_JOIN_ROUND) {
            mode[i] = JOIN_DISK;
        } else {
            mode[i] = JOIN_BEVEL;
        }
        /* keep the extension shorter than the adjacent segments */
        if (ext[i] > l1 * 0.5) ext[i] = l1 * 0.5;
        if (ext[i] > l2 * 0.5) ext[i] = l2 * 0.5;
    }

    /* segment rectangles */
    for (i = 0; i < nseg; i++) {
        int j = (i + 1) % m;
        double ax = p[2 * i], ay = p[2 * i + 1], bx = p[2 * j], by = p[2 * j + 1];
        double dx = bx - ax, dy = by - ay, len = sqrt(dx * dx + dy * dy);
        double ea, eb, nx, ny, q[8];
        if (len < 1e-12) continue;
        dx /= len; dy /= len;
        ea = ext[i];
        eb = ext[j];
        if (!closed && i == 0) ea = (cap == SVG_CAP_SQUARE) ? hw : 0;
        if (!closed && i == nseg - 1) eb = (cap == SVG_CAP_SQUARE) ? hw : 0;
        ax -= dx * ea; ay -= dy * ea;
        bx += dx * eb; by += dy * eb;
        nx = -dy * hw; ny = dx * hw;
        q[0] = ax + nx; q[1] = ay + ny;
        q[2] = bx + nx; q[3] = by + ny;
        q[4] = bx - nx; q[5] = by - ny;
        q[6] = ax - nx; q[7] = ay - ny;
        add_piece(r, q, 4);
    }

    /* explicit join pieces */
    for (i = 0; i < m; i++) {
        int prev = i - 1, next = i + 1;
        double d1x, d1y, d2x, d2y, l1, l2, cross, side, o1x, o1y, o2x, o2y, px, py;
        if (mode[i] == JOIN_NONE || mode[i] == JOIN_EXT) continue;
        if (prev < 0) prev = m - 1;
        if (next >= m) next = 0;
        px = p[2 * i]; py = p[2 * i + 1];
        if (mode[i] == JOIN_DISK) { add_disk(r, px, py, hw, tol); continue; }
        d1x = px - p[2 * prev]; d1y = py - p[2 * prev + 1];
        d2x = p[2 * next] - px; d2y = p[2 * next + 1] - py;
        l1 = sqrt(d1x * d1x + d1y * d1y); l2 = sqrt(d2x * d2x + d2y * d2y);
        d1x /= l1; d1y /= l1; d2x /= l2; d2y /= l2;
        cross = d1x * d2y - d1y * d2x;
        side = cross > 0 ? -1.0 : 1.0;  /* outer side of the turn */
        o1x = -d1y * hw * side; o1y = d1x * hw * side;
        o2x = -d2y * hw * side; o2y = d2x * hw * side;
        {
            double ux = o1x / hw, uy = o1y / hw, vx = o2x / hw, vy = o2y / hw;
            double c = ux * vx + uy * vy;
            double bl = sqrt((ux + vx) * (ux + vx) + (uy + vy) * (uy + vy));
            double bx = bl > 1e-12 ? (ux + vx) / bl : 0, by = bl > 1e-12 ? (uy + vy) / bl : 0;
            double q[8];
            int nq = 0;
            /* apex pulled slightly inside the stroke so no edge coincides with a rectangle edge */
            q[nq++] = px - bx * hw * 0.1; q[nq++] = py - by * hw * 0.1;
            q[nq++] = px + o1x; q[nq++] = py + o1y;
            if (mode[i] == JOIN_WEDGE && c > -0.999) {
                double f = hw / (1.0 + c);
                q[nq++] = px + (ux + vx) * f; q[nq++] = py + (uy + vy) * f;
            }
            q[nq++] = px + o2x; q[nq++] = py + o2y;
            add_piece(r, q, nq / 2);
        }
    }
    if (!closed && cap == SVG_CAP_ROUND) {
        add_disk(r, p[0], p[1], hw, tol);
        add_disk(r, p[2 * (m - 1)], p[2 * (m - 1) + 1], hw, tol);
    }
    free(ext);
    free(mode);
    free(p);
}

/* ------------------------------------------------------------------ */
/* Shapes and colour slots                                             */

typedef struct {
    region_t region;    /* normalised, mm */
    unsigned rgb;
    int slot;
    double area;
} shape_t;

typedef struct {
    unsigned rgb;
    double area;
    int alive;
    int target;         /* merged into */
} colour_entry;

static double colour_dist(unsigned a, unsigned b)
{
    double dr = (double)((a >> 16) & 255) - (double)((b >> 16) & 255);
    double dg = (double)((a >> 8) & 255) - (double)((b >> 8) & 255);
    double db = (double)(a & 255) - (double)(b & 255);
    return sqrt(dr * dr + dg * dg + db * db);
}

/* Bounding box of polylines. */
static void polylist_bbox(const polylist *l, double *bb)
{
    int i, j;
    for (i = 0; i < l->n; i++)
        for (j = 0; j < l->pl[i].pts.n; j++) {
            double x = l->pl[i].pts.pts[2 * j], y = l->pl[i].pts.pts[2 * j + 1];
            if (x < bb[0]) bb[0] = x;
            if (y < bb[1]) bb[1] = y;
            if (x > bb[2]) bb[2] = x;
            if (y > bb[3]) bb[3] = y;
        }
}

static void rounded_rect(region_t *r, double x0, double y0, double x1, double y1, double rad, double tol)
{
    ptbuf b;
    int i, n;
    double w = x1 - x0, h = y1 - y0;
    memset(&b, 0, sizeof(b));
    if (rad > w / 2) rad = w / 2;
    if (rad > h / 2) rad = h / 2;
    if (rad <= 1e-9) {
        ptbuf_push(&b, x0, y0); ptbuf_push(&b, x1, y0); ptbuf_push(&b, x1, y1); ptbuf_push(&b, x0, y1);
    } else {
        double a = rad > tol ? acos(1 - tol / rad) : M_PI / 8;
        double cx[4] = {x1 - rad, x0 + rad, x0 + rad, x1 - rad};
        double cy[4] = {y1 - rad, y1 - rad, y0 + rad, y0 + rad};
        int k;
        n = (int)ceil((M_PI / 2) / a);
        if (n < 4) n = 4;
        if (n > 64) n = 64;
        for (k = 0; k < 4; k++) {
            for (i = 0; i <= n; i++) {
                double t = (M_PI / 2) * k + (M_PI / 2) * i / n;
                ptbuf_push(&b, cx[k] + rad * cos(t), cy[k] + rad * sin(t));
            }
        }
    }
    region_add_contour(r, b.pts, b.n);
    free(b.pts);
}

/* Replace *r by its intersection with clip (when given). Returns 0 if empty. */
static int apply_clip(region_t *r, const region_t *clip)
{
    region_t out;
    if (!clip) return 1;
    if (!region_intersect(&out, r, clip)) return 0;
    region_free(r);
    *r = out;
    return r->n > 0;
}

int model_layout(model_t *m, const svg_doc *doc, const model_params *p, char *err, size_t errlen)
{
    double bb[4] = {DBL_MAX, DBL_MAX, -DBL_MAX, -DBL_MAX};
    double scale, sx, sy, tx, ty, tol_svg;
    shape_t *shapes = NULL;
    int nshapes = 0, cshapes = 0;
    int i, j;
    colour_entry *cols = NULL;
    int ncols = 0;
    int limit;
    region_t *clip_regions = NULL;

    if (err && errlen) err[0] = 0;
    /* reset stage A + B state */
    for (i = 0; i < MAX_SLOTS; i++) region_free(&m->slot_region[i]);
    region_free(&m->base_region);
    region_free(&m->footprint);
    model_free_view(m);
    model_free_chunks(m);
    m->valid = 0;
    m->meshes_valid = 0;
    m->nslots = 0;
    memset(m->slots, 0, sizeof(m->slots));

    if (!doc || doc->npaths == 0) {
        if (err && errlen) snprintf(err, errlen, "the SVG contains no fillable shapes");
        return 0;
    }

    /* 1. rough bounding box from control points to derive the scale */
    for (i = 0; i < doc->npaths; i++) path_control_bbox(&doc->paths[i], bb);
    if (bb[2] - bb[0] <= 0 && bb[3] - bb[1] <= 0) {
        if (err && errlen) snprintf(err, errlen, "degenerate geometry");
        return 0;
    }
    {
        /* first pass: flatten everything with a coarse tolerance to get the true bbox */
        double bb2[4] = {DBL_MAX, DBL_MAX, -DBL_MAX, -DBL_MAX};
        double coarse = ((bb[2] - bb[0]) + (bb[3] - bb[1])) * 1e-4;
        if (coarse <= 0) coarse = 1e-3;
        /* bounding boxes of clip paths (SVG units), for clipped shapes */
        double *clipbb = NULL;
        if (doc->nclips) {
            clipbb = (double *)malloc(sizeof(double) * 4 * (size_t)doc->nclips);
            for (i = 0; i < doc->nclips; i++) {
                double *cb = &clipbb[4 * i];
                int k;
                cb[0] = cb[1] = DBL_MAX; cb[2] = cb[3] = -DBL_MAX;
                for (k = 0; k < doc->clips[i].npaths; k++) {
                    polylist pl;
                    memset(&pl, 0, sizeof(pl));
                    flatten_path(&doc->clip_paths[doc->clips[i].first_path + k], 1, 1, 0, 0, coarse, &pl);
                    polylist_bbox(&pl, cb);
                    polylist_free(&pl);
                }
            }
            /* intersect with parents (clips are created after their parents) */
            for (i = 0; i < doc->nclips; i++) {
                int par = doc->clips[i].parent;
                if (par >= 0 && par < i) {
                    double *cb = &clipbb[4 * i], *pb = &clipbb[4 * par];
                    if (pb[0] > cb[0]) cb[0] = pb[0];
                    if (pb[1] > cb[1]) cb[1] = pb[1];
                    if (pb[2] < cb[2]) cb[2] = pb[2];
                    if (pb[3] < cb[3]) cb[3] = pb[3];
                }
            }
        }
        for (i = 0; i < doc->npaths; i++) {
            polylist pl;
            double b3[4] = {DBL_MAX, DBL_MAX, -DBL_MAX, -DBL_MAX};
            memset(&pl, 0, sizeof(pl));
            flatten_path(&doc->paths[i], 1, 1, 0, 0, coarse, &pl);
            polylist_bbox(&pl, b3);
            if (doc->paths[i].has_stroke) {
                double hw = doc->paths[i].stroke_width / 2;
                b3[0] -= hw; b3[1] -= hw; b3[2] += hw; b3[3] += hw;
            }
            if (doc->paths[i].clip >= 0 && doc->paths[i].clip < doc->nclips) {
                double *cb = &clipbb[4 * doc->paths[i].clip];
                if (cb[0] > b3[0]) b3[0] = cb[0];
                if (cb[1] > b3[1]) b3[1] = cb[1];
                if (cb[2] < b3[2]) b3[2] = cb[2];
                if (cb[3] < b3[3]) b3[3] = cb[3];
            }
            if (b3[0] <= b3[2] && b3[1] <= b3[3]) {
                if (b3[0] < bb2[0]) bb2[0] = b3[0];
                if (b3[1] < bb2[1]) bb2[1] = b3[1];
                if (b3[2] > bb2[2]) bb2[2] = b3[2];
                if (b3[3] > bb2[3]) bb2[3] = b3[3];
            }
            polylist_free(&pl);
        }
        free(clipbb);
        if (bb2[0] < bb2[2] && bb2[1] < bb2[3]) memcpy(bb, bb2, sizeof(bb));
    }
    {
        double w = bb[2] - bb[0], h = bb[3] - bb[1];
        if (w <= 0) w = 1e-9;
        if (h <= 0) h = 1e-9;
        /* the target size is the whole model; subtract the base plate margin */
        double mg = (p->base_enabled && p->base_thickness > 0 && p->base_margin > 0) ? 2.0 * p->base_margin : 0.0;
        double tw = p->width_mm - mg, th = p->height_mm - mg;
        if (tw < 1) tw = 1;
        if (th < 1) th = 1;
        if (p->fit_by_height && p->height_mm > 0) scale = th / h;
        else if (p->width_mm > 0) scale = tw / w;
        else scale = 1.0;
        m->scale = scale;
        /* mm coordinates: centred, y flipped (SVG y goes down) */
        sx = scale * (p->mirror_x ? -1.0 : 1.0);
        sy = -scale;
        tx = -(bb[0] + bb[2]) / 2 * sx;
        ty = -(bb[1] + bb[3]) / 2 * sy;
        m->logo_w = w * scale;
        m->logo_h = h * scale;
        m->logo_min[0] = -m->logo_w / 2; m->logo_max[0] = m->logo_w / 2;
        m->logo_min[1] = -m->logo_h / 2; m->logo_max[1] = m->logo_h / 2;
    }
    tol_svg = (p->curve_tol_mm > 1e-5 ? p->curve_tol_mm : 1e-5) / scale;

    /* 2a. clip regions in mm (own geometry intersected with the parent clip) */
    {
        int k;
        clip_regions = (region_t *)malloc(sizeof(region_t) * (size_t)(doc->nclips > 0 ? doc->nclips : 1));
        for (k = 0; k < doc->nclips; k++) {
            const region_t *parts[64];
            region_t *owned[64];
            int np = 0, q;
            region_t own;
            region_init(&clip_regions[k]);
            for (q = 0; q < doc->clips[k].npaths && np < 64; q++) {
                const svg_path *cp = &doc->clip_paths[doc->clips[k].first_path + q];
                polylist pl;
                region_t raw, norm;
                memset(&pl, 0, sizeof(pl));
                region_init(&raw);
                region_init(&norm);
                flatten_path(cp, sx, sy, tx, ty, tol_svg * scale, &pl);
                for (j = 0; j < pl.n; j++) region_add_contour(&raw, pl.pl[j].pts.pts, pl.pl[j].pts.n);
                if (raw.n && region_normalize(&norm, &raw, cp->fill_evenodd) && norm.n) {
                    owned[np] = (region_t *)malloc(sizeof(region_t));
                    *owned[np] = norm;
                    parts[np] = owned[np];
                    np++;
                } else region_free(&norm);
                region_free(&raw);
                polylist_free(&pl);
            }
            region_init(&own);
            if (np == 1) region_copy(&own, parts[0]);
            else if (np > 1) region_union(&own, parts, np);
            for (q = 0; q < np; q++) { region_free(owned[q]); free(owned[q]); }
            if (doc->clips[k].parent >= 0 && doc->clips[k].parent < k) {
                region_intersect(&clip_regions[k], &own, &clip_regions[doc->clips[k].parent]);
                region_free(&own);
            } else {
                clip_regions[k] = own;
            }
        }
    }

    /* 2. build normalised shape regions in paint order */
    for (i = 0; i < doc->npaths; i++) {
        const svg_path *sp = &doc->paths[i];
        polylist pl;
        const region_t *clip = (sp->clip >= 0 && sp->clip < doc->nclips) ? &clip_regions[sp->clip] : NULL;
        memset(&pl, 0, sizeof(pl));
        flatten_path(sp, sx, sy, tx, ty, tol_svg * scale, &pl);
        if (sp->has_fill) {
            region_t raw, norm;
            region_init(&raw);
            region_init(&norm);
            for (j = 0; j < pl.n; j++) region_add_contour(&raw, pl.pl[j].pts.pts, pl.pl[j].pts.n);
            if (raw.n && region_normalize(&norm, &raw, sp->fill_evenodd) && norm.n && apply_clip(&norm, clip)) {
                if (nshapes == cshapes) { cshapes = cshapes ? cshapes * 2 : 64; shapes = (shape_t *)realloc(shapes, sizeof(shape_t) * cshapes); }
                shapes[nshapes].region = norm;
                shapes[nshapes].rgb = sp->fill_rgb;
                shapes[nshapes].slot = -1;
                shapes[nshapes].area = region_area(&norm);
                nshapes++;
            } else region_free(&norm);
            region_free(&raw);
        }
        if (sp->has_stroke && sp->stroke_width * scale > 1e-4) {
            region_t raw, norm;
            double hw = sp->stroke_width * scale / 2;
            region_init(&raw);
            region_init(&norm);
            for (j = 0; j < pl.n; j++)
                stroke_polyline(&raw, &pl.pl[j], hw, sp->linecap, sp->linejoin, sp->miter_limit, p->curve_tol_mm);
            if (raw.n && region_normalize(&norm, &raw, 0) && norm.n && apply_clip(&norm, clip)) {
                if (nshapes == cshapes) { cshapes = cshapes ? cshapes * 2 : 64; shapes = (shape_t *)realloc(shapes, sizeof(shape_t) * cshapes); }
                shapes[nshapes].region = norm;
                shapes[nshapes].rgb = sp->stroke_rgb;
                shapes[nshapes].slot = -1;
                shapes[nshapes].area = region_area(&norm);
                nshapes++;
            } else region_free(&norm);
            region_free(&raw);
        }
        polylist_free(&pl);
    }
    for (i = 0; i < doc->nclips; i++) region_free(&clip_regions[i]);
    free(clip_regions);
    m->nshapes = nshapes;
    if (nshapes == 0) {
        free(shapes);
        if (err && errlen) snprintf(err, errlen, "no visible geometry after processing");
        return 0;
    }

    /* 3. colour quantisation */
    for (i = 0; i < nshapes; i++) {
        for (j = 0; j < ncols; j++) if (cols[j].rgb == shapes[i].rgb) break;
        if (j == ncols) {
            cols = (colour_entry *)realloc(cols, sizeof(colour_entry) * (ncols + 1));
            cols[ncols].rgb = shapes[i].rgb;
            cols[ncols].area = 0;
            cols[ncols].alive = 1;
            cols[ncols].target = -1;
            ncols++;
        }
        cols[j].area += shapes[i].area;
    }
    m->colors_before_merge = ncols;
    limit = p->max_colors;
    if (limit > MAX_SLOTS) limit = MAX_SLOTS;
    if (limit < 1) limit = 1;
    if (p->base_enabled && p->base_color_slot < 0 && limit > 1) limit--;
    for (;;) {
        int alive = 0, a = -1, b = -1;
        double best = DBL_MAX;
        int k, l;
        for (k = 0; k < ncols; k++) if (cols[k].alive) alive++;
        for (k = 0; k < ncols; k++) {
            if (!cols[k].alive) continue;
            for (l = k + 1; l < ncols; l++) {
                double d;
                if (!cols[l].alive) continue;
                d = colour_dist(cols[k].rgb, cols[l].rgb);
                if (d < best) { best = d; a = k; b = l; }
            }
        }
        if (a < 0) break;
        if (alive <= limit && best > p->merge_threshold) break;
        /* merge smaller into larger */
        if (cols[a].area < cols[b].area) { int t = a; a = b; b = t; }
        cols[b].alive = 0;
        cols[b].target = a;
        cols[a].area += cols[b].area;
    }
    /* order surviving colours by area (descending) into slots */
    {
        int order[MAX_SLOTS * 4];
        int n = 0, k;
        int *slot_of = (int *)malloc(sizeof(int) * (size_t)(ncols > 0 ? ncols : 1));
        for (k = 0; k < ncols; k++) if (cols[k].alive && n < (int)(sizeof(order) / sizeof(order[0]))) order[n++] = k;
        for (k = 1; k < n; k++) {
            int v = order[k], q = k - 1;
            while (q >= 0 && (cols[order[q]].area < cols[v].area ||
                              (cols[order[q]].area == cols[v].area && cols[order[q]].rgb > cols[v].rgb))) { order[q + 1] = order[q]; q--; }
            order[q + 1] = v;
        }
        if (n > MAX_SLOTS) n = MAX_SLOTS;
        for (k = 0; k < ncols; k++) slot_of[k] = -1;
        for (k = 0; k < n; k++) {
            slot_of[order[k]] = k;
            m->slots[k].rgb = cols[order[k]].rgb;
            m->slots[k].merged_into = -1;
        }
        m->nslots = n;
        for (i = 0; i < nshapes; i++) {
            int c;
            for (c = 0; c < ncols; c++) if (cols[c].rgb == shapes[i].rgb) break;
            while (c >= 0 && !cols[c].alive) c = cols[c].target;
            shapes[i].slot = c >= 0 ? slot_of[c] : -1;
            if (shapes[i].slot < 0) shapes[i].slot = 0;
        }
        free(slot_of);
    }
    free(cols);
    /* user merges */
    for (i = 0; i < m->nslots; i++) {
        int t = p->slot_merge_into[i];
        if (t >= 0 && t < m->nslots && t != i) m->slots[i].merged_into = t;
    }
    for (i = 0; i < nshapes; i++) {
        int s = shapes[i].slot, guard = 0;
        while (s >= 0 && m->slots[s].merged_into >= 0 && guard++ < MAX_SLOTS) s = m->slots[s].merged_into;
        shapes[i].slot = s;
    }
    for (i = 0; i < nshapes; i++) m->slots[shapes[i].slot].nshapes++;

    /* footprint of the whole logo: union of the uncut shapes (body of the layered mode) */
    {
        const region_t **all = (const region_t **)malloc(sizeof(region_t *) * (size_t)(nshapes > 0 ? nshapes : 1));
        int na = 0;
        for (i = 0; i < nshapes; i++) if (shapes[i].region.n) all[na++] = &shapes[i].region;
        region_free(&m->footprint);
        if (na == 1) region_copy(&m->footprint, all[0]);
        else if (na > 1) region_union(&m->footprint, all, na);
        region_clean(&m->footprint, 1e-5);
        free(all);
    }

    /* 4. painter's order: each shape minus the shapes above it (different slot only) */
    {
        const region_t **subs = (const region_t **)malloc(sizeof(region_t *) * (size_t)(nshapes > 0 ? nshapes : 1));
        region_t *visible = (region_t *)malloc(sizeof(region_t) * (size_t)(nshapes > 0 ? nshapes : 1));
        for (i = 0; i < nshapes; i++) {
            int ns = 0;
            for (j = i + 1; j < nshapes; j++) {
                if (shapes[j].slot == shapes[i].slot) continue;
                if (!region_bbox_overlap(&shapes[i].region, &shapes[j].region)) continue;
                subs[ns++] = &shapes[j].region;
            }
            if (ns == 0) region_copy(&visible[i], &shapes[i].region);
            else if (!region_subtract(&visible[i], &shapes[i].region, subs, ns)) region_init(&visible[i]);
        }
        /* 5. per-slot union */
        for (i = 0; i < m->nslots; i++) {
            int ns = 0;
            for (j = 0; j < nshapes; j++) if (shapes[j].slot == i && visible[j].n) subs[ns++] = &visible[j];
            if (ns == 1) region_copy(&m->slot_region[i], subs[0]);
            else if (ns > 1) region_union(&m->slot_region[i], subs, ns);
            else region_init(&m->slot_region[i]);
            /* drop numerically empty contours */
            {
                int k, w = 0;
                for (k = 0; k < m->slot_region[i].n; k++) {
                    if (fabs(contour_area(&m->slot_region[i].c[k])) < 1e-5) { free(m->slot_region[i].c[k].pts); continue; }
                    m->slot_region[i].c[w++] = m->slot_region[i].c[k];
                }
                m->slot_region[i].n = w;
                region_update_bbox(&m->slot_region[i]);
            }
            m->slots[i].area = region_area(&m->slot_region[i]);
        }
        for (i = 0; i < nshapes; i++) region_free(&visible[i]);
        free(visible);
        free(subs);
    }
    for (i = 0; i < nshapes; i++) region_free(&shapes[i].region);
    free(shapes);

    region_init(&m->base_region);
    m->valid = 1;
    return 1;
}

/* Rectangle with individually rounded corners (radii: bottom-left, bottom-right, top-right, top-left). */
static void rounded_rect4(region_t *r, double x0, double y0, double x1, double y1, const double *rad, double tol)
{
    ptbuf b;
    int k, i;
    double w = x1 - x0, h = y1 - y0;
    double cx[4] = {x1, x1, x0, x0}, cy[4] = {y0, y1, y1, y0};      /* corner order: BR, TR, TL, BL (CCW) */
    const double *use[4] = {rad + 1, rad + 2, rad + 3, rad + 0};
    memset(&b, 0, sizeof(b));
    for (k = 0; k < 4; k++) {
        double rr = *use[k];
        double a0 = -M_PI / 2 + k * M_PI / 2;
        if (rr > w / 2) rr = w / 2;
        if (rr > h / 2) rr = h / 2;
        if (rr <= 1e-9) { ptbuf_push(&b, cx[k], cy[k]); continue; }
        {
            double ccx = cx[k] + ((k == 2 || k == 3) ? rr : -rr), ccy = cy[k] + ((k == 0 || k == 3) ? rr : -rr);
            double a = rr > tol ? acos(1 - tol / rr) : M_PI / 8;
            int n = (int)ceil((M_PI / 2) / a);
            if (n < 4) n = 4;
            if (n > 64) n = 64;
            for (i = 0; i <= n; i++) {
                double t = a0 + (M_PI / 2) * i / n;
                ptbuf_push(&b, ccx + rr * cos(t), ccy + rr * sin(t));
            }
        }
    }
    region_add_contour(r, b.pts, b.n);
    free(b.pts);
}

typedef struct {
    int type;           /* 0 none, 1 tab (male), 2 socket (female) */
    double s0, s1;      /* range along the edge shared with the neighbour (local coords) */
} joint_spec;

/* Dovetail on one side of a plate.  side: 0 left (x=x0), 1 right (x=x1), 2 bottom (y=y0), 3 top (y=y1).
 * Tabs protrude outward from left/bottom edges; sockets are cut inward from right/top edges. */
static void dovetail_side(region_t *r, int side, double edge, double cc, double neck, double head, double len, double grow)
{
    double q[8];
    double n2 = neck / 2 + grow, h2 = head / 2 + grow, L = len + grow, e = 0.1;
    if (side == 0 || side == 1) {
        q[0] = edge + e; q[1] = cc - n2;
        q[2] = edge - L; q[3] = cc - h2;
        q[4] = edge - L; q[5] = cc + h2;
        q[6] = edge + e; q[7] = cc + n2;
    } else {
        q[0] = cc - n2; q[1] = edge + e;
        q[2] = cc - h2; q[3] = edge - L;
        q[4] = cc + h2; q[5] = edge - L;
        q[6] = cc + n2; q[7] = edge + e;
    }
    add_piece(r, q, 4);
}

/* Plate rectangle (local coords) with per-corner radii and dovetail joints. */
static void plate_build(region_t *out, const double *rect, const double *rad, const joint_spec *joints, double clearance, double tol)
{
    region_t base, norm, tabs, socks, tmp;
    int side, j, have_tabs = 0, have_socks = 0;
    region_init(&base);
    rounded_rect4(&base, rect[0], rect[1], rect[2], rect[3], rad, tol);
    region_normalize(&norm, &base, 0);
    region_free(&base);
    region_init(&tabs);
    region_init(&socks);
    for (side = 0; side < 4; side++) {
        const joint_spec *js = &joints[side];
        double shared, len, neck, head, edge;
        int n;
        if (js->type == 0) continue;
        shared = js->s1 - js->s0;
        if (shared < 6) continue;
        len = shared * 0.25;
        if (len < 3) len = 3;
        if (len > 12) len = 12;
        if (side < 2 && len > (rect[2] - rect[0]) * 0.4) len = (rect[2] - rect[0]) * 0.4;
        if (side >= 2 && len > (rect[3] - rect[1]) * 0.4) len = (rect[3] - rect[1]) * 0.4;
        neck = len;
        head = len * 1.7;
        n = (int)floor(shared / 60.0 + 0.5);
        if (n < 1) n = 1;
        if (n > 6) n = 6;
        if (head * n * 1.6 > shared) n = 1;
        if (head * 1.2 > shared) continue;
        edge = (side == 0) ? rect[0] : (side == 1) ? rect[2] : (side == 2) ? rect[1] : rect[3];
        for (j = 0; j < n; j++) {
            double cc = js->s0 + shared * (j + 0.5) / n;
            if (js->type == 1) { dovetail_side(&tabs, side, edge, cc, neck, head, len, 0); have_tabs = 1; }
            else { dovetail_side(&socks, side, edge, cc, neck, head, len, clearance); have_socks = 1; }
        }
    }
    if (have_tabs) {
        region_t ntabs;
        const region_t *rs[2];
        region_normalize(&ntabs, &tabs, 0);
        rs[0] = &norm; rs[1] = &ntabs;
        region_union(&tmp, rs, 2);
        region_free(&norm);
        region_free(&ntabs);
        norm = tmp;
    }
    if (have_socks) {
        region_t nsocks;
        const region_t *rs[1];
        region_normalize(&nsocks, &socks, 0);
        rs[0] = &nsocks;
        region_subtract(&tmp, &norm, rs, 1);
        region_free(&norm);
        region_free(&nsocks);
        norm = tmp;
    }
    region_free(&tabs);
    region_free(&socks);
    region_clean(&norm, 1e-4);
    *out = norm;
}

/* Neighbour of chunk i in the same tile group: side 0 left, 1 right, 2 bottom, 3 top; -1 if none. */
static int tile_neighbour(const model_t *m, int i, int side)
{
    const chunk_t *c = &m->chunks[i];
    int dx = side == 0 ? -1 : side == 1 ? 1 : 0, dy = side == 2 ? -1 : side == 3 ? 1 : 0, k;
    for (k = 0; k < m->nchunks; k++) {
        const chunk_t *o = &m->chunks[k];
        if (k == i || o->group != c->group) continue;
        if (o->ix == c->ix + dx && o->iy == c->iy + dy) return k;
    }
    return -1;
}

/* Nearest piece of another group in the same row to the left (side 0) or right (side 1); -1 if none. */
static int strip_neighbour(const model_t *m, int i, int side, double gx0, double gx1)
{
    const chunk_t *c = &m->chunks[i];
    int k, best = -1;
    double bestd = DBL_MAX;
    for (k = 0; k < m->nchunks; k++) {
        const chunk_t *o = &m->chunks[k];
        double d;
        if (o->group == c->group || o->row != c->row) continue;
        if (side == 0) { if (o->tile[2] > gx0 + 1e-9) continue; d = gx0 - o->tile[2]; }
        else { if (o->tile[0] < gx1 - 1e-9) continue; d = o->tile[0] - gx1; }
        if (d < bestd) { bestd = d; best = k; }
    }
    return best;
}

/* Plate rectangles of every chunk (model coords), from tile cut lines, row extents and margins. */
static void compute_plates(model_t *m, const model_params *p, double mg)
{
    int i, k;
    for (i = 0; i < m->nchunks; i++) {
        chunk_t *c = &m->chunks[i];
        double gx0 = DBL_MAX, gx1 = -DBL_MAX, ry0 = DBL_MAX, ry1 = -DBL_MAX;
        int nbL, nbR, nbB, nbT, sL, sR;
        for (k = 0; k < m->nchunks; k++) {
            const chunk_t *o = &m->chunks[k];
            if (o->group == c->group) {
                if (o->tile[0] < gx0) gx0 = o->tile[0];
                if (o->tile[2] > gx1) gx1 = o->tile[2];
            }
            if (o->row == c->row) {
                if (o->gmin[1] < ry0) ry0 = o->gmin[1];
                if (o->gmax[1] > ry1) ry1 = o->gmax[1];
            }
        }
        nbL = tile_neighbour(m, i, 0); nbR = tile_neighbour(m, i, 1);
        nbB = tile_neighbour(m, i, 2); nbT = tile_neighbour(m, i, 3);
        sL = p->chunk_mode == CHUNK_TILES ? -1 : strip_neighbour(m, i, 0, gx0, gx1);
        sR = p->chunk_mode == CHUNK_TILES ? -1 : strip_neighbour(m, i, 1, gx0, gx1);
        c->plate[0] = nbL >= 0 ? c->tile[0] : (sL >= 0 ? (m->chunks[sL].tile[2] + gx0) / 2 : c->tile[0] - mg);
        c->plate[2] = nbR >= 0 ? c->tile[2] : (sR >= 0 ? (gx1 + m->chunks[sR].tile[0]) / 2 : c->tile[2] + mg);
        if (p->chunk_mode == CHUNK_TILES) {
            c->plate[1] = nbB >= 0 ? c->tile[1] : c->tile[1] - mg;
            c->plate[3] = nbT >= 0 ? c->tile[3] : c->tile[3] + mg;
        } else {
            c->plate[1] = nbB >= 0 ? c->tile[1] : ry0 - mg;
            c->plate[3] = nbT >= 0 ? c->tile[3] : ry1 + mg;
        }
    }
}

/* Append src to dst without welding: every shell keeps its own vertices. */
static void mesh_append_raw(mesh_t *dst, const mesh_t *src)
{
    unsigned base = (unsigned)dst->nv;
    int i;
    if (dst->nv + src->nv > dst->cv) {
        dst->cv = dst->nv + src->nv + 1024;
        dst->v = (double *)realloc(dst->v, sizeof(double) * 3 * (size_t)dst->cv);
    }
    memcpy(dst->v + 3 * dst->nv, src->v, sizeof(double) * 3 * (size_t)src->nv);
    dst->nv += src->nv;
    free(dst->hash);
    dst->hash = NULL;
    dst->hcap = 0;
    for (i = 0; i < src->nt; i++) mesh_add_tri(dst, src->t[3 * i] + base, src->t[3 * i + 1] + base, src->t[3 * i + 2] + base);
}

/* ------------------------------------------------------------------ */
/* Extrusion                                                           */

static int extrude_region(mesh_t *mesh, const region_t *r, double z0, double z1)
{
    double *v; int nv; int *t; int nt, i, j;
    if (!region_triangulate(r, &v, &nv, &t, &nt)) return 0;
    for (i = 0; i < nt; i++) {
        unsigned a = mesh_add_vertex(mesh, v[2 * t[3 * i]], v[2 * t[3 * i] + 1], z1);
        unsigned b = mesh_add_vertex(mesh, v[2 * t[3 * i + 1]], v[2 * t[3 * i + 1] + 1], z1);
        unsigned c = mesh_add_vertex(mesh, v[2 * t[3 * i + 2]], v[2 * t[3 * i + 2] + 1], z1);
        mesh_add_tri(mesh, a, b, c);
        a = mesh_add_vertex(mesh, v[2 * t[3 * i]], v[2 * t[3 * i] + 1], z0);
        b = mesh_add_vertex(mesh, v[2 * t[3 * i + 1]], v[2 * t[3 * i + 1] + 1], z0);
        c = mesh_add_vertex(mesh, v[2 * t[3 * i + 2]], v[2 * t[3 * i + 2] + 1], z0);
        mesh_add_tri(mesh, a, c, b);
    }
    free(v);
    free(t);
    for (i = 0; i < r->n; i++) {
        const contour_t *c = &r->c[i];
        for (j = 0; j < c->n; j++) {
            int k = (j + 1) % c->n;
            double px = c->pts[2 * j], py = c->pts[2 * j + 1], qx = c->pts[2 * k], qy = c->pts[2 * k + 1];
            unsigned p0 = mesh_add_vertex(mesh, px, py, z0);
            unsigned q0 = mesh_add_vertex(mesh, qx, qy, z0);
            unsigned q1 = mesh_add_vertex(mesh, qx, qy, z1);
            unsigned p1 = mesh_add_vertex(mesh, px, py, z1);
            mesh_add_tri(mesh, p0, q0, q1);
            mesh_add_tri(mesh, p0, q1, p1);
        }
    }
    return 1;
}

static void mesh_bbox(const mesh_t *m, double *mn, double *mx)
{
    int i;
    for (i = 0; i < m->nv; i++) {
        int k;
        for (k = 0; k < 3; k++) {
            if (m->v[3 * i + k] < mn[k]) mn[k] = m->v[3 * i + k];
            if (m->v[3 * i + k] > mx[k]) mx[k] = m->v[3 * i + k];
        }
    }
}

/* ------------------------------------------------------------------ */
/* Chunking                                                            */

static int point_in_contour(const contour_t *c, double x, double y)
{
    int i, j, inside = 0;
    for (i = 0, j = c->n - 1; i < c->n; j = i++) {
        double xi = c->pts[2 * i], yi = c->pts[2 * i + 1], xj = c->pts[2 * j], yj = c->pts[2 * j + 1];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) inside = !inside;
    }
    return inside;
}

/* Even-odd test over all contours of a normalised region. */
static int point_in_region(const region_t *r, double x, double y)
{
    int i, inside = 0;
    if (x < r->minx || x > r->maxx || y < r->miny || y > r->maxy) return 0;
    for (i = 0; i < r->n; i++) if (point_in_contour(&r->c[i], x, y)) inside = !inside;
    return inside;
}

/* A point strictly inside a contour whose interior lies on its left. */
static void contour_rep_point(const contour_t *c, double *x, double *y)
{
    int i, best = 0;
    double bl = -1, eps;
    for (i = 0; i < c->n; i++) {
        int j = (i + 1) % c->n;
        double dx = c->pts[2 * j] - c->pts[2 * i], dy = c->pts[2 * j + 1] - c->pts[2 * i + 1];
        double l = dx * dx + dy * dy;
        if (l > bl) { bl = l; best = i; }
    }
    {
        int j = (best + 1) % c->n;
        double dx = c->pts[2 * j] - c->pts[2 * best], dy = c->pts[2 * j + 1] - c->pts[2 * best + 1];
        double l = sqrt(dx * dx + dy * dy);
        eps = 1e-3 * sqrt(fabs(contour_area(c)));
        if (eps > 0.01) eps = 0.01;
        if (eps > l * 0.1) eps = l * 0.1;
        if (l < 1e-12) { *x = c->pts[0]; *y = c->pts[1]; return; }
        *x = (c->pts[2 * best] + c->pts[2 * j]) / 2 - dy / l * eps;
        *y = (c->pts[2 * best + 1] + c->pts[2 * j + 1]) / 2 + dx / l * eps;
    }
}

static void contour_bbox(const contour_t *c, double *bb)
{
    int i;
    bb[0] = bb[1] = DBL_MAX; bb[2] = bb[3] = -DBL_MAX;
    for (i = 0; i < c->n; i++) {
        double x = c->pts[2 * i], y = c->pts[2 * i + 1];
        if (x < bb[0]) bb[0] = x;
        if (y < bb[1]) bb[1] = y;
        if (x > bb[2]) bb[2] = x;
        if (y > bb[3]) bb[3] = y;
    }
}

static void region_add_rect(region_t *r, double x0, double y0, double x1, double y1)
{
    double q[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
    region_add_contour(r, q, 4);
}

static void region_translate(region_t *r, double dx, double dy)
{
    int i, j;
    for (i = 0; i < r->n; i++)
        for (j = 0; j < r->c[i].n; j++) { r->c[i].pts[2 * j] += dx; r->c[i].pts[2 * j + 1] += dy; }
    region_update_bbox(r);
}

static void region_append(region_t *dst, const region_t *src, double dx, double dy)
{
    int i, j;
    for (i = 0; i < src->n; i++) {
        const contour_t *c = &src->c[i];
        double *tmp = (double *)malloc(sizeof(double) * 2 * (size_t)c->n);
        for (j = 0; j < c->n; j++) { tmp[2 * j] = c->pts[2 * j] + dx; tmp[2 * j + 1] = c->pts[2 * j + 1] + dy; }
        region_add_contour(dst, tmp, c->n);
        free(tmp);
    }
}

static int find_root(int *parent, int i)
{
    while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; }
    return i;
}

typedef struct {
    chunk_t *c;
    int n, cap;
} chunklist;

static chunk_t *chunklist_add(chunklist *l)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->c = (chunk_t *)realloc(l->c, sizeof(chunk_t) * (size_t)l->cap);
    }
    chunk_init(&l->c[l->n]);
    return &l->c[l->n++];
}

/* Geometry bbox of a chunk from its slot regions (model coords). */
static int chunk_geometry_bbox(chunk_t *c)
{
    int i, any = 0;
    c->gmin[0] = c->gmin[1] = DBL_MAX;
    c->gmax[0] = c->gmax[1] = -DBL_MAX;
    for (i = 0; i < MAX_SLOTS; i++) {
        const region_t *r = &c->slot_region[i];
        if (r->n == 0) continue;
        any = 1;
        if (r->minx < c->gmin[0]) c->gmin[0] = r->minx;
        if (r->miny < c->gmin[1]) c->gmin[1] = r->miny;
        if (r->maxx > c->gmax[0]) c->gmax[0] = r->maxx;
        if (r->maxy > c->gmax[1]) c->gmax[1] = r->maxy;
    }
    return any;
}

/* Cut a chunk (model coords) along explicit boundaries: xb has nx+1 values, yb has ny+1. */
static void cut_chunk_grid(chunklist *out, const chunk_t *src, int nslots, const double *xb, int nx, const double *yb, int ny, int group)
{
    int ix, iy, s;
    for (iy = 0; iy < ny; iy++) {
        for (ix = 0; ix < nx; ix++) {
            double x0 = xb[ix], x1 = xb[ix + 1];
            double y0 = yb[iy], y1 = yb[iy + 1];
            region_t rect;
            chunk_t tmp;
            int any = 0;
            chunk_init(&tmp);
            tmp.group = group;
            tmp.ix = ix;
            tmp.iy = iy;
            tmp.row = src->row;
            tmp.tile[0] = x0; tmp.tile[1] = y0; tmp.tile[2] = x1; tmp.tile[3] = y1;
            region_init(&rect);
            region_add_rect(&rect, x0 - 1e-6, y0 - 1e-6, x1 + 1e-6, y1 + 1e-6);
            for (s = 0; s < nslots; s++) {
                if (src->slot_region[s].n == 0) continue;
                if (!region_intersect(&tmp.slot_region[s], &src->slot_region[s], &rect)) region_init(&tmp.slot_region[s]);
                if (tmp.slot_region[s].n) any = 1;
            }
            if (src->body_region.n && !region_intersect(&tmp.body_region, &src->body_region, &rect)) region_init(&tmp.body_region);
            if (any && chunk_geometry_bbox(&tmp)) {
                chunk_t *c = chunklist_add(out);
                region_free(&c->clip);
                *c = tmp;
                region_copy(&c->clip, &rect);
            } else chunk_free(&tmp);
            region_free(&rect);
        }
    }
}

/* Cut a chunk (model coords) into a grid of equal tiles no larger than tw x td. */
static void cut_chunk_into_tiles(chunklist *out, const chunk_t *src, int nslots, double tw, double td, int group)
{
    double w = src->gmax[0] - src->gmin[0], d = src->gmax[1] - src->gmin[1];
    int nx = (int)ceil(w / tw - 1e-9), ny = (int)ceil(d / td - 1e-9), i;
    double *xb, *yb;
    if (nx < 1) nx = 1;
    if (ny < 1) ny = 1;
    xb = (double *)malloc(sizeof(double) * (size_t)(nx + 1));
    yb = (double *)malloc(sizeof(double) * (size_t)(ny + 1));
    for (i = 0; i <= nx; i++) xb[i] = src->gmin[0] + w * i / nx;
    for (i = 0; i <= ny; i++) yb[i] = src->gmin[1] + d * i / ny;
    cut_chunk_grid(out, src, nslots, xb, nx, yb, ny, group);
    free(xb);
    free(yb);
}

/* Length of geometry (normalised region, even-odd) crossed by the line x = pos (axis 0) or y = pos (axis 1). */
static double cut_crossing(const region_t *r, int axis, double pos)
{
    double *xs = NULL;
    int n = 0, cap = 0, i, j;
    double total = 0;
    for (i = 0; i < r->n; i++) {
        const contour_t *c = &r->c[i];
        for (j = 0; j < c->n; j++) {
            int k = (j + 1) % c->n;
            double a0 = c->pts[2 * j + axis], a1 = c->pts[2 * k + axis];
            double b0 = c->pts[2 * j + 1 - axis], b1 = c->pts[2 * k + 1 - axis];
            if ((a0 <= pos) == (a1 <= pos)) continue;
            if (n == cap) { cap = cap ? cap * 2 : 64; xs = (double *)realloc(xs, sizeof(double) * (size_t)cap); }
            xs[n++] = b0 + (b1 - b0) * (pos - a0) / (a1 - a0);
        }
    }
    for (i = 1; i < n; i++) {
        double v = xs[i];
        j = i - 1;
        while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; j--; }
        xs[j + 1] = v;
    }
    for (i = 0; i + 1 < n; i += 2) total += xs[i + 1] - xs[i];
    free(xs);
    return total;
}

static int fits_upright(double w, double d, double W, double D)
{
    return (w <= W + 1e-6 && d <= D + 1e-6) || (d <= W + 1e-6 && w <= D + 1e-6);
}

/* Split a piece made of several objects between the objects so that every part fits the
 * plate upright.  Tries a partition along x, then along y.  Returns the number of parts
 * (>= 2) with the cut positions in `bounds` (npart+1 values), or 0. */
static int natural_split(const chunk_t *c, double W, double D, int *axis, double *bounds, int maxparts)
{
    int ncomp = 0, i, k, a;
    double *bb;
    int *order;
    for (i = 0; i < c->clip.n; i++) if (contour_area(&c->clip.c[i]) > 0) ncomp++;
    if (ncomp < 2) return 0;
    bb = (double *)malloc(sizeof(double) * 4 * (size_t)ncomp);
    order = (int *)malloc(sizeof(int) * (size_t)ncomp);
    for (i = 0, k = 0; i < c->clip.n; i++)
        if (contour_area(&c->clip.c[i]) > 0) contour_bbox(&c->clip.c[i], &bb[4 * k++]);
    for (a = 0; a < 2; a++) {
        /* components sorted along the axis */
        double cur[4];
        int nparts = 0, ok = 1, j;
        for (i = 0; i < ncomp; i++) order[i] = i;
        for (i = 1; i < ncomp; i++) {
            int v = order[i];
            j = i - 1;
            while (j >= 0 && bb[4 * order[j] + a] > bb[4 * v + a]) { order[j + 1] = order[j]; j--; }
            order[j + 1] = v;
        }
        memcpy(cur, &bb[4 * order[0]], sizeof(cur));
        if (!fits_upright(cur[2] - cur[0], cur[3] - cur[1], W, D)) ok = 0;
        bounds[0] = a == 0 ? c->gmin[0] : c->gmin[1];
        for (i = 1; i < ncomp && ok; i++) {
            const double *nb = &bb[4 * order[i]];
            double un[4];
            un[0] = cur[0] < nb[0] ? cur[0] : nb[0];
            un[1] = cur[1] < nb[1] ? cur[1] : nb[1];
            un[2] = cur[2] > nb[2] ? cur[2] : nb[2];
            un[3] = cur[3] > nb[3] ? cur[3] : nb[3];
            if (fits_upright(un[2] - un[0], un[3] - un[1], W, D)) {
                memcpy(cur, un, sizeof(cur));
            } else {
                if (!fits_upright(nb[2] - nb[0], nb[3] - nb[1], W, D)) { ok = 0; break; }
                if (nparts + 2 >= maxparts) { ok = 0; break; }
                {
                    /* the parts must really be side by side along the axis, and neither may be a
                     * small fragment (a dot or a cap is kept with its letter) */
                    double ecur = cur[2 + a] - cur[a], enb = nb[2 + a] - nb[a];
                    double emin = ecur < enb ? ecur : enb, overlap = cur[2 + a] - nb[a];
                    double plate = a == 0 ? W : D;
                    if (overlap > 0.3 * emin) { ok = 0; break; }
                    if (ecur < 0.2 * plate || enb < 0.2 * plate) { ok = 0; break; }
                }
                if (cur[2 + a] < nb[a]) {
                    /* a real gap between the parts: cut in the middle of it */
                    bounds[nparts + 1] = (cur[2 + a] + nb[a]) / 2;
                } else {
                    /* the parts overlap along the axis: find the position crossing the least
                     * geometry, and refuse the split when it would cut through a shape */
                    double lo = nb[a], hi = cur[2 + a], best = DBL_MAX, bestpos = (lo + hi) / 2;
                    double perp = (cur[3 - a] > nb[3 - a] ? cur[3 - a] : nb[3 - a]) - (cur[1 - a] < nb[1 - a] ? cur[1 - a] : nb[1 - a]);
                    int q;
                    for (q = 0; q <= 24; q++) {
                        double pos = lo + (hi - lo) * q / 24.0, cost = cut_crossing(&c->clip, a, pos);
                        if (cost < best) { best = cost; bestpos = pos; }
                    }
                    if (best > 0.7 * perp) { ok = 0; break; }   /* only refuse cuts through solid material */
                    bounds[nparts + 1] = bestpos;
                }
                nparts++;
                memcpy(cur, nb, sizeof(cur));
            }
        }
        if (ok && nparts >= 1) {
            /* the greedy pass gives the smallest part count; now balance the parts */
            int k = nparts + 1, ncut = k - 1, gaps = ncomp - 1;
            int *sel = (int *)malloc(sizeof(int) * (size_t)(ncut > 0 ? ncut : 1));
            int *bestsel = (int *)malloc(sizeof(int) * (size_t)(ncut > 0 ? ncut : 1));
            double bestmax = DBL_MAX;
            long tries = 0;
            int q, done = 0, found = 0;
            double plate = a == 0 ? W : D;
            for (q = 0; q < ncut; q++) sel[q] = q;      /* split after component order[sel[q]] */
            while (!done && tries < 20000) {
                /* evaluate this selection */
                double maxext = 0, prev_end = -DBL_MAX;
                int start = 0, valid = 1, part;
                tries++;
                for (part = 0; part <= ncut && valid; part++) {
                    int end = part < ncut ? sel[part] : ncomp - 1;   /* inclusive */
                    double u[4];
                    int t;
                    memcpy(u, &bb[4 * order[start]], sizeof(u));
                    for (t = start + 1; t <= end; t++) {
                        const double *b2 = &bb[4 * order[t]];
                        if (b2[0] < u[0]) u[0] = b2[0];
                        if (b2[1] < u[1]) u[1] = b2[1];
                        if (b2[2] > u[2]) u[2] = b2[2];
                        if (b2[3] > u[3]) u[3] = b2[3];
                    }
                    if (!fits_upright(u[2] - u[0], u[3] - u[1], W, D)) valid = 0;
                    if (u[2 + a] - u[a] < 0.2 * plate) valid = 0;
                    if (part > 0) {
                        double next_start = bb[4 * order[start] + a];
                        double overlap = prev_end - next_start;
                        if (overlap > 0.3 * (u[2 + a] - u[a])) valid = 0;
                    }
                    if (u[2 + a] - u[a] > maxext) maxext = u[2 + a] - u[a];
                    prev_end = u[2 + a];
                    start = end + 1;
                }
                if (valid && maxext < bestmax) { bestmax = maxext; memcpy(bestsel, sel, sizeof(int) * (size_t)ncut); found = 1; }
                /* next combination of ncut split indices out of `gaps` */
                q = ncut - 1;
                while (q >= 0 && sel[q] == gaps - ncut + q) q--;
                if (q < 0) done = 1;
                else { int t; sel[q]++; for (t = q + 1; t < ncut; t++) sel[t] = sel[t - 1] + 1; }
            }
            if (found) {
                double prev_end;
                int start = 0;
                for (q = 0; q < ncut; q++) {
                    int end = bestsel[q], t;
                    double hi = -DBL_MAX, lo;
                    for (t = start; t <= end; t++) if (bb[4 * order[t] + 2 + a] > hi) hi = bb[4 * order[t] + 2 + a];
                    lo = bb[4 * order[end + 1] + a];
                    prev_end = hi;
                    if (prev_end < lo) bounds[q + 1] = (prev_end + lo) / 2;
                    else {
                        double best = DBL_MAX, bestpos = (lo + prev_end) / 2;
                        int r;
                        for (r = 0; r <= 24; r++) {
                            double pos = lo + (prev_end - lo) * r / 24.0, cost = cut_crossing(&c->clip, a, pos);
                            if (cost < best) { best = cost; bestpos = pos; }
                        }
                        bounds[q + 1] = bestpos;
                    }
                    start = end + 1;
                }
            }
            free(sel);
            free(bestsel);
            nparts++;
            bounds[nparts] = a == 0 ? c->gmax[0] : c->gmax[1];
            *axis = a;
            free(bb);
            free(order);
            return nparts;
        }
    }
    free(bb);
    free(order);
    return 0;
}

/* Sort key: row (top to bottom) then x; rows are assigned beforehand so the
 * comparison is a strict ordering. */
typedef struct { int row; double x; int idx; } chunk_key;

static int chunk_key_cmp(const void *a, const void *b)
{
    const chunk_key *ka = (const chunk_key *)a, *kb = (const chunk_key *)b;
    if (ka->row != kb->row) return ka->row < kb->row ? -1 : 1;
    if (ka->x < kb->x) return -1;
    if (ka->x > kb->x) return 1;
    return ka->idx < kb->idx ? -1 : (ka->idx > kb->idx ? 1 : 0);
}

/* Order chunks in reading order: rows from top to bottom, left to right.
 * Chunks whose y-ranges overlap clearly belong to the same row. */
static void sort_chunks(chunklist *list, int keep_rows)
{
    chunk_key *keys;
    chunk_t *sorted;
    int *parent, *row_of;
    double *row_y;
    int i, k, nrows = 0;
    if (list->n < 2) { if (list->n == 1 && !keep_rows) list->c[0].row = 0; return; }
    if (keep_rows) {
        keys = (chunk_key *)malloc(sizeof(chunk_key) * (size_t)list->n);
        for (i = 0; i < list->n; i++) { keys[i].row = list->c[i].row; keys[i].x = list->c[i].gmin[0]; keys[i].idx = i; }
        qsort(keys, (size_t)list->n, sizeof(chunk_key), chunk_key_cmp);
        sorted = (chunk_t *)malloc(sizeof(chunk_t) * (size_t)list->n);
        for (i = 0; i < list->n; i++) sorted[i] = list->c[keys[i].idx];
        free(list->c);
        list->c = sorted;
        free(keys);
        return;
    }
    parent = (int *)malloc(sizeof(int) * (size_t)list->n);
    row_of = (int *)malloc(sizeof(int) * (size_t)list->n);
    row_y = (double *)malloc(sizeof(double) * (size_t)list->n);
    keys = (chunk_key *)malloc(sizeof(chunk_key) * (size_t)list->n);
    for (i = 0; i < list->n; i++) parent[i] = i;
    for (i = 0; i < list->n; i++) {
        for (k = i + 1; k < list->n; k++) {
            const chunk_t *a = &list->c[i], *b = &list->c[k];
            double ov = (a->gmax[1] < b->gmax[1] ? a->gmax[1] : b->gmax[1]) - (a->gmin[1] > b->gmin[1] ? a->gmin[1] : b->gmin[1]);
            double ha = a->gmax[1] - a->gmin[1], hb = b->gmax[1] - b->gmin[1];
            double hmin = ha < hb ? ha : hb;
            if (ov > 0.5 * hmin) parent[find_root(parent, i)] = find_root(parent, k);
        }
    }
    for (i = 0; i < list->n; i++) row_of[i] = -1;
    for (i = 0; i < list->n; i++) {
        int r = find_root(parent, i);
        if (row_of[r] < 0) { row_of[r] = nrows; row_y[nrows] = 0; nrows++; }
        row_of[i] = row_of[r];
    }
    /* row order: by the highest y centre in the row (top first) */
    for (i = 0; i < nrows; i++) row_y[i] = -DBL_MAX;
    for (i = 0; i < list->n; i++) {
        double yc = (list->c[i].gmin[1] + list->c[i].gmax[1]) / 2;
        if (yc > row_y[row_of[i]]) row_y[row_of[i]] = yc;
    }
    for (i = 0; i < list->n; i++) {
        int rank = 0;
        for (k = 0; k < nrows; k++) if (row_y[k] > row_y[row_of[i]] || (row_y[k] == row_y[row_of[i]] && k < row_of[i])) rank++;
        keys[i].row = rank;
        keys[i].x = list->c[i].gmin[0];
        keys[i].idx = i;
    }
    qsort(keys, (size_t)list->n, sizeof(chunk_key), chunk_key_cmp);
    sorted = (chunk_t *)malloc(sizeof(chunk_t) * (size_t)list->n);
    for (i = 0; i < list->n; i++) { sorted[i] = list->c[keys[i].idx]; sorted[i].row = keys[i].row; }
    free(list->c);
    list->c = sorted;
    free(keys);
    free(parent);
    free(row_of);
    free(row_y);
}

double model_fit_angle(double w, double d, double W, double D)
{
    /* upright first, then every 5 degrees (45 preferred among the diagonals) */
    static const double angles[] = {0, 90, 45, 30, 60, 15, 75, 10, 20, 40, 50, 70, 80, 5, 25, 35, 55, 65, 85};
    int i;
    for (i = 0; i < (int)(sizeof(angles) / sizeof(angles[0])); i++) {
        double a = angles[i] * M_PI / 180.0, c = fabs(cos(a)), s = fabs(sin(a));
        if (w * c + d * s <= W + 1e-6 && w * s + d * c <= D + 1e-6) return angles[i];
    }
    /* fine search for the borderline cases */
    for (i = 1; i < 90; i++) {
        double a = i * M_PI / 180.0, c = fabs(cos(a)), s = fabs(sin(a));
        if (w * c + d * s <= W + 1e-6 && w * s + d * c <= D + 1e-6) return i;
    }
    return -1;
}

void mesh_xform_copy(mesh_t *dst, const mesh_t *src, double deg, double sxy)
{
    double a = deg * M_PI / 180.0, c = cos(a) * sxy, s = sin(a) * sxy;
    int i;
    mesh_init(dst);
    for (i = 0; i < src->nv; i++) {
        double x = src->v[3 * i], y = src->v[3 * i + 1];
        mesh_add_vertex(dst, c * x - s * y, s * x + c * y, src->v[3 * i + 2]);
    }
    for (i = 0; i < src->nt; i++) mesh_add_tri(dst, src->t[3 * i], src->t[3 * i + 1], src->t[3 * i + 2]);
}

void region_xform_copy(region_t *dst, const region_t *src, double deg, double sxy)
{
    double a = deg * M_PI / 180.0, c = cos(a) * sxy, s = sin(a) * sxy;
    int i, j;
    region_init(dst);
    for (i = 0; i < src->n; i++) {
        const contour_t *ct = &src->c[i];
        double *tmp = (double *)malloc(sizeof(double) * 2 * (size_t)ct->n);
        for (j = 0; j < ct->n; j++) {
            double x = ct->pts[2 * j], y = ct->pts[2 * j + 1];
            tmp[2 * j] = c * x - s * y;
            tmp[2 * j + 1] = s * x + c * y;
        }
        region_add_contour(dst, tmp, ct->n);
        free(tmp);
    }
}

double model_max_fit_scale(double w, double d, double margin, double W, double D)
{
    double lo = 0, hi = 16, mid;
    int it;
    if (model_fit_angle(2 * margin, 2 * margin, W, D) < 0) return 0;
    if (model_fit_angle(w * hi + 2 * margin, d * hi + 2 * margin, W, D) >= 0) return hi;
    for (it = 0; it < 48; it++) {
        mid = (lo + hi) / 2;
        if (model_fit_angle(w * mid + 2 * margin, d * mid + 2 * margin, W, D) >= 0) lo = mid; else hi = mid;
    }
    return lo;
}

static int z_has_base(const model_params *p)
{
    return p->base_enabled && p->base_thickness > 0;
}

static void compute_chunks(model_t *m, const model_params *p)
{
    chunklist list;
    int i, s;
    double mg = (p->base_enabled && p->base_thickness > 0 && p->base_margin > 0) ? p->base_margin : 0.0;
    double tw = p->chunk_max_w - 2 * mg, td = p->chunk_max_d - 2 * mg;
    if (p->chunk_joints && z_has_base(p)) { tw -= 12; td -= 12; }   /* room for the dovetail tabs */
    if (tw < 5) tw = 5;
    if (td < 5) td = 5;
    memset(&list, 0, sizeof(list));
    model_free_chunks(m);

    if (p->chunk_mode == CHUNK_TILES) {
        chunk_t whole;
        chunk_init(&whole);
        for (s = 0; s < m->nslots; s++) region_copy(&whole.slot_region[s], &m->slot_region[s]);
        region_copy(&whole.body_region, &m->footprint);
        if (chunk_geometry_bbox(&whole)) cut_chunk_into_tiles(&list, &whole, m->nslots, tw, td, 0);
        chunk_free(&whole);
        /* reading order: tile rows from the top */
        {
            int maxiy = 0;
            for (i = 0; i < list.n; i++) if (list.c[i].iy > maxiy) maxiy = list.c[i].iy;
            for (i = 0; i < list.n; i++) list.c[i].row = maxiy - list.c[i].iy;
        }
    } else if (p->chunk_mode == CHUNK_OBJECTS && m->nslots > 0) {
        /* connected pieces of the union of every colour */
        region_t all;
        const region_t *rs[MAX_SLOTS];
        int ncomp = 0, *parent, *group_of, ngroups = 0;
        double *cbb;
        double gap = p->chunk_join_pct / 100.0 * m->logo_h;
        for (s = 0; s < m->nslots; s++) rs[s] = &m->slot_region[s];
        if (!region_union(&all, rs, m->nslots)) region_init(&all);
        cbb = (double *)malloc(sizeof(double) * 4 * (size_t)(all.n > 0 ? all.n : 1));
        parent = (int *)malloc(sizeof(int) * (size_t)(all.n > 0 ? all.n : 1));
        group_of = (int *)malloc(sizeof(int) * (size_t)(all.n > 0 ? all.n : 1));
        /* outer contours are components; holes are attached to their outer */
        for (i = 0; i < all.n; i++) {
            contour_bbox(&all.c[i], &cbb[4 * i]);
            parent[i] = i;
            if (contour_area(&all.c[i]) > 0) ncomp++;
        }
        for (i = 0; i < all.n; i++) {
            if (contour_area(&all.c[i]) > 0) continue;
            {
                int best = -1, k;
                double best_area = DBL_MAX;
                for (k = 0; k < all.n; k++) {
                    double a = contour_area(&all.c[k]);
                    if (a <= 0) continue;
                    if (cbb[4 * k] > cbb[4 * i] || cbb[4 * k + 1] > cbb[4 * i + 1] || cbb[4 * k + 2] < cbb[4 * i + 2] || cbb[4 * k + 3] < cbb[4 * i + 3]) continue;
                    if (a < best_area && point_in_contour(&all.c[k], all.c[i].pts[0], all.c[i].pts[1])) { best_area = a; best = k; }
                }
                if (best >= 0) parent[find_root(parent, i)] = find_root(parent, best);
            }
        }
        /* join components that belong together:
         *  - stacked pieces whose x-ranges overlap clearly (dot of an i, accents,
         *    nested chevrons), up to half the logo height apart,
         *  - side-by-side pieces closer than the join gap,
         *  - overlapping bounding boxes (a dot inside an o). */
        for (i = 0; i < all.n; i++) {
            int k;
            if (contour_area(&all.c[i]) <= 0) continue;
            for (k = i + 1; k < all.n; k++) {
                const double *a = &cbb[4 * i], *b = &cbb[4 * k];
                double xgap, ygap, xov, wmin, join = 0;
                if (contour_area(&all.c[k]) <= 0) continue;
                xgap = (a[0] > b[0] ? a[0] : b[0]) - (a[2] < b[2] ? a[2] : b[2]);   /* negative = overlap */
                ygap = (a[1] > b[1] ? a[1] : b[1]) - (a[3] < b[3] ? a[3] : b[3]);
                xov = -xgap;
                wmin = (a[2] - a[0] < b[2] - b[0]) ? a[2] - a[0] : b[2] - b[0];
                {
                    double ha = a[3] - a[1], hb = b[3] - b[1];
                    double hmax = ha > hb ? ha : hb, hmin = ha < hb ? ha : hb;
                    if (xgap <= 0 && ygap <= 0) join = 1;                                   /* overlapping boxes */
                    else if (xov >= 0.3 * wmin && ygap <= 0.5 * hmax && hmin <= 0.4 * hmax) join = 1; /* dot / accent above or below */
                    else if (ygap <= 0 && xgap <= gap) join = 1;                            /* side by side, very close */
                }
                if (join) parent[find_root(parent, i)] = find_root(parent, k);
            }
        }
        /* numerical slivers (tiny outer contours) join the nearest real component */
        for (i = 0; i < all.n; i++) {
            double area = contour_area(&all.c[i]);
            int k, best = -1;
            double bestd = DBL_MAX;
            if (area <= 0 || area > 0.25) continue;
            for (k = 0; k < all.n; k++) {
                double dx, dy, d;
                if (k == i || contour_area(&all.c[k]) <= 0.25) continue;
                dx = (cbb[4 * k] > cbb[4 * i + 2]) ? cbb[4 * k] - cbb[4 * i + 2] : (cbb[4 * i] > cbb[4 * k + 2] ? cbb[4 * i] - cbb[4 * k + 2] : 0);
                dy = (cbb[4 * k + 1] > cbb[4 * i + 3]) ? cbb[4 * k + 1] - cbb[4 * i + 3] : (cbb[4 * i + 1] > cbb[4 * k + 3] ? cbb[4 * i + 1] - cbb[4 * k + 3] : 0);
                d = dx * dx + dy * dy;
                if (d < bestd) { bestd = d; best = k; }
            }
            if (best >= 0) parent[find_root(parent, i)] = find_root(parent, best);
        }
        /* one chunk per group, clip = the group's contours */
        for (i = 0; i < all.n; i++) group_of[i] = -1;
        for (i = 0; i < all.n; i++) {
            int r = find_root(parent, i);
            if (group_of[r] < 0) { group_of[r] = ngroups++; chunklist_add(&list); }
            group_of[i] = group_of[r];
            region_add_contour(&list.c[group_of[i]].clip, all.c[i].pts, all.c[i].n);
        }
        /* distribute the colour contours (outer + its holes) to the chunks; s == nslots is the footprint */
        for (s = 0; s <= m->nslots; s++) {
            const region_t *r = s < m->nslots ? &m->slot_region[s] : &m->footprint;
            int *owner = (int *)malloc(sizeof(int) * (size_t)(r->n > 0 ? r->n : 1));
            int k;
            for (k = 0; k < r->n; k++) owner[k] = -1;
            for (k = 0; k < r->n; k++) {
                double px, py;
                int g;
                if (contour_area(&r->c[k]) <= 0) continue;
                contour_rep_point(&r->c[k], &px, &py);
                for (g = 0; g < list.n; g++) if (point_in_region(&list.c[g].clip, px, py)) { owner[k] = g; break; }
                if (owner[k] < 0) {
                    /* numerical miss: nearest chunk by bounding box centre */
                    double bb[4], best = DBL_MAX;
                    contour_bbox(&r->c[k], bb);
                    for (g = 0; g < list.n; g++) {
                        const region_t *cl = &list.c[g].clip;
                        double dx = (cl->minx + cl->maxx) / 2 - (bb[0] + bb[2]) / 2, dy = (cl->miny + cl->maxy) / 2 - (bb[1] + bb[3]) / 2;
                        if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; owner[k] = g; }
                    }
                }
            }
            /* holes follow the smallest outer contour containing them */
            for (k = 0; k < r->n; k++) {
                int q, best = -1;
                double best_area = DBL_MAX;
                if (contour_area(&r->c[k]) > 0) continue;
                for (q = 0; q < r->n; q++) {
                    double a = contour_area(&r->c[q]);
                    if (a <= 0) continue;
                    if (a < best_area && point_in_contour(&r->c[q], r->c[k].pts[0], r->c[k].pts[1])) { best_area = a; best = q; }
                }
                owner[k] = best >= 0 ? owner[best] : -1;
            }
            for (k = 0; k < r->n; k++)
                if (owner[k] >= 0) region_add_contour(s < m->nslots ? &list.c[owner[k]].slot_region[s] : &list.c[owner[k]].body_region, r->c[k].pts, r->c[k].n);
            free(owner);
        }
        for (i = 0; i < list.n; i++) chunk_geometry_bbox(&list.c[i]);
        free(cbb);
        free(parent);
        free(group_of);
        region_free(&all);
        /* rows are decided on the uncut pieces; every piece is its own tile group */
        sort_chunks(&list, 0);
        for (i = 0; i < list.n; i++) {
            chunk_t *c = &list.c[i];
            c->group = i + 1;
            c->ix = c->iy = 0;
            c->tile[0] = c->gmin[0]; c->tile[1] = c->gmin[1]; c->tile[2] = c->gmax[0]; c->tile[3] = c->gmax[1];
        }
        /* pieces too large for the plate that consist of several objects are split between them */
        {
            chunklist split;
            double Wp = p->chunk_max_w - 2 * mg, Dp = p->chunk_max_d - 2 * mg;
            if (p->chunk_joints && z_has_base(p)) { Wp -= 12; Dp -= 12; }
            memset(&split, 0, sizeof(split));
            for (i = 0; i < list.n; i++) {
                chunk_t *c = &list.c[i];
                double bounds[34];
                int axis = 0, nparts = 0;
                if (!fits_upright(c->gmax[0] - c->gmin[0], c->gmax[1] - c->gmin[1], Wp, Dp))
                    nparts = natural_split(c, Wp, Dp, &axis, bounds, 32);
                if (nparts >= 2) {
                    double other[2];
                    if (axis == 0) { other[0] = c->gmin[1]; other[1] = c->gmax[1]; cut_chunk_grid(&split, c, m->nslots, bounds, nparts, other, 1, c->group); }
                    else { other[0] = c->gmin[0]; other[1] = c->gmax[0]; cut_chunk_grid(&split, c, m->nslots, other, 1, bounds, nparts, c->group); }
                    chunk_free(c);
                } else {
                    *chunklist_add(&split) = *c;
                }
            }
            free(list.c);
            list = split;
        }
        /* how far the whole logo could grow (or must shrink) so that every piece fits uncut */
        m->chunk_fit_scale = 16;
        for (i = 0; i < list.n; i++) {
            chunk_t *c = &list.c[i];
            double s = model_max_fit_scale(c->gmax[0] - c->gmin[0], c->gmax[1] - c->gmin[1], mg, p->chunk_max_w, p->chunk_max_d);
            if (s < m->chunk_fit_scale) m->chunk_fit_scale = s;
        }
        if (fabs(m->chunk_fit_scale - 1.0) < 1e-4) m->chunk_fit_scale = 1;
        /* cut pieces that do not fit the plate at any rotation */
        if (p->chunk_oversize == 0) {
            chunklist cut;
            memset(&cut, 0, sizeof(cut));
            for (i = 0; i < list.n; i++) {
                chunk_t *c = &list.c[i];
                if (model_fit_angle(c->gmax[0] - c->gmin[0] + 2 * mg, c->gmax[1] - c->gmin[1] + 2 * mg, p->chunk_max_w, p->chunk_max_d) < 0) {
                    cut_chunk_into_tiles(&cut, c, m->nslots, tw, td, c->group);
                    chunk_free(c);
                } else {
                    *chunklist_add(&cut) = *c;
                }
            }
            free(list.c);
            list = cut;
        }
    }
    if (list.n == 0) {
        /* no chunking (or nothing found): the whole model is one chunk */
        chunk_t *c = chunklist_add(&list);
        for (s = 0; s < m->nslots; s++) region_copy(&c->slot_region[s], &m->slot_region[s]);
        region_copy(&c->body_region, &m->footprint);
        if (!chunk_geometry_bbox(c)) {
            c->gmin[0] = m->logo_min[0]; c->gmin[1] = m->logo_min[1];
            c->gmax[0] = m->logo_max[0]; c->gmax[1] = m->logo_max[1];
        }
        c->group = 1;
        c->tile[0] = c->gmin[0]; c->tile[1] = c->gmin[1]; c->tile[2] = c->gmax[0]; c->tile[3] = c->gmax[1];
    }
    /* drop empty chunks, order them, centre them */
    {
        int w = 0;
        for (i = 0; i < list.n; i++) {
            if (chunk_geometry_bbox(&list.c[i]) || list.n == 1) list.c[w++] = list.c[i];
            else chunk_free(&list.c[i]);
        }
        list.n = w;
    }
    sort_chunks(&list, p->chunk_mode != CHUNK_OFF);
    for (i = 0; i < list.n; i++) {
        chunk_t *c = &list.c[i];
        snprintf(c->name, sizeof(c->name), "chunk%02d", i + 1);
        if (p->chunk_mode == CHUNK_OFF) {
            c->center[0] = c->center[1] = 0;
        } else {
            c->center[0] = (c->gmin[0] + c->gmax[0]) / 2;
            c->center[1] = (c->gmin[1] + c->gmax[1]) / 2;
        }
        for (s = 0; s < MAX_SLOTS; s++) region_translate(&c->slot_region[s], -c->center[0], -c->center[1]);
        region_translate(&c->body_region, -c->center[0], -c->center[1]);
    }
    m->chunks = list.c;
    m->nchunks = list.n;
    m->chunks_valid = 1;
    m->chunk_mode_used = p->chunk_mode;
    m->chunk_join_used = p->chunk_join_pct;
    m->chunk_max_w_used = p->chunk_max_w;
    m->chunk_max_d_used = p->chunk_max_d;
    m->chunk_oversize_used = p->chunk_oversize;
    if (p->chunk_mode != CHUNK_OBJECTS) m->chunk_fit_scale = 1;
}

static int chunk_params_changed(const model_t *m, const model_params *p)
{
    if (!m->chunks_valid) return 1;
    if (m->chunk_mode_used != p->chunk_mode) return 1;
    if (p->chunk_mode == CHUNK_OFF) return 0;
    if (m->chunk_join_used != p->chunk_join_pct || m->chunk_oversize_used != p->chunk_oversize) return 1;
    if (m->chunk_max_w_used != p->chunk_max_w || m->chunk_max_d_used != p->chunk_max_d) return 1;
    return 0;
}

void model_chunk_size(const model_t *m, int chunk, double *w, double *d)
{
    const chunk_t *c;
    if (chunk < 0 || chunk >= m->nchunks) { *w = *d = 0; return; }
    c = &m->chunks[chunk];
    *w = (c->bbox_max[0] - c->bbox_min[0]) * c->scale;
    *d = (c->bbox_max[1] - c->bbox_min[1]) * c->scale;
}

int model_chunk_at(const model_t *m, const model_params *p, double x, double y)
{
    int i;
    if (!m->meshes_valid) return -1;
    if (p->chunk_view > 0 && p->chunk_view <= m->nchunks) return p->chunk_view - 1;
    for (i = 0; i < m->nchunks; i++) {
        const chunk_t *c = &m->chunks[i];
        if (x >= c->bbox_min[0] + c->place[0] && x <= c->bbox_max[0] + c->place[0] &&
            y >= c->bbox_min[1] + c->place[1] && y <= c->bbox_max[1] + c->place[1]) return i;
    }
    return -1;
}

static void mesh_translate(mesh_t *m, double dx, double dy)
{
    int i;
    for (i = 0; i < m->nv; i++) { m->v[3 * i] += dx; m->v[3 * i + 1] += dy; }
    free(m->hash);          /* welding table no longer matches the coordinates */
    m->hash = NULL;
    m->hcap = 0;
}

static void mesh_append(mesh_t *dst, const mesh_t *src, double dx, double dy)
{
    int i;
    unsigned *map = (unsigned *)malloc(sizeof(unsigned) * (size_t)(src->nv > 0 ? src->nv : 1));
    for (i = 0; i < src->nv; i++) map[i] = mesh_add_vertex(dst, src->v[3 * i] + dx, src->v[3 * i + 1] + dy, src->v[3 * i + 2]);
    for (i = 0; i < src->nt; i++) mesh_add_tri(dst, map[src->t[3 * i]], map[src->t[3 * i + 1]], map[src->t[3 * i + 2]]);
    free(map);
}

int model_build_meshes(model_t *m, const model_params *p)
{
    int i, s, k;
    double z0;
    double mg;
    if (!m->valid) return 0;
    z0 = (p->base_enabled && p->base_thickness > 0) ? p->base_thickness : 0;
    mg = (z0 > 0 && p->base_margin > 0) ? p->base_margin : 0;
    m->z_logo_bottom = z0;
    if (chunk_params_changed(m, p)) compute_chunks(m, p);

    /* per-chunk base plate and meshes in local coordinates */
    for (i = 0; i < m->nchunks; i++) {
        chunk_t *c = &m->chunks[i];
        double lminx = c->gmin[0] - c->center[0], lminy = c->gmin[1] - c->center[1];
        double lmaxx = c->gmax[0] - c->center[0], lmaxy = c->gmax[1] - c->center[1];
        region_free(&c->base_region);
        mesh_free(&c->base_mesh);
        for (s = 0; s < MAX_SLOTS; s++) mesh_free(&c->slot_mesh[s]);
        for (k = 0; k < 3; k++) { c->bbox_min[k] = DBL_MAX; c->bbox_max[k] = -DBL_MAX; }
        c->ntris = 0;
        c->plate_first = c->plate_last = 1;
        if (z0 > 0) {
            if (p->chunk_mode != CHUNK_OFF && p->chunk_joints && m->nchunks > 1) {
                /* connected plates: cut lines, row extents and dovetails towards every neighbour */
                double rect[4], rad[4];
                joint_spec js[4];
                int nb[4], sL, sR, side;
                double gx0 = DBL_MAX, gx1 = -DBL_MAX;
                int q;
                if (i == 0) compute_plates(m, p, mg);
                for (q = 0; q < m->nchunks; q++) {
                    if (m->chunks[q].group != c->group) continue;
                    if (m->chunks[q].tile[0] < gx0) gx0 = m->chunks[q].tile[0];
                    if (m->chunks[q].tile[2] > gx1) gx1 = m->chunks[q].tile[2];
                }
                for (side = 0; side < 4; side++) nb[side] = tile_neighbour(m, i, side);
                sL = p->chunk_mode == CHUNK_TILES ? -1 : strip_neighbour(m, i, 0, gx0, gx1);
                sR = p->chunk_mode == CHUNK_TILES ? -1 : strip_neighbour(m, i, 1, gx0, gx1);
                if (nb[0] < 0 && sL >= 0) nb[0] = sL;
                if (nb[1] < 0 && sR >= 0) nb[1] = sR;
                rect[0] = c->plate[0] - c->center[0]; rect[1] = c->plate[1] - c->center[1];
                rect[2] = c->plate[2] - c->center[0]; rect[3] = c->plate[3] - c->center[1];
                /* corners are rounded only where both adjacent sides are free */
                rad[0] = (nb[0] < 0 && nb[2] < 0) ? p->base_radius : 0;   /* BL */
                rad[1] = (nb[1] < 0 && nb[2] < 0) ? p->base_radius : 0;   /* BR */
                rad[2] = (nb[1] < 0 && nb[3] < 0) ? p->base_radius : 0;   /* TR */
                rad[3] = (nb[0] < 0 && nb[3] < 0) ? p->base_radius : 0;   /* TL */
                for (side = 0; side < 4; side++) {
                    const chunk_t *o;
                    js[side].type = 0;
                    js[side].s0 = js[side].s1 = 0;
                    if (nb[side] < 0) continue;
                    o = &m->chunks[nb[side]];
                    js[side].type = (side == 0 || side == 2) ? 1 : 2;
                    if (side < 2) {
                        js[side].s0 = (c->plate[1] > o->plate[1] ? c->plate[1] : o->plate[1]) - c->center[1];
                        js[side].s1 = (c->plate[3] < o->plate[3] ? c->plate[3] : o->plate[3]) - c->center[1];
                    } else {
                        js[side].s0 = (c->plate[0] > o->plate[0] ? c->plate[0] : o->plate[0]) - c->center[0];
                        js[side].s1 = (c->plate[2] < o->plate[2] ? c->plate[2] : o->plate[2]) - c->center[0];
                    }
                }
                c->plate_first = nb[0] < 0;
                c->plate_last = nb[1] < 0;
                plate_build(&c->base_region, rect, rad, js, p->joint_clearance, p->curve_tol_mm);
            } else {

                rounded_rect(&c->base_region, lminx - mg, lminy - mg, lmaxx + mg, lmaxy + mg, p->base_radius, p->curve_tol_mm);
            }
            if (c->base_region.n) {
                extrude_region(&c->base_mesh, &c->base_region, 0, z0);
                c->ntris += c->base_mesh.nt;
                mesh_bbox(&c->base_mesh, c->bbox_min, c->bbox_max);
            }
        }
        {
            int body = model_body_slot(m, p);
            double hbody = body >= 0 ? p->body_height : 0;
            for (s = 0; s < m->nslots; s++) {
                if (!model_slot_active(m, p, s)) continue;
                if (body >= 0 && s == body) {
                    if (c->body_region.n == 0) continue;
                    if (!p->layered_flush) {
                        extrude_region(&c->slot_mesh[s], &c->body_region, z0, z0 + hbody);
                    } else {
                        /* body with pockets for the inlaid layers */
                        double hmax = 0;
                        const region_t *subs[MAX_SLOTS];
                        int ns = 0, q;
                        region_t upper;
                        for (q = 0; q < m->nslots; q++) {
                            double hq;
                            if (q == body || !model_slot_active(m, p, q) || c->slot_region[q].n == 0) continue;
                            hq = p->slot_height[q] < hbody ? p->slot_height[q] : hbody;
                            if (hq > hmax) hmax = hq;
                            subs[ns++] = &c->slot_region[q];
                        }
                        {
                            /* every slab is a closed shell of its own */
                            mesh_t part;
                            mesh_init(&part);
                            if (hbody - hmax > 1e-6) { extrude_region(&part, &c->body_region, z0, z0 + hbody - hmax); mesh_append_raw(&c->slot_mesh[s], &part); mesh_free(&part); }
                            if (hmax > 1e-6) {
                                /* snap the outlines to a fine grid so shared edges cancel exactly */
                                region_t sbody, ssubs[MAX_SLOTS];
                                const region_t *sp[MAX_SLOTS];
                                region_copy(&sbody, &c->body_region);
                                region_snap(&sbody, 0.002);
                                for (q = 0; q < ns; q++) { region_copy(&ssubs[q], subs[q]); region_snap(&ssubs[q], 0.002); sp[q] = &ssubs[q]; }
                                if (region_subtract(&upper, &sbody, sp, ns)) {
                                    region_clean(&upper, 1e-4);
                                    if (upper.n) { extrude_region(&part, &upper, z0 + hbody - hmax, z0 + hbody); mesh_append_raw(&c->slot_mesh[s], &part); mesh_free(&part); }
                                }
                                region_free(&upper);
                                region_free(&sbody);
                                for (q = 0; q < ns; q++) region_free(&ssubs[q]);
                                for (q = 0; q < ns; q++) {
                                    /* thinner layers get body material below them */
                                    int qs = -1, r;
                                    double hq;
                                    for (r = 0; r < m->nslots; r++) if (&c->slot_region[r] == subs[q]) qs = r;
                                    if (qs < 0) continue;
                                    hq = p->slot_height[qs] < hbody ? p->slot_height[qs] : hbody;
                                    if (hmax - hq > 1e-6) { extrude_region(&part, subs[q], z0 + hbody - hmax, z0 + hbody - hq); mesh_append_raw(&c->slot_mesh[s], &part); mesh_free(&part); }
                                }
                            }
                        }
                    }
                } else if (body >= 0) {
                    double zlo, zhi;
                    if (c->slot_region[s].n == 0) continue;
                    model_slot_zrange(m, p, s, &zlo, &zhi);
                    if (zhi - zlo > 1e-6) extrude_region(&c->slot_mesh[s], &c->slot_region[s], zlo, zhi);
                } else {
                    if (c->slot_region[s].n == 0) continue;
                    extrude_region(&c->slot_mesh[s], &c->slot_region[s], z0, z0 + p->slot_height[s]);
                }
                c->ntris += c->slot_mesh[s].nt;
                mesh_bbox(&c->slot_mesh[s], c->bbox_min, c->bbox_max);
            }
        }
        if (c->bbox_min[0] > c->bbox_max[0]) {
            c->bbox_min[0] = lminx - mg; c->bbox_min[1] = lminy - mg; c->bbox_min[2] = 0;
            c->bbox_max[0] = lmaxx + mg; c->bbox_max[1] = lmaxy + mg; c->bbox_max[2] = 0;
        }
        c->rot = 0;
        c->scale = 1;
        c->fits = 1;
    }
    /* centre every piece on its full footprint (plate and tabs included), so it
     * sits in the middle of the build plate on its own and in the grid */
    if (p->chunk_mode != CHUNK_OFF) {
        for (i = 0; i < m->nchunks; i++) {
            chunk_t *c = &m->chunks[i];
            double cx = (c->bbox_min[0] + c->bbox_max[0]) / 2, cy = (c->bbox_min[1] + c->bbox_max[1]) / 2;
            if (fabs(cx) < 1e-9 && fabs(cy) < 1e-9) continue;
            mesh_translate(&c->base_mesh, -cx, -cy);
            region_translate(&c->base_region, -cx, -cy);
            region_translate(&c->body_region, -cx, -cy);
            for (s = 0; s < MAX_SLOTS; s++) {
                mesh_translate(&c->slot_mesh[s], -cx, -cy);
                region_translate(&c->slot_region[s], -cx, -cy);
            }
            c->bbox_min[0] -= cx; c->bbox_max[0] -= cx;
            c->bbox_min[1] -= cy; c->bbox_max[1] -= cy;
            c->center[0] += cx;
            c->center[1] += cy;
        }
    }

    /* fitting the plate: turn, shrink (all pieces alike, or each on its own) */
    m->chunk_uniform_scale = 1;
    if (p->chunk_mode != CHUNK_OFF) {
        double uni = 1, fit = 16;
        /* largest uniform scale at which every piece (plate and tabs included) fits, turned if needed */
        for (i = 0; i < m->nchunks; i++) {
            chunk_t *c = &m->chunks[i];
            double s = model_max_fit_scale(c->bbox_max[0] - c->bbox_min[0], c->bbox_max[1] - c->bbox_min[1], 0, p->chunk_max_w, p->chunk_max_d);
            if (s < fit) fit = s;
        }
        if (fabs(fit - 1.0) < 1e-4) fit = 1;
        if (p->chunk_mode == CHUNK_OBJECTS && p->chunk_oversize != 0) m->chunk_fit_scale = fit;
        if (p->chunk_oversize == 1) {
            /* same factor for every piece: the one that makes the largest piece fit */
            uni = fit < 1 ? fit : 1;
            if (uni > 0.9999) uni = 1;
            if (uni < 0.01) uni = 0.01;
            m->chunk_uniform_scale = uni;
        }
        for (i = 0; i < m->nchunks; i++) {
            chunk_t *c = &m->chunks[i];
            double w = c->bbox_max[0] - c->bbox_min[0], d = c->bbox_max[1] - c->bbox_min[1];
            double ang;
            if (p->chunk_oversize == 1) {
                c->scale = uni;
                ang = model_fit_angle(w * uni, d * uni, p->chunk_max_w, p->chunk_max_d);
                if (ang >= 0) c->rot = ang; else c->fits = 0;
                continue;
            }
            ang = model_fit_angle(w, d, p->chunk_max_w, p->chunk_max_d);
            if (ang >= 0) c->rot = ang;
            else if (p->chunk_oversize == 2) {
                double s = model_max_fit_scale(w, d, 0, p->chunk_max_w, p->chunk_max_d);
                if (s > 0.9999) s = 1;
                if (s > 0.01) {
                    c->scale = s;
                    c->rot = model_fit_angle(w * s, d * s, p->chunk_max_w, p->chunk_max_d);
                    if (c->rot < 0) c->rot = 0;
                } else c->fits = 0;
            } else c->fits = 0;
        }
    }
    return model_build_view(m, p);
}

int model_build_view(model_t *m, const model_params *p)
{
    int i, s, k;
    if (!m->valid || m->nchunks == 0) return 0;

    /* preview placement: original positions, pushed apart where plates would overlap */
    for (i = 0; i < m->nchunks; i++) {
        chunk_t *c = &m->chunks[i];
        c->place[0] = c->center[0];
        c->place[1] = c->center[1];
    }
    if (p->chunk_mode != CHUNK_OFF) {
        for (i = 0; i < m->nchunks; i++) {
            chunk_t *c = &m->chunks[i];
            double shift = 0;
            int j;
            for (j = 0; j < i; j++) {
                const chunk_t *o = &m->chunks[j];
                double omin_y = o->bbox_min[1] + o->place[1], omax_y = o->bbox_max[1] + o->place[1];
                double cmin_y = c->bbox_min[1] + c->place[1], cmax_y = c->bbox_max[1] + c->place[1];
                double need;
                if (omax_y < cmin_y || cmax_y < omin_y) continue;                /* different rows */
                if (o->bbox_min[0] + o->place[0] > c->bbox_min[0] + c->place[0]) continue; /* o is to the right */
                need = (o->bbox_max[0] + o->place[0] + p->chunk_spacing) - (c->bbox_min[0] + c->place[0]);
                if (need > shift) shift = need;
            }
            c->place[0] += shift;
        }
    }

    /* preview geometry */
    model_free_view(m);
    for (s = 0; s < MAX_SLOTS; s++) m->slot_volume[s] = 0;
    m->base_volume = 0;
    m->total_tris = 0;
    for (k = 0; k < 3; k++) { m->bbox_min[k] = DBL_MAX; m->bbox_max[k] = -DBL_MAX; }
    for (i = 0; i < m->nchunks; i++) {
        chunk_t *c = &m->chunks[i];
        double dx = c->place[0], dy = c->place[1];
        int shown = 1, single = 0;
        if (p->chunk_view > 0 && p->chunk_view <= m->nchunks) { shown = (i == p->chunk_view - 1); dx = dy = 0; single = 1; }
        m->base_volume += mesh_volume(&c->base_mesh);
        for (s = 0; s < m->nslots; s++) m->slot_volume[s] += mesh_volume(&c->slot_mesh[s]);
        if (!shown) continue;
        if (single && (c->rot != 0 || c->scale != 1)) {
            /* show the piece as it will be exported: turned / shrunk to fit the plate */
            mesh_t rm;
            region_t rr;
            mesh_xform_copy(&rm, &c->base_mesh, c->rot, c->scale);
            mesh_append(&m->base_mesh, &rm, 0, 0);
            mesh_free(&rm);
            region_xform_copy(&rr, &c->base_region, c->rot, c->scale);
            region_append(&m->view_base_region, &rr, 0, 0);
            region_free(&rr);
            for (s = 0; s < m->nslots; s++) {
                mesh_xform_copy(&rm, &c->slot_mesh[s], c->rot, c->scale);
                mesh_append(&m->slot_mesh[s], &rm, 0, 0);
                mesh_free(&rm);
                region_xform_copy(&rr, (s == model_body_slot(m, p)) ? &c->body_region : &c->slot_region[s], c->rot, c->scale);
                region_append(&m->view_slot_region[s], &rr, 0, 0);
                region_free(&rr);
            }
            m->total_tris += c->ntris;
            {
                double a = c->rot * M_PI / 180.0, cs = fabs(cos(a)), sn = fabs(sin(a));
                double w = (c->bbox_max[0] - c->bbox_min[0]) * c->scale, d = (c->bbox_max[1] - c->bbox_min[1]) * c->scale;
                double rw = w * cs + d * sn, rd = w * sn + d * cs;
                m->bbox_min[0] = -rw / 2; m->bbox_max[0] = rw / 2;
                m->bbox_min[1] = -rd / 2; m->bbox_max[1] = rd / 2;
                m->bbox_min[2] = c->bbox_min[2]; m->bbox_max[2] = c->bbox_max[2];
            }
            continue;
        }
        region_append(&m->view_base_region, &c->base_region, dx, dy);
        mesh_append(&m->base_mesh, &c->base_mesh, dx, dy);
        for (s = 0; s < m->nslots; s++) {
            region_append(&m->view_slot_region[s], (s == model_body_slot(m, p)) ? &c->body_region : &c->slot_region[s], dx, dy);
            mesh_append(&m->slot_mesh[s], &c->slot_mesh[s], dx, dy);
        }
        m->total_tris += c->ntris;
        for (k = 0; k < 2; k++) {
            if (c->bbox_min[k] + (k == 0 ? dx : dy) < m->bbox_min[k]) m->bbox_min[k] = c->bbox_min[k] + (k == 0 ? dx : dy);
            if (c->bbox_max[k] + (k == 0 ? dx : dy) > m->bbox_max[k]) m->bbox_max[k] = c->bbox_max[k] + (k == 0 ? dx : dy);
        }
        if (c->bbox_min[2] < m->bbox_min[2]) m->bbox_min[2] = c->bbox_min[2];
        if (c->bbox_max[2] > m->bbox_max[2]) m->bbox_max[2] = c->bbox_max[2];
    }
    if (m->bbox_min[0] > m->bbox_max[0]) {
        for (k = 0; k < 3; k++) { m->bbox_min[k] = 0; m->bbox_max[k] = 0; }
    }
    m->meshes_valid = 1;
    return 1;
}
