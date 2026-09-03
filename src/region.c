#include "region.h"
#include "tesselator.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

void region_init(region_t *r)
{
    memset(r, 0, sizeof(*r));
    r->minx = r->miny = DBL_MAX;
    r->maxx = r->maxy = -DBL_MAX;
}

void region_free(region_t *r)
{
    int i;
    if (!r) return;
    for (i = 0; i < r->n; i++) free(r->c[i].pts);
    free(r->c);
    region_init(r);
}

void region_add_contour(region_t *r, const double *pts, int n)
{
    contour_t c;
    int i, m = 0;
    if (n < 3) return;
    c.pts = (double *)malloc(sizeof(double) * 2 * n);
    for (i = 0; i < n; i++) {
        double x = pts[2 * i], y = pts[2 * i + 1];
        if (m > 0 && fabs(c.pts[2 * (m - 1)] - x) < 1e-12 && fabs(c.pts[2 * (m - 1) + 1] - y) < 1e-12) continue;
        c.pts[2 * m] = x;
        c.pts[2 * m + 1] = y;
        m++;
    }
    /* drop closing duplicate */
    while (m > 1 && fabs(c.pts[0] - c.pts[2 * (m - 1)]) < 1e-12 && fabs(c.pts[1] - c.pts[2 * (m - 1) + 1]) < 1e-12) m--;
    if (m < 3) { free(c.pts); return; }
    c.n = m;
    r->c = (contour_t *)realloc(r->c, sizeof(contour_t) * (r->n + 1));
    r->c[r->n++] = c;
    for (i = 0; i < m; i++) {
        double x = c.pts[2 * i], y = c.pts[2 * i + 1];
        if (x < r->minx) r->minx = x;
        if (x > r->maxx) r->maxx = x;
        if (y < r->miny) r->miny = y;
        if (y > r->maxy) r->maxy = y;
    }
}

void region_update_bbox(region_t *r)
{
    int i, j;
    r->minx = r->miny = DBL_MAX;
    r->maxx = r->maxy = -DBL_MAX;
    for (i = 0; i < r->n; i++)
        for (j = 0; j < r->c[i].n; j++) {
            double x = r->c[i].pts[2 * j], y = r->c[i].pts[2 * j + 1];
            if (x < r->minx) r->minx = x;
            if (x > r->maxx) r->maxx = x;
            if (y < r->miny) r->miny = y;
            if (y > r->maxy) r->maxy = y;
        }
}

int region_empty(const region_t *r)
{
    return r->n == 0;
}

int region_bbox_overlap(const region_t *a, const region_t *b)
{
    if (a->n == 0 || b->n == 0) return 0;
    return !(a->maxx < b->minx || b->maxx < a->minx || a->maxy < b->miny || b->maxy < a->miny);
}

double contour_area(const contour_t *c)
{
    double s = 0;
    int i;
    for (i = 0; i < c->n; i++) {
        int j = (i + 1) % c->n;
        s += c->pts[2 * i] * c->pts[2 * j + 1] - c->pts[2 * j] * c->pts[2 * i + 1];
    }
    return s * 0.5;
}

void contour_reverse(contour_t *c)
{
    int i, j;
    for (i = 0, j = c->n - 1; i < j; i++, j--) {
        double tx = c->pts[2 * i], ty = c->pts[2 * i + 1];
        c->pts[2 * i] = c->pts[2 * j];
        c->pts[2 * i + 1] = c->pts[2 * j + 1];
        c->pts[2 * j] = tx;
        c->pts[2 * j + 1] = ty;
    }
}

double region_area(const region_t *r)
{
    double s = 0;
    int i;
    for (i = 0; i < r->n; i++) s += contour_area(&r->c[i]);
    return s;
}

int region_copy(region_t *out, const region_t *in)
{
    int i;
    region_init(out);
    for (i = 0; i < in->n; i++) region_add_contour(out, in->c[i].pts, in->c[i].n);
    return 1;
}

void region_clean(region_t *r, double min_area)
{
    int i, w = 0;
    for (i = 0; i < r->n; i++) {
        if (fabs(contour_area(&r->c[i])) < min_area) { free(r->c[i].pts); continue; }
        r->c[w++] = r->c[i];
    }
    r->n = w;
    region_update_bbox(r);
}

void region_snap(region_t *r, double grid)
{
    int i, j;
    region_t out;
    region_init(&out);
    for (i = 0; i < r->n; i++) {
        contour_t *c = &r->c[i];
        for (j = 0; j < c->n; j++) {
            c->pts[2 * j] = floor(c->pts[2 * j] / grid + 0.5) * grid;
            c->pts[2 * j + 1] = floor(c->pts[2 * j + 1] / grid + 0.5) * grid;
        }
        region_add_contour(&out, c->pts, c->n);   /* removes duplicates created by snapping */
    }
    region_free(r);
    *r = out;
}

/* ------------------------------------------------------------------ */

static void add_region_contours(TESStesselator *t, const region_t *r, int reverse)
{
    int i;
    for (i = 0; i < r->n; i++) {
        const contour_t *c = &r->c[i];
        if (!reverse) {
            tessAddContour(t, 2, c->pts, sizeof(double) * 2, c->n);
        } else {
            double *tmp = (double *)malloc(sizeof(double) * 2 * c->n);
            int j;
            for (j = 0; j < c->n; j++) {
                tmp[2 * j] = c->pts[2 * (c->n - 1 - j)];
                tmp[2 * j + 1] = c->pts[2 * (c->n - 1 - j) + 1];
            }
            tessAddContour(t, 2, tmp, sizeof(double) * 2, c->n);
            free(tmp);
        }
    }
}

/* Run a boundary extraction and fill `out` from the result. */
static int boundary_to_region(TESStesselator *t, int winding, region_t *out)
{
    const TESSreal *verts;
    const TESSindex *elems;
    int nelems, i;
    TESSreal normal[3] = {0, 0, 1};
    region_init(out);
    if (!tessTesselate(t, winding, TESS_BOUNDARY_CONTOURS, 0, 2, normal)) return 0;
    verts = tessGetVertices(t);
    elems = tessGetElements(t);
    nelems = tessGetElementCount(t);
    for (i = 0; i < nelems; i++) {
        int start = elems[2 * i], count = elems[2 * i + 1];
        double *pts = (double *)malloc(sizeof(double) * 2 * count);
        int j;
        for (j = 0; j < count; j++) {
            pts[2 * j] = verts[2 * (start + j)];
            pts[2 * j + 1] = verts[2 * (start + j) + 1];
        }
        region_add_contour(out, pts, count);
        free(pts);
    }
    return 1;
}

int region_normalize(region_t *out, const region_t *in, int evenodd)
{
    TESStesselator *t;
    int ok;
    if (in->n == 0) { region_init(out); return 1; }
    t = tessNewTess(NULL);
    if (!t) return 0;
    add_region_contours(t, in, 0);
    ok = boundary_to_region(t, evenodd ? TESS_WINDING_ODD : TESS_WINDING_NONZERO, out);
    tessDeleteTess(t);
    return ok;
}

int region_union(region_t *out, const region_t *const *rs, int n)
{
    TESStesselator *t;
    int i, ok, any = 0;
    for (i = 0; i < n; i++) if (rs[i]->n) any = 1;
    if (!any) { region_init(out); return 1; }
    t = tessNewTess(NULL);
    if (!t) return 0;
    for (i = 0; i < n; i++) add_region_contours(t, rs[i], 0);
    ok = boundary_to_region(t, TESS_WINDING_POSITIVE, out);
    tessDeleteTess(t);
    return ok;
}

int region_subtract(region_t *out, const region_t *a, const region_t *const *subs, int nsubs)
{
    TESStesselator *t;
    int i, ok;
    if (a->n == 0) { region_init(out); return 1; }
    t = tessNewTess(NULL);
    if (!t) return 0;
    add_region_contours(t, a, 0);
    for (i = 0; i < nsubs; i++) add_region_contours(t, subs[i], 1);
    ok = boundary_to_region(t, TESS_WINDING_POSITIVE, out);
    tessDeleteTess(t);
    return ok;
}

/* A intersect B. Both inputs are normalised (positive outer contours,
 * negative holes, no self-overlap), so the winding number is exactly 2 where
 * both cover: one ABS_GEQ_TWO pass is the whole job. Do not "improve" this
 * into A - (A - B): that feeds libtess2 edges coinciding with A's own
 * boundary, and the rounded crossing vertices it re-inserts along them give
 * contours with extra near-collinear vertices. The cap
 * triangulation and the side walls then disagree about those edges and the
 * extruded mesh comes out open (seen on clip-path'ed logo letters). Tile
 * clipping against a rectangle uses region_clip_rect(), not this. */
int region_intersect(region_t *out, const region_t *a, const region_t *b)
{
    TESStesselator *t;
    int ok;
    if (a->n == 0 || b->n == 0 || !region_bbox_overlap(a, b)) { region_init(out); return 1; }
    t = tessNewTess(NULL);
    if (!t) return 0;
    add_region_contours(t, a, 0);
    add_region_contours(t, b, 0);
    ok = boundary_to_region(t, TESS_WINDING_ABS_GEQ_TWO, out);
    tessDeleteTess(t);
    return ok;
}

/* Sutherland-Hodgman: clip a (possibly non-convex, but simple) polygon
 * against one half-plane "keep(x,y)". `in`/`out` are flat x,y arrays. */
typedef int (*sh_keep_fn)(double x, double y, double edge);

static int sh_keep_xge(double x, double y, double e) { (void)y; return x >= e; }
static int sh_keep_xle(double x, double y, double e) { (void)y; return x <= e; }
static int sh_keep_yge(double x, double y, double e) { (void)x; return y >= e; }
static int sh_keep_yle(double x, double y, double e) { (void)x; return y <= e; }

/* Intersection of segment (x0,y0)-(x1,y1) with the boundary line of the
 * half-plane (vertical line x=edge for the x tests, horizontal for y). */
static void sh_isect(double x0, double y0, double x1, double y1, double edge, int is_x, double *ox, double *oy)
{
    double t;
    if (is_x) {
        t = (edge - x0) / (x1 - x0);
        *ox = edge;
        *oy = y0 + t * (y1 - y0);
    } else {
        t = (edge - y0) / (y1 - y0);
        *ox = x0 + t * (x1 - x0);
        *oy = edge;
    }
}

static int sh_clip_pass(const double *in, int nin, double *out, double edge, int is_x, sh_keep_fn keep)
{
    int nout = 0, i;
    if (nin == 0) return 0;
    for (i = 0; i < nin; i++) {
        double cx = in[2 * i], cy = in[2 * i + 1];
        int j = (i + 1) % nin;
        double nx = in[2 * j], ny = in[2 * j + 1];
        int cin = keep(cx, cy, edge), nin_ = keep(nx, ny, edge);
        if (cin) {
            out[2 * nout] = cx; out[2 * nout + 1] = cy; nout++;
            if (!nin_) {
                double ix, iy;
                sh_isect(cx, cy, nx, ny, edge, is_x, &ix, &iy);
                out[2 * nout] = ix; out[2 * nout + 1] = iy; nout++;
            }
        } else if (nin_) {
            double ix, iy;
            sh_isect(cx, cy, nx, ny, edge, is_x, &ix, &iy);
            out[2 * nout] = ix; out[2 * nout + 1] = iy; nout++;
        }
    }
    return nout;
}

int region_clip_rect(region_t *out, const region_t *in, double x0, double y0, double x1, double y1)
{
    int i, cap = 0;
    double *buf = NULL, *tmp = NULL;
    region_t raw;
    region_init(out);
    if (in->n == 0 || in->maxx < x0 || in->minx > x1 || in->maxy < y0 || in->miny > y1) return 1;
    region_init(&raw);
    for (i = 0; i < in->n; i++) {
        const contour_t *c = &in->c[i];
        int n = c->n, cn, need;
        /* each of the 4 clip passes can at most double the vertex count */
        need = n * 16 + 32;
        if (need > cap) { cap = need; buf = (double *)realloc(buf, sizeof(double) * 2 * (size_t)cap); tmp = (double *)realloc(tmp, sizeof(double) * 2 * (size_t)cap); }
        memcpy(buf, c->pts, sizeof(double) * 2 * (size_t)n);
        cn = sh_clip_pass(buf, n, tmp, x0, 1, sh_keep_xge); memcpy(buf, tmp, sizeof(double) * 2 * (size_t)cn); n = cn;
        cn = sh_clip_pass(buf, n, tmp, x1, 1, sh_keep_xle); memcpy(buf, tmp, sizeof(double) * 2 * (size_t)cn); n = cn;
        cn = sh_clip_pass(buf, n, tmp, y0, 0, sh_keep_yge); memcpy(buf, tmp, sizeof(double) * 2 * (size_t)cn); n = cn;
        cn = sh_clip_pass(buf, n, tmp, y1, 0, sh_keep_yle); memcpy(buf, tmp, sizeof(double) * 2 * (size_t)cn); n = cn;
        if (n >= 3) region_add_contour(&raw, buf, n);
    }
    free(buf);
    free(tmp);
    /* Sutherland-Hodgman clips every contour on its own: a shape the
     * rectangle cuts into several parts comes back as one contour whose parts
     * hang together by zero-width bridges along the clip edge, and a cut hole
     * overlaps its outer contour there. Normalising (non-zero winding) turns
     * that into clean separate contours, which the extruder needs: the bridge
     * edges otherwise leave the side walls open (seen as open tile meshes on a
     * ring cut by a tile edge). Only the few contours near the rectangle reach
     * the tessellator here, the easy case for it. */
    if (raw.n == 0) return 1;
    if (!region_normalize(out, &raw, 0)) { *out = raw; return 1; }   /* keep the raw clip rather than nothing */
    region_free(&raw);
    return 1;
}

int region_triangulate(const region_t *r, double **verts, int *nverts, int **tris, int *ntris)
{
    TESStesselator *t;
    const TESSreal *v;
    const TESSindex *e;
    int nv, ne, i, m;
    TESSreal normal[3] = {0, 0, 1};
    *verts = NULL; *nverts = 0; *tris = NULL; *ntris = 0;
    if (r->n == 0) return 1;
    t = tessNewTess(NULL);
    if (!t) return 0;
    add_region_contours(t, r, 0);
    if (!tessTesselate(t, TESS_WINDING_POSITIVE, TESS_POLYGONS, 3, 2, normal)) { tessDeleteTess(t); return 0; }
    v = tessGetVertices(t);
    nv = tessGetVertexCount(t);
    e = tessGetElements(t);
    ne = tessGetElementCount(t);
    *verts = (double *)malloc(sizeof(double) * 2 * (nv ? nv : 1));
    for (i = 0; i < nv; i++) { (*verts)[2 * i] = v[2 * i]; (*verts)[2 * i + 1] = v[2 * i + 1]; }
    *nverts = nv;
    *tris = (int *)malloc(sizeof(int) * 3 * (ne ? ne : 1));
    m = 0;
    for (i = 0; i < ne; i++) {
        int a = e[3 * i], b = e[3 * i + 1], c = e[3 * i + 2];
        if (a == TESS_UNDEF || b == TESS_UNDEF || c == TESS_UNDEF) continue;
        (*tris)[3 * m] = a; (*tris)[3 * m + 1] = b; (*tris)[3 * m + 2] = c;
        m++;
    }
    *ntris = m;
    tessDeleteTess(t);
    return 1;
}
