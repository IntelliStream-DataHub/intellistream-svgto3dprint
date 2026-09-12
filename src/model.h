/* Turns a parsed SVG into per-colour 3D meshes. */
#ifndef LOGO3D_MODEL_H
#define LOGO3D_MODEL_H

#include "svg.h"
#include "region.h"

#define MAX_SLOTS 8

typedef struct {
    double *v;          /* xyz triples */
    int nv, cv;
    unsigned *t;        /* vertex index triples, CCW seen from outside */
    int nt, ct;
    /* vertex welding table (only valid while building) */
    int *hash;
    int hcap;
} mesh_t;

typedef struct {
    unsigned rgb;       /* colour as found in the SVG (after quantisation) */
    double area;        /* visible area in mm^2 */
    int nshapes;        /* shapes assigned to the slot */
    int merged_into;    /* -1, or slot index this slot was merged into by the user */
} slot_info;

/* One printable piece of the model (the whole model when not chunking). */
typedef struct {
    char name[16];
    region_t clip;              /* outline used to select geometry (model coords) */
    double gmin[2], gmax[2];    /* geometry bounding box in model coords */
    double center[2];           /* chunk origin in model coords */
    double place[2];            /* preview offset of the local origin */
    int row;                    /* reading-order row */
    int plate_first, plate_last;/* first / last piece of its row (rounded outer corners) */
    int group, ix, iy;          /* tile grid membership: group id and grid indices */
    double tile[4];             /* tile rectangle in model coords (x0,y0,x1,y1); the bbox for uncut pieces */
    double plate[4];            /* plate rectangle in model coords */
    double rot;                 /* rotation (degrees, about Z) applied on export so the piece fits the plate */
    double scale;               /* XY scale applied on export (1 = as designed; < 1 = shrunk to fit) */
    int fits;                   /* exported footprint fits the plate */
    int on_plate;               /* printer plate the piece is arranged on (0-based) */
    double plate_pos[2];        /* centre of its exported footprint on that plate (mm from the front-left corner) */
    region_t slot_region[MAX_SLOTS];    /* local coords (centred on the chunk) */
    region_t body_region;               /* layered mode: union of all colours */
    /* the artwork as cut at the tile lines; slot_region / body_region are
     * derived from these on every mesh build (jigsaw plates reshape them) */
    region_t cut_slot_region[MAX_SLOTS];
    region_t cut_body_region;
    region_t base_region;
    mesh_t slot_mesh[MAX_SLOTS];
    mesh_t base_mesh;
    double bbox_min[3], bbox_max[3];    /* local coords */
    int ntris;
} chunk_t;

typedef enum { CHUNK_OFF = 0, CHUNK_OBJECTS = 1, CHUNK_TILES = 2 } chunk_mode_t;

/* How neighbouring base plates are joined (model_params.chunk_joints). */
typedef enum {
    JOINTS_NONE = 0,        /* separate rounded plates */
    JOINTS_JIGSAW = 1,      /* one strip per row, jigsaw dovetail tabs (drop in from above) */
    JOINTS_KEYS = 2         /* jigsaw tabs plus sliding dovetail keys on the underside that lock the pieces together */
} joint_style_t;

/* Base plates thinner than this get no keys: the slot needs a ceiling and ledges. */
#define KEY_MIN_PLATE 3.0

/* One sliding dovetail key: a separate print that slides in a slot on the
 * underside across a seam.  Parked inside piece `a` (the socket side), pushed
 * half way into piece `b` after assembly. */
typedef struct {
    double len;             /* mm along the slot */
    int a, b;               /* chunk indices */
    int side;               /* seam side of `a`: 1 right, 3 top */
} joint_key_t;

typedef struct {
    /* sizing: overall model footprint, base plate margin included */
    double width_mm;        /* target model width */
    double height_mm;       /* target model height (used when fit_by_height) */
    int fit_by_height;
    int mirror_x;
    double curve_tol_mm;    /* curve flattening tolerance */
    double merge_threshold; /* colours closer than this (0..441) are merged */
    int max_colors;         /* total material limit (base plate included) */
    /* base plate */
    int base_enabled;
    double base_thickness;
    double base_margin;
    double base_radius;
    int base_color_slot;    /* -1 = own colour (base_rgb) */
    unsigned base_rgb;
    /* per-slot settings */
    double slot_height[MAX_SLOTS];
    unsigned slot_rgb[MAX_SLOTS];       /* print colour override */
    int slot_rgb_override[MAX_SLOTS];
    int slot_visible[MAX_SLOTS];
    int slot_merge_into[MAX_SLOTS];     /* -1 = none */
    /* chunking (multi-plate prints) */
    int chunk_mode;             /* chunk_mode_t */
    double chunk_join_pct;      /* objects closer than this (% of logo height) form one chunk */
    int chunk_oversize;         /* pieces larger than the plate: 0 cut into tiles, 1 scale all pieces uniformly,
                                   2 scale each piece on its own, 3 keep and warn */
    double chunk_max_w;         /* largest chunk footprint (mm): the plate minus plate_padding */
    double chunk_max_d;
    double chunk_spacing;       /* preview spacing between chunks (mm) */
    int chunk_view;             /* preview: 0 = all chunks, n = chunk n only, centred */
    double plate_padding;       /* mm kept free on the plate in total (half on each side), for the
                                   pieces and for a one-piece model fitted to the plate */
    int chunk_joints;           /* joint_style_t: how neighbouring base plates are joined */
    double joint_clearance;     /* mm of play between tab and socket, and around a key */
    double joint_spacing;       /* wanted distance between dovetail tab centres along a seam (mm) */
    double joint_width;         /* width of a dovetail tab at its widest (mm), 0 = sized to the seam */
    double joint_offset;        /* mm the tabs are slid along their seam (+ = up / right),
                                   capped per seam at the room its end tabs have */
    /* layered colours: one colour forms the whole logo body, the others are
     * thin layers on top of it (raised) or inlaid flush with its top */
    int layered;
    int layered_flush;
    int body_slot;              /* colour slot that forms the body, -1 = largest visible colour */
    double body_height;         /* height of the body in layered mode */
    /* export */
    int export_color_objects;   /* 3MF: every colour is its own object (else parts of one object) */
} model_params;

typedef struct {
    int valid;
    /* stage A (layout) */
    int nslots;
    slot_info slots[MAX_SLOTS];
    region_t slot_region[MAX_SLOTS];
    region_t footprint;         /* union of every shape before painter's-order cutting (layered body) */
    region_t base_region;
    double scale;               /* mm per SVG user unit */
    double logo_w, logo_h;      /* logo bounding box size in mm */
    double logo_min[2], logo_max[2];
    int colors_before_merge;
    int nshapes;
    /* stage B (chunks + meshes) */
    int meshes_valid;
    int nchunks;
    chunk_t *chunks;
    int chunks_valid;           /* chunks match chunk_* params below */
    int chunk_mode_used;
    double chunk_join_used, chunk_max_w_used, chunk_max_d_used;
    int chunk_oversize_used;
    int chunk_joints_used;
    double chunk_fit_scale;     /* largest uniform scale (relative to now) at which every piece fits uncut */
    double chunk_uniform_scale; /* scale applied to every piece (policy 1), 1 when nothing was shrunk */
    /* sliding dovetail keys (JOINTS_KEYS), one per seam end that has room */
    int nkeys;
    joint_key_t *keys;
    int keys_too_thin;          /* keys wanted but the base plate is thinner than KEY_MIN_PLATE */
    /* arrangement of the pieces on printer plates (model_pack_plates) */
    int nplates;
    double plate_w, plate_d;    /* the printer plate: piece limit plus 2 mm clearance each side */
    /* preview geometry: all chunks placed side by side (or one chunk centred) */
    region_t view_slot_region[MAX_SLOTS];
    region_t view_base_region;
    mesh_t slot_mesh[MAX_SLOTS];
    mesh_t base_mesh;
    double slot_volume[MAX_SLOTS];      /* totals over all chunks */
    double base_volume;
    double bbox_min[3], bbox_max[3];    /* preview bbox */
    double z_logo_bottom;
    int total_tris;
} model_t;

void model_params_default(model_params *p);
void model_init(model_t *m);
void model_free(model_t *m);

/* Stage A: regions and colour slots. Returns 0 on failure. */
int model_layout(model_t *m, const svg_doc *doc, const model_params *p, char *err, size_t errlen);
/* Stage B: meshes from regions.  Requires a successful layout. */
int model_build_meshes(model_t *m, const model_params *p);
/* Only rebuild the preview geometry (piece placement / selected piece). */
int model_build_view(model_t *m, const model_params *p);

/* Colour used for display/export of a slot (override or SVG colour). */
unsigned model_slot_rgb(const model_t *m, const model_params *p, int slot);
unsigned model_base_rgb(const model_t *m, const model_params *p);
/* Whether a slot contributes geometry. */
int model_slot_active(const model_t *m, const model_params *p, int slot);
/* Number of distinct materials currently used (base included). */
int model_material_count(const model_t *m, const model_params *p);
/* Chunk whose placed footprint contains the preview point, or -1. */
int model_chunk_at(const model_t *m, const model_params *p, double x, double y);
/* Effective body slot in layered mode, -1 when not layered. */
int model_body_slot(const model_t *m, const model_params *p);
/* Z range occupied by a colour slot. */
void model_slot_zrange(const model_t *m, const model_params *p, int slot, double *zlo, double *zhi);
/* Size of a chunk's footprint including its base plate (unrotated). */
void model_chunk_size(const model_t *m, int chunk, double *w, double *d);
/* Footprint of a piece as exported: turned and scaled to fit the plate. */
void model_chunk_footprint(const model_t *m, int chunk, double *w, double *d);
/* Arrange the pieces on printer plates (shelf packing in piece order); fills
 * chunk plate / plate_pos and nplates. Called by model_build_view. */
void model_pack_plates(model_t *m, const model_params *p);
/* Angle (degrees) that lets a w x d footprint fit a W x D plate, or -1. */
double model_fit_angle(double w, double d, double W, double D);
/* Copy of a mesh / region rotated (degrees about Z) and scaled in XY. */
void mesh_xform_copy(mesh_t *dst, const mesh_t *src, double deg, double sxy);
void region_xform_copy(region_t *dst, const region_t *src, double deg, double sxy);
/* Largest uniform scale of the geometry (margins stay) at which w x d fits W x D. */
double model_max_fit_scale(double w, double d, double margin, double W, double D);

/* Size of every key (all share one cross-section): width, height as printed
 * (wide face down), and the ledge width the slot's flanks keep. */
void model_key_size(const model_params *p, double *w, double *h);
/* Mesh of key k, centred at the origin, long axis along X, printed wide face
 * down (z = 0).  Returns 0 when there is no such key. */
int model_key_mesh(const model_t *m, const model_params *p, int k, mesh_t *out);
/* Joint test print: two small plates joined by the current joint style, and
 * the key when keys are on.  Meshes are centred on their own footprints;
 * returns the number of meshes (2 or 3, 0 when there is nothing to test). */
int model_build_coupon(const model_params *p, mesh_t *left, mesh_t *right, mesh_t *key, double *gap);

void mesh_init(mesh_t *m);
void mesh_free(mesh_t *m);
unsigned mesh_add_vertex(mesh_t *m, double x, double y, double z);
void mesh_add_tri(mesh_t *m, unsigned a, unsigned b, unsigned c);
double mesh_volume(const mesh_t *m);

#endif
