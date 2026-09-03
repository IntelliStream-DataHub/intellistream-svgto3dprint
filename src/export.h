/* Mesh exporters. */
#ifndef LOGO3D_EXPORT_H
#define LOGO3D_EXPORT_H

#include "model.h"

/* One printable part (base plate or colour) of a chunk. */
typedef struct {
    const mesh_t *src;      /* piece geometry */
    mesh_t rotated;         /* transformed copy when the piece is turned/scaled (nt == 0 otherwise) */
    unsigned rgb;
    char name[64];
    double tx, ty;          /* placement offset (all-pieces exports) */
} export_part;

/* Geometry to write for a part: the transformed copy when there is one. */
static inline const mesh_t *export_part_mesh(const export_part *p)
{
    return p->rotated.nt > 0 ? &p->rotated : p->src;
}

/* Parts of every piece, placed and scaled as in the assembled layout.  Returns a
 * malloc'd array (release with export_release_parts + free) and its count. */
export_part *export_collect_all_pieces(const model_t *m, const model_params *p, int *count);
/* Parts of the pieces arranged on printer plate `plate`, placed on it (the
 * plate's front-left corner is the origin).  Same ownership as above. */
export_part *export_collect_plate_pieces(const model_t *m, const model_params *p, int plate, int *count);

/* Parts of one chunk (chunk >= 0) or of the preview geometry (chunk == -1).
 * Fills up to MAX_SLOTS+1 entries; returns the count.  Release with
 * export_release_parts(). */
int export_collect_parts(const model_t *m, const model_params *p, int chunk, export_part *parts);
void export_release_parts(export_part *parts, int n);

/* Geometry selection of the exporters below: chunk >= 0 is one piece, -1 the
 * preview geometry, -2 every piece in the assembled layout; plate >= 0
 * overrides that with the pieces arranged on that printer plate. */

/* Binary STL with every part in one file. */
int export_stl(const model_t *m, const model_params *p, int chunk, int plate, const char *path, char *err, size_t errlen);
/* One binary STL per part; `path` is a prefix ("logo.stl" -> "logo_color1_ff0000.stl").
 * Returns the number of files written (0 on failure). */
int export_stl_per_color(const model_t *m, const model_params *p, int chunk, int plate, const char *path, char *err, size_t errlen);
/* 3MF: one object per chunk with one part per colour.  Writes PrusaSlicer's
 * part/extruder metadata and 3MF material colours, so PrusaSlicer/SuperSlicer
 * open it as a multi-part object and Bambu Studio/OrcaSlicer offer to map the
 * colours to filaments. */
int export_3mf(const model_t *m, const model_params *p, int chunk, int plate, const char *path, char *err, size_t errlen);

/* High level export.  kind: 0 = STL, 1 = one STL per colour, 2 = 3MF.
 * mode: 0 = everything in one file (assembled layout), 1 = one file per piece
 * ("logo.3mf" -> "logo_chunk01.3mf"), 2 = one file per printer plate with its
 * pieces arranged on it ("logo_plate01.3mf").  Returns the number of files
 * written, 0 on error. */
int export_model(const model_t *m, const model_params *p, int kind, int mode, const char *path, char *err, size_t errlen);

#endif
