#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    const char *p, *end;
    char *err;
    size_t errlen;
    int line;
} parser;

static void set_err(parser *ps, const char *msg)
{
    if (ps->err && ps->errlen)
        snprintf(ps->err, ps->errlen, "XML error at line %d: %s", ps->line, msg);
}

static int starts_with(parser *ps, const char *s)
{
    size_t n = strlen(s);
    return (size_t)(ps->end - ps->p) >= n && memcmp(ps->p, s, n) == 0;
}

static void advance(parser *ps, size_t n)
{
    while (n-- && ps->p < ps->end) {
        if (*ps->p == '\n') ps->line++;
        ps->p++;
    }
}

static void skip_ws(parser *ps)
{
    while (ps->p < ps->end && isspace((unsigned char)*ps->p)) advance(ps, 1);
}

static char *dupn(const char *s, size_t n)
{
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

static void append_utf8(char **out, size_t *len, size_t *cap, unsigned cp)
{
    char buf[4];
    int n = 0;
    if (cp < 0x80) { buf[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { buf[0] = (char)(0xC0 | (cp >> 6)); buf[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
    else if (cp < 0x10000) { buf[0] = (char)(0xE0 | (cp >> 12)); buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
    else { buf[0] = (char)(0xF0 | (cp >> 18)); buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
    if (*len + n + 1 > *cap) {
        *cap = (*cap + n + 64) * 2;
        *out = (char *)realloc(*out, *cap);
    }
    memcpy(*out + *len, buf, n);
    *len += n;
}

/* Decode entities in [s, s+n) into a fresh string. */
static char *decode_entities(const char *s, size_t n)
{
    size_t cap = n + 1, len = 0;
    char *out = (char *)malloc(cap);
    size_t i = 0;
    if (!out) return NULL;
    while (i < n) {
        if (s[i] == '&') {
            const char *semi = memchr(s + i, ';', n - i);
            if (semi && (size_t)(semi - (s + i)) <= 12) {
                size_t elen = semi - (s + i) - 1;
                const char *e = s + i + 1;
                unsigned cp = 0;
                int ok = 1;
                if (elen == 2 && !memcmp(e, "lt", 2)) cp = '<';
                else if (elen == 2 && !memcmp(e, "gt", 2)) cp = '>';
                else if (elen == 3 && !memcmp(e, "amp", 3)) cp = '&';
                else if (elen == 4 && !memcmp(e, "quot", 4)) cp = '"';
                else if (elen == 4 && !memcmp(e, "apos", 4)) cp = '\'';
                else if (elen >= 2 && e[0] == '#') {
                    if (e[1] == 'x' || e[1] == 'X') cp = (unsigned)strtoul(e + 2, NULL, 16);
                    else cp = (unsigned)strtoul(e + 1, NULL, 10);
                    if (cp == 0) ok = 0;
                } else ok = 0;
                if (ok) {
                    append_utf8(&out, &len, &cap, cp);
                    i += elen + 2;
                    continue;
                }
            }
        }
        if (len + 2 > cap) { cap = cap * 2 + 16; out = (char *)realloc(out, cap); }
        out[len++] = s[i++];
    }
    out[len] = 0;
    return out;
}

static xml_node *node_new(const char *name, size_t n)
{
    xml_node *nd = (xml_node *)calloc(1, sizeof(xml_node));
    if (!nd) return NULL;
    /* strip namespace prefix */
    {
        const char *colon = memchr(name, ':', n);
        if (colon) { n -= (colon + 1 - name); name = colon + 1; }
    }
    nd->name = dupn(name, n);
    return nd;
}

static void node_add_child(xml_node *parent, xml_node *child)
{
    parent->children = (xml_node **)realloc(parent->children, sizeof(xml_node *) * (parent->nchildren + 1));
    parent->children[parent->nchildren++] = child;
    child->parent = parent;
}

static void node_add_text(xml_node *nd, const char *s, size_t n)
{
    size_t old = nd->text ? strlen(nd->text) : 0;
    char *dec = decode_entities(s, n);
    size_t dn = strlen(dec);
    nd->text = (char *)realloc(nd->text, old + dn + 1);
    memcpy(nd->text + old, dec, dn + 1);
    free(dec);
}

static int is_name_char(int c)
{
    return isalnum(c) || c == '_' || c == '-' || c == '.' || c == ':' || c >= 0x80;
}

/* Parses attributes into node; stops at '>' or '/>'. Returns 1 if self-closing, 0 otherwise, -1 on error. */
static int parse_attrs(parser *ps, xml_node *nd)
{
    for (;;) {
        skip_ws(ps);
        if (ps->p >= ps->end) { set_err(ps, "unexpected end in tag"); return -1; }
        if (*ps->p == '>') { advance(ps, 1); return 0; }
        if (starts_with(ps, "/>")) { advance(ps, 2); return 1; }
        {
            const char *ns = ps->p;
            while (ps->p < ps->end && is_name_char((unsigned char)*ps->p)) advance(ps, 1);
            if (ps->p == ns) { set_err(ps, "bad attribute name"); return -1; }
            {
                size_t nlen = ps->p - ns;
                char *name = dupn(ns, nlen);
                char *value = NULL;
                skip_ws(ps);
                if (ps->p < ps->end && *ps->p == '=') {
                    advance(ps, 1);
                    skip_ws(ps);
                    if (ps->p < ps->end && (*ps->p == '"' || *ps->p == '\'')) {
                        char q = *ps->p;
                        const char *vs;
                        advance(ps, 1);
                        vs = ps->p;
                        while (ps->p < ps->end && *ps->p != q) advance(ps, 1);
                        value = decode_entities(vs, ps->p - vs);
                        if (ps->p < ps->end) advance(ps, 1);
                    } else {
                        const char *vs = ps->p;
                        while (ps->p < ps->end && !isspace((unsigned char)*ps->p) && *ps->p != '>' && *ps->p != '/') advance(ps, 1);
                        value = decode_entities(vs, ps->p - vs);
                    }
                } else {
                    value = dupn("", 0);
                }
                nd->attrs = (xml_attr *)realloc(nd->attrs, sizeof(xml_attr) * (nd->nattrs + 1));
                nd->attrs[nd->nattrs].name = name;
                nd->attrs[nd->nattrs].value = value;
                nd->nattrs++;
            }
        }
    }
}

/* Skip a "<!...>" construct such as DOCTYPE, including an internal subset. */
static void skip_decl(parser *ps)
{
    int depth = 0;
    while (ps->p < ps->end) {
        char c = *ps->p;
        if (c == '[') depth++;
        else if (c == ']') depth--;
        else if (c == '>' && depth <= 0) { advance(ps, 1); return; }
        advance(ps, 1);
    }
}

static int skip_misc(parser *ps)
{
    for (;;) {
        skip_ws(ps);
        if (starts_with(ps, "<?")) {
            while (ps->p < ps->end && !starts_with(ps, "?>")) advance(ps, 1);
            advance(ps, 2);
        } else if (starts_with(ps, "<!--")) {
            while (ps->p < ps->end && !starts_with(ps, "-->")) advance(ps, 1);
            advance(ps, 3);
        } else if (starts_with(ps, "<!")) {
            skip_decl(ps);
        } else {
            return 0;
        }
    }
}

static xml_node *parse_element(parser *ps, int depth);

static int parse_content(parser *ps, xml_node *nd, int depth)
{
    for (;;) {
        if (ps->p >= ps->end) { set_err(ps, "unexpected end of document"); return -1; }
        if (*ps->p == '<') {
            if (starts_with(ps, "</")) {
                const char *ns;
                advance(ps, 2);
                ns = ps->p;
                while (ps->p < ps->end && *ps->p != '>') advance(ps, 1);
                (void)ns;
                if (ps->p < ps->end) advance(ps, 1);
                return 0;
            } else if (starts_with(ps, "<!--")) {
                while (ps->p < ps->end && !starts_with(ps, "-->")) advance(ps, 1);
                advance(ps, 3);
            } else if (starts_with(ps, "<![CDATA[")) {
                const char *cs;
                advance(ps, 9);
                cs = ps->p;
                while (ps->p < ps->end && !starts_with(ps, "]]>")) advance(ps, 1);
                {
                    size_t old = nd->text ? strlen(nd->text) : 0;
                    size_t n = ps->p - cs;
                    nd->text = (char *)realloc(nd->text, old + n + 1);
                    memcpy(nd->text + old, cs, n);
                    nd->text[old + n] = 0;
                }
                advance(ps, 3);
            } else if (starts_with(ps, "<?")) {
                while (ps->p < ps->end && !starts_with(ps, "?>")) advance(ps, 1);
                advance(ps, 2);
            } else if (starts_with(ps, "<!")) {
                skip_decl(ps);
            } else {
                xml_node *child = parse_element(ps, depth + 1);
                if (!child) return -1;
                node_add_child(nd, child);
            }
        } else {
            const char *ts = ps->p;
            while (ps->p < ps->end && *ps->p != '<') advance(ps, 1);
            node_add_text(nd, ts, ps->p - ts);
        }
    }
}

static xml_node *parse_element(parser *ps, int depth)
{
    const char *ns;
    xml_node *nd;
    int sc;
    if (depth > 256) { set_err(ps, "nesting too deep"); return NULL; }
    if (ps->p >= ps->end || *ps->p != '<') { set_err(ps, "expected '<'"); return NULL; }
    advance(ps, 1);
    ns = ps->p;
    while (ps->p < ps->end && is_name_char((unsigned char)*ps->p)) advance(ps, 1);
    if (ps->p == ns) { set_err(ps, "expected element name"); return NULL; }
    nd = node_new(ns, ps->p - ns);
    if (!nd) { set_err(ps, "out of memory"); return NULL; }
    sc = parse_attrs(ps, nd);
    if (sc < 0) { xml_free(nd); return NULL; }
    if (sc == 1) return nd;
    if (parse_content(ps, nd, depth) < 0) { xml_free(nd); return NULL; }
    return nd;
}

xml_node *xml_parse(const char *data, size_t len, char *err, size_t errlen)
{
    parser ps;
    xml_node *root;
    ps.p = data;
    ps.end = data + len;
    ps.err = err;
    ps.errlen = errlen;
    ps.line = 1;
    if (err && errlen) err[0] = 0;
    /* UTF-8 BOM */
    if (len >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF)
        ps.p += 3;
    skip_misc(&ps);
    if (ps.p >= ps.end) { set_err(&ps, "empty document"); return NULL; }
    root = parse_element(&ps, 0);
    return root;
}

void xml_free(xml_node *node)
{
    int i;
    if (!node) return;
    for (i = 0; i < node->nchildren; i++) xml_free(node->children[i]);
    free(node->children);
    for (i = 0; i < node->nattrs; i++) { free(node->attrs[i].name); free(node->attrs[i].value); }
    free(node->attrs);
    free(node->name);
    free(node->text);
    free(node);
}

const char *xml_attr_get(const xml_node *node, const char *name)
{
    int i;
    for (i = 0; i < node->nattrs; i++)
        if (!strcmp(node->attrs[i].name, name)) return node->attrs[i].value;
    return NULL;
}
