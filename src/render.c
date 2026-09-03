#include "render.h"
#include "glapi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Matrices (column-major float[16])                                   */

static void mat_identity(float *m)
{
    memset(m, 0, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat_mul(float *out, const float *a, const float *b)
{
    float r[16];
    int i, j, k;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            float s = 0;
            for (k = 0; k < 4; k++) s += a[k * 4 + i] * b[j * 4 + k];
            r[j * 4 + i] = s;
        }
    memcpy(out, r, sizeof(r));
}

static void mat_perspective(float *m, float fovy_deg, float aspect, float znear, float zfar)
{
    float f = 1.0f / tanf(fovy_deg * (float)M_PI / 360.0f);
    memset(m, 0, sizeof(float) * 16);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zfar + znear) / (znear - zfar);
    m[11] = -1.0f;
    m[14] = (2.0f * zfar * znear) / (znear - zfar);
}

static void mat_ortho(float *m, float l, float r, float b, float t, float n, float f)
{
    memset(m, 0, sizeof(float) * 16);
    m[0] = 2.0f / (r - l);
    m[5] = 2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
    m[15] = 1.0f;
}

static void v3_cross(float *o, const float *a, const float *b)
{
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

static void v3_norm(float *v)
{
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0) { v[0] /= l; v[1] /= l; v[2] /= l; }
}

static void mat_lookat(float *m, const float *eye, const float *center, const float *up)
{
    float f[3], s[3], u[3];
    f[0] = center[0] - eye[0]; f[1] = center[1] - eye[1]; f[2] = center[2] - eye[2];
    v3_norm(f);
    v3_cross(s, f, up);
    v3_norm(s);
    v3_cross(u, s, f);
    mat_identity(m);
    m[0] = s[0]; m[4] = s[1]; m[8] = s[2];
    m[1] = u[0]; m[5] = u[1]; m[9] = u[2];
    m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2];
    m[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
    m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[14] = (f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
}

static void mat_transform(const float *m, const float *p, float *out)
{
    int i;
    for (i = 0; i < 4; i++) out[i] = m[i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i] * p[3];
}

static int mat_inverse(const float *m, float *out)
{
    double inv[16], det;
    int i;
    inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (fabs(det) < 1e-20) return 0;
    det = 1.0 / det;
    for (i = 0; i < 16; i++) out[i] = (float)(inv[i] * det);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Camera                                                              */

static void camera_eye(const camera_t *cam, float *eye)
{
    float cp = cosf(cam->pitch);
    eye[0] = cam->target[0] + cam->dist * cp * cosf(cam->yaw);
    eye[1] = cam->target[1] + cam->dist * cp * sinf(cam->yaw);
    eye[2] = cam->target[2] + cam->dist * sinf(cam->pitch);
}

void camera_matrices(const camera_t *cam, int vw, int vh, float *view, float *proj)
{
    float eye[3], up[3] = {0, 0, 1};
    float aspect = vh > 0 ? (float)vw / (float)vh : 1.0f;
    float znear = cam->dist * 0.01f, zfar = cam->dist * 30.0f + 10.0f;
    camera_eye(cam, eye);
    if (znear < 0.05f) znear = 0.05f;
    mat_lookat(view, eye, cam->target, up);
    if (cam->ortho) {
        float h = cam->dist * tanf(cam->fov * (float)M_PI / 360.0f);
        mat_ortho(proj, -h * aspect, h * aspect, -h, h, -zfar, zfar);
    } else {
        mat_perspective(proj, cam->fov, aspect, znear, zfar);
    }
}

int camera_project(const camera_t *cam, int vw, int vh, const double *p, float *sx, float *sy)
{
    float view[16], proj[16], vp[16], in[4], out[4];
    camera_matrices(cam, vw, vh, view, proj);
    mat_mul(vp, proj, view);
    in[0] = (float)p[0]; in[1] = (float)p[1]; in[2] = (float)p[2]; in[3] = 1.0f;
    mat_transform(vp, in, out);
    if (out[3] <= 1e-6f) return 0;
    *sx = (out[0] / out[3] * 0.5f + 0.5f) * (float)vw;
    *sy = (1.0f - (out[1] / out[3] * 0.5f + 0.5f)) * (float)vh;
    return 1;
}

void camera_screen_dir(const camera_t *cam, const float *dir, float *dx, float *dy, float *depth)
{
    float view[16], proj[16], in[4], out[4];
    camera_matrices(cam, 100, 100, view, proj);
    in[0] = dir[0]; in[1] = dir[1]; in[2] = dir[2]; in[3] = 0.0f;
    mat_transform(view, in, out);
    *dx = out[0];
    *dy = -out[1];
    *depth = -out[2];
}

void camera_ray(const camera_t *cam, int vw, int vh, float sx, float sy, double *origin, double *dir)
{
    float view[16], proj[16], vp[16], inv[16], a[4], b[4], pa[4], pb[4];
    int i;
    camera_matrices(cam, vw, vh, view, proj);
    mat_mul(vp, proj, view);
    if (!mat_inverse(vp, inv)) { mat_identity(inv); }
    a[0] = b[0] = (sx / (float)vw) * 2.0f - 1.0f;
    a[1] = b[1] = 1.0f - (sy / (float)vh) * 2.0f;
    a[2] = -1.0f; b[2] = 1.0f;
    a[3] = b[3] = 1.0f;
    mat_transform(inv, a, pa);
    mat_transform(inv, b, pb);
    for (i = 0; i < 3; i++) { pa[i] /= pa[3]; pb[i] /= pb[3]; }
    for (i = 0; i < 3; i++) { origin[i] = pa[i]; dir[i] = pb[i] - pa[i]; }
    {
        double l = sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
        if (l > 0) for (i = 0; i < 3; i++) dir[i] /= l;
    }
}

void camera_fit_bbox(camera_t *cam, const double *mn, const double *mx)
{
    float r;
    int i;
    for (i = 0; i < 3; i++) cam->target[i] = (float)((mn[i] + mx[i]) / 2);
    r = 0.5f * (float)sqrt((mx[0] - mn[0]) * (mx[0] - mn[0]) + (mx[1] - mn[1]) * (mx[1] - mn[1]) + (mx[2] - mn[2]) * (mx[2] - mn[2]));
    if (r < 1) r = 1;
    cam->dist = r / sinf(cam->fov * (float)M_PI / 360.0f) * 1.25f;
}

void camera_fit(camera_t *cam, const model_t *m)
{
    if (!m || !m->meshes_valid || m->bbox_max[0] <= m->bbox_min[0]) {
        cam->target[0] = cam->target[1] = 0; cam->target[2] = 0;
        cam->dist = 150;
        return;
    }
    camera_fit_bbox(cam, m->bbox_min, m->bbox_max);
}

void camera_preset(camera_t *cam, int preset)
{
    float d2r = (float)M_PI / 180.0f;
    switch (preset) {
    case 1: cam->yaw = -90 * d2r; cam->pitch = 89.5f * d2r; break;     /* top */
    case 2: cam->yaw = -90 * d2r; cam->pitch = 0; break;               /* front */
    case 3: cam->yaw = 0; cam->pitch = 0; break;                       /* right */
    case 4: cam->yaw = 180 * d2r; cam->pitch = 0; break;               /* left */
    case 5: cam->yaw = 90 * d2r; cam->pitch = 0; break;                /* back */
    case 6: cam->yaw = -90 * d2r; cam->pitch = -89.5f * d2r; break;    /* bottom */
    default: cam->yaw = -60 * d2r; cam->pitch = 32 * d2r; break;       /* iso */
    }
}

void camera_orbit(camera_t *cam, float dx, float dy)
{
    float lim = 89.5f * (float)M_PI / 180.0f;
    cam->yaw -= dx * 0.008f;
    cam->pitch += dy * 0.008f;
    if (cam->pitch > lim) cam->pitch = lim;
    if (cam->pitch < -lim) cam->pitch = -lim;
}

void camera_pan(camera_t *cam, float dx, float dy, int vh)
{
    float eye[3], f[3], s[3], u[3], up[3] = {0, 0, 1};
    float k = 2.0f * cam->dist * tanf(cam->fov * (float)M_PI / 360.0f) / (float)(vh > 0 ? vh : 1);
    camera_eye(cam, eye);
    f[0] = cam->target[0] - eye[0]; f[1] = cam->target[1] - eye[1]; f[2] = cam->target[2] - eye[2];
    v3_norm(f);
    v3_cross(s, f, up);
    v3_norm(s);
    v3_cross(u, s, f);
    cam->target[0] += (-dx * s[0] + dy * u[0]) * k;
    cam->target[1] += (-dx * s[1] + dy * u[1]) * k;
    cam->target[2] += (-dx * s[2] + dy * u[2]) * k;
}

void camera_zoom(camera_t *cam, float steps)
{
    cam->dist *= powf(0.88f, steps);
    if (cam->dist < 1.0f) cam->dist = 1.0f;
    if (cam->dist > 20000.0f) cam->dist = 20000.0f;
}

/* ------------------------------------------------------------------ */
/* Picking                                                             */

static int ray_tri(const double *o, const double *d, const double *a, const double *b, const double *c, double *t)
{
    double e1[3], e2[3], p[3], q[3], tv[3], det, inv, u, v;
    int i;
    for (i = 0; i < 3; i++) { e1[i] = b[i] - a[i]; e2[i] = c[i] - a[i]; }
    p[0] = d[1] * e2[2] - d[2] * e2[1]; p[1] = d[2] * e2[0] - d[0] * e2[2]; p[2] = d[0] * e2[1] - d[1] * e2[0];
    det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (fabs(det) < 1e-12) return 0;
    inv = 1.0 / det;
    for (i = 0; i < 3; i++) tv[i] = o[i] - a[i];
    u = (tv[0] * p[0] + tv[1] * p[1] + tv[2] * p[2]) * inv;
    if (u < 0 || u > 1) return 0;
    q[0] = tv[1] * e1[2] - tv[2] * e1[1]; q[1] = tv[2] * e1[0] - tv[0] * e1[2]; q[2] = tv[0] * e1[1] - tv[1] * e1[0];
    v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
    if (v < 0 || u + v > 1) return 0;
    *t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
    return *t > 1e-9;
}

static int pick_mesh(const mesh_t *m, const double *o, const double *d, double *best_t)
{
    int i, hit = 0;
    for (i = 0; i < m->nt; i++) {
        double t;
        if (ray_tri(o, d, &m->v[3 * m->t[3 * i]], &m->v[3 * m->t[3 * i + 1]], &m->v[3 * m->t[3 * i + 2]], &t) && t < *best_t) {
            *best_t = t;
            hit = 1;
        }
    }
    return hit;
}

int model_pick(const model_t *m, const model_params *p, const double *origin, const double *dir,
               double *hit, int *slot, double *t_out)
{
    double best = DBL_MAX;
    int i, found = 0;
    if (!m->meshes_valid) return 0;
    if (p->base_enabled && p->base_thickness > 0 && pick_mesh(&m->base_mesh, origin, dir, &best)) { found = 1; *slot = -1; }
    for (i = 0; i < m->nslots; i++) {
        if (!model_slot_active(m, p, i)) continue;
        if (pick_mesh(&m->slot_mesh[i], origin, dir, &best)) { found = 1; *slot = i; }
    }
    if (!found) return 0;
    for (i = 0; i < 3; i++) hit[i] = origin[i] + dir[i] * best;
    if (t_out) *t_out = best;
    return 1;
}

/* ------------------------------------------------------------------ */
/* GL resources                                                        */

typedef struct {
    GLuint vao, vbo;
    int nverts;
    GLuint line_vao, line_vbo;
    int nlines;             /* number of line vertices */
    float color[3];
    int active;
} part_gl;

typedef struct {
    part_gl parts[MAX_SLOTS + 1];
    double bbox_min[3], bbox_max[3];
} chunk_gl;

struct render_s {
    GLuint mesh_prog, line_prog;
    GLint m_mvp, m_mv, m_color, m_highlight;
    GLint l_mvp, l_color;
    part_gl parts[MAX_SLOTS + 1];   /* [0] = base, [1..] = slots */
    GLuint tmp_vao, tmp_vbo;        /* streaming line buffer */
    double bbox_min[3], bbox_max[3];
    int has_model;
    chunk_gl *chunks;               /* per-piece geometry for the grid view */
    int nchunks;
};

static void part_gl_init(part_gl *pg)
{
    memset(pg, 0, sizeof(*pg));
    glGenVertexArrays(1, &pg->vao);
    glGenBuffers(1, &pg->vbo);
    glBindVertexArray(pg->vao);
    glBindBuffer(GL_ARRAY_BUFFER, pg->vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glGenVertexArrays(1, &pg->line_vao);
    glGenBuffers(1, &pg->line_vbo);
    glBindVertexArray(pg->line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, pg->line_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
}

static void part_gl_free(part_gl *pg)
{
    glDeleteBuffers(1, &pg->vbo);
    glDeleteVertexArrays(1, &pg->vao);
    glDeleteBuffers(1, &pg->line_vbo);
    glDeleteVertexArrays(1, &pg->line_vao);
}

static void free_chunks(render_t *r)
{
    int i, k;
    for (i = 0; i < r->nchunks; i++)
        for (k = 0; k < MAX_SLOTS + 1; k++) part_gl_free(&r->chunks[i].parts[k]);
    free(r->chunks);
    r->chunks = NULL;
    r->nchunks = 0;
}

static const char *mesh_vs =
    "#version 150\n"
    "uniform mat4 MVP;\n"
    "uniform mat4 MV;\n"
    "in vec3 Position;\n"
    "in vec3 Normal;\n"
    "out vec3 vNormal;\n"
    "out vec3 vPos;\n"
    "void main() {\n"
    "  vNormal = mat3(MV) * Normal;\n"
    "  vPos = (MV * vec4(Position, 1.0)).xyz;\n"
    "  gl_Position = MVP * vec4(Position, 1.0);\n"
    "}\n";

static const char *mesh_fs =
    "#version 150\n"
    "uniform vec3 Color;\n"
    "uniform float Highlight;\n"
    "in vec3 vNormal;\n"
    "in vec3 vPos;\n"
    "out vec4 Out;\n"
    "void main() {\n"
    "  vec3 n = normalize(vNormal);\n"
    "  if (!gl_FrontFacing) n = -n;\n"
    "  vec3 l1 = normalize(vec3(0.4, 0.6, 1.0));\n"
    "  vec3 l2 = normalize(vec3(-0.7, -0.3, 0.4));\n"
    "  float d1 = max(dot(n, l1), 0.0);\n"
    "  float d2 = max(dot(n, l2), 0.0);\n"
    "  vec3 v = normalize(-vPos);\n"
    "  vec3 h = normalize(l1 + v);\n"
    "  float spec = pow(max(dot(n, h), 0.0), 40.0) * 0.18;\n"
    "  float diff = 0.34 + 0.55 * d1 + 0.22 * d2;\n"
    "  vec3 c = Color * diff + vec3(spec);\n"
    "  c = mix(c, vec3(1.0, 0.95, 0.6), Highlight * 0.35);\n"
    "  Out = vec4(c, 1.0);\n"
    "}\n";

static const char *line_vs =
    "#version 150\n"
    "uniform mat4 MVP;\n"
    "in vec3 Position;\n"
    "void main() { gl_Position = MVP * vec4(Position, 1.0); }\n";

static const char *line_fs =
    "#version 150\n"
    "uniform vec4 Color;\n"
    "out vec4 Out;\n"
    "void main() { Out = Color; }\n";

static GLuint compile(GLenum type, const char *src, char *err, size_t errlen)
{
    GLuint s = glCreateShader(type);
    GLint ok;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        if (err && errlen) snprintf(err, errlen, "shader: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link(const char *vs, const char *fs, char *err, size_t errlen)
{
    GLuint v = compile(GL_VERTEX_SHADER, vs, err, errlen), f, p;
    GLint ok;
    if (!v) return 0;
    f = compile(GL_FRAGMENT_SHADER, fs, err, errlen);
    if (!f) { glDeleteShader(v); return 0; }
    p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        if (err && errlen) snprintf(err, errlen, "program: %s", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

render_t *render_create(char *err, size_t errlen)
{
    render_t *r = (render_t *)calloc(1, sizeof(render_t));
    int i;
    if (err && errlen) err[0] = 0;
    r->mesh_prog = link(mesh_vs, mesh_fs, err, errlen);
    if (!r->mesh_prog) { free(r); return NULL; }
    r->line_prog = link(line_vs, line_fs, err, errlen);
    if (!r->line_prog) { glDeleteProgram(r->mesh_prog); free(r); return NULL; }
    r->m_mvp = glGetUniformLocation(r->mesh_prog, "MVP");
    r->m_mv = glGetUniformLocation(r->mesh_prog, "MV");
    r->m_color = glGetUniformLocation(r->mesh_prog, "Color");
    r->m_highlight = glGetUniformLocation(r->mesh_prog, "Highlight");
    r->l_mvp = glGetUniformLocation(r->line_prog, "MVP");
    r->l_color = glGetUniformLocation(r->line_prog, "Color");
    for (i = 0; i < MAX_SLOTS + 1; i++) part_gl_init(&r->parts[i]);
    glGenVertexArrays(1, &r->tmp_vao);
    glGenBuffers(1, &r->tmp_vbo);
    glBindVertexArray(r->tmp_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->tmp_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glBindVertexArray(0);
    return r;
}

void render_destroy(render_t *r)
{
    int i;
    if (!r) return;
    for (i = 0; i < MAX_SLOTS + 1; i++) part_gl_free(&r->parts[i]);
    free_chunks(r);
    glDeleteBuffers(1, &r->tmp_vbo);
    glDeleteVertexArrays(1, &r->tmp_vao);
    glDeleteProgram(r->mesh_prog);
    glDeleteProgram(r->line_prog);
    free(r);
}

static void rgb_to_f(unsigned rgb, float *c)
{
    c[0] = ((rgb >> 16) & 255) / 255.0f;
    c[1] = ((rgb >> 8) & 255) / 255.0f;
    c[2] = (rgb & 255) / 255.0f;
}

static void upload_part(part_gl *pg, const mesh_t *m, const region_t *region, double z0, double z1)
{
    float *buf;
    int i, j, n = 0;
    /* flat-shaded expanded triangles */
    buf = (float *)malloc(sizeof(float) * 18 * (size_t)(m->nt > 0 ? m->nt : 1));
    for (i = 0; i < m->nt; i++) {
        const double *a = &m->v[3 * m->t[3 * i]], *b = &m->v[3 * m->t[3 * i + 1]], *c = &m->v[3 * m->t[3 * i + 2]];
        double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
        double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
        double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        double l = sqrt(nx * nx + ny * ny + nz * nz);
        const double *pts[3];
        if (l > 0) { nx /= l; ny /= l; nz /= l; }
        pts[0] = a; pts[1] = b; pts[2] = c;
        for (j = 0; j < 3; j++) {
            buf[n++] = (float)pts[j][0]; buf[n++] = (float)pts[j][1]; buf[n++] = (float)pts[j][2];
            buf[n++] = (float)nx; buf[n++] = (float)ny; buf[n++] = (float)nz;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, pg->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(float) * (size_t)n), buf, GL_STATIC_DRAW);
    pg->nverts = m->nt * 3;
    free(buf);
    /* outline: contour edges at both heights */
    {
        int total = 0, k = 0;
        for (i = 0; i < region->n; i++) total += region->c[i].n;
        buf = (float *)malloc(sizeof(float) * 12 * (size_t)(total > 0 ? total : 1));
        for (i = 0; i < region->n; i++) {
            const contour_t *c = &region->c[i];
            for (j = 0; j < c->n; j++) {
                int nx = (j + 1) % c->n;
                buf[k++] = (float)c->pts[2 * j]; buf[k++] = (float)c->pts[2 * j + 1]; buf[k++] = (float)z0;
                buf[k++] = (float)c->pts[2 * nx]; buf[k++] = (float)c->pts[2 * nx + 1]; buf[k++] = (float)z0;
                buf[k++] = (float)c->pts[2 * j]; buf[k++] = (float)c->pts[2 * j + 1]; buf[k++] = (float)z1;
                buf[k++] = (float)c->pts[2 * nx]; buf[k++] = (float)c->pts[2 * nx + 1]; buf[k++] = (float)z1;
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, pg->line_vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(float) * (size_t)k), buf, GL_STATIC_DRAW);
        pg->nlines = k / 3;
        free(buf);
    }
}

void render_set_colors(render_t *r, const model_t *m, const model_params *p)
{
    int i, c;
    rgb_to_f(model_base_rgb(m, p), r->parts[0].color);
    for (i = 0; i < m->nslots; i++) rgb_to_f(model_slot_rgb(m, p, i), r->parts[i + 1].color);
    for (c = 0; c < r->nchunks; c++) {
        rgb_to_f(model_base_rgb(m, p), r->chunks[c].parts[0].color);
        for (i = 0; i < m->nslots; i++) rgb_to_f(model_slot_rgb(m, p, i), r->chunks[c].parts[i + 1].color);
    }
}

void render_set_model(render_t *r, const model_t *m, const model_params *p)
{
    int i;
    for (i = 0; i < MAX_SLOTS + 1; i++) r->parts[i].active = 0;
    r->has_model = m && m->meshes_valid;
    if (!r->has_model) return;
    if (p->base_enabled && p->base_thickness > 0 && m->base_mesh.nt > 0) {
        upload_part(&r->parts[0], &m->base_mesh, &m->view_base_region, 0, p->base_thickness);
        r->parts[0].active = 1;
    }
    for (i = 0; i < m->nslots && i < MAX_SLOTS; i++) {
        double zlo, zhi;
        if (!model_slot_active(m, p, i) || m->slot_mesh[i].nt == 0) continue;
        model_slot_zrange(m, p, i, &zlo, &zhi);
        upload_part(&r->parts[i + 1], &m->slot_mesh[i], &m->view_slot_region[i], zlo, zhi);
        r->parts[i + 1].active = 1;
    }
    memcpy(r->bbox_min, m->bbox_min, sizeof(r->bbox_min));
    memcpy(r->bbox_max, m->bbox_max, sizeof(r->bbox_max));
    /* per-piece geometry (turned as exported) for the grid view */
    free_chunks(r);
    if (m->nchunks > 1) {
        int c, k;
        r->chunks = (chunk_gl *)calloc((size_t)m->nchunks, sizeof(chunk_gl));
        r->nchunks = m->nchunks;
        for (c = 0; c < m->nchunks; c++) {
            const chunk_t *ch = &m->chunks[c];
            chunk_gl *cg = &r->chunks[c];
            for (k = 0; k < MAX_SLOTS + 1; k++) part_gl_init(&cg->parts[k]);
            for (k = 0; k < 3; k++) { cg->bbox_min[k] = 1e30; cg->bbox_max[k] = -1e30; }
            for (k = -1; k < m->nslots; k++) {
                const mesh_t *src = k < 0 ? &ch->base_mesh : &ch->slot_mesh[k];
                const region_t *reg = k < 0 ? &ch->base_region : ((k == model_body_slot(m, p)) ? &ch->body_region : &ch->slot_region[k]);
                mesh_t rm;
                region_t rr;
                int q;
                double zlo = 0, zhi = p->base_thickness;
                if (k < 0 && !(p->base_enabled && p->base_thickness > 0)) continue;
                if (k >= 0 && !model_slot_active(m, p, k)) continue;
                if (src->nt == 0) continue;
                if (k >= 0) model_slot_zrange(m, p, k, &zlo, &zhi);
                mesh_xform_copy(&rm, src, ch->rot, ch->scale);
                region_xform_copy(&rr, reg, ch->rot, ch->scale);
                upload_part(&cg->parts[k + 1], &rm, &rr, zlo, zhi);
                cg->parts[k + 1].active = 1;
                for (q = 0; q < rm.nv; q++) {
                    int a;
                    for (a = 0; a < 3; a++) {
                        if (rm.v[3 * q + a] < cg->bbox_min[a]) cg->bbox_min[a] = rm.v[3 * q + a];
                        if (rm.v[3 * q + a] > cg->bbox_max[a]) cg->bbox_max[a] = rm.v[3 * q + a];
                    }
                }
                mesh_free(&rm);
                region_free(&rr);
            }
            if (cg->bbox_min[0] > cg->bbox_max[0]) { for (k = 0; k < 3; k++) { cg->bbox_min[k] = 0; cg->bbox_max[k] = 0; } }
        }
    }
    render_set_colors(r, m, p);
    glBindVertexArray(0);
}

int render_chunk_bbox(const render_t *r, int chunk, double *mn, double *mx)
{
    if (chunk < 0 || chunk >= r->nchunks) return 0;
    memcpy(mn, r->chunks[chunk].bbox_min, sizeof(double) * 3);
    memcpy(mx, r->chunks[chunk].bbox_max, sizeof(double) * 3);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */

typedef struct {
    float *v;
    int n, cap;
} linebuf;

static void lb_push(linebuf *b, float x, float y, float z)
{
    if (b->n + 3 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 3072;
        b->v = (float *)realloc(b->v, sizeof(float) * (size_t)b->cap);
    }
    b->v[b->n++] = x; b->v[b->n++] = y; b->v[b->n++] = z;
}

static void lb_seg(linebuf *b, float x0, float y0, float z0, float x1, float y1, float z1)
{
    lb_push(b, x0, y0, z0);
    lb_push(b, x1, y1, z1);
}

static void draw_lines(render_t *r, const linebuf *b, const float *mvp, float cr, float cg, float cb, float ca)
{
    if (b->n == 0) return;
    glUseProgram(r->line_prog);
    glUniformMatrix4fv(r->l_mvp, 1, GL_FALSE, mvp);
    glUniform4f(r->l_color, cr, cg, cb, ca);
    glBindVertexArray(r->tmp_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->tmp_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(float) * (size_t)b->n), b->v, GL_STREAM_DRAW);
    glDrawArrays(GL_LINES, 0, b->n / 3);
}

static void draw_scene(render_t *r, part_gl *parts, const double *bmin, const double *bmax, int has_model,
                       const camera_t *cam, const view_opts *vo, int vx, int vy, int vw, int vh)
{
    float view[16], proj[16], mvp[16];
    linebuf lb;
    int i;
    if (vw <= 0 || vh <= 0) return;
    memset(&lb, 0, sizeof(lb));
    camera_matrices(cam, vw, vh, view, proj);
    mat_mul(mvp, proj, view);

    glViewport(vx, vy, vw, vh);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vx, vy, vw, vh);
    glClearColor(vo->bg[0], vo->bg[1], vo->bg[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* grid and bed on z = -0.02 so the base plate bottom does not z-fight */
    if (vo->show_grid || vo->show_bed) {
        float half_w, half_d, step = vo->grid_step > 0.1f ? vo->grid_step : 10.0f;
        float gz = -0.02f;
        if (vo->show_bed) { half_w = vo->bed_w / 2; half_d = vo->bed_d / 2; }
        else {
            float ex = 0, ey = 0;
            if (has_model) {
                ex = (float)(bmax[0] > -bmin[0] ? bmax[0] : -bmin[0]);
                ey = (float)(bmax[1] > -bmin[1] ? bmax[1] : -bmin[1]);
            }
            half_w = ceilf((ex * 1.4f + step) / step) * step;
            half_d = ceilf((ey * 1.4f + step) / step) * step;
            if (half_w < step * 3) half_w = step * 3;
            if (half_d < step * 3) half_d = step * 3;
        }
        if (vo->show_grid) {
            int nx = (int)(half_w / step), ny = (int)(half_d / step);
            linebuf minor, major;
            memset(&minor, 0, sizeof(minor));
            memset(&major, 0, sizeof(major));
            for (i = -nx; i <= nx; i++) {
                float x = i * step;
                lb_seg((i % 5 == 0) ? &major : &minor, x, -half_d, gz, x, half_d, gz);
            }
            for (i = -ny; i <= ny; i++) {
                float y = i * step;
                lb_seg((i % 5 == 0) ? &major : &minor, -half_w, y, gz, half_w, y, gz);
            }
            draw_lines(r, &minor, mvp, 0.55f, 0.58f, 0.62f, 0.35f);
            draw_lines(r, &major, mvp, 0.45f, 0.48f, 0.55f, 0.6f);
            free(minor.v);
            free(major.v);
        }
        if (vo->show_bed) {
            lb.n = 0;
            lb_seg(&lb, -half_w, -half_d, gz, half_w, -half_d, gz);
            lb_seg(&lb, half_w, -half_d, gz, half_w, half_d, gz);
            lb_seg(&lb, half_w, half_d, gz, -half_w, half_d, gz);
            lb_seg(&lb, -half_w, half_d, gz, -half_w, -half_d, gz);
            draw_lines(r, &lb, mvp, 0.25f, 0.5f, 0.85f, 0.9f);
        }
        /* axes at the origin */
        lb.n = 0;
        lb_seg(&lb, 0, 0, gz, step * 2, 0, gz);
        draw_lines(r, &lb, mvp, 0.85f, 0.25f, 0.25f, 1.0f);
        lb.n = 0;
        lb_seg(&lb, 0, 0, gz, 0, step * 2, gz);
        draw_lines(r, &lb, mvp, 0.25f, 0.7f, 0.25f, 1.0f);
    }

    /* meshes */
    if (has_model) {
        glUseProgram(r->mesh_prog);
        glUniformMatrix4fv(r->m_mvp, 1, GL_FALSE, mvp);
        glUniformMatrix4fv(r->m_mv, 1, GL_FALSE, view);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        for (i = 0; i < MAX_SLOTS + 1; i++) {
            part_gl *pg = &parts[i];
            float hl;
            if (!pg->active || pg->nverts == 0) continue;
            hl = (vo->highlight_slot == i - 1) ? 1.0f : 0.0f;
            glUniform3f(r->m_color, pg->color[0], pg->color[1], pg->color[2]);
            glUniform1f(r->m_highlight, hl);
            glBindVertexArray(pg->vao);
            glDrawArrays(GL_TRIANGLES, 0, pg->nverts);
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
        if (vo->show_outline) {
            glUseProgram(r->line_prog);
            glUniformMatrix4fv(r->l_mvp, 1, GL_FALSE, mvp);
            glUniform4f(r->l_color, 0.08f, 0.08f, 0.1f, 0.55f);
            for (i = 0; i < MAX_SLOTS + 1; i++) {
                part_gl *pg = &parts[i];
                if (!pg->active || pg->nlines == 0) continue;
                glBindVertexArray(pg->line_vao);
                glDrawArrays(GL_LINES, 0, pg->nlines);
            }
        }
        if (vo->show_bbox) {
            float x0 = (float)bmin[0], y0 = (float)bmin[1], z0 = (float)bmin[2];
            float x1 = (float)bmax[0], y1 = (float)bmax[1], z1 = (float)bmax[2];
            lb.n = 0;
            lb_seg(&lb, x0, y0, z0, x1, y0, z0); lb_seg(&lb, x1, y0, z0, x1, y1, z0);
            lb_seg(&lb, x1, y1, z0, x0, y1, z0); lb_seg(&lb, x0, y1, z0, x0, y0, z0);
            lb_seg(&lb, x0, y0, z1, x1, y0, z1); lb_seg(&lb, x1, y0, z1, x1, y1, z1);
            lb_seg(&lb, x1, y1, z1, x0, y1, z1); lb_seg(&lb, x0, y1, z1, x0, y0, z1);
            lb_seg(&lb, x0, y0, z0, x0, y0, z1); lb_seg(&lb, x1, y0, z0, x1, y0, z1);
            lb_seg(&lb, x1, y1, z0, x1, y1, z1); lb_seg(&lb, x0, y1, z0, x0, y1, z1);
            glDisable(GL_DEPTH_TEST);
            draw_lines(r, &lb, mvp, 0.95f, 0.55f, 0.1f, 0.9f);
            glEnable(GL_DEPTH_TEST);
        }
    }
    free(lb.v);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_SCISSOR_TEST);
}

void render_draw(render_t *r, const camera_t *cam, const view_opts *vo, int vx, int vy, int vw, int vh)
{
    draw_scene(r, r->parts, r->bbox_min, r->bbox_max, r->has_model, cam, vo, vx, vy, vw, vh);
}

void render_draw_chunk(render_t *r, int chunk, const camera_t *cam, const view_opts *vo, int vx, int vy, int vw, int vh)
{
    if (chunk < 0 || chunk >= r->nchunks) return;
    draw_scene(r, r->chunks[chunk].parts, r->chunks[chunk].bbox_min, r->chunks[chunk].bbox_max, 1, cam, vo, vx, vy, vw, vh);
}
