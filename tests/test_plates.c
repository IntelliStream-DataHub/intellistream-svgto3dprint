/* Layout checks for jigsaw plates: margins, empty tiles, row alignment and
 * "does this look right" coverage of the artwork.  Loads real and fixture
 * SVGs at several widths, margins and plate sizes.
 *
 *   test_plates [project-root]
 */
#include "app.h"
#include "region.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EXAMPLES_DIR
#define EXAMPLES_DIR "examples"
#endif
#ifndef FIXTURES_DIR
#define FIXTURES_DIR "tests/fixtures"
#endif

static int nfail, ncheck;
static char root[1024];

static void check_(int ok, const char *what, int line)
{
    ncheck++;
    if (!ok) {
        nfail++;
        fprintf(stderr, "FAIL test_plates.c:%d: %s\n", line, what);
    }
}
#define CHECK(c) check_((c) != 0, #c, __LINE__)

static void failf(int line, const char *fmt, ...)
{
    va_list ap;
    ncheck++;
    nfail++;
    fprintf(stderr, "FAIL test_plates.c:%d: ", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static const char *path_join(char *buf, size_t n, const char *dir, const char *file)
{
    if (dir[0] == '/' || (dir[0] && dir[1] == ':'))
        snprintf(buf, n, "%s/%s", dir, file);
    else
        snprintf(buf, n, "%s/%s/%s", root[0] ? root : ".", dir, file);
    return buf;
}

static int pip_contour(const contour_t *c, double x, double y)
{
    int i, j, inside = 0;
    for (i = 0, j = c->n - 1; i < c->n; j = i++) {
        double xi = c->pts[2 * i], yi = c->pts[2 * i + 1];
        double xj = c->pts[2 * j], yj = c->pts[2 * j + 1];
        if ((yi > y) != (yj > y) &&
            x < (xj - xi) * (y - yi) / ((yj - yi) != 0 ? (yj - yi) : 1e-18) + xi)
            inside = !inside;
    }
    return inside;
}

static int region_contains_xy(const region_t *r, double x, double y)
{
    int i, n = 0;
    if (r->n == 0) return 0;
    if (x < r->minx - 1e-6 || x > r->maxx + 1e-6 || y < r->miny - 1e-6 || y > r->maxy + 1e-6)
        return 0;
    for (i = 0; i < r->n; i++) if (pip_contour(&r->c[i], x, y)) n++;
    return n & 1;
}

static void world_plate(region_t *out, const chunk_t *c)
{
    int i, k;
    region_copy(out, &c->base_region);
    for (i = 0; i < out->n; i++) {
        for (k = 0; k < out->c[i].n; k++) {
            out->c[i].pts[2 * k] += c->center[0];
            out->c[i].pts[2 * k + 1] += c->center[1];
        }
    }
    region_update_bbox(out);
}

static int edge_neighbours(const chunk_t *a, const chunk_t *b)
{
    if (a->group != b->group) return 0;
    return (a->ix == b->ix && (a->iy == b->iy + 1 || a->iy == b->iy - 1))
        || (a->iy == b->iy && (a->ix == b->ix + 1 || a->ix == b->ix - 1));
}

static int has_nb(const model_t *m, int i, int dx, int dy)
{
    const chunk_t *c = &m->chunks[i];
    int k;
    for (k = 0; k < m->nchunks; k++) {
        const chunk_t *o = &m->chunks[k];
        if (k == i || o->group != c->group) continue;
        if (o->ix == c->ix + dx && o->iy == c->iy + dy) return 1;
    }
    return 0;
}

static double dist_rect(double x, double y, double x0, double y0, double x1, double y1)
{
    double dx = 0, dy = 0;
    if (x < x0) dx = x0 - x;
    else if (x > x1) dx = x - x1;
    if (y < y0) dy = y0 - y;
    else if (y > y1) dy = y - y1;
    return hypot(dx, dy);
}

static void check_overlaps(const char *tag, const model_t *m)
{
    int i, j;
    region_t *w = (region_t *)calloc((size_t)m->nchunks, sizeof(region_t));
    if (!w) return;
    for (i = 0; i < m->nchunks; i++) world_plate(&w[i], &m->chunks[i]);
    for (i = 0; i < m->nchunks; i++) {
        if (w[i].n == 0) continue;
        for (j = i + 1; j < m->nchunks; j++) {
            region_t hit;
            if (w[j].n == 0 || edge_neighbours(&m->chunks[i], &m->chunks[j])) continue;
            if (!region_bbox_overlap(&w[i], &w[j])) continue;
            if (!region_intersect(&hit, &w[i], &w[j])) continue;
            {
                const chunk_t *a = &m->chunks[i], *b = &m->chunks[j];
                double area = region_area(&hit);
                int diag = a->group == b->group
                    && (a->ix == b->ix + 1 || a->ix == b->ix - 1)
                    && (a->iy == b->iy + 1 || a->iy == b->iy - 1);
                /* orthogonal neighbours are skipped; a diagonal pair may share
                 * a 12 mm × 12 mm tab corner */
                if (area > (diag ? 200.0 : 8.0))
                    failf(__LINE__, "%s: pieces %d and %d overlap by %.1f mm^2 (%s)",
                          tag, i + 1, j + 1, area, diag ? "diagonal" : "not neighbours");
            }
            region_free(&hit);
        }
    }
    for (i = 0; i < m->nchunks; i++) region_free(&w[i]);
    free(w);
}

/* Artwork plus margin must sit on some plate (rounded corners excluded). */
static void check_coverage(const char *tag, const model_t *m, double mg)
{
    int i, s, ix, iy, npts = 0, miss = 0;
    region_t *w;
    double reach, step;
    if (mg < 1 || m->nchunks == 0) return;
    reach = mg - (mg > 8 ? 4.0 : 1.5); /* stay inside rounded corners */
    if (reach < 1) reach = 1;
    w = (region_t *)calloc((size_t)m->nchunks, sizeof(region_t));
    if (!w) return;
    for (i = 0; i < m->nchunks; i++) world_plate(&w[i], &m->chunks[i]);
    step = mg > 20 ? 8.0 : 5.0;
    for (s = 0; s < m->nchunks; s++) {
        const chunk_t *c = &m->chunks[s];
        double x0 = c->gmin[0] - reach, y0 = c->gmin[1] - reach;
        double x1 = c->gmax[0] + reach, y1 = c->gmax[1] + reach;
        int nx, ny;
        if (x1 - x0 < 1 || y1 - y0 < 1) continue;
        nx = (int)((x1 - x0) / step); if (nx < 2) nx = 2; if (nx > 24) nx = 24;
        ny = (int)((y1 - y0) / step); if (ny < 2) ny = 2; if (ny > 24) ny = 24;
        for (ix = 0; ix <= nx; ix++) {
            for (iy = 0; iy <= ny; iy++) {
                double x = x0 + (x1 - x0) * ix / nx;
                double y = y0 + (y1 - y0) * iy / ny;
                double dmin = 1e9;
                int hit = 0, k;
                for (k = 0; k < m->nchunks; k++) {
                    double d = dist_rect(x, y, m->chunks[k].gmin[0], m->chunks[k].gmin[1],
                                         m->chunks[k].gmax[0], m->chunks[k].gmax[1]);
                    if (d < dmin) dmin = d;
                }
                if (dmin > reach + 0.2) continue;
                npts++;
                for (k = 0; k < m->nchunks; k++) {
                    if (region_contains_xy(&w[k], x, y)) { hit = 1; break; }
                }
                if (!hit) miss++;
            }
        }
    }
    if (npts > 0 && miss * 20 > npts) /* more than 5% uncovered */
        failf(__LINE__, "%s: artwork+margin coverage %d/%d samples missed", tag, miss, npts);
    for (i = 0; i < m->nchunks; i++) region_free(&w[i]);
    free(w);
}

static void check_row_alignment(const char *tag, const model_t *m)
{
    int i, j;
    for (i = 0; i < m->nchunks; i++) {
        const chunk_t *a = &m->chunks[i];
        int top = !has_nb(m, i, 0, 1), bot = !has_nb(m, i, 0, -1);
        if (!top && !bot) continue;
        for (j = i + 1; j < m->nchunks; j++) {
            const chunk_t *b = &m->chunks[j];
            if (a->group != b->group || a->iy != b->iy) continue;
            if (top && !has_nb(m, j, 0, 1) && fabs(a->plate[3] - b->plate[3]) > 0.6)
                failf(__LINE__, "%s: pieces %d and %d (row iy=%d) top misaligned by %.2f mm",
                      tag, i + 1, j + 1, a->iy, fabs(a->plate[3] - b->plate[3]));
            if (bot && !has_nb(m, j, 0, -1) && fabs(a->plate[1] - b->plate[1]) > 0.6)
                failf(__LINE__, "%s: pieces %d and %d (row iy=%d) bottom misaligned by %.2f mm",
                      tag, i + 1, j + 1, a->iy, fabs(a->plate[1] - b->plate[1]));
        }
    }
}

/* A sliver of artwork in a large empty cell must not keep a tile-wide plate. */
static void check_shrink_wrap(const char *tag, const model_t *m, double mg)
{
    int i;
    if (mg < 2) return;
    for (i = 0; i < m->nchunks; i++) {
        const chunk_t *c = &m->chunks[i];
        double inset, want;
        if (has_nb(m, i, -1, 0)) continue;
        inset = c->gmin[0] - c->tile[0];
        if (inset < 5) continue;
        want = c->gmin[0] - mg;
        if (c->plate[0] < want - 2.0)
            failf(__LINE__, "%s: piece %d left plate %.1f is %.1f mm past artwork+margin (inset %.1f)",
                  tag, i + 1, c->plate[0], want - c->plate[0], inset);
    }
    for (i = 0; i < m->nchunks; i++) {
        const chunk_t *c = &m->chunks[i];
        double inset, want;
        if (has_nb(m, i, 1, 0)) continue;
        inset = c->tile[2] - c->gmax[0];
        if (inset < 5) continue;
        want = c->gmax[0] + mg;
        if (c->plate[2] > want + 2.0)
            failf(__LINE__, "%s: piece %d right plate %.1f is %.1f mm past artwork+margin (inset %.1f)",
                  tag, i + 1, c->plate[2], c->plate[2] - want, inset);
    }
}

static void check_logo_on_plate(const char *tag, const model_t *m)
{
    int i, s;
    for (i = 0; i < m->nchunks; i++) {
        const chunk_t *c = &m->chunks[i];
        if (c->base_region.n == 0) continue;
        for (s = 0; s < m->nslots; s++) {
            region_t extra;
            const region_t *subs[1];
            double a;
            if (c->slot_region[s].n == 0) continue;
            subs[0] = &c->base_region;
            if (!region_subtract(&extra, &c->slot_region[s], subs, 1)) continue;
            a = region_area(&extra);
            region_free(&extra);
            if (a > 15.0)
                failf(__LINE__, "%s: piece %d slot %d has %.1f mm^2 of logo off the plate",
                      tag, i + 1, s + 1, a);
        }
    }
}

static int run_case(const char *name, const char *svg, double width, double margin, double plate)
{
    app_state a;
    char tag[256];
    int before = nfail;
    snprintf(tag, sizeof tag, "%s w=%.0f mg=%.0f plate=%.0f", name, width, margin, plate);
    app_init(&a);
    a.params.width_mm = width;
    a.width_from_cli = 1;
    a.params.base_margin = margin;
    a.params.chunk_mode = CHUNK_TILES;
    a.params.chunk_joints = 1;
    a.params.chunk_max_w = plate - 4;
    a.params.chunk_max_d = plate - 4;
    if (!app_load_svg(&a, svg)) {
        failf(__LINE__, "%s: load failed: %s", tag, a.last_error);
        app_free(&a);
        return 0;
    }
    CHECK(a.model.nchunks >= 1);
    if (a.model.nchunks > 90) {
        printf("skip %s (%d pieces)\n", tag, a.model.nchunks);
        app_free(&a);
        return 1;
    }
    check_overlaps(tag, &a.model);
    check_coverage(tag, &a.model, margin);
    check_row_alignment(tag, &a.model);
    check_shrink_wrap(tag, &a.model, margin);
    check_logo_on_plate(tag, &a.model);
    printf("%s: %d pieces%s\n", tag, a.model.nchunks, nfail > before ? "  FAILED" : "");
    app_free(&a);
    return 1;
}

int main(int argc, char **argv)
{
    char p[1024];
    if (argc > 1) snprintf(root, sizeof root, "%s", argv[1]);
    else snprintf(root, sizeof root, ".");

#define EX(f) path_join(p, sizeof p, EXAMPLES_DIR, f)
#define FX(f) path_join(p, sizeof p, FIXTURES_DIR, f)

    /* filled logo, several plate sizes */
    run_case("simple", EX("simple.svg"), 200, 3, 250);
    run_case("simple", EX("simple.svg"), 200, 20, 120);
    run_case("simple", EX("simple.svg"), 400, 10, 180);

    /* wide wordmark: empty cells, large margin (the layout bugs) */
    run_case("logo", EX("intellistream-logo.svg"), 400, 3, 80);
    run_case("logo", EX("intellistream-logo.svg"), 1500, 50, 250);
    run_case("logo", EX("intellistream-logo.svg"), 1500, 50, 270);
    run_case("logo", EX("intellistream-logo.svg"), 800, 20, 180);
    run_case("logo", EX("intellistream-logo.svg"), 600, 8, 220);

    /* dense clip-path art */
    run_case("clip", EX("clip_pattern.svg"), 200, 3, 80);
    run_case("clip", EX("clip_pattern.svg"), 280, 12, 100);

    /* arcs and holes */
    run_case("arcs", EX("evenodd_arcs.svg"), 180, 8, 100);
    run_case("arcs", EX("evenodd_arcs.svg"), 240, 20, 140);

    run_case("overlap", EX("overlap.svg"), 200, 15, 90);
    run_case("colors", EX("many_colors.svg"), 200, 5, 80);

    /* synthetic silhouettes */
    run_case("L", FX("l_shape.svg"), 300, 40, 140);
    run_case("L", FX("l_shape.svg"), 500, 50, 180);
    run_case("L", FX("l_shape.svg"), 240, 12, 90);

    run_case("sparse", FX("sparse_squares.svg"), 400, 50, 180);
    run_case("sparse", FX("sparse_squares.svg"), 250, 20, 120);

    run_case("circle", FX("circle.svg"), 180, 15, 90);
    run_case("circle", FX("circle.svg"), 300, 40, 160);

    run_case("bar", FX("wide_bar.svg"), 800, 20, 200);
    run_case("bar", FX("wide_bar.svg"), 400, 8, 120);

    run_case("blobs", FX("three_blobs.svg"), 400, 25, 150);
    run_case("blobs", FX("three_blobs.svg"), 220, 8, 90);

#undef EX
#undef FX

    printf("test_plates: %d checks, %d failed\n", ncheck, nfail);
    return nfail ? 1 : 0;
}
