/* SVG parser: produces a flat list of filled/stroked paths in root user
 * coordinates, with resolved colours.  No rasterisation happens here. */
#ifndef LOGO3D_SVG_H
#define LOGO3D_SVG_H

#include <stddef.h>

typedef enum {
    SVG_SEG_MOVE,
    SVG_SEG_LINE,
    SVG_SEG_CUBIC,
    SVG_SEG_CLOSE
} svg_segtype;

typedef struct {
    svg_segtype type;
    double x[3], y[3];      /* MOVE/LINE: [0]; CUBIC: c1, c2, end */
} svg_seg;

typedef enum { SVG_CAP_BUTT, SVG_CAP_ROUND, SVG_CAP_SQUARE } svg_linecap;
typedef enum { SVG_JOIN_MITER, SVG_JOIN_ROUND, SVG_JOIN_BEVEL } svg_linejoin;

typedef struct {
    svg_seg *segs;
    int nsegs;
    int has_fill;
    unsigned fill_rgb;      /* 0xRRGGBB */
    int fill_evenodd;
    int has_stroke;
    unsigned stroke_rgb;
    double stroke_width;    /* in root user units (after transform) */
    svg_linecap linecap;
    svg_linejoin linejoin;
    double miter_limit;
    char *id;
    int clip;               /* index into svg_doc.clips, or -1 */
} svg_path;

/* A clipPath: the union of its paths (each with its own fill rule),
 * intersected with the parent clip when nested. */
typedef struct {
    int first_path;         /* index into svg_doc.clip_paths */
    int npaths;
    int parent;             /* enclosing clip index or -1 */
} svg_clip;

typedef struct {
    svg_path *paths;
    int npaths;
    svg_path *clip_paths;   /* geometry of clip paths (fill only) */
    int nclip_paths;
    svg_clip *clips;
    int nclips;
    /* Root coordinate system (viewBox or width/height). */
    double vb_x, vb_y, vb_w, vb_h;
    /* Physical size in mm if the document specifies it (0 otherwise). */
    double width_mm, height_mm;
    /* Counters. */
    int n_text;             /* text elements rendered with a font */
    int n_text_skipped;     /* text elements without a usable font */
    int n_image;
    int n_unsupported;
    int n_gradients;
    char font_used[512];    /* font file used for text (empty when none) */
} svg_doc;

/* font_file: TrueType/OpenType file used for all <text>, or NULL to pick system fonts. */
svg_doc *svg_parse_data(const char *data, size_t len, const char *font_file, char *err, size_t errlen);
svg_doc *svg_parse_file(const char *path, const char *font_file, char *err, size_t errlen);
void svg_free(svg_doc *doc);

/* Parse a CSS colour string; returns 1 on success. Handles #rgb, #rrggbb,
 * rgb(), rgba(), and the SVG named colours. */
int svg_parse_color(const char *s, unsigned *rgb);

#endif
