#include "svg.h"
#include "xml.h"
#include "textfont.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */

static char *str_dup(const char *s)
{
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (d) memcpy(d, s, n + 1);
    return d;
}

static const char *skip_space(const char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void trim_copy(char *dst, size_t cap, const char *s, size_t n)
{
    while (n && isspace((unsigned char)*s)) { s++; n--; }
    while (n && isspace((unsigned char)s[n - 1])) n--;
    if (n >= cap) n = cap - 1;
    memcpy(dst, s, n);
    dst[n] = 0;
}

/* ------------------------------------------------------------------ */
/* Colours                                                              */

typedef struct { const char *name; unsigned rgb; } named_color;

static const named_color named_colors[] = {
    {"aliceblue",0xF0F8FF},{"antiquewhite",0xFAEBD7},{"aqua",0x00FFFF},{"aquamarine",0x7FFFD4},
    {"azure",0xF0FFFF},{"beige",0xF5F5DC},{"bisque",0xFFE4C4},{"black",0x000000},
    {"blanchedalmond",0xFFEBCD},{"blue",0x0000FF},{"blueviolet",0x8A2BE2},{"brown",0xA52A2A},
    {"burlywood",0xDEB887},{"cadetblue",0x5F9EA0},{"chartreuse",0x7FFF00},{"chocolate",0xD2691E},
    {"coral",0xFF7F50},{"cornflowerblue",0x6495ED},{"cornsilk",0xFFF8DC},{"crimson",0xDC143C},
    {"cyan",0x00FFFF},{"darkblue",0x00008B},{"darkcyan",0x008B8B},{"darkgoldenrod",0xB8860B},
    {"darkgray",0xA9A9A9},{"darkgreen",0x006400},{"darkgrey",0xA9A9A9},{"darkkhaki",0xBDB76B},
    {"darkmagenta",0x8B008B},{"darkolivegreen",0x556B2F},{"darkorange",0xFF8C00},{"darkorchid",0x9932CC},
    {"darkred",0x8B0000},{"darksalmon",0xE9967A},{"darkseagreen",0x8FBC8F},{"darkslateblue",0x483D8B},
    {"darkslategray",0x2F4F4F},{"darkslategrey",0x2F4F4F},{"darkturquoise",0x00CED1},{"darkviolet",0x9400D3},
    {"deeppink",0xFF1493},{"deepskyblue",0x00BFFF},{"dimgray",0x696969},{"dimgrey",0x696969},
    {"dodgerblue",0x1E90FF},{"firebrick",0xB22222},{"floralwhite",0xFFFAF0},{"forestgreen",0x228B22},
    {"fuchsia",0xFF00FF},{"gainsboro",0xDCDCDC},{"ghostwhite",0xF8F8FF},{"gold",0xFFD700},
    {"goldenrod",0xDAA520},{"gray",0x808080},{"grey",0x808080},{"green",0x008000},
    {"greenyellow",0xADFF2F},{"honeydew",0xF0FFF0},{"hotpink",0xFF69B4},{"indianred",0xCD5C5C},
    {"indigo",0x4B0082},{"ivory",0xFFFFF0},{"khaki",0xF0E68C},{"lavender",0xE6E6FA},
    {"lavenderblush",0xFFF0F5},{"lawngreen",0x7CFC00},{"lemonchiffon",0xFFFACD},{"lightblue",0xADD8E6},
    {"lightcoral",0xF08080},{"lightcyan",0xE0FFFF},{"lightgoldenrodyellow",0xFAFAD2},{"lightgray",0xD3D3D3},
    {"lightgreen",0x90EE90},{"lightgrey",0xD3D3D3},{"lightpink",0xFFB6C1},{"lightsalmon",0xFFA07A},
    {"lightseagreen",0x20B2AA},{"lightskyblue",0x87CEFA},{"lightslategray",0x778899},{"lightslategrey",0x778899},
    {"lightsteelblue",0xB0C4DE},{"lightyellow",0xFFFFE0},{"lime",0x00FF00},{"limegreen",0x32CD32},
    {"linen",0xFAF0E6},{"magenta",0xFF00FF},{"maroon",0x800000},{"mediumaquamarine",0x66CDAA},
    {"mediumblue",0x0000CD},{"mediumorchid",0xBA55D3},{"mediumpurple",0x9370DB},{"mediumseagreen",0x3CB371},
    {"mediumslateblue",0x7B68EE},{"mediumspringgreen",0x00FA9A},{"mediumturquoise",0x48D1CC},{"mediumvioletred",0xC71585},
    {"midnightblue",0x191970},{"mintcream",0xF5FFFA},{"mistyrose",0xFFE4E1},{"moccasin",0xFFE4B5},
    {"navajowhite",0xFFDEAD},{"navy",0x000080},{"oldlace",0xFDF5E6},{"olive",0x808000},
    {"olivedrab",0x6B8E23},{"orange",0xFFA500},{"orangered",0xFF4500},{"orchid",0xDA70D6},
    {"palegoldenrod",0xEEE8AA},{"palegreen",0x98FB98},{"paleturquoise",0xAFEEEE},{"palevioletred",0xDB7093},
    {"papayawhip",0xFFEFD5},{"peachpuff",0xFFDAB9},{"peru",0xCD853F},{"pink",0xFFC0CB},
    {"plum",0xDDA0DD},{"powderblue",0xB0E0E6},{"purple",0x800080},{"rebeccapurple",0x663399},
    {"red",0xFF0000},{"rosybrown",0xBC8F8F},{"royalblue",0x4169E1},{"saddlebrown",0x8B4513},
    {"salmon",0xFA8072},{"sandybrown",0xF4A460},{"seagreen",0x2E8B57},{"seashell",0xFFF5EE},
    {"sienna",0xA0522D},{"silver",0xC0C0C0},{"skyblue",0x87CEEB},{"slateblue",0x6A5ACD},
    {"slategray",0x708090},{"slategrey",0x708090},{"snow",0xFFFAFA},{"springgreen",0x00FF7F},
    {"steelblue",0x4682B4},{"tan",0xD2B48C},{"teal",0x008080},{"thistle",0xD8BFD8},
    {"tomato",0xFF6347},{"turquoise",0x40E0D0},{"violet",0xEE82EE},{"wheat",0xF5DEB3},
    {"white",0xFFFFFF},{"whitesmoke",0xF5F5F5},{"yellow",0xFFFF00},{"yellowgreen",0x9ACD32},
};

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

static unsigned pack_rgb(double r, double g, double b)
{
    unsigned R = (unsigned)(clamp01(r) * 255.0 + 0.5);
    unsigned G = (unsigned)(clamp01(g) * 255.0 + 0.5);
    unsigned B = (unsigned)(clamp01(b) * 255.0 + 0.5);
    return (R << 16) | (G << 8) | B;
}

static double hue_to_rgb(double p, double q, double t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0 / 6) return p + (q - p) * 6 * t;
    if (t < 0.5) return q;
    if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
    return p;
}

/* Parses up to 4 numeric components "a, b, c" or "a b c / d"; % handled. */
static int parse_components(const char *s, double *comp, int *ispct, int max, int *count)
{
    int n = 0;
    s = skip_space(s);
    while (*s && *s != ')' && n < max) {
        char *end;
        double v = strtod(s, &end);
        if (end == s) return 0;
        s = end;
        ispct[n] = 0;
        if (*s == '%') { ispct[n] = 1; s++; }
        comp[n++] = v;
        s = skip_space(s);
        if (*s == ',' || *s == '/') s = skip_space(s + 1);
    }
    *count = n;
    return n > 0;
}

int svg_parse_color(const char *s, unsigned *rgb)
{
    size_t n;
    s = skip_space(s);
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    if (n == 0) return 0;
    if (s[0] == '#') {
        int i, v[8];
        size_t hn = n - 1;
        if (hn != 3 && hn != 4 && hn != 6 && hn != 8) return 0;
        for (i = 0; i < (int)hn; i++) { v[i] = hexval(s[1 + i]); if (v[i] < 0) return 0; }
        if (hn == 3 || hn == 4) *rgb = (unsigned)((v[0] * 17) << 16 | (v[1] * 17) << 8 | (v[2] * 17));
        else *rgb = (unsigned)((v[0] * 16 + v[1]) << 16 | (v[2] * 16 + v[3]) << 8 | (v[4] * 16 + v[5]));
        return 1;
    }
    if (!strncmp(s, "rgb", 3) || !strncmp(s, "RGB", 3)) {
        const char *p = strchr(s, '(');
        double c[4]; int pct[4]; int cnt;
        if (!p || !parse_components(p + 1, c, pct, 4, &cnt) || cnt < 3) return 0;
        {
            double r = pct[0] ? c[0] / 100.0 : c[0] / 255.0;
            double g = pct[1] ? c[1] / 100.0 : c[1] / 255.0;
            double b = pct[2] ? c[2] / 100.0 : c[2] / 255.0;
            *rgb = pack_rgb(r, g, b);
        }
        return 1;
    }
    if (!strncmp(s, "hsl", 3) || !strncmp(s, "HSL", 3)) {
        const char *p = strchr(s, '(');
        double c[4]; int pct[4]; int cnt;
        if (!p || !parse_components(p + 1, c, pct, 4, &cnt) || cnt < 3) return 0;
        {
            double h = fmod(c[0], 360.0) / 360.0, sat = clamp01(c[1] / 100.0), l = clamp01(c[2] / 100.0);
            double q = l < 0.5 ? l * (1 + sat) : l + sat - l * sat;
            double pp = 2 * l - q;
            if (h < 0) h += 1;
            *rgb = pack_rgb(hue_to_rgb(pp, q, h + 1.0 / 3), hue_to_rgb(pp, q, h), hue_to_rgb(pp, q, h - 1.0 / 3));
        }
        return 1;
    }
    {
        char name[40];
        size_t i;
        if (n >= sizeof(name)) return 0;
        for (i = 0; i < n; i++) name[i] = (char)tolower((unsigned char)s[i]);
        name[n] = 0;
        for (i = 0; i < sizeof(named_colors) / sizeof(named_colors[0]); i++) {
            if (!strcmp(named_colors[i].name, name)) { *rgb = named_colors[i].rgb; return 1; }
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Lengths and numbers                                                 */

/* Parse a length; returns value in user units (px). `pct_ref` is the
 * reference for percentages. */
static double parse_length(const char *s, double pct_ref, int *ok)
{
    char *end;
    double v;
    if (!s) { if (ok) *ok = 0; return 0; }
    s = skip_space(s);
    v = strtod(s, &end);
    if (end == s) { if (ok) *ok = 0; return 0; }
    if (ok) *ok = 1;
    end = (char *)skip_space(end);
    if (!strncmp(end, "px", 2)) return v;
    if (!strncmp(end, "mm", 2)) return v * 96.0 / 25.4;
    if (!strncmp(end, "cm", 2)) return v * 96.0 / 2.54;
    if (!strncmp(end, "in", 2)) return v * 96.0;
    if (!strncmp(end, "pt", 2)) return v * 96.0 / 72.0;
    if (!strncmp(end, "pc", 2)) return v * 16.0;
    if (!strncmp(end, "em", 2)) return v * 16.0;
    if (*end == '%') return v / 100.0 * pct_ref;
    return v;
}

/* Physical length in millimetres, 0 when unknown (percent / unitless). */
static double parse_length_mm(const char *s)
{
    char *end;
    double v;
    if (!s) return 0;
    s = skip_space(s);
    v = strtod(s, &end);
    if (end == s) return 0;
    end = (char *)skip_space(end);
    if (!strncmp(end, "mm", 2)) return v;
    if (!strncmp(end, "cm", 2)) return v * 10.0;
    if (!strncmp(end, "in", 2)) return v * 25.4;
    if (!strncmp(end, "pt", 2)) return v * 25.4 / 72.0;
    if (!strncmp(end, "pc", 2)) return v * 25.4 / 6.0;
    if (!strncmp(end, "px", 2) || *end == 0) return v * 25.4 / 96.0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Affine transforms  (a c e ; b d f)                                  */

typedef struct { double a, b, c, d, e, f; } xform;

static xform xf_identity(void) { xform m = {1, 0, 0, 1, 0, 0}; return m; }

/* Returns A*B (apply B first, then A). */
static xform xf_mul(xform A, xform B)
{
    xform m;
    m.a = A.a * B.a + A.c * B.b;
    m.b = A.b * B.a + A.d * B.b;
    m.c = A.a * B.c + A.c * B.d;
    m.d = A.b * B.c + A.d * B.d;
    m.e = A.a * B.e + A.c * B.f + A.e;
    m.f = A.b * B.e + A.d * B.f + A.f;
    return m;
}

static void xf_apply(const xform *m, double x, double y, double *ox, double *oy)
{
    *ox = m->a * x + m->c * y + m->e;
    *oy = m->b * x + m->d * y + m->f;
}

static double xf_scale(const xform *m)
{
    return sqrt(fabs(m->a * m->d - m->b * m->c));
}

static int parse_numbers(const char **ps, double *v, int max)
{
    const char *s = *ps;
    int n = 0;
    s = skip_space(s);
    while (n < max) {
        char *end;
        double x = strtod(s, &end);
        if (end == s) break;
        v[n++] = x;
        s = skip_space(end);
        if (*s == ',') s = skip_space(s + 1);
    }
    *ps = s;
    return n;
}

static xform parse_transform(const char *s)
{
    xform m = xf_identity();
    if (!s) return m;
    for (;;) {
        char name[16];
        int ni = 0;
        double v[8];
        int n;
        xform t = xf_identity();
        s = skip_space(s);
        if (*s == ',') { s++; continue; }
        if (!*s) break;
        while (*s && (isalpha((unsigned char)*s)) && ni < 15) name[ni++] = *s++;
        name[ni] = 0;
        s = skip_space(s);
        if (*s != '(') break;
        s++;
        n = parse_numbers(&s, v, 8);
        s = skip_space(s);
        if (*s == ')') s++;
        if (!strcmp(name, "matrix") && n == 6) {
            t.a = v[0]; t.b = v[1]; t.c = v[2]; t.d = v[3]; t.e = v[4]; t.f = v[5];
        } else if (!strcmp(name, "translate") && n >= 1) {
            t.e = v[0]; t.f = n > 1 ? v[1] : 0;
        } else if (!strcmp(name, "scale") && n >= 1) {
            t.a = v[0]; t.d = n > 1 ? v[1] : v[0];
        } else if (!strcmp(name, "rotate") && n >= 1) {
            double a = v[0] * M_PI / 180.0, ca = cos(a), sa = sin(a);
            xform r; r.a = ca; r.b = sa; r.c = -sa; r.d = ca; r.e = 0; r.f = 0;
            if (n >= 3) {
                xform t1 = xf_identity(), t2 = xf_identity();
                t1.e = v[1]; t1.f = v[2];
                t2.e = -v[1]; t2.f = -v[2];
                t = xf_mul(t1, xf_mul(r, t2));
            } else t = r;
        } else if (!strcmp(name, "skewX") && n >= 1) {
            t.c = tan(v[0] * M_PI / 180.0);
        } else if (!strcmp(name, "skewY") && n >= 1) {
            t.b = tan(v[0] * M_PI / 180.0);
        } else {
            continue;
        }
        m = xf_mul(m, t);
    }
    return m;
}

/* ------------------------------------------------------------------ */
/* Styles                                                              */

typedef enum { PAINT_NONE, PAINT_COLOR, PAINT_CURRENT } paint_kind;

typedef struct {
    paint_kind fill_kind;
    unsigned fill;
    int fill_evenodd;
    int clip_evenodd;
    paint_kind stroke_kind;
    unsigned stroke;
    double stroke_width;
    svg_linecap linecap;
    svg_linejoin linejoin;
    double miterlimit;
    unsigned color;
    int display_none;
    int visibility_hidden;
    /* text */
    double font_size;
    char font_family[128];
    int bold;
    int italic;
    int anchor;             /* 0 start, 1 middle, 2 end */
} style_t;

typedef struct {
    char tag[64];
    char id[128];
    char classes[8][64];
    int nclasses;
    int specificity;
    int order;
    char *decls;
} css_rule;

typedef struct {
    const xml_node *node;
    const char *id;
} id_entry;

typedef struct {
    svg_doc *doc;
    css_rule *rules;
    int nrules;
    id_entry *ids;
    int nids;
    double root_w, root_h;      /* viewport size for percentage lengths */
    /* current path being built */
    svg_seg *segs;
    int nsegs, csegs;
    /* dynamic path list capacity */
    int cpaths;
    int cclip_paths;
    int depth;
    /* clipping */
    int current_clip;
    int in_clip;
    /* text */
    const char *font_file;
} ctx_t;

static void style_default(style_t *st)
{
    memset(st, 0, sizeof(*st));
    st->fill_kind = PAINT_COLOR;
    st->fill = 0x000000;
    st->stroke_kind = PAINT_NONE;
    st->stroke_width = 1.0;
    st->linecap = SVG_CAP_BUTT;
    st->linejoin = SVG_JOIN_MITER;
    st->miterlimit = 4.0;
    st->color = 0x000000;
    st->font_size = 16.0;
    snprintf(st->font_family, sizeof(st->font_family), "sans-serif");
}

static const xml_node *find_by_id(ctx_t *c, const char *id)
{
    int i;
    for (i = 0; i < c->nids; i++)
        if (!strcmp(c->ids[i].id, id)) return c->ids[i].node;
    return NULL;
}

static const char *node_attr(const xml_node *n, const char *name)
{
    return xml_attr_get(n, name);
}

static const char *node_href(const xml_node *n)
{
    const char *h = xml_attr_get(n, "href");
    if (!h) h = xml_attr_get(n, "xlink:href");
    return h;
}

/* Extract a property value from a style="" declaration string. */
static int style_decl_get(const char *decls, const char *prop, char *out, size_t cap)
{
    const char *s = decls;
    size_t plen = strlen(prop);
    while (s && *s) {
        const char *colon, *semi;
        s = skip_space(s);
        colon = strchr(s, ':');
        if (!colon) break;
        semi = strchr(colon, ';');
        {
            size_t nlen = colon - s;
            while (nlen && isspace((unsigned char)s[nlen - 1])) nlen--;
            if (nlen == plen && !strncmp(s, prop, plen)) {
                trim_copy(out, cap, colon + 1, semi ? (size_t)(semi - colon - 1) : strlen(colon + 1));
                return 1;
            }
        }
        if (!semi) break;
        s = semi + 1;
    }
    return 0;
}

static int resolve_gradient_color(ctx_t *c, const xml_node *g, unsigned *rgb, int depth);

/* Resolve stop colour of a gradient stop element. */
static int stop_color(ctx_t *c, const xml_node *stop, unsigned *rgb)
{
    char buf[128];
    const char *v = node_attr(stop, "stop-color");
    const char *st = node_attr(stop, "style");
    if (st && style_decl_get(st, "stop-color", buf, sizeof(buf))) v = buf;
    if (!v) { *rgb = 0; return 1; }
    if (!strcmp(v, "currentColor")) { *rgb = 0; return 1; }
    (void)c;
    return svg_parse_color(v, rgb);
}

static int resolve_gradient_color(ctx_t *c, const xml_node *g, unsigned *rgb, int depth)
{
    int i, n = 0;
    double r = 0, gg = 0, b = 0;
    if (depth > 6) return 0;
    for (i = 0; i < g->nchildren; i++) {
        const xml_node *ch = g->children[i];
        unsigned col;
        if (!strcmp(ch->name, "stop") && stop_color(c, ch, &col)) {
            r += (col >> 16) & 255; gg += (col >> 8) & 255; b += col & 255; n++;
        }
    }
    if (n == 0) {
        const char *h = node_href(g);
        if (h && h[0] == '#') {
            const xml_node *ref = find_by_id(c, h + 1);
            if (ref) return resolve_gradient_color(c, ref, rgb, depth + 1);
        }
        return 0;
    }
    *rgb = pack_rgb(r / n / 255.0, gg / n / 255.0, b / n / 255.0);
    return 1;
}

/* Parse a paint value. Returns 0 if the value should be ignored (inherit). */
static int parse_paint(ctx_t *c, const char *v, paint_kind *kind, unsigned *rgb)
{
    v = skip_space(v);
    if (!strcmp(v, "inherit")) return 0;
    if (!strncmp(v, "none", 4) || !strncmp(v, "transparent", 11)) { *kind = PAINT_NONE; return 1; }
    if (!strncmp(v, "currentColor", 12) || !strncmp(v, "currentcolor", 12)) { *kind = PAINT_CURRENT; return 1; }
    if (!strncmp(v, "url(", 4)) {
        const char *p = v + 4;
        const char *close;
        char id[128];
        p = skip_space(p);
        if (*p == '\'' || *p == '"') p++;
        close = strchr(p, ')');
        if (!close) { *kind = PAINT_COLOR; *rgb = 0x808080; return 1; }
        {
            size_t n = close - p;
            while (n && (p[n - 1] == '\'' || p[n - 1] == '"' || isspace((unsigned char)p[n - 1]))) n--;
            if (n >= sizeof(id)) n = sizeof(id) - 1;
            memcpy(id, p, n);
            id[n] = 0;
        }
        if (id[0] == '#') {
            const xml_node *ref = find_by_id(c, id + 1);
            if (ref && (!strcmp(ref->name, "linearGradient") || !strcmp(ref->name, "radialGradient"))) {
                c->doc->n_gradients++;
                if (resolve_gradient_color(c, ref, rgb, 0)) { *kind = PAINT_COLOR; return 1; }
            }
        }
        /* fallback colour after the url() */
        {
            const char *fb = skip_space(close + 1);
            unsigned col;
            if (*fb && svg_parse_color(fb, &col)) { *kind = PAINT_COLOR; *rgb = col; return 1; }
            if (!strncmp(fb, "none", 4)) { *kind = PAINT_NONE; return 1; }
        }
        *kind = PAINT_COLOR;
        *rgb = 0x808080;
        c->doc->n_unsupported++;
        return 1;
    }
    {
        unsigned col;
        if (svg_parse_color(v, &col)) { *kind = PAINT_COLOR; *rgb = col; return 1; }
    }
    return 0;
}

static void apply_property(ctx_t *c, style_t *st, const char *name, const char *value)
{
    char v[256];
    trim_copy(v, sizeof(v), value, strlen(value));
    /* strip !important */
    {
        char *bang = strstr(v, "!important");
        if (bang) { *bang = 0; trim_copy(v, sizeof(v), v, strlen(v)); }
    }
    if (!strcmp(name, "fill")) {
        paint_kind k; unsigned rgb = st->fill;
        if (parse_paint(c, v, &k, &rgb)) { st->fill_kind = k; st->fill = rgb; }
    } else if (!strcmp(name, "fill-rule")) {
        if (!strcmp(v, "evenodd")) st->fill_evenodd = 1;
        else if (!strcmp(v, "nonzero")) st->fill_evenodd = 0;
    } else if (!strcmp(name, "clip-rule")) {
        if (!strcmp(v, "evenodd")) st->clip_evenodd = 1;
        else if (!strcmp(v, "nonzero")) st->clip_evenodd = 0;
    } else if (!strcmp(name, "stroke")) {
        paint_kind k; unsigned rgb = st->stroke;
        if (parse_paint(c, v, &k, &rgb)) { st->stroke_kind = k; st->stroke = rgb; }
    } else if (!strcmp(name, "stroke-width")) {
        int ok;
        double w = parse_length(v, c->root_w, &ok);
        if (ok) st->stroke_width = w;
    } else if (!strcmp(name, "stroke-linecap")) {
        if (!strcmp(v, "round")) st->linecap = SVG_CAP_ROUND;
        else if (!strcmp(v, "square")) st->linecap = SVG_CAP_SQUARE;
        else if (!strcmp(v, "butt")) st->linecap = SVG_CAP_BUTT;
    } else if (!strcmp(name, "stroke-linejoin")) {
        if (!strcmp(v, "round")) st->linejoin = SVG_JOIN_ROUND;
        else if (!strcmp(v, "bevel")) st->linejoin = SVG_JOIN_BEVEL;
        else if (!strncmp(v, "miter", 5)) st->linejoin = SVG_JOIN_MITER;
    } else if (!strcmp(name, "stroke-miterlimit")) {
        double m = atof(v);
        if (m >= 1) st->miterlimit = m;
    } else if (!strcmp(name, "color")) {
        unsigned col;
        if (svg_parse_color(v, &col)) st->color = col;
    } else if (!strcmp(name, "display")) {
        if (!strcmp(v, "none")) st->display_none = 1;
    } else if (!strcmp(name, "visibility")) {
        if (!strcmp(v, "hidden") || !strcmp(v, "collapse")) st->visibility_hidden = 1;
        else if (!strcmp(v, "visible")) st->visibility_hidden = 0;
    } else if (!strcmp(name, "fill-opacity") || !strcmp(name, "opacity")) {
        double o = atof(v);
        if (strchr(v, '%')) o /= 100.0;
        if (o <= 0.001 && strlen(v) > 0) { st->fill_kind = PAINT_NONE; if (!strcmp(name, "opacity")) st->stroke_kind = PAINT_NONE; }
    } else if (!strcmp(name, "stroke-opacity")) {
        double o = atof(v);
        if (strchr(v, '%')) o /= 100.0;
        if (o <= 0.001 && strlen(v) > 0) st->stroke_kind = PAINT_NONE;
    } else if (!strcmp(name, "font-size")) {
        int ok;
        double fs;
        if (strstr(v, "em")) fs = atof(v) * st->font_size;
        else if (strchr(v, '%')) fs = atof(v) / 100.0 * st->font_size;
        else fs = parse_length(v, st->font_size, &ok);
        if (fs > 0) st->font_size = fs;
    } else if (!strcmp(name, "font-family")) {
        snprintf(st->font_family, sizeof(st->font_family), "%s", v);
    } else if (!strcmp(name, "font-weight")) {
        if (!strcmp(v, "bold") || !strcmp(v, "bolder") || atoi(v) >= 600) st->bold = 1;
        else if (!strcmp(v, "normal") || !strcmp(v, "lighter") || (atoi(v) > 0 && atoi(v) < 600)) st->bold = 0;
    } else if (!strcmp(name, "font-style")) {
        if (!strcmp(v, "italic") || !strcmp(v, "oblique")) st->italic = 1;
        else if (!strcmp(v, "normal")) st->italic = 0;
    } else if (!strcmp(name, "text-anchor")) {
        if (!strcmp(v, "middle")) st->anchor = 1;
        else if (!strcmp(v, "end")) st->anchor = 2;
        else st->anchor = 0;
    } else if (!strcmp(name, "font")) {
        /* shorthand: [style] [weight] size[/line-height] family */
        const char *q = v;
        while (*q) {
            char tok[128];
            int n = 0;
            q = skip_space(q);
            while (*q && !isspace((unsigned char)*q) && n < 127) tok[n++] = *q++;
            tok[n] = 0;
            if (!n) break;
            if (!strcmp(tok, "italic") || !strcmp(tok, "oblique")) st->italic = 1;
            else if (!strcmp(tok, "bold") || !strcmp(tok, "bolder") || atoi(tok) >= 600) st->bold = 1;
            else if (isdigit((unsigned char)tok[0]) || tok[0] == '.') {
                int ok;
                double fs = parse_length(tok, st->font_size, &ok);
                if (fs > 0) st->font_size = fs;
                q = skip_space(q);
                if (*q) snprintf(st->font_family, sizeof(st->font_family), "%s", q);
                break;
            }
        }
    }
}

static void apply_decls(ctx_t *c, style_t *st, const char *decls)
{
    const char *s = decls;
    while (s && *s) {
        const char *colon, *semi;
        char name[64], value[256];
        s = skip_space(s);
        colon = strchr(s, ':');
        if (!colon) break;
        semi = strchr(colon, ';');
        trim_copy(name, sizeof(name), s, colon - s);
        trim_copy(value, sizeof(value), colon + 1, semi ? (size_t)(semi - colon - 1) : strlen(colon + 1));
        apply_property(c, st, name, value);
        if (!semi) break;
        s = semi + 1;
    }
}

static const char *presentation_attrs[] = {
    "fill", "fill-rule", "stroke", "stroke-width", "stroke-linecap", "stroke-linejoin",
    "stroke-miterlimit", "color", "display", "visibility", "fill-opacity", "stroke-opacity", "opacity", "clip-rule",
    "font-size", "font-family", "font-weight", "font-style", "text-anchor", "font", NULL
};

static int has_class(const xml_node *n, const char *cls)
{
    const char *cl = node_attr(n, "class");
    size_t len = strlen(cls);
    if (!cl) return 0;
    while (*cl) {
        const char *e;
        cl = skip_space(cl);
        e = cl;
        while (*e && !isspace((unsigned char)*e)) e++;
        if ((size_t)(e - cl) == len && !strncmp(cl, cls, len)) return 1;
        cl = e;
    }
    return 0;
}

static int rule_matches(const css_rule *r, const xml_node *n)
{
    int i;
    if (r->tag[0] && strcmp(r->tag, "*") && strcmp(r->tag, n->name)) return 0;
    if (r->id[0]) {
        const char *id = node_attr(n, "id");
        if (!id || strcmp(id, r->id)) return 0;
    }
    for (i = 0; i < r->nclasses; i++)
        if (!has_class(n, r->classes[i])) return 0;
    return 1;
}

/* Compute the style of an element from the parent's style. */
static void compute_style(ctx_t *c, const xml_node *n, const style_t *parent, style_t *out)
{
    int i;
    *out = *parent;
    out->display_none = 0;
    /* 1. presentation attributes */
    for (i = 0; presentation_attrs[i]; i++) {
        const char *v = node_attr(n, presentation_attrs[i]);
        if (v) apply_property(c, out, presentation_attrs[i], v);
    }
    /* 2. stylesheet rules in specificity order */
    if (c->nrules) {
        int *match = (int *)malloc(sizeof(int) * c->nrules);
        int nm = 0, j;
        for (i = 0; i < c->nrules; i++)
            if (rule_matches(&c->rules[i], n)) match[nm++] = i;
        /* insertion sort by specificity (stable, keeps document order) */
        for (i = 1; i < nm; i++) {
            int v = match[i];
            j = i - 1;
            while (j >= 0 && c->rules[match[j]].specificity > c->rules[v].specificity) { match[j + 1] = match[j]; j--; }
            match[j + 1] = v;
        }
        for (i = 0; i < nm; i++) apply_decls(c, out, c->rules[match[i]].decls);
        free(match);
    }
    /* 3. inline style */
    {
        const char *st = node_attr(n, "style");
        if (st) apply_decls(c, out, st);
    }
}

/* ------------------------------------------------------------------ */
/* CSS stylesheet parsing                                              */

static void css_add_rule(ctx_t *c, const char *sel, size_t sellen, const char *decls, size_t dlen)
{
    /* the selector may be a list separated by commas */
    const char *s = sel, *end = sel + sellen;
    while (s < end) {
        const char *comma = memchr(s, ',', end - s);
        const char *one_end = comma ? comma : end;
        char buf[256];
        char *p, *last;
        css_rule r;
        int skip = 0;
        trim_copy(buf, sizeof(buf), s, one_end - s);
        s = one_end + 1;
        if (!buf[0]) continue;
        /* keep only the last compound selector */
        last = buf;
        for (p = buf; *p; p++) {
            if (isspace((unsigned char)*p) || *p == '>' || *p == '+' || *p == '~') {
                while (*p && (isspace((unsigned char)*p) || *p == '>' || *p == '+' || *p == '~')) p++;
                if (*p) last = p; else break;
                p--;
            }
        }
        memset(&r, 0, sizeof(r));
        p = last;
        if (*p && *p != '.' && *p != '#' && *p != '[' && *p != ':') {
            int i = 0;
            while (*p && *p != '.' && *p != '#' && *p != '[' && *p != ':' && i < 63) r.tag[i++] = *p++;
            r.tag[i] = 0;
            if (strcmp(r.tag, "*")) r.specificity += 1;
        }
        while (*p) {
            if (*p == '.') {
                int i = 0;
                p++;
                if (r.nclasses < 8) {
                    while (*p && *p != '.' && *p != '#' && *p != '[' && *p != ':' && i < 63) r.classes[r.nclasses][i++] = *p++;
                    r.classes[r.nclasses][i] = 0;
                    r.nclasses++;
                    r.specificity += 10;
                } else while (*p && *p != '.' && *p != '#' && *p != '[' && *p != ':') p++;
            } else if (*p == '#') {
                int i = 0;
                p++;
                while (*p && *p != '.' && *p != '#' && *p != '[' && *p != ':' && i < 127) r.id[i++] = *p++;
                r.id[i] = 0;
                r.specificity += 100;
            } else if (*p == '[') {
                while (*p && *p != ']') p++;
                if (*p) p++;
                r.specificity += 10;
            } else if (*p == ':') {
                skip = 1; /* pseudo classes never match a static document */
                break;
            } else p++;
        }
        if (skip) continue;
        r.order = c->nrules;
        r.decls = (char *)malloc(dlen + 1);
        memcpy(r.decls, decls, dlen);
        r.decls[dlen] = 0;
        c->rules = (css_rule *)realloc(c->rules, sizeof(css_rule) * (c->nrules + 1));
        c->rules[c->nrules++] = r;
    }
}

static void css_parse(ctx_t *c, const char *text)
{
    /* strip comments into a copy */
    size_t n = strlen(text);
    char *buf = (char *)malloc(n + 1);
    size_t i = 0, o = 0;
    const char *s;
    while (i < n) {
        if (text[i] == '/' && i + 1 < n && text[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(text[i] == '*' && text[i + 1] == '/')) i++;
            i += 2;
            continue;
        }
        buf[o++] = text[i++];
    }
    buf[o] = 0;
    s = buf;
    for (;;) {
        const char *open, *close;
        s = skip_space(s);
        if (!*s) break;
        if (*s == '@') {
            /* skip at-rule: either up to ';' or a whole block */
            const char *semi = strchr(s, ';');
            open = strchr(s, '{');
            if (open && (!semi || open < semi)) {
                int depth = 0;
                const char *p = open;
                for (; *p; p++) {
                    if (*p == '{') depth++;
                    else if (*p == '}') { depth--; if (depth == 0) { p++; break; } }
                }
                s = p;
            } else s = semi ? semi + 1 : s + strlen(s);
            continue;
        }
        open = strchr(s, '{');
        if (!open) break;
        close = strchr(open, '}');
        if (!close) break;
        css_add_rule(c, s, open - s, open + 1, close - open - 1);
        s = close + 1;
    }
    free(buf);
}

/* ------------------------------------------------------------------ */
/* Path building                                                       */

static void seg_push(ctx_t *c, svg_seg seg)
{
    if (c->nsegs == c->csegs) {
        c->csegs = c->csegs ? c->csegs * 2 : 64;
        c->segs = (svg_seg *)realloc(c->segs, sizeof(svg_seg) * c->csegs);
    }
    c->segs[c->nsegs++] = seg;
}

static void emit_move(ctx_t *c, const xform *m, double x, double y)
{
    svg_seg s; memset(&s, 0, sizeof(s));
    s.type = SVG_SEG_MOVE;
    xf_apply(m, x, y, &s.x[0], &s.y[0]);
    seg_push(c, s);
}

static void emit_line(ctx_t *c, const xform *m, double x, double y)
{
    svg_seg s; memset(&s, 0, sizeof(s));
    s.type = SVG_SEG_LINE;
    xf_apply(m, x, y, &s.x[0], &s.y[0]);
    seg_push(c, s);
}

static void emit_cubic(ctx_t *c, const xform *m, double x1, double y1, double x2, double y2, double x, double y)
{
    svg_seg s; memset(&s, 0, sizeof(s));
    s.type = SVG_SEG_CUBIC;
    xf_apply(m, x1, y1, &s.x[0], &s.y[0]);
    xf_apply(m, x2, y2, &s.x[1], &s.y[1]);
    xf_apply(m, x, y, &s.x[2], &s.y[2]);
    seg_push(c, s);
}

static void emit_close(ctx_t *c)
{
    svg_seg s; memset(&s, 0, sizeof(s));
    s.type = SVG_SEG_CLOSE;
    seg_push(c, s);
}

/* Convert an SVG elliptical arc to cubic segments (SVG spec F.6.5). */
static void emit_arc(ctx_t *c, const xform *m, double x1, double y1, double rx, double ry,
                     double phi_deg, int large, int sweep, double x2, double y2)
{
    double phi, cphi, sphi, dx2, dy2, x1p, y1p, lambda, sign, coef, cxp, cyp, cx, cy;
    double theta1, dtheta, ux, uy, vx, vy, dot, len;
    int nseg, i;
    if (fabs(x1 - x2) < 1e-12 && fabs(y1 - y2) < 1e-12) return;
    rx = fabs(rx); ry = fabs(ry);
    if (rx < 1e-12 || ry < 1e-12) { emit_line(c, m, x2, y2); return; }
    phi = phi_deg * M_PI / 180.0;
    cphi = cos(phi); sphi = sin(phi);
    dx2 = (x1 - x2) / 2.0; dy2 = (y1 - y2) / 2.0;
    x1p = cphi * dx2 + sphi * dy2;
    y1p = -sphi * dx2 + cphi * dy2;
    lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1) { rx *= sqrt(lambda); ry *= sqrt(lambda); }
    {
        double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
        double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
        coef = den > 0 ? sqrt(num > 0 ? num / den : 0) : 0;
    }
    sign = (large != sweep) ? 1.0 : -1.0;
    cxp = sign * coef * (rx * y1p / ry);
    cyp = sign * coef * (-ry * x1p / rx);
    cx = cphi * cxp - sphi * cyp + (x1 + x2) / 2.0;
    cy = sphi * cxp + cphi * cyp + (y1 + y2) / 2.0;
    ux = (x1p - cxp) / rx; uy = (y1p - cyp) / ry;
    vx = (-x1p - cxp) / rx; vy = (-y1p - cyp) / ry;
    theta1 = atan2(uy, ux);
    len = sqrt(ux * ux + uy * uy) * sqrt(vx * vx + vy * vy);
    dot = (ux * vx + uy * vy) / (len > 0 ? len : 1);
    if (dot > 1) dot = 1;
    if (dot < -1) dot = -1;
    dtheta = acos(dot);
    if (ux * vy - uy * vx < 0) dtheta = -dtheta;
    if (!sweep && dtheta > 0) dtheta -= 2 * M_PI;
    if (sweep && dtheta < 0) dtheta += 2 * M_PI;
    nseg = (int)ceil(fabs(dtheta) / (M_PI / 2) - 1e-9);
    if (nseg < 1) nseg = 1;
    for (i = 0; i < nseg; i++) {
        double t1 = theta1 + dtheta * i / nseg;
        double t2 = theta1 + dtheta * (i + 1) / nseg;
        double alpha = 4.0 / 3.0 * tan((t2 - t1) / 4.0);
        double p1x = cos(t1), p1y = sin(t1), p2x = cos(t2), p2y = sin(t2);
        double q1x = p1x - alpha * sin(t1), q1y = p1y + alpha * cos(t1);
        double q2x = p2x + alpha * sin(t2), q2y = p2y - alpha * cos(t2);
        double ex = (i == nseg - 1) ? x2 : cx + rx * p2x * cphi - ry * p2y * sphi;
        double ey = (i == nseg - 1) ? y2 : cy + rx * p2x * sphi + ry * p2y * cphi;
        emit_cubic(c, m,
                   cx + rx * q1x * cphi - ry * q1y * sphi, cy + rx * q1x * sphi + ry * q1y * cphi,
                   cx + rx * q2x * cphi - ry * q2y * sphi, cy + rx * q2x * sphi + ry * q2y * cphi,
                   ex, ey);
    }
}

/* Parse path data into the context segment buffer. */
static void parse_path_data(ctx_t *c, const xform *m, const char *d)
{
    const char *s = d;
    char cmd = 0;
    double cx = 0, cy = 0;          /* current point */
    double sx = 0, sy = 0;          /* subpath start */
    double lcx = 0, lcy = 0;        /* last control point (for S/T) */
    char lastcmd = 0;
    int open = 0;

    while (*s) {
        double v[7];
        int n = 0, need;
        s = skip_space(s);
        if (*s == ',') { s++; continue; }
        if (!*s) break;
        if (isalpha((unsigned char)*s)) {
            cmd = *s++;
        } else if (cmd == 0) {
            break;
        } else if (cmd == 'M') cmd = 'L';
        else if (cmd == 'm') cmd = 'l';
        switch (cmd) {
        case 'M': case 'm': case 'L': case 'l': case 'T': case 't': need = 2; break;
        case 'H': case 'h': case 'V': case 'v': need = 1; break;
        case 'C': case 'c': need = 6; break;
        case 'S': case 's': case 'Q': case 'q': need = 4; break;
        case 'A': case 'a': need = 7; break;
        case 'Z': case 'z': need = 0; break;
        default: return;
        }
        if (need) {
            int i;
            for (i = 0; i < need; i++) {
                char *end;
                s = skip_space(s);
                if (*s == ',') s = skip_space(s + 1);
                if ((cmd == 'A' || cmd == 'a') && (i == 3 || i == 4)) {
                    if (*s == '0' || *s == '1') { v[i] = *s - '0'; s++; continue; }
                    return;
                }
                v[i] = strtod(s, &end);
                if (end == s) return;
                s = end;
                n++;
            }
        }
        switch (cmd) {
        case 'M': case 'm':
            if (cmd == 'm') { v[0] += cx; v[1] += cy; }
            if (open) { /* implicit: previous subpath stays open */ }
            emit_move(c, m, v[0], v[1]);
            cx = sx = v[0]; cy = sy = v[1];
            open = 1;
            break;
        case 'L': case 'l':
            if (cmd == 'l') { v[0] += cx; v[1] += cy; }
            emit_line(c, m, v[0], v[1]);
            cx = v[0]; cy = v[1];
            break;
        case 'H': case 'h':
            if (cmd == 'h') v[0] += cx;
            emit_line(c, m, v[0], cy);
            cx = v[0];
            break;
        case 'V': case 'v':
            if (cmd == 'v') v[0] += cy;
            emit_line(c, m, cx, v[0]);
            cy = v[0];
            break;
        case 'C': case 'c':
            if (cmd == 'c') { v[0] += cx; v[1] += cy; v[2] += cx; v[3] += cy; v[4] += cx; v[5] += cy; }
            emit_cubic(c, m, v[0], v[1], v[2], v[3], v[4], v[5]);
            lcx = v[2]; lcy = v[3];
            cx = v[4]; cy = v[5];
            break;
        case 'S': case 's': {
            double c1x, c1y;
            if (cmd == 's') { v[0] += cx; v[1] += cy; v[2] += cx; v[3] += cy; }
            if (lastcmd == 'C' || lastcmd == 'c' || lastcmd == 'S' || lastcmd == 's') { c1x = 2 * cx - lcx; c1y = 2 * cy - lcy; }
            else { c1x = cx; c1y = cy; }
            emit_cubic(c, m, c1x, c1y, v[0], v[1], v[2], v[3]);
            lcx = v[0]; lcy = v[1];
            cx = v[2]; cy = v[3];
            break;
        }
        case 'Q': case 'q': {
            double c1x, c1y, c2x, c2y;
            if (cmd == 'q') { v[0] += cx; v[1] += cy; v[2] += cx; v[3] += cy; }
            c1x = cx + 2.0 / 3.0 * (v[0] - cx); c1y = cy + 2.0 / 3.0 * (v[1] - cy);
            c2x = v[2] + 2.0 / 3.0 * (v[0] - v[2]); c2y = v[3] + 2.0 / 3.0 * (v[1] - v[3]);
            emit_cubic(c, m, c1x, c1y, c2x, c2y, v[2], v[3]);
            lcx = v[0]; lcy = v[1];
            cx = v[2]; cy = v[3];
            break;
        }
        case 'T': case 't': {
            double qx, qy, c1x, c1y, c2x, c2y;
            if (cmd == 't') { v[0] += cx; v[1] += cy; }
            if (lastcmd == 'Q' || lastcmd == 'q' || lastcmd == 'T' || lastcmd == 't') { qx = 2 * cx - lcx; qy = 2 * cy - lcy; }
            else { qx = cx; qy = cy; }
            c1x = cx + 2.0 / 3.0 * (qx - cx); c1y = cy + 2.0 / 3.0 * (qy - cy);
            c2x = v[0] + 2.0 / 3.0 * (qx - v[0]); c2y = v[1] + 2.0 / 3.0 * (qy - v[1]);
            emit_cubic(c, m, c1x, c1y, c2x, c2y, v[0], v[1]);
            lcx = qx; lcy = qy;
            cx = v[0]; cy = v[1];
            break;
        }
        case 'A': case 'a':
            if (cmd == 'a') { v[5] += cx; v[6] += cy; }
            emit_arc(c, m, cx, cy, v[0], v[1], v[2], (int)v[3], (int)v[4], v[5], v[6]);
            cx = v[5]; cy = v[6];
            break;
        case 'Z': case 'z':
            emit_close(c);
            cx = sx; cy = sy;
            open = 0;
            break;
        }
        lastcmd = cmd;
    }
}

/* Emit an ellipse as four cubic arcs. */
static void emit_ellipse(ctx_t *c, const xform *m, double cx, double cy, double rx, double ry)
{
    const double k = 0.5522847498307936;
    emit_move(c, m, cx + rx, cy);
    emit_cubic(c, m, cx + rx, cy + ry * k, cx + rx * k, cy + ry, cx, cy + ry);
    emit_cubic(c, m, cx - rx * k, cy + ry, cx - rx, cy + ry * k, cx - rx, cy);
    emit_cubic(c, m, cx - rx, cy - ry * k, cx - rx * k, cy - ry, cx, cy - ry);
    emit_cubic(c, m, cx + rx * k, cy - ry, cx + rx, cy - ry * k, cx + rx, cy);
    emit_close(c);
}

static void emit_rect(ctx_t *c, const xform *m, double x, double y, double w, double h, double rx, double ry)
{
    const double k = 1.0 - 0.5522847498307936;
    if (rx > w / 2) rx = w / 2;
    if (ry > h / 2) ry = h / 2;
    if (rx <= 0 || ry <= 0) {
        emit_move(c, m, x, y);
        emit_line(c, m, x + w, y);
        emit_line(c, m, x + w, y + h);
        emit_line(c, m, x, y + h);
        emit_close(c);
        return;
    }
    emit_move(c, m, x + rx, y);
    emit_line(c, m, x + w - rx, y);
    emit_cubic(c, m, x + w - rx * k, y, x + w, y + ry * k, x + w, y + ry);
    emit_line(c, m, x + w, y + h - ry);
    emit_cubic(c, m, x + w, y + h - ry * k, x + w - rx * k, y + h, x + w - rx, y + h);
    emit_line(c, m, x + rx, y + h);
    emit_cubic(c, m, x + rx * k, y + h, x, y + h - ry * k, x, y + h - ry);
    emit_line(c, m, x, y + ry);
    emit_cubic(c, m, x, y + ry * k, x + rx * k, y, x + rx, y);
    emit_close(c);
}

/* Finish the current segment buffer into a path record. */
static void finish_path(ctx_t *c, const xml_node *n, const style_t *st, const xform *m)
{
    svg_path p;
    int has_fill, has_stroke;
    const char *id;
    if (c->nsegs == 0) return;
    if (c->in_clip) {
        /* geometry of a clipPath: always filled, never stroked */
        memset(&p, 0, sizeof(p));
        p.segs = (svg_seg *)malloc(sizeof(svg_seg) * c->nsegs);
        memcpy(p.segs, c->segs, sizeof(svg_seg) * c->nsegs);
        p.nsegs = c->nsegs;
        c->nsegs = 0;
        p.has_fill = 1;
        p.fill_evenodd = st->clip_evenodd;
        p.clip = -1;
        if (c->doc->nclip_paths == c->cclip_paths) {
            c->cclip_paths = c->cclip_paths ? c->cclip_paths * 2 : 16;
            c->doc->clip_paths = (svg_path *)realloc(c->doc->clip_paths, sizeof(svg_path) * c->cclip_paths);
        }
        c->doc->clip_paths[c->doc->nclip_paths++] = p;
        return;
    }
    if (st->visibility_hidden) { c->nsegs = 0; return; }
    has_fill = st->fill_kind != PAINT_NONE;
    has_stroke = st->stroke_kind != PAINT_NONE && st->stroke_width > 0;
    if (!has_fill && !has_stroke) { c->nsegs = 0; return; }
    memset(&p, 0, sizeof(p));
    p.segs = (svg_seg *)malloc(sizeof(svg_seg) * c->nsegs);
    memcpy(p.segs, c->segs, sizeof(svg_seg) * c->nsegs);
    p.nsegs = c->nsegs;
    c->nsegs = 0;
    p.clip = c->current_clip;
    p.has_fill = has_fill;
    p.fill_rgb = st->fill_kind == PAINT_CURRENT ? st->color : st->fill;
    p.fill_evenodd = st->fill_evenodd;
    p.has_stroke = has_stroke;
    p.stroke_rgb = st->stroke_kind == PAINT_CURRENT ? st->color : st->stroke;
    p.stroke_width = st->stroke_width * xf_scale(m);
    p.linecap = st->linecap;
    p.linejoin = st->linejoin;
    p.miter_limit = st->miterlimit;
    id = node_attr(n, "id");
    p.id = id ? str_dup(id) : NULL;
    if (c->doc->npaths == c->cpaths) {
        c->cpaths = c->cpaths ? c->cpaths * 2 : 64;
        c->doc->paths = (svg_path *)realloc(c->doc->paths, sizeof(svg_path) * c->cpaths);
    }
    c->doc->paths[c->doc->npaths++] = p;
}

/* ------------------------------------------------------------------ */
/* Element traversal                                                   */

static void render_node(ctx_t *c, const xml_node *n, const style_t *parent_style, xform ctm);

static void render_children(ctx_t *c, const xml_node *n, const style_t *st, xform ctm)
{
    int i;
    for (i = 0; i < n->nchildren; i++) render_node(c, n->children[i], st, ctm);
}

static double attr_len(const xml_node *n, const char *name, double pct_ref, double def)
{
    int ok;
    double v = parse_length(node_attr(n, name), pct_ref, &ok);
    return ok ? v : def;
}

static int parse_viewbox(const char *s, double *vb)
{
    int n;
    if (!s) return 0;
    n = parse_numbers(&s, vb, 4);
    return n == 4 && vb[2] > 0 && vb[3] > 0;
}

/* viewport transform for nested svg / symbol: fits viewBox into w x h (xMidYMid meet). */
static xform viewport_xform(double x, double y, double w, double h, const double *vb, int has_vb)
{
    xform t = xf_identity();
    if (!has_vb) { t.e = x; t.f = y; return t; }
    if (w > 0 && h > 0) {
        double sx = w / vb[2], sy = h / vb[3];
        double sc = sx < sy ? sx : sy;
        double tx = x + (w - vb[2] * sc) / 2.0 - vb[0] * sc;
        double ty = y + (h - vb[3] * sc) / 2.0 - vb[1] * sc;
        t.a = sc; t.d = sc; t.e = tx; t.f = ty;
    } else {
        t.e = x - vb[0]; t.f = y - vb[1];
    }
    return t;
}

static void render_use(ctx_t *c, const xml_node *n, const style_t *st, xform ctm)
{
    const char *href = node_href(n);
    const xml_node *ref;
    xform t = xf_identity();
    if (!href || href[0] != '#') return;
    ref = find_by_id(c, href + 1);
    if (!ref) return;
    if (c->depth > 32) return;
    t.e = attr_len(n, "x", c->root_w, 0);
    t.f = attr_len(n, "y", c->root_w, 0);
    ctm = xf_mul(ctm, t);
    c->depth++;
    if (!strcmp(ref->name, "symbol") || !strcmp(ref->name, "svg")) {
        double vb[4];
        int has_vb = parse_viewbox(node_attr(ref, "viewBox"), vb);
        double w = attr_len(n, "width", c->root_w, attr_len(ref, "width", c->root_w, 0));
        double h = attr_len(n, "height", c->root_h, attr_len(ref, "height", c->root_h, 0));
        style_t sst;
        xform vt = viewport_xform(0, 0, w, h, vb, has_vb);
        compute_style(c, ref, st, &sst);
        if (!sst.display_none) render_children(c, ref, &sst, xf_mul(xf_mul(ctm, parse_transform(node_attr(ref, "transform"))), vt));
    } else {
        render_node(c, ref, st, ctm);
    }
    c->depth--;
}

static void render_element(ctx_t *c, const xml_node *n, const style_t *stp, xform ctm);

/* ------------------------------------------------------------------ */
/* Text                                                                */

typedef struct {
    ctx_t *c;
    const xform *m;
    double px, py;          /* pen position (user units) */
    double scale;           /* font units -> user units */
    double lastx, lasty;    /* current point in font units */
} glyph_sink;

static void gs_map(const glyph_sink *g, double fx, double fy, double *ux, double *uy)
{
    *ux = g->px + fx * g->scale;
    *uy = g->py - fy * g->scale;
}

static void gs_move(void *ud, double x, double y)
{
    glyph_sink *g = (glyph_sink *)ud;
    double ux, uy;
    gs_map(g, x, y, &ux, &uy);
    emit_move(g->c, g->m, ux, uy);
    g->lastx = x; g->lasty = y;
}

static void gs_line(void *ud, double x, double y)
{
    glyph_sink *g = (glyph_sink *)ud;
    double ux, uy;
    gs_map(g, x, y, &ux, &uy);
    emit_line(g->c, g->m, ux, uy);
    g->lastx = x; g->lasty = y;
}

static void gs_quad(void *ud, double cx, double cy, double x, double y)
{
    glyph_sink *g = (glyph_sink *)ud;
    double c1x = g->lastx + 2.0 / 3.0 * (cx - g->lastx), c1y = g->lasty + 2.0 / 3.0 * (cy - g->lasty);
    double c2x = x + 2.0 / 3.0 * (cx - x), c2y = y + 2.0 / 3.0 * (cy - y);
    double u1x, u1y, u2x, u2y, ux, uy;
    gs_map(g, c1x, c1y, &u1x, &u1y);
    gs_map(g, c2x, c2y, &u2x, &u2y);
    gs_map(g, x, y, &ux, &uy);
    emit_cubic(g->c, g->m, u1x, u1y, u2x, u2y, ux, uy);
    g->lastx = x; g->lasty = y;
}

static void gs_cubic(void *ud, double c1x, double c1y, double c2x, double c2y, double x, double y)
{
    glyph_sink *g = (glyph_sink *)ud;
    double u1x, u1y, u2x, u2y, ux, uy;
    gs_map(g, c1x, c1y, &u1x, &u1y);
    gs_map(g, c2x, c2y, &u2x, &u2y);
    gs_map(g, x, y, &ux, &uy);
    emit_cubic(g->c, g->m, u1x, u1y, u2x, u2y, ux, uy);
    g->lastx = x; g->lasty = y;
}

static void gs_close(void *ud)
{
    glyph_sink *g = (glyph_sink *)ud;
    emit_close(g->c);
}

/* Collapse white space like the SVG default; a newline character stays a line break
 * (draw.io and similar tools rely on that).  Returns a malloc'd string. */
static char *normalise_text(const char *t)
{
    size_t n = strlen(t), i, o = 0;
    char *out = (char *)malloc(n + 1);
    int pending_space = 0, at_line_start = 1;
    for (i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)t[i];
        if (ch == '\n') {
            /* trim trailing spaces of the line */
            while (o > 0 && out[o - 1] == ' ') o--;
            if (o > 0) out[o++] = '\n';
            pending_space = 0;
            at_line_start = 1;
        } else if (ch == ' ' || ch == '\t' || ch == '\r') {
            if (!at_line_start) pending_space = 1;
        } else {
            if (pending_space) out[o++] = ' ';
            pending_space = 0;
            at_line_start = 0;
            out[o++] = (char)ch;
        }
    }
    while (o > 0 && (out[o - 1] == ' ' || out[o - 1] == '\n')) o--;
    out[o] = 0;
    return out;
}

static double line_width(textfont *f, const char *line, size_t len, double scale)
{
    const char *p = line, *end = line + len;
    double w = 0;
    int prev = 0;
    while (p < end) {
        int cp = textfont_utf8_next(&p);
        if (prev) w += textfont_kern(f, prev, cp) * scale;
        w += textfont_advance(f, cp) * scale;
        prev = cp;
    }
    return w;
}

/* Lay out one run of text at (x, y) and emit its glyph outlines. */
static void emit_text_run(ctx_t *c, const xform *m, const style_t *st, double x, double y, const char *text, double *pen_x)
{
    textfont *f = textfont_open(st->font_family, st->bold, st->italic, c->font_file);
    char *norm;
    const char *line;
    double scale, line_height, cy = y;
    static const textfont_sink sink = {gs_move, gs_line, gs_quad, gs_cubic, gs_close};
    if (!f) { c->doc->n_text_skipped++; return; }
    if (!c->doc->font_used[0]) snprintf(c->doc->font_used, sizeof(c->doc->font_used), "%s", textfont_path(f));
    scale = textfont_scale(f, st->font_size);
    line_height = st->font_size * 1.2;
    norm = normalise_text(text);
    line = norm;
    while (*line) {
        const char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        double w = line_width(f, line, len, scale);
        double px = x - (st->anchor == 1 ? w / 2 : (st->anchor == 2 ? w : 0));
        const char *p = line, *end = line + len;
        int prev = 0;
        while (p < end) {
            int cp = textfont_utf8_next(&p);
            glyph_sink gs;
            if (prev) px += textfont_kern(f, prev, cp) * scale;
            gs.c = c; gs.m = m; gs.px = px; gs.py = cy; gs.scale = scale; gs.lastx = gs.lasty = 0;
            textfont_outline(f, cp, &sink, &gs);
            px += textfont_advance(f, cp) * scale;
            prev = cp;
        }
        if (pen_x) *pen_x = px;
        if (!nl) break;
        line = nl + 1;
        cy += line_height;
    }
    free(norm);
}

static double attr_num(const xml_node *n, const char *name, double def)
{
    const char *v = node_attr(n, name);
    int ok;
    double r;
    if (!v) return def;
    r = parse_length(v, 0, &ok);
    return ok ? r : def;
}

static void render_text(ctx_t *c, const xml_node *n, const style_t *st, xform ctm)
{
    double x = attr_num(n, "x", 0) + attr_num(n, "dx", 0);
    double y = attr_num(n, "y", 0) + attr_num(n, "dy", 0);
    int i, has_tspan = 0;
    if (c->in_clip) return;
    /* draw.io's "Text is not SVG" notice is not part of the drawing */
    if (n->text && strstr(n->text, "Text is not SVG")) return;
    for (i = 0; i < n->nchildren; i++) if (!strcmp(n->children[i]->name, "tspan")) has_tspan = 1;
    if (has_tspan) {
        double pen = x;
        for (i = 0; i < n->nchildren; i++) {
            const xml_node *ts = n->children[i];
            style_t tst;
            double tx, ty;
            if (strcmp(ts->name, "tspan") || !ts->text) continue;
            compute_style(c, ts, st, &tst);
            if (tst.display_none) continue;
            tx = node_attr(ts, "x") ? attr_num(ts, "x", pen) : pen;
            ty = node_attr(ts, "y") ? attr_num(ts, "y", y) : y;
            tx += attr_num(ts, "dx", 0);
            ty += attr_num(ts, "dy", 0);
            y = ty;
            c->nsegs = 0;
            emit_text_run(c, &ctm, &tst, tx, ty, ts->text, &pen);
            if (c->nsegs) { finish_path(c, ts, &tst, &ctm); c->doc->n_text++; }
        }
    } else if (n->text) {
        c->nsegs = 0;
        emit_text_run(c, &ctm, st, x, y, n->text, NULL);
        if (c->nsegs) { finish_path(c, n, st, &ctm); c->doc->n_text++; }
    }
}

/* Create a clip entry from a clip-path reference; returns the clip index or -1. */
static int make_clip(ctx_t *c, const char *ref, const style_t *st, xform ctm)
{
    char id[128];
    const char *p, *close;
    const xml_node *node;
    const char *units;
    int idx, first;
    style_t cst;
    ref = skip_space(ref);
    if (strncmp(ref, "url(", 4)) return -1;
    p = skip_space(ref + 4);
    if (*p == '\'' || *p == '"') p++;
    close = strchr(p, ')');
    if (!close) return -1;
    {
        size_t n = close - p;
        while (n && (p[n - 1] == '\'' || p[n - 1] == '"' || isspace((unsigned char)p[n - 1]))) n--;
        if (n >= sizeof(id)) n = sizeof(id) - 1;
        memcpy(id, p, n);
        id[n] = 0;
    }
    if (id[0] != '#') return -1;
    node = find_by_id(c, id + 1);
    if (!node || strcmp(node->name, "clipPath")) return -1;
    units = node_attr(node, "clipPathUnits");
    if (units && !strcmp(units, "objectBoundingBox")) { c->doc->n_unsupported++; return -1; }
    if (c->depth > 32) return -1;
    idx = c->doc->nclips;
    first = c->doc->nclip_paths;
    c->doc->clips = (svg_clip *)realloc(c->doc->clips, sizeof(svg_clip) * (idx + 1));
    c->doc->clips[idx].first_path = first;
    c->doc->clips[idx].npaths = 0;
    c->doc->clips[idx].parent = c->current_clip;
    c->doc->nclips++;
    style_default(&cst);
    cst.clip_evenodd = st->clip_evenodd;
    c->in_clip++;
    c->depth++;
    {
        style_t nst;
        compute_style(c, node, &cst, &nst);
        render_children(c, node, &nst, xf_mul(ctm, parse_transform(node_attr(node, "transform"))));
    }
    c->depth--;
    c->in_clip--;
    c->doc->clips[idx].npaths = c->doc->nclip_paths - first;
    return idx;
}

static void render_node(ctx_t *c, const xml_node *n, const style_t *parent_style, xform ctm)
{
    style_t st;
    const char *name = n->name;
    xform local;
    int saved_clip;

    if (!strcmp(name, "defs") || !strcmp(name, "clipPath") || !strcmp(name, "mask") ||
        !strcmp(name, "marker") || !strcmp(name, "pattern") || !strcmp(name, "symbol") ||
        !strcmp(name, "linearGradient") || !strcmp(name, "radialGradient") || !strcmp(name, "filter") ||
        !strcmp(name, "metadata") || !strcmp(name, "title") || !strcmp(name, "desc") || !strcmp(name, "style") ||
        !strcmp(name, "script"))
        return;
    if (!strcmp(name, "image")) { c->doc->n_image++; return; }

    compute_style(c, n, parent_style, &st);
    if (st.display_none) return;
    local = parse_transform(node_attr(n, "transform"));
    ctm = xf_mul(ctm, local);

    saved_clip = c->current_clip;
    if (!c->in_clip) {
        const char *cp = node_attr(n, "clip-path");
        char buf[256];
        if (!cp) {
            const char *sa = node_attr(n, "style");
            if (sa && style_decl_get(sa, "clip-path", buf, sizeof(buf))) cp = buf;
        }
        if (cp && strncmp(skip_space(cp), "none", 4)) {
            int idx = make_clip(c, cp, &st, ctm);
            if (idx >= 0) c->current_clip = idx;
        }
    }
    render_element(c, n, &st, ctm);
    c->current_clip = saved_clip;
}

static void render_element(ctx_t *c, const xml_node *n, const style_t *stp, xform ctm)
{
    style_t st = *stp;
    const char *name = n->name;

    if (!strcmp(name, "g") || !strcmp(name, "a")) {
        const char *href = node_href(n);
        if (href && strstr(href, "svg-export-text-problems")) return;   /* draw.io notice */
        render_children(c, n, &st, ctm);
    } else if (!strcmp(name, "switch")) {
        int i;
        for (i = 0; i < n->nchildren; i++) {
            const xml_node *ch = n->children[i];
            if (node_attr(ch, "systemLanguage") || node_attr(ch, "requiredExtensions") || node_attr(ch, "requiredFeatures")) continue;
            render_node(c, ch, &st, ctm);
            break;
        }
    } else if (!strcmp(name, "svg")) {
        double vb[4];
        int has_vb = parse_viewbox(node_attr(n, "viewBox"), vb);
        double x = attr_len(n, "x", c->root_w, 0), y = attr_len(n, "y", c->root_h, 0);
        double w = attr_len(n, "width", c->root_w, has_vb ? vb[2] : 0), h = attr_len(n, "height", c->root_h, has_vb ? vb[3] : 0);
        render_children(c, n, &st, xf_mul(ctm, viewport_xform(x, y, w, h, vb, has_vb)));
    } else if (!strcmp(name, "use")) {
        render_use(c, n, &st, ctm);
    } else if (!strcmp(name, "text")) {
        render_text(c, n, &st, ctm);
    } else if (!strcmp(name, "path")) {
        const char *d = node_attr(n, "d");
        if (d) { parse_path_data(c, &ctm, d); finish_path(c, n, &st, &ctm); }
    } else if (!strcmp(name, "rect")) {
        double x = attr_len(n, "x", c->root_w, 0), y = attr_len(n, "y", c->root_h, 0);
        double w = attr_len(n, "width", c->root_w, 0), h = attr_len(n, "height", c->root_h, 0);
        double rx = attr_len(n, "rx", c->root_w, -1), ry = attr_len(n, "ry", c->root_h, -1);
        if (rx < 0 && ry < 0) rx = ry = 0;
        else if (rx < 0) rx = ry;
        else if (ry < 0) ry = rx;
        if (w > 0 && h > 0) { emit_rect(c, &ctm, x, y, w, h, rx, ry); finish_path(c, n, &st, &ctm); }
    } else if (!strcmp(name, "circle")) {
        double cx = attr_len(n, "cx", c->root_w, 0), cy = attr_len(n, "cy", c->root_h, 0), r = attr_len(n, "r", c->root_w, 0);
        if (r > 0) { emit_ellipse(c, &ctm, cx, cy, r, r); finish_path(c, n, &st, &ctm); }
    } else if (!strcmp(name, "ellipse")) {
        double cx = attr_len(n, "cx", c->root_w, 0), cy = attr_len(n, "cy", c->root_h, 0);
        double rx = attr_len(n, "rx", c->root_w, 0), ry = attr_len(n, "ry", c->root_h, 0);
        if (rx > 0 && ry > 0) { emit_ellipse(c, &ctm, cx, cy, rx, ry); finish_path(c, n, &st, &ctm); }
    } else if (!strcmp(name, "line")) {
        double x1 = attr_len(n, "x1", c->root_w, 0), y1 = attr_len(n, "y1", c->root_h, 0);
        double x2 = attr_len(n, "x2", c->root_w, 0), y2 = attr_len(n, "y2", c->root_h, 0);
        style_t ls = st;
        ls.fill_kind = PAINT_NONE;
        emit_move(c, &ctm, x1, y1);
        emit_line(c, &ctm, x2, y2);
        finish_path(c, n, &ls, &ctm);
    } else if (!strcmp(name, "polyline") || !strcmp(name, "polygon")) {
        const char *pts = node_attr(n, "points");
        if (pts) {
            double v[2];
            int first = 1;
            const char *s = pts;
            while (parse_numbers(&s, v, 2) == 2) {
                if (first) emit_move(c, &ctm, v[0], v[1]); else emit_line(c, &ctm, v[0], v[1]);
                first = 0;
            }
            if (!first) {
                if (!strcmp(name, "polygon")) emit_close(c);
                finish_path(c, n, &st, &ctm);
            }
        }
    } else {
        /* unknown element: descend anyway (foreignObject etc. are skipped) */
        if (strcmp(name, "foreignObject")) render_children(c, n, &st, ctm);
    }
}

/* ------------------------------------------------------------------ */
/* Document                                                            */

static void collect_ids_and_styles(ctx_t *c, const xml_node *n)
{
    int i;
    const char *id = node_attr(n, "id");
    if (id && id[0]) {
        c->ids = (id_entry *)realloc(c->ids, sizeof(id_entry) * (c->nids + 1));
        c->ids[c->nids].node = n;
        c->ids[c->nids].id = id;
        c->nids++;
    }
    if (!strcmp(n->name, "style") && n->text) css_parse(c, n->text);
    for (i = 0; i < n->nchildren; i++) collect_ids_and_styles(c, n->children[i]);
}

svg_doc *svg_parse_data(const char *data, size_t len, const char *font_file, char *err, size_t errlen)
{
    xml_node *root;
    svg_doc *doc;
    ctx_t c;
    style_t st;
    double vb[4];
    int has_vb, i;

    if (err && errlen) err[0] = 0;
    root = xml_parse(data, len, err, errlen);
    if (!root) return NULL;
    if (strcmp(root->name, "svg")) {
        if (err && errlen) snprintf(err, errlen, "root element is <%s>, not <svg>", root->name);
        xml_free(root);
        return NULL;
    }
    doc = (svg_doc *)calloc(1, sizeof(svg_doc));
    memset(&c, 0, sizeof(c));
    c.doc = doc;
    c.current_clip = -1;
    c.font_file = font_file;
    collect_ids_and_styles(&c, root);

    has_vb = parse_viewbox(node_attr(root, "viewBox"), vb);
    doc->width_mm = parse_length_mm(node_attr(root, "width"));
    doc->height_mm = parse_length_mm(node_attr(root, "height"));
    {
        int okw, okh;
        double w = parse_length(node_attr(root, "width"), 0, &okw);
        double h = parse_length(node_attr(root, "height"), 0, &okh);
        if (has_vb) {
            doc->vb_x = vb[0]; doc->vb_y = vb[1]; doc->vb_w = vb[2]; doc->vb_h = vb[3];
        } else {
            doc->vb_x = 0; doc->vb_y = 0;
            doc->vb_w = (okw && w > 0) ? w : 0;
            doc->vb_h = (okh && h > 0) ? h : 0;
        }
    }
    c.root_w = doc->vb_w > 0 ? doc->vb_w : 1000;
    c.root_h = doc->vb_h > 0 ? doc->vb_h : 1000;

    style_default(&st);
    compute_style(&c, root, &st, &st);
    if (!st.display_none) {
        xform ctm = xf_identity();
        render_children(&c, root, &st, xf_mul(ctm, parse_transform(node_attr(root, "transform"))));
    }

    for (i = 0; i < c.nrules; i++) free(c.rules[i].decls);
    free(c.rules);
    free(c.ids);
    free(c.segs);
    xml_free(root);
    return doc;
}

svg_doc *svg_parse_file(const char *path, const char *font_file, char *err, size_t errlen)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    svg_doc *doc;
    if (!f) {
        if (err && errlen) snprintf(err, errlen, "cannot open '%s'", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) n = 0;
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); if (err && errlen) snprintf(err, errlen, "out of memory"); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f); free(buf);
        if (err && errlen) snprintf(err, errlen, "read error on '%s'", path);
        return NULL;
    }
    fclose(f);
    buf[n] = 0;
    /* compressed SVGZ is not supported */
    if (n >= 2 && (unsigned char)buf[0] == 0x1F && (unsigned char)buf[1] == 0x8B) {
        free(buf);
        if (err && errlen) snprintf(err, errlen, "compressed .svgz is not supported; save as plain .svg");
        return NULL;
    }
    doc = svg_parse_data(buf, (size_t)n, font_file, err, errlen);
    free(buf);
    return doc;
}

void svg_free(svg_doc *doc)
{
    int i;
    if (!doc) return;
    for (i = 0; i < doc->npaths; i++) { free(doc->paths[i].segs); free(doc->paths[i].id); }
    free(doc->paths);
    for (i = 0; i < doc->nclip_paths; i++) { free(doc->clip_paths[i].segs); free(doc->clip_paths[i].id); }
    free(doc->clip_paths);
    free(doc->clips);
    free(doc);
}
