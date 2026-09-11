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
/* dovetail tab width and spacing for the next cases (0 width = sized to the seam) */
static double case_joint_width = 0, case_joint_spacing = 60;

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
        if (top && bot) continue;   /* alone in its column: may shrink-wrap to fit the bed */
        for (j = i + 1; j < m->nchunks; j++) {
            const chunk_t *b = &m->chunks[j];
            if (a->group != b->group || a->iy != b->iy) continue;
            if (!has_nb(m, j, 0, 1) && !has_nb(m, j, 0, -1)) continue;
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

/* Tiles are as large as the plate allows: the grid has exactly the columns
 * and rows the budget calls for (margin on the outer side of an axis, a tab
 * on the other; both margins when the axis is not split), and every cell is
 * a full one. */
static void check_fill_plate(const char *tag, const model_t *m, const model_params *p)
{
    double mg = p->base_margin, side = p->chunk_joints ? 12 : mg;
    double tw = p->chunk_max_w - mg - side, td = p->chunk_max_d - mg - side;
    double maxw = 0, maxd = 0;
    int i, nx = 0, ny = 0, want_nx, want_ny;
    if (m->nchunks < 2) return;
    for (i = 0; i < m->nchunks; i++) {
        const chunk_t *c = &m->chunks[i];
        if (c->tile[2] - c->tile[0] > maxw) maxw = c->tile[2] - c->tile[0];
        if (c->tile[3] - c->tile[1] > maxd) maxd = c->tile[3] - c->tile[1];
        if (c->ix + 1 > nx) nx = c->ix + 1;
        if (c->iy + 1 > ny) ny = c->iy + 1;
    }
    /* one tile when the extent fits with both margins, else at least two */
    if (m->logo_w <= p->chunk_max_w - 2 * mg + 1e-6) want_nx = 1;
    else { want_nx = (int)ceil(m->logo_w / tw - 1e-6); if (want_nx < 2) want_nx = 2; }
    if (m->logo_h <= p->chunk_max_d - 2 * mg + 1e-6) want_ny = 1;
    else { want_ny = (int)ceil(m->logo_h / td - 1e-6); if (want_ny < 2) want_ny = 2; }
    if (nx != want_nx || ny != want_ny)
        failf(__LINE__, "%s: %d x %d tiles, the plate allows %d x %d (cells %.0f x %.0f mm)", tag, nx, ny, want_nx, want_ny, tw, td);
    else if (maxw < m->logo_w / want_nx - 0.5 || maxd < m->logo_h / want_ny - 0.5)
        failf(__LINE__, "%s: largest tile %.0f x %.0f mm, cells should be %.0f x %.0f", tag, maxw, maxd, m->logo_w / want_nx, m->logo_h / want_ny);
}

/* Every piece fits its plate as designed: unscaled, and a tile upright. */
static void check_fits(const char *tag, const model_t *m, const model_params *p)
{
    int i;
    for (i = 0; i < m->nchunks; i++) {
        const chunk_t *c = &m->chunks[i];
        double w = c->bbox_max[0] - c->bbox_min[0], d = c->bbox_max[1] - c->bbox_min[1];
        if (!c->fits || c->scale != 1)
            failf(__LINE__, "%s: piece %d (%.1f x %.1f mm) does not fit the %.0f x %.0f mm plate%s", tag, i + 1, w, d,
                  p->chunk_max_w, p->chunk_max_d, c->scale != 1 ? " (shrunk to fit)" : "");
        else if (p->chunk_mode == CHUNK_TILES && (w > p->chunk_max_w + 1e-6 || d > p->chunk_max_d + 1e-6))
            failf(__LINE__, "%s: tile %d (%.1f x %.1f mm) fits the %.0f x %.0f mm plate only turned", tag, i + 1, w, d,
                  p->chunk_max_w, p->chunk_max_d);
    }
}

/* One plate per piece: no islands, no holes. */
static void check_one_plate(const char *tag, const model_t *m)
{
    int i, k;
    for (i = 0; i < m->nchunks; i++) {
        const chunk_t *c = &m->chunks[i];
        if (c->base_region.n == 1) continue;
        failf(__LINE__, "%s: piece %d has %d plate contours (plate %.1f,%.1f-%.1f,%.1f, tile %.1f,%.1f-%.1f,%.1f)", tag, i + 1,
              c->base_region.n, c->plate[0], c->plate[1], c->plate[2], c->plate[3], c->tile[0], c->tile[1], c->tile[2], c->tile[3]);
        for (k = 0; k < c->base_region.n; k++) {
            region_t one;
            region_init(&one);
            region_add_contour(&one, c->base_region.c[k].pts, c->base_region.c[k].n);
            fprintf(stderr, "    contour %d: %.1f mm^2 at %.1f,%.1f-%.1f,%.1f (model coords)\n", k + 1, contour_area(&one.c[0]),
                    one.minx + c->center[0], one.miny + c->center[1], one.maxx + c->center[0], one.maxy + c->center[1]);
            region_free(&one);
        }
    }
}

/* Splitting neither invents nor duplicates artwork.  Jigsaw sockets may lose
 * the clearance ring around each tab; nothing else goes missing. */
static void check_conservation(const char *tag, const model_t *m, const model_params *p)
{
    int s, i;
    for (s = -1; s < m->nslots; s++) {
        double whole = s < 0 ? region_area(&m->footprint) : m->slots[s].area, split = 0;
        double tol = 1e-3 * whole + 0.05, lost_max = p->chunk_joints ? 0.01 * whole + 1.0 * m->nchunks : tol;
        if (s >= 0 && m->slots[s].merged_into >= 0) continue;
        for (i = 0; i < m->nchunks; i++)
            split += region_area(s < 0 ? &m->chunks[i].body_region : &m->chunks[i].slot_region[s]);
        if (split > whole + tol)
            failf(__LINE__, "%s: %s has %.1f mm^2 on the pieces but %.1f in the model: artwork invented", tag,
                  s < 0 ? "body" : "slot", split, whole);
        else if (split < whole - lost_max)
            failf(__LINE__, "%s: %s %d lost %.1f of %.1f mm^2 to the split", tag, s < 0 ? "body" : "slot", s + 1, whole - split, whole);
    }
}

static int run_case(const char *name, const char *svg, double width, double margin, double plate, int mode, int joints)
{
    app_state a;
    char tag[256];
    int before = nfail;
    snprintf(tag, sizeof tag, "%s %s%s w=%.0f mg=%.0f plate=%.0f", name, mode == CHUNK_TILES ? "tiles" : "objects",
             joints ? "" : "/loose", width, margin, plate);
    if (case_joint_width > 0 || case_joint_spacing != 60)
        snprintf(tag + strlen(tag), sizeof tag - strlen(tag), " tab=%.0f/%.0f", case_joint_width, case_joint_spacing);
    app_init(&a);
    a.params.width_mm = width;
    a.width_from_cli = 1;
    a.params.base_margin = margin;
    a.params.chunk_mode = mode;
    a.params.chunk_joints = joints;
    a.params.joint_width = case_joint_width;
    a.params.joint_spacing = case_joint_spacing;
    if (mode == CHUNK_OBJECTS) a.params.chunk_oversize = 0;   /* cut: pieces must fit as cut, never shrunk */
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
    check_fits(tag, &a.model, &a.params);
    check_one_plate(tag, &a.model);
    check_conservation(tag, &a.model, &a.params);
    check_coverage(tag, &a.model, margin);
    check_logo_on_plate(tag, &a.model);
    if (mode == CHUNK_TILES) check_fill_plate(tag, &a.model, &a.params);
    if (mode == CHUNK_TILES && joints) {
        /* the plate rectangles exist for connected plates only; loose tiles
         * are each their own artwork plus margin */
        check_overlaps(tag, &a.model);
        check_row_alignment(tag, &a.model);
        check_shrink_wrap(tag, &a.model, margin);
    }
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
#define TILES(name, f, w, mg, plate) run_case(name, f, w, mg, plate, CHUNK_TILES, 1)

    /* filled logo, several plate sizes */
    TILES("simple", EX("simple.svg"), 200, 3, 250);
    TILES("simple", EX("simple.svg"), 200, 20, 120);
    TILES("simple", EX("simple.svg"), 400, 10, 180);

    /* wide wordmark: empty cells, large margin (the layout bugs) */
    TILES("logo", EX("intellistream-logo.svg"), 400, 3, 80);
    TILES("logo", EX("intellistream-logo.svg"), 1500, 50, 250);
    TILES("logo", EX("intellistream-logo.svg"), 1500, 50, 270);
    TILES("logo", EX("intellistream-logo.svg"), 2000, 50, 250);
    TILES("logo", EX("intellistream-logo.svg"), 800, 20, 180);
    TILES("logo", EX("intellistream-logo.svg"), 600, 8, 220);
    TILES("logo", EX("intellistream-logo.svg"), 722, 20, 200);

    /* dense clip-path art */
    TILES("clip", EX("clip_pattern.svg"), 200, 3, 80);
    TILES("clip", EX("clip_pattern.svg"), 280, 12, 100);

    /* arcs and holes */
    TILES("arcs", EX("evenodd_arcs.svg"), 180, 8, 100);
    TILES("arcs", EX("evenodd_arcs.svg"), 240, 20, 140);

    TILES("overlap", EX("overlap.svg"), 200, 15, 90);
    TILES("colors", EX("many_colors.svg"), 200, 5, 80);

    /* synthetic silhouettes */
    TILES("L", FX("l_shape.svg"), 300, 40, 140);
    TILES("L", FX("l_shape.svg"), 500, 50, 180);
    TILES("L", FX("l_shape.svg"), 240, 12, 90);

    TILES("sparse", FX("sparse_squares.svg"), 400, 50, 180);
    TILES("sparse", FX("sparse_squares.svg"), 250, 20, 120);

    TILES("circle", FX("circle.svg"), 180, 15, 90);
    TILES("circle", FX("circle.svg"), 300, 40, 160);

    TILES("bar", FX("wide_bar.svg"), 800, 20, 200);
    TILES("bar", FX("wide_bar.svg"), 400, 8, 120);

    TILES("blobs", FX("three_blobs.svg"), 400, 25, 150);
    TILES("blobs", FX("three_blobs.svg"), 220, 8, 90);

    /* staggered column: a neighbour shrink-wrapped off the seam leaves a flap, not an island */
    TILES("stair", FX("stair3.svg"), 100, 3, 120);

    /* tiles without joints: every plate is its artwork plus the margin all round */
    run_case("simple", EX("simple.svg"), 400, 10, 180, CHUNK_TILES, 0);
    run_case("logo", EX("intellistream-logo.svg"), 770, 5, 200, CHUNK_TILES, 0);
    run_case("logo", EX("intellistream-logo.svg"), 1500, 50, 250, CHUNK_TILES, 0);
    run_case("L", FX("l_shape.svg"), 500, 50, 180, CHUNK_TILES, 0);

    /* split by object: every letter keeps its own artwork, seams carry only tabs */
    run_case("colors", EX("many_colors.svg"), 200, 3, 60, CHUNK_OBJECTS, 1);
    run_case("colors", EX("many_colors.svg"), 200, 3, 60, CHUNK_OBJECTS, 0);
    run_case("arcs", EX("evenodd_arcs.svg"), 240, 20, 140, CHUNK_OBJECTS, 1);
    run_case("logo", EX("intellistream-logo.svg"), 400, 3, 250, CHUNK_OBJECTS, 1);
    run_case("rects", FX("two_rects.svg"), 106, 3, 250, CHUNK_OBJECTS, 1);
    run_case("squares", FX("two_squares.svg"), 58, 3, 60, CHUNK_OBJECTS, 0);
    run_case("squares", FX("two_squares.svg"), 90, 3, 60, CHUNK_OBJECTS, 1);
    run_case("bar", FX("wide_bar.svg"), 1046, 3, 60, CHUNK_OBJECTS, 0);

    /* chosen tab sizes: a tab wider than the auto maximum stays within the
     * depth the tiles leave for it, and small close tabs keep the artwork */
    case_joint_width = 40; case_joint_spacing = 150;
    TILES("logo", EX("intellistream-logo.svg"), 400, 3, 80);
    TILES("simple", EX("simple.svg"), 400, 10, 180);
    run_case("arcs", EX("evenodd_arcs.svg"), 240, 20, 140, CHUNK_OBJECTS, 1);
    case_joint_width = 6; case_joint_spacing = 15;
    TILES("logo", EX("intellistream-logo.svg"), 400, 3, 80);
    TILES("L", FX("l_shape.svg"), 240, 12, 90);
    case_joint_width = 0; case_joint_spacing = 60;

#undef EX
#undef FX
#undef TILES

    printf("test_plates: %d checks, %d failed\n", ncheck, nfail);
    return nfail ? 1 : 0;
}
