/* 2D polygon regions with boolean operations, built on libtess2.
 *
 * A region is a set of closed contours.  A *normalised* region has winding
 * number exactly 1 inside and 0 outside, with every contour oriented so the
 * interior is on its left (outer boundaries CCW, holes CW), which is what
 * libtess2 produces for TESS_BOUNDARY_CONTOURS. */
#ifndef LOGO3D_REGION_H
#define LOGO3D_REGION_H

typedef struct {
    double *pts;    /* x0,y0,x1,y1,... */
    int n;          /* number of points */
} contour_t;

typedef struct {
    contour_t *c;
    int n;
    double minx, miny, maxx, maxy;
} region_t;

void region_init(region_t *r);
void region_free(region_t *r);
/* Append a copy of a closed contour (consecutive duplicate points removed). */
void region_add_contour(region_t *r, const double *pts, int n);
void region_update_bbox(region_t *r);
int region_empty(const region_t *r);
int region_bbox_overlap(const region_t *a, const region_t *b);

/* Normalise raw contours with the given fill rule (even-odd or non-zero). */
int region_normalize(region_t *out, const region_t *in, int evenodd);
/* Union of normalised regions. */
int region_union(region_t *out, const region_t *const *rs, int n);
/* a minus the union of subs (all normalised). */
int region_subtract(region_t *out, const region_t *a, const region_t *const *subs, int nsubs);
/* Intersection of two normalised regions. */
int region_intersect(region_t *out, const region_t *a, const region_t *b);
/* Clip a normalised region against an axis-aligned rectangle with
 * Sutherland-Hodgman (per contour, independent of libtess2). Far more robust
 * than region_intersect() for this common case: a whole design's per-colour
 * region, with many disjoint and geometrically complex contours, clipped
 * against one small tile rectangle. */
int region_clip_rect(region_t *out, const region_t *in, double x0, double y0, double x1, double y1);
/* Triangulate a normalised region. verts: x,y pairs; tris: 3 indices each. */
int region_triangulate(const region_t *r, double **verts, int *nverts, int **tris, int *ntris);
/* Signed area (positive for a normalised region). */
double region_area(const region_t *r);
double contour_area(const contour_t *c);
void contour_reverse(contour_t *c);
/* Deep copy. */
int region_copy(region_t *out, const region_t *in);
/* Drop contours whose |area| is below min_area (numerical slivers). */
void region_clean(region_t *r, double min_area);
/* Snap every coordinate to a grid so nearly coincident edges become identical. */
void region_snap(region_t *r, double grid);

#endif
