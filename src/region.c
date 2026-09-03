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
