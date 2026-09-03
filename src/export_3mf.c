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
 *    object with one extruder per part. */
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

typedef struct {
    int chunk;              /* index or -1 for the preview geometry */
    int part;               /* -1 = all parts of the chunk in one object, else one part */
    double tx, ty;          /* build item translation */
    char name[96];
    int asm_id;             /* id of the assembly object written for it */
} obj_spec;

int export_3mf(const model_t *m, const model_params *p, int chunk, const char *path, char *err, size_t errlen)
{
    obj_spec *objs = NULL;
    int nobjs = 0, cobjs = 0;
    unsigned mats[MAX_SLOTS + 1];
    int nmats = 0;
    strbuf model, cfg, bbs;
    zip_writer z;
    int i, j, k, ok;
    int next_id;
    int nchunk_objs = 0, ci;

    if (err && errlen) err[0] = 0;
    if (!m->meshes_valid) {
        if (err && errlen) snprintf(err, errlen, "nothing to export");
        return 0;
    }
    /* which chunks, and one object per chunk or per (chunk, colour) */
    for (ci = 0; ci < (chunk == -2 ? m->nchunks : 1); ci++) {
        int ch = chunk == -2 ? ci : ((chunk >= 0 && chunk < m->nchunks) ? chunk : -1);
        /* pieces in one file keep their assembled layout, scaled like the geometry */
        double tx = chunk == -2 ? m->chunks[ci].place[0] * m->chunks[ci].scale : 0;
        double ty = chunk == -2 ? m->chunks[ci].place[1] * m->chunks[ci].scale : 0;
        const char *cname = ch >= 0 ? m->chunks[ch].name : "logo";
        export_part parts[MAX_SLOTS + 1];
        int n = export_collect_parts(m, p, ch, parts);
        int np = p->export_color_objects ? n : (n > 0 ? 1 : 0);
        for (j = 0; j < np; j++) {
            if (nobjs == cobjs) { cobjs = cobjs ? cobjs * 2 : 16; objs = (obj_spec *)realloc(objs, sizeof(obj_spec) * (size_t)cobjs); }
            objs[nobjs].chunk = ch;
            objs[nobjs].part = p->export_color_objects ? j : -1;
            objs[nobjs].tx = tx;
            objs[nobjs].ty = ty;
            objs[nobjs].asm_id = 0;
            if (p->export_color_objects) {
                if (chunk == -2 || m->nchunks > 1) snprintf(objs[nobjs].name, sizeof(objs[nobjs].name), "%s_%s", cname, parts[j].name);
                else snprintf(objs[nobjs].name, sizeof(objs[nobjs].name), "%s", parts[j].name);
            } else snprintf(objs[nobjs].name, sizeof(objs[nobjs].name), "%s", cname);
            nobjs++;
        }
        for (j = 0; j < n; j++) {
            for (k = 0; k < nmats; k++) if (mats[k] == parts[j].rgb) break;
            if (k == nmats && nmats < MAX_SLOTS + 1) mats[nmats++] = parts[j].rgb;
        }
        export_release_parts(parts, n);
        nchunk_objs++;
    }
    if (nmats == 0 || nobjs == 0) {
        free(objs);
        if (err && errlen) snprintf(err, errlen, "nothing to export");
        return 0;
    }

    memset(&model, 0, sizeof(model));
    memset(&cfg, 0, sizeof(cfg));
    memset(&bbs, 0, sizeof(bbs));
    sb_put(&model, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    sb_put(&model, "<model unit=\"millimeter\" xml:lang=\"en-US\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\""
                   " xmlns:m=\"http://schemas.microsoft.com/3dmanufacturing/material/2015/02\">\n");
    sb_put(&model, " <metadata name=\"Application\">logo3dprint</metadata>\n");
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
        export_part all[MAX_SLOTS + 1], parts[MAX_SLOTS + 1];
        int nall = export_collect_parts(m, p, objs[i].chunk, all), n;
        int first_id = next_id, asm_id;
        if (objs[i].part >= 0) { n = objs[i].part < nall ? 1 : 0; if (n) parts[0] = all[objs[i].part]; }
        else { n = nall; for (j = 0; j < n; j++) parts[j] = all[j]; }
        if (n == 0) { export_release_parts(all, nall); continue; }
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
        objs[i].asm_id = asm_id;
        export_release_parts(all, nall);
    }
    sb_put(&cfg, "</config>\n");
    sb_put(&bbs, "</config>\n");
    sb_put(&model, " </resources>\n <build>\n");
    for (i = 0; i < nobjs; i++) {
        if (objs[i].asm_id <= 0) continue;
        if (objs[i].tx != 0 || objs[i].ty != 0)
            sb_printf(&model, "  <item objectid=\"%d\" transform=\"1 0 0 0 1 0 0 0 1 %.4f %.4f 0\"/>\n", objs[i].asm_id, objs[i].tx, objs[i].ty);
        else
            sb_printf(&model, "  <item objectid=\"%d\"/>\n", objs[i].asm_id);
    }
    sb_put(&model, " </build>\n</model>\n");

    free(objs);
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
