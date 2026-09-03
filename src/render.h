/* OpenGL rendering of the model preview plus camera and picking helpers. */
#ifndef LOGO3D_RENDER_H
#define LOGO3D_RENDER_H

#include "model.h"

typedef struct {
    float yaw, pitch;       /* radians; yaw around Z, pitch above the XY plane */
    float dist;             /* distance from target */
    float target[3];
    float fov;              /* vertical field of view in degrees */
    int ortho;
} camera_t;

typedef struct {
    int show_grid;
    int show_bed;
    int show_bbox;
    int show_outline;
    float grid_step;        /* mm */
    float bed_w, bed_d;     /* printer bed size in mm */
    int highlight_slot;     /* -2 none, -1 base, 0.. slot */
    float bg[3];
} view_opts;

typedef struct render_s render_t;

render_t *render_create(char *err, size_t errlen);
void render_destroy(render_t *r);

/* Upload the model geometry (call after every rebuild). */
void render_set_model(render_t *r, const model_t *m, const model_params *p);
/* Only refresh part colours. */
void render_set_colors(render_t *r, const model_t *m, const model_params *p);

/* Draw the scene into a viewport given in framebuffer pixels (GL convention:
 * vy measured from the bottom). */
void render_draw(render_t *r, const camera_t *cam, const view_opts *vo, int vx, int vy, int vw, int vh);
/* Draw one piece (centred, turned as exported) into a viewport. */
void render_draw_chunk(render_t *r, int chunk, const camera_t *cam, const view_opts *vo, int vx, int vy, int vw, int vh);
/* Bounding box of an uploaded piece; returns 0 if none. */
int render_chunk_bbox(const render_t *r, int chunk, double *mn, double *mx);
void camera_fit_bbox(camera_t *cam, const double *mn, const double *mx);

/* Compute the view-projection for a viewport; used by projection helpers. */
void camera_matrices(const camera_t *cam, int vw, int vh, float *view, float *proj);
/* Project a world point to viewport pixel coordinates (origin top-left of the
 * viewport). Returns 0 when behind the camera. */
int camera_project(const camera_t *cam, int vw, int vh, const double *p, float *sx, float *sy);
/* Screen-space direction (unit, y down) of a world direction; for the axis triad. */
void camera_screen_dir(const camera_t *cam, const float *dir, float *dx, float *dy, float *depth);
/* World ray through viewport pixel (origin top-left). */
void camera_ray(const camera_t *cam, int vw, int vh, float sx, float sy, double *origin, double *dir);
void camera_fit(camera_t *cam, const model_t *m);
/* Presets: 0 iso, 1 top, 2 front, 3 right, 4 left, 5 back, 6 bottom */
void camera_preset(camera_t *cam, int preset);
void camera_orbit(camera_t *cam, float dx, float dy);
void camera_pan(camera_t *cam, float dx, float dy, int vh);
void camera_zoom(camera_t *cam, float steps);

/* Nearest intersection of a ray with the model; slot -1 = base plate.
 * Returns 0 when nothing is hit. */
int model_pick(const model_t *m, const model_params *p, const double *origin, const double *dir,
               double *hit, int *slot, double *t_out);

#endif
