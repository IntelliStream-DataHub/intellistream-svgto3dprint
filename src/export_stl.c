#include "export.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int export_collect_parts(const model_t *m, const model_params *p, int chunk, export_part *parts)
{
    int n = 0, i;
    const mesh_t *base, *slots;
    double rot = 0, scale = 1;
    if (!m->meshes_valid) return 0;
    if (chunk >= 0 && chunk < m->nchunks) {
        base = &m->chunks[chunk].base_mesh;
        slots = m->chunks[chunk].slot_mesh;
        rot = m->chunks[chunk].rot;
        scale = m->chunks[chunk].scale;
    } else {
        base = &m->base_mesh;
        slots = m->slot_mesh;
    }
    if (p->base_enabled && p->base_thickness > 0 && base->nt > 0) {
        parts[n].src = base;
        parts[n].rgb = model_base_rgb(m, p);
        snprintf(parts[n].name, sizeof(parts[n].name), "base_%06x", parts[n].rgb);
        n++;
    }
    for (i = 0; i < m->nslots; i++) {
        if (!model_slot_active(m, p, i) || slots[i].nt == 0) continue;
        parts[n].src = &slots[i];
        parts[n].rgb = model_slot_rgb(m, p, i);
        snprintf(parts[n].name, sizeof(parts[n].name), "color%d_%06x", i + 1, parts[n].rgb);
        n++;
    }
    /* pieces that only fit the plate when turned or shrunk are exported that way */
    for (i = 0; i < n; i++) {
        parts[i].tx = parts[i].ty = 0;
        if (rot != 0 || scale != 1) mesh_xform_copy(&parts[i].rotated, parts[i].src, rot, scale);
        else mesh_init(&parts[i].rotated);
    }
    return n;
}

void export_release_parts(export_part *parts, int n)
{
    int i;
    for (i = 0; i < n; i++) mesh_free(&parts[i].rotated);
}

export_part *export_collect_all_pieces(const model_t *m, const model_params *p, int *count)
{
    export_part *all = NULL;
    int n = 0, i, j;
    *count = 0;
    if (!m->meshes_valid) return NULL;
    for (i = 0; i < m->nchunks; i++) {
        export_part parts[MAX_SLOTS + 1];
        int k = export_collect_parts(m, p, i, parts);
        const chunk_t *c = &m->chunks[i];
        all = (export_part *)realloc(all, sizeof(export_part) * (size_t)(n + k));
        for (j = 0; j < k; j++) {
            all[n] = parts[j];
            /* the layout offset scales with the pieces */
            all[n].tx = c->place[0] * c->scale;
            all[n].ty = c->place[1] * c->scale;
            if (m->nchunks > 1) snprintf(all[n].name, sizeof(all[n].name), "%s_%s", c->name, parts[j].name);
            n++;
        }
    }
    *count = n;
    return all;
}

static void put_f32(FILE *f, double v)
{
    float x = (float)v;
    fwrite(&x, 4, 1, f);
}

static void write_tri(FILE *f, const double *a, const double *b, const double *c, double tx, double ty)
{
    double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    double len = sqrt(nx * nx + ny * ny + nz * nz);
    unsigned short attr = 0;
    if (len > 0) { nx /= len; ny /= len; nz /= len; } else { nx = ny = 0; nz = 1; }
    put_f32(f, nx); put_f32(f, ny); put_f32(f, nz);
    put_f32(f, a[0] + tx); put_f32(f, a[1] + ty); put_f32(f, a[2]);
    put_f32(f, b[0] + tx); put_f32(f, b[1] + ty); put_f32(f, b[2]);
    put_f32(f, c[0] + tx); put_f32(f, c[1] + ty); put_f32(f, c[2]);
    fwrite(&attr, 2, 1, f);
}

static int write_stl_file(const char *path, const export_part *parts, int nparts, char *err, size_t errlen)
{
    FILE *f = fopen(path, "wb");
    char header[80];
    unsigned total = 0;
    int i, j;
    if (!f) {
        if (err && errlen) snprintf(err, errlen, "cannot write '%s'", path);
        return 0;
    }
    memset(header, 0, sizeof(header));
    snprintf(header, sizeof(header), "logo3dprint binary STL");
    fwrite(header, 1, 80, f);
    for (i = 0; i < nparts; i++) total += (unsigned)export_part_mesh(&parts[i])->nt;
    {
        unsigned char b[4] = {(unsigned char)(total & 0xFF), (unsigned char)((total >> 8) & 0xFF),
                              (unsigned char)((total >> 16) & 0xFF), (unsigned char)((total >> 24) & 0xFF)};
        fwrite(b, 1, 4, f);
    }
    for (i = 0; i < nparts; i++) {
        const mesh_t *m = export_part_mesh(&parts[i]);
        for (j = 0; j < m->nt; j++)
            write_tri(f, &m->v[3 * m->t[3 * j]], &m->v[3 * m->t[3 * j + 1]], &m->v[3 * m->t[3 * j + 2]], parts[i].tx, parts[i].ty);
    }
    if (ferror(f)) {
        fclose(f);
        if (err && errlen) snprintf(err, errlen, "write error on '%s'", path);
        return 0;
    }
    fclose(f);
    return 1;
}

export_part *export_collect_plate_pieces(const model_t *m, const model_params *p, int plate, int *count)
{
    export_part *all = NULL;
    int n = 0, i, j;
    *count = 0;
    if (!m->meshes_valid) return NULL;
    for (i = 0; i < m->nchunks; i++) {
        export_part parts[MAX_SLOTS + 1];
        const chunk_t *c = &m->chunks[i];
        int k;
        if (c->on_plate != plate) continue;
        k = export_collect_parts(m, p, i, parts);
        all = (export_part *)realloc(all, sizeof(export_part) * (size_t)(n + k));
        for (j = 0; j < k; j++) {
            all[n] = parts[j];
            all[n].tx = c->plate_pos[0];
            all[n].ty = c->plate_pos[1];
            if (m->nchunks > 1) snprintf(all[n].name, sizeof(all[n].name), "%s_%s", c->name, parts[j].name);
            n++;
        }
    }
    *count = n;
    return all;
}

int export_stl(const model_t *m, const model_params *p, int chunk, int plate, const char *path, char *err, size_t errlen)
{
    export_part local[MAX_SLOTS + 1], *parts = local, *all = NULL;
    int n, ok;
    if (err && errlen) err[0] = 0;
    if (plate >= 0) { all = export_collect_plate_pieces(m, p, plate, &n); parts = all; }
    else if (chunk < 0 && m->nchunks > 1) { all = export_collect_all_pieces(m, p, &n); parts = all; }
    else n = export_collect_parts(m, p, chunk, local);
    if (n == 0) {
        free(all);
        if (err && errlen) snprintf(err, errlen, "nothing to export");
        return 0;
    }
    ok = write_stl_file(path, parts, n, err, errlen);
    export_release_parts(parts, n);
    free(all);
    return ok;
}

static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

/* Copy `path` without a known extension into prefix. */
static void strip_ext(char *prefix, size_t cap, const char *path, const char *ext)
{
    size_t len = strlen(path), el = strlen(ext);
    if (len >= cap - 1) len = cap - 1;
    memcpy(prefix, path, len);
    prefix[len] = 0;
    if (len > el && ieq(prefix + len - el, ext)) prefix[len - el] = 0;
}

int export_stl_per_color(const model_t *m, const model_params *p, int chunk, int plate, const char *path, char *err, size_t errlen)
{
    export_part local[MAX_SLOTS + 1], *parts = local, *all = NULL;
    int n, i, files = 0;
    char prefix[1024];
    if (err && errlen) err[0] = 0;
    if (plate >= 0) { all = export_collect_plate_pieces(m, p, plate, &n); parts = all; }
    else if (chunk < 0 && m->nchunks > 1) { all = export_collect_all_pieces(m, p, &n); parts = all; }
    else n = export_collect_parts(m, p, chunk, local);
    if (n == 0) {
        free(all);
        if (err && errlen) snprintf(err, errlen, "nothing to export");
        return 0;
    }
    strip_ext(prefix, sizeof(prefix), path, ".stl");
    if (all) {
        /* one file per colour holding that colour of every piece, placed as assembled */
        unsigned done[MAX_SLOTS + 1];
        int ndone = 0, j, k;
        for (i = 0; i < n; i++) {
            export_part *group = NULL;
            int ng = 0;
            char fn[1200];
            for (k = 0; k < ndone; k++) if (done[k] == parts[i].rgb) break;
            if (k < ndone) continue;
            done[ndone++] = parts[i].rgb;
            for (j = 0; j < n; j++) if (parts[j].rgb == parts[i].rgb) {
                group = (export_part *)realloc(group, sizeof(export_part) * (size_t)(ng + 1));
                group[ng++] = parts[j];
            }
            snprintf(fn, sizeof(fn), "%s_%s.stl", prefix, strchr(parts[i].name, '_') ? strchr(parts[i].name, '_') + 1 : parts[i].name);
            if (!write_stl_file(fn, group, ng, err, errlen)) { free(group); export_release_parts(parts, n); free(all); return 0; }
            free(group);
            files++;
        }
        export_release_parts(parts, n);
        free(all);
        return files;
    }
    for (i = 0; i < n; i++) {
        char fn[1200];
        snprintf(fn, sizeof(fn), "%s_%s.stl", prefix, parts[i].name);
        if (!write_stl_file(fn, &parts[i], 1, err, errlen)) { export_release_parts(parts, n); return 0; }
    }
    export_release_parts(parts, n);
    return n;
}

static int export_one(const model_t *m, const model_params *p, int kind, int chunk, int plate, const char *path, char *err, size_t errlen)
{
    if (kind == 2) return export_3mf(m, p, chunk, plate, path, err, errlen);
    if (kind == 1) return export_stl_per_color(m, p, chunk, plate, path, err, errlen);
    return export_stl(m, p, chunk, plate, path, err, errlen);
}

int export_model(const model_t *m, const model_params *p, int kind, int mode, const char *path, char *err, size_t errlen)
{
    const char *ext = kind == 2 ? ".3mf" : ".stl";
    if (err && errlen) err[0] = 0;
    if (!m->meshes_valid || m->nchunks == 0) {
        if (err && errlen) snprintf(err, errlen, "nothing to export");
        return 0;
    }
    if (m->nchunks > 1 && mode == 1) {
        char prefix[1024];
        int i, files = 0;
        strip_ext(prefix, sizeof(prefix), path, ext);
        for (i = 0; i < m->nchunks; i++) {
            char fn[1200];
            int r;
            snprintf(fn, sizeof(fn), "%s_%s%s", prefix, m->chunks[i].name, ext);
            r = export_one(m, p, kind, i, -1, fn, err, errlen);
            if (!r) return 0;
            files += r;
        }
        return files;
    }
    if (m->nchunks > 1 && mode == 2) {
        char prefix[1024];
        int k, files = 0;
        if (m->nplates <= 1) return export_one(m, p, kind, -2, 0, path, err, errlen);
        strip_ext(prefix, sizeof(prefix), path, ext);
        for (k = 0; k < m->nplates; k++) {
            char fn[1200];
            int r;
            snprintf(fn, sizeof(fn), "%s_plate%02d%s", prefix, k + 1, ext);
            r = export_one(m, p, kind, -2, k, fn, err, errlen);
            if (!r) return 0;
            files += r;
        }
        return files;
    }
    return export_one(m, p, kind, m->nchunks > 1 ? (kind == 2 ? -2 : -1) : 0, -1, path, err, errlen);
}
