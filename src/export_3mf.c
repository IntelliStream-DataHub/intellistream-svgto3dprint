/* 3MF writer.
 *
 * Layout: one mesh object per chunk containing every part (base plate first,
 * then one shell per colour).  Colours are stored three ways so the common
 * slicers pick them up:
 *  - core "basematerials" (spec compliant, used by Windows 3D Builder etc.),
 *  - a materials-extension colour group referenced by every triangle, which
 *    Bambu Studio / OrcaSlicer read from third-party files and offer to map
 *    to filaments,
 *  - Metadata/Slic3r_PE_model.config with the triangle range and extruder of
 *    every part, which PrusaSlicer / SuperSlicer use to create a multi-part
 *    object with one extruder per part.
 *
 * Several pieces in one file keep their assembled layout, or (plate >= 0)
 * the pieces of one printer plate are placed on it. (Build plates cannot be
 * assigned from here: Orca Slicer and Bambu Studio create plates only from
 * their own project files, which carry the user's printer settings, and load
 * any other 3MF as geometry on one plate; their Arrange command spreads the
 * pieces over new plates.) */
#include "export.h"
#include "zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    char *s;
    size_t n, cap;
} strbuf;

static void sb_put(strbuf *b, const char *s)
{
    size_t l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        b->cap = (b->cap + l + 1) * 2;
        b->s = (char *)realloc(b->s, b->cap);
    }
    memcpy(b->s + b->n, s, l + 1);
    b->n += l;
}

static void sb_printf(strbuf *b, const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    sb_put(b, tmp);
}

static const char *content_types =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
    "  <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
    "  <Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>\n"
    "  <Default Extension=\"config\" ContentType=\"text/xml\"/>\n"
    "</Types>\n";

static const char *rels =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
    "  <Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>\n"
    "</Relationships>\n";

static int write_3mf(const export_object *objs, int nobjs, const char *path, char *err, size_t errlen)
{
    unsigned mats[MAX_SLOTS + 1];
    int nmats = 0;
    strbuf model, cfg, bbs;
    zip_writer z;
    int i, j, k, ok, next_id;
    int *asm_ids;

    if (err && errlen) err[0] = 0;
    for (i = 0; i < nobjs; i++)
        for (j = 0; j < objs[i].n; j++) {
            for (k = 0; k < nmats; k++) if (mats[k] == objs[i].parts[j].rgb) break;
            if (k == nmats && nmats < MAX_SLOTS + 1) mats[nmats++] = objs[i].parts[j].rgb;
        }
    if (nmats == 0 || nobjs == 0) {
        if (err && errlen) snprintf(err, errlen, "nothing to export");
        return 0;
    }
    asm_ids = (int *)calloc((size_t)nobjs, sizeof(int));

    memset(&model, 0, sizeof(model));
    memset(&cfg, 0, sizeof(cfg));
    memset(&bbs, 0, sizeof(bbs));
    sb_put(&model, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    sb_put(&model, "<model unit=\"millimeter\" xml:lang=\"en-US\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\""
                   " xmlns:m=\"http://schemas.microsoft.com/3dmanufacturing/material/2015/02\">\n");
    sb_put(&model, " <metadata name=\"Application\">intellistream-svgto3dprint</metadata>\n");
    sb_put(&model, " <metadata name=\"Title\">logo</metadata>\n");
    sb_put(&model, " <resources>\n");
    sb_put(&model, "  <basematerials id=\"1\">\n");
    for (i = 0; i < nmats; i++)
        sb_printf(&model, "   <base name=\"Material %d #%06X\" displaycolor=\"#%06XFF\"/>\n", i + 1, mats[i], mats[i]);
    sb_put(&model, "  </basematerials>\n");
    sb_put(&model, "  <m:colorgroup id=\"2\">\n");
    for (i = 0; i < nmats; i++) sb_printf(&model, "   <m:color color=\"#%06XFF\"/>\n", mats[i]);
    sb_put(&model, "  </m:colorgroup>\n");

    sb_put(&cfg, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<config>\n");
    sb_put(&bbs, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<config>\n");
    /* Objects: every piece is an assembly of one component object per colour.
     * Orca/Bambu turn the components into parts of one object and read the
     * extruder of each part from model_settings.config; PrusaSlicer loads the
     * component objects (with their extruder from Slic3r_PE_model.config) and
     * offers to merge them into one multi-part object. */
    next_id = 3;
    for (i = 0; i < nobjs; i++) {
        const export_part *parts = objs[i].parts;
        int n = objs[i].n, first_id = next_id, asm_id;
        if (n == 0) continue;
        for (j = 0; j < n; j++) {
            const mesh_t *mesh = export_part_mesh(&parts[j]);
            int mat = 0, id = next_id++;
            for (k = 0; k < nmats; k++) if (mats[k] == parts[j].rgb) mat = k;
            sb_printf(&model, "  <object id=\"%d\" name=\"%s\" type=\"model\" pid=\"1\" pindex=\"%d\">\n   <mesh>\n    <vertices>\n", id, parts[j].name, mat);
            for (k = 0; k < mesh->nv; k++)
                sb_printf(&model, "     <vertex x=\"%.4f\" y=\"%.4f\" z=\"%.4f\"/>\n", mesh->v[3 * k], mesh->v[3 * k + 1], mesh->v[3 * k + 2]);
            sb_put(&model, "    </vertices>\n    <triangles>\n");
            for (k = 0; k < mesh->nt; k++)
                sb_printf(&model, "     <triangle v1=\"%u\" v2=\"%u\" v3=\"%u\" pid=\"2\" p1=\"%d\"/>\n",
                          mesh->t[3 * k], mesh->t[3 * k + 1], mesh->t[3 * k + 2], mat);
            sb_put(&model, "    </triangles>\n   </mesh>\n  </object>\n");
            /* PrusaSlicer: the component object as a one-volume object with its extruder */
            sb_printf(&cfg, " <object id=\"%d\" instances_count=\"1\">\n  <metadata type=\"object\" key=\"name\" value=\"%s\"/>\n", id, parts[j].name);
            sb_printf(&cfg, "  <metadata type=\"object\" key=\"extruder\" value=\"%d\"/>\n", mat + 1);
            sb_printf(&cfg, "  <volume firstid=\"0\" lastid=\"%d\">\n", mesh->nt - 1);
            sb_printf(&cfg, "   <metadata type=\"volume\" key=\"name\" value=\"%s\"/>\n", parts[j].name);
            sb_put(&cfg, "   <metadata type=\"volume\" key=\"volume_type\" value=\"ModelPart\"/>\n");
            sb_printf(&cfg, "   <metadata type=\"volume\" key=\"extruder\" value=\"%d\"/>\n", mat + 1);
            sb_put(&cfg, "  </volume>\n </object>\n");
        }
        /* the assembly that the build references */
        asm_id = next_id++;
        sb_printf(&model, "  <object id=\"%d\" name=\"%s\" type=\"model\">\n   <components>\n", asm_id, objs[i].name);
        for (j = 0; j < n; j++) sb_printf(&model, "    <component objectid=\"%d\"/>\n", first_id + j);
        sb_put(&model, "   </components>\n  </object>\n");
        /* Orca / Bambu: parts of the assembly with their extruders */
        sb_printf(&bbs, "  <object id=\"%d\">\n    <metadata key=\"name\" value=\"%s\"/>\n", asm_id, objs[i].name);
        for (j = 0; j < n; j++) {
            int mat = 0;
            for (k = 0; k < nmats; k++) if (mats[k] == parts[j].rgb) mat = k;
            sb_printf(&bbs, "    <part id=\"%d\" subtype=\"normal_part\">\n", first_id + j);
            sb_printf(&bbs, "      <metadata key=\"name\" value=\"%s\"/>\n", parts[j].name);
            sb_printf(&bbs, "      <metadata key=\"extruder\" value=\"%d\"/>\n", mat + 1);
            sb_put(&bbs, "    </part>\n");
        }
        sb_put(&bbs, "  </object>\n");
        asm_ids[i] = asm_id;
    }
    sb_put(&cfg, "</config>\n");
    sb_put(&bbs, "</config>\n");
    sb_put(&model, " </resources>\n <build>\n");
    for (i = 0; i < nobjs; i++) {
        if (asm_ids[i] <= 0) continue;
        if (objs[i].tx != 0 || objs[i].ty != 0)
            sb_printf(&model, "  <item objectid=\"%d\" transform=\"1 0 0 0 1 0 0 0 1 %.4f %.4f 0\"/>\n", asm_ids[i], objs[i].tx, objs[i].ty);
        else
            sb_printf(&model, "  <item objectid=\"%d\"/>\n", asm_ids[i]);
    }
    sb_put(&model, " </build>\n</model>\n");
    free(asm_ids);

    if (!zip_open(&z, path)) {
        free(model.s);
        free(cfg.s);
        free(bbs.s);
        if (err && errlen) snprintf(err, errlen, "cannot write '%s'", path);
        return 0;
    }
    ok = zip_add(&z, "[Content_Types].xml", content_types, strlen(content_types));
    ok = ok && zip_add(&z, "_rels/.rels", rels, strlen(rels));
    ok = ok && zip_add(&z, "3D/3dmodel.model", model.s, model.n);
    ok = ok && zip_add(&z, "Metadata/Slic3r_PE_model.config", cfg.s, cfg.n);
    ok = ok && zip_add(&z, "Metadata/model_settings.config", bbs.s, bbs.n);
    ok = zip_close(&z) && ok;
    free(model.s);
    free(cfg.s);
    free(bbs.s);
    if (!ok && err && errlen) snprintf(err, errlen, "write error on '%s'", path);
    return ok;
}

int export_objects_3mf(const export_object *objs, int n, const char *path, char *err, size_t errlen)
{
    return write_3mf(objs, n, path, err, errlen);
}

int export_3mf(const model_t *m, const model_params *p, int chunk, int plate, const char *path, char *err, size_t errlen)
{
    export_object *objs = NULL;
    int nobjs = 0, cobjs = 0;
    int i, j, ok, ci;
    int all = plate >= 0 || chunk == -2;

    if (err && errlen) err[0] = 0;
    if (!m->meshes_valid) {
        if (err && errlen) snprintf(err, errlen, "nothing to export");
        return 0;
    }
    /* which chunks, where, and one object per chunk or per (chunk, colour) */
    for (ci = 0; ci < (all ? m->nchunks : 1); ci++) {
        int ch = all ? ci : ((chunk >= 0 && chunk < m->nchunks) ? chunk : -1);
        const chunk_t *c = ch >= 0 ? &m->chunks[ch] : NULL;
        double tx = 0, ty = 0;
        const char *cname = c ? c->name : "logo";
        export_part parts[MAX_SLOTS + 1];
        int n;
        if (plate >= 0 && c->on_plate != plate) continue;
        if (plate >= 0) { tx = c->plate_pos[0]; ty = c->plate_pos[1]; }
        else if (chunk == -2) { tx = c->place[0] * c->scale; ty = c->place[1] * c->scale; }   /* the assembled layout, scaled like the geometry */
        n = export_collect_parts(m, p, ch, parts);
        if (n == 0) continue;
        if (nobjs + n > cobjs) { cobjs = (nobjs + n) * 2 + 8; objs = (export_object *)realloc(objs, sizeof(export_object) * (size_t)cobjs); }
        if (p->export_color_objects) {
            for (j = 0; j < n; j++) {
                export_object *o = &objs[nobjs++];
                o->parts[0] = parts[j];     /* takes over the rotated copy */
                o->n = 1;
                o->tx = tx;
                o->ty = ty;
                if (all || m->nchunks > 1) snprintf(o->name, sizeof(o->name), "%s_%s", cname, parts[j].name);
                else snprintf(o->name, sizeof(o->name), "%s", parts[j].name);
            }
        } else {
            export_object *o = &objs[nobjs++];
            for (j = 0; j < n; j++) o->parts[j] = parts[j];
            o->n = n;
            o->tx = tx;
            o->ty = ty;
            snprintf(o->name, sizeof(o->name), "%s", cname);
        }
    }
    /* every piece in one file: the keys come along as objects of their own,
     * in a grid in front of the layout, so the slicer's Arrange finds them */
    if (chunk == -2 && plate < 0 && m->nkeys > 0) {
        mesh_t *kmeshes;
        export_object *kobjs;
        double s = m->chunk_uniform_scale > 0 ? m->chunk_uniform_scale : 1;
        int nk;
        nk = export_collect_keys(m, p, m->bbox_min[0] * s, m->bbox_min[1] * s - 12, &kmeshes, &kobjs);
        if (nk > 0) {
            objs = (export_object *)realloc(objs, sizeof(export_object) * (size_t)(nobjs + nk));
            for (j = 0; j < nk; j++) objs[nobjs + j] = kobjs[j];
            ok = write_3mf(objs, nobjs + nk, path, err, errlen);
            for (j = 0; j < nk; j++) mesh_free(&kmeshes[j]);
            free(kmeshes);
            free(kobjs);
            for (i = 0; i < nobjs; i++) export_release_parts(objs[i].parts, objs[i].n);
            free(objs);
            return ok;
        }
    }
    ok = write_3mf(objs, nobjs, path, err, errlen);
    for (i = 0; i < nobjs; i++) export_release_parts(objs[i].parts, objs[i].n);
    free(objs);
    return ok;
}
