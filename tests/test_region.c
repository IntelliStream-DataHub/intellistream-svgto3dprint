/* Unit tests for the polygon-set operations in src/region.c: normalisation,
 * union, subtraction, intersection and rectangle clipping. They pin down the
 * geometric contract the model builder relies on: areas and contour counts,
 * and that boolean results are clean (every vertex a real corner, no sliver
 * edges, no bridged contours), so a "smarter" implementation cannot quietly
 * break the extruded meshes. Exit status is non-zero when a check fails. */
#include "region.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int nfail = 0, ncheck = 0;

static void check_(int ok, const char *what, int line)
{
    ncheck++;
    if (!ok) { nfail++; fprintf(stderr, "FAIL test_region.c:%d: %s\n", line, what); }
}

static void check_near_(double a, double b, double eps, const char *what, int line)
{
    ncheck++;
    if (!(fabs(a - b) <= eps)) {
        nfail++;
        fprintf(stderr, "FAIL test_region.c:%d: %s: %.9g vs %.9g (eps %g)\n", line, what, a, b, eps);
    }
}

#define CHECK(c) check_((c) != 0, #c, __LINE__)
#define CHECK_NEAR(a, b, eps) check_near_((a), (b), (eps), #a " == " #b, __LINE__)

/* ---- builders; every make_* result is a normalised region ---- */

static void add_rect(region_t *r, double x0, double y0, double x1, double y1, int clockwise)
{
    double ccw[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
    double cw[8] = {x0, y0, x0, y1, x1, y1, x1, y0};
    region_add_contour(r, clockwise ? cw : ccw, 4);
}

static void add_ngon(region_t *r, double cx, double cy, double rad, int n)
{
    double *p = (double *)malloc(sizeof(double) * 2 * (size_t)n);
    int i;
    for (i = 0; i < n; i++) {
        double a = 2 * M_PI * i / n;
        p[2 * i] = cx + rad * cos(a);
        p[2 * i + 1] = cy + rad * sin(a);
    }
    region_add_contour(r, p, n);
    free(p);
}

static void finish(region_t *out, region_t *raw, int evenodd)
{
    CHECK(region_normalize(out, raw, evenodd));
    region_free(raw);
}

static void make_rect(region_t *out, double x0, double y0, double x1, double y1)
{
    region_t raw;
    region_init(&raw);
    add_rect(&raw, x0, y0, x1, y1, 0);
    finish(out, &raw, 0);
}

static void make_ngon(region_t *out, double cx, double cy, double rad, int n)
{
    region_t raw;
    region_init(&raw);
    add_ngon(&raw, cx, cy, rad, n);
    finish(out, &raw, 0);
}

/* n-gon of radius `outer` with an n2-gon hole of radius `inner` */
static void make_ring(region_t *out, double cx, double cy, double outer, int n, double inner, int n2)
{
    region_t raw;
    region_init(&raw);
    add_ngon(&raw, cx, cy, outer, n);
    add_ngon(&raw, cx, cy, inner, n2);
    finish(out, &raw, 1);   /* even-odd: the inner contour is a hole */
}

/* a U: 10 x 10 square with the middle third cut away from the top, plus a
 * small hole in the left arm */
static void make_u(region_t *out)
{
    double u[16] = {0, 0, 10, 0, 10, 10, 7, 10, 7, 3, 3, 3, 3, 10, 0, 10};
    region_t raw;
    region_init(&raw);
    region_add_contour(&raw, u, 8);
    add_rect(&raw, 1, 4, 2, 7, 0);
    finish(out, &raw, 1);
}

/* ---- inspection ---- */

static int nverts(const region_t *r)
{
    int i, n = 0;
    for (i = 0; i < r->n; i++) n += r->c[i].n;
    return n;
}

static int nholes(const region_t *r)
{
    int i, n = 0;
    for (i = 0; i < r->n; i++) if (contour_area(&r->c[i]) < 0) n++;
    return n;
}

static double min_edge(const region_t *r)
{
    double m = 1e300;
    int i, j;
    for (i = 0; i < r->n; i++) {
        const contour_t *c = &r->c[i];
        for (j = 0; j < c->n; j++) {
            int k = (j + 1) % c->n;
            double dx = c->pts[2 * k] - c->pts[2 * j], dy = c->pts[2 * k + 1] - c->pts[2 * j + 1];
            double d = sqrt(dx * dx + dy * dy);
            if (d < m) m = d;
        }
    }
    return m;
}

/* Vertices that are not real corners: their two edges are collinear (the sine
 * of the turning angle is below eps). A clean boolean result has none; points
 * re-inserted along an existing straight edge show up here. */
static int flat_verts(const region_t *r, double eps)
{
    int i, j, n = 0;
    for (i = 0; i < r->n; i++) {
        const contour_t *c = &r->c[i];
        for (j = 0; j < c->n; j++) {
            int p = (j + c->n - 1) % c->n, q = (j + 1) % c->n;
            double ax = c->pts[2 * j] - c->pts[2 * p], ay = c->pts[2 * j + 1] - c->pts[2 * p + 1];
            double bx = c->pts[2 * q] - c->pts[2 * j], by = c->pts[2 * q + 1] - c->pts[2 * j + 1];
            double la = sqrt(ax * ax + ay * ay), lb = sqrt(bx * bx + by * by);
            if (la == 0 || lb == 0 || fabs(ax * by - ay * bx) <= eps * la * lb) n++;
        }
    }
    return n;
}

/* Directed edges that appear more than once, or together with their reverse:
 * zero-width bridges and overlapping contours. A normalised region has none. */
static int shared_edges(const region_t *r)
{
    int i, j, k, l, n = 0;
    for (i = 0; i < r->n; i++) {
        const contour_t *a = &r->c[i];
        for (j = 0; j < a->n; j++) {
            double ax0 = a->pts[2 * j], ay0 = a->pts[2 * j + 1];
            double ax1 = a->pts[2 * ((j + 1) % a->n)], ay1 = a->pts[2 * ((j + 1) % a->n) + 1];
            for (k = i; k < r->n; k++) {
                const contour_t *b = &r->c[k];
                for (l = (k == i ? j + 1 : 0); l < b->n; l++) {
                    double bx0 = b->pts[2 * l], by0 = b->pts[2 * l + 1];
                    double bx1 = b->pts[2 * ((l + 1) % b->n)], by1 = b->pts[2 * ((l + 1) % b->n) + 1];
                    int same = fabs(ax0 - bx0) < 1e-9 && fabs(ay0 - by0) < 1e-9 && fabs(ax1 - bx1) < 1e-9 && fabs(ay1 - by1) < 1e-9;
                    int rev = fabs(ax0 - bx1) < 1e-9 && fabs(ay0 - by1) < 1e-9 && fabs(ax1 - bx0) < 1e-9 && fabs(ay1 - by0) < 1e-9;
                    if (same || rev) n++;
                }
            }
        }
    }
    return n;
}

static double tri_area(const region_t *r)
{
    double *v = NULL, s = 0;
    int *t = NULL, nv = 0, nt = 0, i;
    CHECK(region_triangulate(r, &v, &nv, &t, &nt));
    for (i = 0; i < nt; i++) {
        const double *a = v + 2 * t[3 * i], *b = v + 2 * t[3 * i + 1], *c = v + 2 * t[3 * i + 2];
        s += 0.5 * ((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]));
    }
    free(v);
    free(t);
    return s;
}

static double ngon_area(double rad, int n) { return 0.5 * n * rad * rad * sin(2 * M_PI / n); }

/* ---- tests ---- */

static void test_normalize(void)
{
    region_t raw, r;

    /* a clockwise input comes out counter-clockwise (positive area) */
    region_init(&raw);
    add_rect(&raw, 0, 0, 10, 10, 1);
    finish(&r, &raw, 0);
    CHECK(r.n == 1);
    CHECK(nverts(&r) == 4);
    CHECK_NEAR(region_area(&r), 100, 1e-6);
    CHECK_NEAR(r.minx, 0, 1e-9);
    CHECK_NEAR(r.maxx, 10, 1e-9);
    CHECK_NEAR(r.miny, 0, 1e-9);
    CHECK_NEAR(r.maxy, 10, 1e-9);
    region_free(&r);

    /* nested contours wound the same way: even-odd punches a hole, non-zero does not */
    region_init(&raw);
    add_rect(&raw, 0, 0, 10, 10, 0);
    add_rect(&raw, 3, 3, 7, 7, 0);
    finish(&r, &raw, 1);
    CHECK(r.n == 2);
    CHECK(nholes(&r) == 1);
    CHECK_NEAR(region_area(&r), 84, 1e-6);
    region_free(&r);
    region_init(&raw);
    add_rect(&raw, 0, 0, 10, 10, 0);
    add_rect(&raw, 3, 3, 7, 7, 0);
    finish(&r, &raw, 0);
    CHECK(r.n == 1);
    CHECK_NEAR(region_area(&r), 100, 1e-6);
    region_free(&r);

    /* a hole wound the other way is a hole under both rules */
    region_init(&raw);
    add_rect(&raw, 0, 0, 10, 10, 0);
    add_rect(&raw, 3, 3, 7, 7, 1);
    finish(&r, &raw, 0);
    CHECK(r.n == 2);
    CHECK_NEAR(region_area(&r), 84, 1e-6);
    region_free(&r);

    /* overlapping contours merge into one clean outline */
    region_init(&raw);
    add_rect(&raw, 0, 0, 10, 10, 0);
    add_rect(&raw, 5, 5, 15, 15, 0);
    finish(&r, &raw, 0);
    CHECK(r.n == 1);
    CHECK(nverts(&r) == 8);
    CHECK_NEAR(region_area(&r), 175, 1e-6);
    region_free(&r);

    /* empty in, empty out; degenerate contours are dropped on input */
    region_init(&raw);
    finish(&r, &raw, 0);
    CHECK(r.n == 0);
    CHECK(region_empty(&r));
    region_free(&r);
    region_init(&raw);
    {
        double line[6] = {0, 0, 5, 5, 0, 0};
        region_add_contour(&raw, line, 3);
        CHECK(raw.n == 0);
    }
    region_free(&raw);
}

static void test_union(void)
{
    region_t a, b, out;
    const region_t *rs[2];

    make_rect(&a, 0, 0, 10, 10);
    make_rect(&b, 5, 5, 15, 15);
    rs[0] = &a;
    rs[1] = &b;
    CHECK(region_union(&out, rs, 2));
    CHECK(out.n == 1);
    CHECK(nverts(&out) == 8);
    CHECK_NEAR(region_area(&out), 175, 1e-6);
    region_free(&out);

    /* disjoint inputs stay separate contours */
    region_free(&b);
    make_rect(&b, 20, 0, 30, 10);
    CHECK(region_union(&out, rs, 2));
    CHECK(out.n == 2);
    CHECK_NEAR(region_area(&out), 200, 1e-6);
    region_free(&out);

    /* one inside the other: the outer wins */
    region_free(&b);
    make_rect(&b, 2, 2, 4, 4);
    CHECK(region_union(&out, rs, 2));
    CHECK(out.n == 1);
    CHECK_NEAR(region_area(&out), 100, 1e-6);
    region_free(&out);
    region_free(&a);
    region_free(&b);
}

static void test_subtract(void)
{
    region_t a, b, out;
    const region_t *subs[1];

    make_rect(&a, 0, 0, 10, 10);
    make_rect(&b, 3, 3, 7, 7);
    subs[0] = &b;
    CHECK(region_subtract(&out, &a, subs, 1));
    CHECK(out.n == 2);
    CHECK(nholes(&out) == 1);
    CHECK_NEAR(region_area(&out), 84, 1e-6);
    region_free(&out);

    /* subtracting a cover leaves nothing; a disjoint subtrahend changes nothing */
    region_free(&b);
    make_rect(&b, -1, -1, 11, 11);
    CHECK(region_subtract(&out, &a, subs, 1));
    CHECK(out.n == 0);
    region_free(&out);
    region_free(&b);
    make_rect(&b, 20, 20, 30, 30);
    CHECK(region_subtract(&out, &a, subs, 1));
    CHECK(out.n == 1);
    CHECK_NEAR(region_area(&out), 100, 1e-6);
    region_free(&out);

    /* a bite out of one corner leaves an L */
    region_free(&b);
    make_rect(&b, 5, -1, 11, 5);
    CHECK(region_subtract(&out, &a, subs, 1));
    CHECK(out.n == 1);
    CHECK(nverts(&out) == 6);
    CHECK_NEAR(region_area(&out), 75, 1e-6);
    region_free(&out);
    region_free(&a);
    region_free(&b);
}

static void test_intersect_basic(void)
{
    region_t a, b, e, out;

    make_rect(&a, 0, 0, 10, 10);
    make_rect(&b, 5, 5, 15, 15);
    CHECK(region_intersect(&out, &a, &b));
    CHECK(out.n == 1);
    CHECK(nverts(&out) == 4);
    CHECK_NEAR(region_area(&out), 25, 1e-6);
    CHECK_NEAR(out.minx, 5, 1e-9);
    CHECK_NEAR(out.maxx, 10, 1e-9);
    CHECK_NEAR(out.miny, 5, 1e-9);
    CHECK_NEAR(out.maxy, 10, 1e-9);
    region_free(&out);
    CHECK(region_intersect(&out, &b, &a));   /* commutative */
    CHECK_NEAR(region_area(&out), 25, 1e-6);
    region_free(&out);

    /* disjoint: empty */
    region_free(&b);
    make_rect(&b, 20, 0, 30, 10);
    CHECK(region_intersect(&out, &a, &b));
    CHECK(out.n == 0);
    region_free(&out);

    /* touching along an edge only: nothing of any area */
    region_free(&b);
    make_rect(&b, 10, 0, 20, 10);
    CHECK(region_intersect(&out, &a, &b));
    CHECK_NEAR(region_area(&out), 0, 1e-9);
    region_free(&out);

    /* contained: the smaller one, unchanged */
    region_free(&b);
    make_rect(&b, 2, 2, 4, 4);
    CHECK(region_intersect(&out, &a, &b));
    CHECK(out.n == 1);
    CHECK(nverts(&out) == 4);
    CHECK_NEAR(region_area(&out), 4, 1e-6);
    region_free(&out);
    CHECK(region_intersect(&out, &b, &a));
    CHECK_NEAR(region_area(&out), 4, 1e-6);
    region_free(&out);

    /* an empty operand */
    region_init(&e);
    CHECK(region_intersect(&out, &a, &e));
    CHECK(out.n == 0);
    region_free(&out);
    region_free(&a);
    region_free(&b);
}

static void test_intersect_hole(void)
{
    region_t raw, a, b, out;

    /* a square with a hole, clipped to its lower half: the hole becomes a notch */
    region_init(&raw);
    add_rect(&raw, 0, 0, 10, 10, 0);
    add_rect(&raw, 3, 3, 7, 7, 0);
    finish(&a, &raw, 1);
    make_rect(&b, -1, -1, 11, 5);
    CHECK(region_intersect(&out, &a, &b));
    CHECK(out.n == 1);
    CHECK(nverts(&out) == 8);
    CHECK_NEAR(region_area(&out), 50 - 8, 1e-6);
    region_free(&out);

    /* clipped to a window around the hole: the hole survives as a hole */
    region_free(&b);
    make_rect(&b, 1, 1, 9, 9);
    CHECK(region_intersect(&out, &a, &b));
    CHECK(out.n == 2);
    CHECK(nholes(&out) == 1);
    CHECK_NEAR(region_area(&out), 64 - 16, 1e-6);
    region_free(&out);
    region_free(&a);
    region_free(&b);
}

/* The clip-path case that once broke: curved outlines (many short edges)
 * clipped by a boundary that crosses them at coordinates that are not
 * exactly representable. The result must be the plain geometric
 * intersection with every vertex a real corner. (Computing it as
 * A - (A - B) re-inserted the rounded crossing points along A's own edges,
 * and the extruded mesh came out open.) */
static void test_intersect_curved(void)
{
    region_t a, b, out, ref;
    double xc, expect;

    /* 12-gon of radius 10 (a vertex on +x) cut to the strip x >= 0, |y| <= 3:
     * an exactly known pentagon */
    make_ngon(&a, 0, 0, 10, 12);
    make_rect(&b, 0, -3, 20, 3);
    CHECK(region_intersect(&out, &a, &b));
    xc = 10 - (10 - 10 * cos(M_PI / 6)) * (3.0 / 5.0);   /* y = 3 meets the edge from (10cos30, 5) to (10, 0) */
    expect = 6 * xc + 0.5 * 6 * (10 - xc);
    CHECK(out.n == 1);
    CHECK(nverts(&out) == 5);
    CHECK_NEAR(region_area(&out), expect, 1e-4);
    CHECK(flat_verts(&out, 1e-6) == 0);
    region_free(&out);
    region_free(&a);
    region_free(&b);

    /* an off-grid ring (64-gon with a 32-gon hole) cut by a rectangle that
     * opens the hole: compared with the rectangle clipper, an independent
     * algorithm that is exact for this case */
    make_ring(&a, 0.3, 0.7, 10, 64, 4, 32);
    make_rect(&b, -2.2, -13, 13, 2.9);
    CHECK(region_intersect(&out, &a, &b));
    CHECK(region_clip_rect(&ref, &a, -2.2, -13, 13, 2.9));
    CHECK(out.n == 1);
    CHECK_NEAR(region_area(&out), region_area(&ref), 1e-3);
    CHECK(flat_verts(&out, 1e-6) == 0);
    CHECK(min_edge(&out) > 0.5 * min_edge(&ref));
    region_free(&out);
    region_free(&ref);

    /* the same ring in a window that cuts the outline on all four sides but
     * keeps the hole whole */
    region_free(&b);
    make_rect(&b, -9, -7, 9.5, 7.3);
    CHECK(region_intersect(&out, &a, &b));
    CHECK(region_clip_rect(&ref, &a, -9, -7, 9.5, 7.3));
    CHECK(out.n == 2);
    CHECK(nholes(&out) == 1);
    CHECK(nverts(&out) == nverts(&ref));
    CHECK_NEAR(region_area(&out), region_area(&ref), 1e-3);
    CHECK(flat_verts(&out, 1e-6) == 0);
    region_free(&out);
    region_free(&ref);
    region_free(&a);
    region_free(&b);

    /* a clip that is not a rectangle: a pentagon over a 48-gon */
    make_ngon(&a, 1.1, -0.4, 9, 48);
    make_ngon(&b, 4.2, 2.3, 7, 5);
    CHECK(region_intersect(&out, &a, &b));
    CHECK(out.n == 1);
    CHECK(region_area(&out) > 0.5 * ngon_area(7, 5) && region_area(&out) < ngon_area(7, 5));
    CHECK(flat_verts(&out, 1e-6) == 0);
    CHECK(min_edge(&out) > 1e-3);
    CHECK_NEAR(tri_area(&out), region_area(&out), 1e-4);
    region_free(&out);
    region_free(&a);
    region_free(&b);
}

static void test_clip_rect(void)
{
    region_t a, b, out, ref;
    double xc, expect;

    /* the exactly known pentagon again, through the clipper */
    make_ngon(&a, 0, 0, 10, 12);
    CHECK(region_clip_rect(&out, &a, 0, -3, 20, 3));
    xc = 10 - (10 - 10 * cos(M_PI / 6)) * (3.0 / 5.0);
    expect = 6 * xc + 0.5 * 6 * (10 - xc);
    CHECK(out.n == 1);
    CHECK(nverts(&out) == 5);
    CHECK_NEAR(region_area(&out), expect, 1e-4);
    CHECK(out.minx >= -1e-9 && out.miny >= -3 - 1e-9 && out.maxy <= 3 + 1e-9);
    region_free(&out);

    /* a rectangle around everything changes nothing; one outside leaves nothing */
    CHECK(region_clip_rect(&out, &a, -20, -20, 20, 20));
    CHECK(out.n == 1);
    CHECK(nverts(&out) == 12);
    CHECK_NEAR(region_area(&out), ngon_area(10, 12), 1e-4);
    region_free(&out);
    CHECK(region_clip_rect(&out, &a, 20, 20, 30, 30));
    CHECK(out.n == 0);
    region_free(&out);

    /* a corner nick: the triangle between x = 5.5, y = 5.5 and the edge x + y = 10cos30 + 5 */
    CHECK(region_clip_rect(&out, &a, 5.5, 5.5, 20, 20));
    {
        double s = 10 * cos(M_PI / 6) + 5 - 11;   /* leg of the triangle */
        CHECK(out.n == 1);
        CHECK(nverts(&out) == 3);
        CHECK_NEAR(region_area(&out), 0.5 * s * s, 1e-4);
    }
    region_free(&out);
    region_free(&a);

    /* a concave shape with a hole, cut through both arms and the hole: the
     * two arms come back as two clean contours (no zero-width bridge along
     * the cut), the same shape the boolean intersection gives, and it
     * triangulates to the same area, which is what the tile cutter needs */
    make_u(&a);
    make_rect(&b, -1, 5, 11, 12);
    CHECK(region_clip_rect(&out, &a, -1, 5, 11, 12));
    CHECK(region_intersect(&ref, &a, &b));
    CHECK(out.n == 2);
    CHECK(nholes(&out) == 0);
    CHECK(shared_edges(&out) == 0);
    CHECK_NEAR(region_area(&out), 30 - 2, 1e-4);
    CHECK_NEAR(region_area(&out), region_area(&ref), 1e-4);
    CHECK(nverts(&out) == nverts(&ref));
    CHECK_NEAR(tri_area(&out), 28, 1e-4);
    CHECK(out.minx >= -1 - 1e-9 && out.maxx <= 11 + 1e-9 && out.miny >= 5 - 1e-9 && out.maxy <= 12 + 1e-9);
    region_free(&out);
    region_free(&ref);
    region_free(&b);

    /* cut so the hole stays whole: outer contour plus hole */
    CHECK(region_clip_rect(&out, &a, -1, -1, 11, 8));
    CHECK(out.n == 2);
    CHECK(nholes(&out) == 1);
    CHECK(shared_edges(&out) == 0);
    CHECK_NEAR(region_area(&out), (10 * 8 - 4 * 5) - 3, 1e-4);
    region_free(&out);
    region_free(&a);

    /* a ring cut through the hole: one contour, hole opened into a notch */
    make_ring(&a, 0.3, 0.7, 10, 64, 4, 32);
    CHECK(region_clip_rect(&out, &a, -2.2, -13, 13, 2.9));
    CHECK(out.n == 1);
    CHECK(nholes(&out) == 0);
    CHECK(shared_edges(&out) == 0);
    CHECK_NEAR(tri_area(&out), region_area(&out), 1e-4);
    region_free(&out);
    region_free(&a);
}

static void test_clean_snap_copy(void)
{
    region_t raw, r, c;

    region_init(&raw);
    add_rect(&raw, 0, 0, 10, 10, 0);
    add_rect(&raw, 20, 20, 20.01, 20.01, 0);
    finish(&r, &raw, 0);
    CHECK(r.n == 2);
    CHECK(region_copy(&c, &r));
    CHECK(c.n == 2 && nverts(&c) == nverts(&r));
    CHECK_NEAR(region_area(&c), region_area(&r), 1e-12);
    region_free(&c);

    /* clean drops the numerical sliver and keeps the bounding box honest */
    region_clean(&r, 0.001);
    CHECK(r.n == 1);
    CHECK_NEAR(region_area(&r), 100, 1e-6);
    CHECK_NEAR(r.maxx, 10, 1e-9);

    /* snapping to a coarse grid keeps a square a square ... */
    region_snap(&r, 0.5);
    CHECK(r.n == 1 && nverts(&r) == 4);
    CHECK_NEAR(region_area(&r), 100, 1e-9);
    region_free(&r);

    /* ... and collapses a contour thinner than the grid */
    region_init(&r);
    add_rect(&r, 0, 0, 0.1, 10, 0);
    region_snap(&r, 0.5);
    CHECK(r.n == 0);
    region_free(&r);

    CHECK(region_bbox_overlap(&raw, &raw) == 0);   /* empty never overlaps */
}

int main(void)
{
    test_normalize();
    test_union();
    test_subtract();
    test_intersect_basic();
    test_intersect_hole();
    test_intersect_curved();
    test_clip_rect();
    test_clean_snap_copy();
    printf("test_region: %d checks, %d failed\n", ncheck, nfail);
    return nfail ? 1 : 0;
}
