#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC          /* Nuklear carries its own copy of stb_truetype */
#include "stb_truetype.h"
#include "textfont.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

struct textfont {
    char *path;
    unsigned char *data;
    stbtt_fontinfo info;
    int ok;
};

/* ---- font file list ------------------------------------------------- */

static char **font_files = NULL;
static int nfont_files = 0, cfont_files = 0, files_scanned = 0;

static int ext_is(const char *ext, const char *want)
{
    while (*ext && *want) {
        if (tolower((unsigned char)*ext) != tolower((unsigned char)*want)) return 0;
        ext++; want++;
    }
    return *ext == *want;
}

static void add_font_file(const char *path)
{
    size_t n = strlen(path);
    const char *ext = n > 4 ? path + n - 4 : "";
    if (!ext_is(ext, ".ttf") && !ext_is(ext, ".otf") && !ext_is(ext, ".ttc")) return;
    if (nfont_files == cfont_files) {
        cfont_files = cfont_files ? cfont_files * 2 : 256;
        font_files = (char **)realloc(font_files, sizeof(char *) * (size_t)cfont_files);
    }
    font_files[nfont_files] = (char *)malloc(n + 1);
    memcpy(font_files[nfont_files], path, n + 1);
    nfont_files++;
}

static void scan_dir(const char *dir, int depth)
{
#ifdef _WIN32
    char pattern[1024];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    if (depth > 5) return;
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char path[1024];
        if (fd.cFileName[0] == '.') continue;
        snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) scan_dir(path, depth + 1);
        else add_font_file(path);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d;
    struct dirent *e;
    if (depth > 5) return;
    d = opendir(dir);
    if (!d) return;
    while ((e = readdir(d)) != NULL) {
        char path[1024];
        struct stat st;
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) scan_dir(path, depth + 1);
        else if (S_ISREG(st.st_mode)) add_font_file(path);
    }
    closedir(d);
#endif
}

static void scan_fonts(void)
{
    const char *home;
    char buf[1024];
    if (files_scanned) return;
    files_scanned = 1;
#ifdef _WIN32
    scan_dir("C:\\Windows\\Fonts", 0);
    home = getenv("LOCALAPPDATA");
    if (home) { snprintf(buf, sizeof(buf), "%s\\Microsoft\\Windows\\Fonts", home); scan_dir(buf, 0); }
#elif defined(__APPLE__)
    scan_dir("/System/Library/Fonts", 0);
    scan_dir("/Library/Fonts", 0);
    home = getenv("HOME");
    if (home) { snprintf(buf, sizeof(buf), "%s/Library/Fonts", home); scan_dir(buf, 0); }
#else
    scan_dir("/usr/share/fonts", 0);
    scan_dir("/usr/local/share/fonts", 0);
    home = getenv("HOME");
    if (home) {
        snprintf(buf, sizeof(buf), "%s/.fonts", home); scan_dir(buf, 0);
        snprintf(buf, sizeof(buf), "%s/.local/share/fonts", home); scan_dir(buf, 0);
    }
#endif
}

/* lowercase letters and digits only */
static void normalise(const char *s, char *out, size_t cap)
{
    size_t n = 0;
    for (; *s && n + 1 < cap; s++)
        if (isalnum((unsigned char)*s)) out[n++] = (char)tolower((unsigned char)*s);
    out[n] = 0;
}

static void base_name(const char *path, char *out, size_t cap)
{
    const char *b = strrchr(path, '/');
    const char *b2 = strrchr(path, '\\');
    const char *dot;
    size_t n;
    if (b2 && (!b || b2 > b)) b = b2;
    b = b ? b + 1 : path;
    dot = strrchr(b, '.');
    n = dot ? (size_t)(dot - b) : strlen(b);
    if (n >= cap) n = cap - 1;
    memcpy(out, b, n);
    out[n] = 0;
}

static int score_file(const char *path, const char *fam, int bold, int italic)
{
    char base[256], norm[256];
    int s, has_bold, has_it;
    base_name(path, base, sizeof(base));
    normalise(base, norm, sizeof(norm));
    if (!strstr(norm, fam)) return -1;
    s = 200 - (int)(strlen(norm) - strlen(fam)) * 2;
    has_bold = strstr(norm, "bold") != NULL || strstr(norm, "heavy") != NULL;
    has_it = strstr(norm, "italic") != NULL || strstr(norm, "oblique") != NULL;
    s += (has_bold == (bold != 0)) ? 60 : -40;
    s += (has_it == (italic != 0)) ? 60 : -40;
    if (strstr(norm, "narrow") || strstr(norm, "condensed") || strstr(norm, "light") || strstr(norm, "thin") ||
        strstr(norm, "black") || strstr(norm, "extra") || strstr(norm, "semi") || strstr(norm, "medium"))
        s -= 50;
    if (strstr(norm, "mono") && !strstr(fam, "mono")) s -= 60;
    if (strstr(norm, "serif") && !strstr(fam, "serif")) s -= 60;   /* "sans" families vs serif files */
    return s;
}

static const char *sans_list[] = {"liberationsans", "arimo", "dejavusans", "freesans", "notosans", "arial", "helvetica", "roboto", "opensans", "segoeui", NULL};
static const char *serif_list[] = {"liberationserif", "tinos", "dejavuserif", "freeserif", "notoserif", "timesnewroman", "times", "georgia", NULL};
static const char *mono_list[] = {"liberationmono", "cousine", "dejavusansmono", "freemono", "notosansmono", "couriernew", "courier", "consolas", NULL};

/* CSS generic families and vague system names: never matched against file
 * names (a font called "SansSerif" would win over Liberation Sans), only
 * resolved through the substitute lists. */
static int is_generic_family(const char *fam)
{
    return !strcmp(fam, "sansserif") || !strcmp(fam, "serif") || !strcmp(fam, "monospace") || !strcmp(fam, "cursive") ||
           !strcmp(fam, "fantasy") || !strcmp(fam, "system") || !strcmp(fam, "systemui") || !strcmp(fam, "uisansserif") ||
           !strcmp(fam, "uiserif") || !strcmp(fam, "uimonospace") || !strcmp(fam, "math") || !strcmp(fam, "emoji");
}

static const char **alias_list(const char *fam)
{
    if (!strcmp(fam, "sansserif") || !strcmp(fam, "helvetica") || !strcmp(fam, "helveticaneue") || !strcmp(fam, "arial") ||
        !strcmp(fam, "arialmt") || !strcmp(fam, "verdana") || !strcmp(fam, "tahoma") || !strcmp(fam, "calibri") ||
        !strcmp(fam, "segoeui") || !strcmp(fam, "roboto") || !strcmp(fam, "opensans") || !strcmp(fam, "lato") ||
        !strcmp(fam, "inter") || !strcmp(fam, "system") || !strcmp(fam, "systemui"))
        return sans_list;
    if (!strcmp(fam, "serif") || !strcmp(fam, "times") || !strcmp(fam, "timesnewroman") || !strcmp(fam, "georgia") || !strcmp(fam, "cambria"))
        return serif_list;
    if (!strcmp(fam, "monospace") || !strcmp(fam, "courier") || !strcmp(fam, "couriernew") || !strcmp(fam, "consolas") || !strcmp(fam, "menlo"))
        return mono_list;
    return sans_list;
}

static const char *best_file_for(const char *fam, int bold, int italic, int *best_score)
{
    int i, best = -1;
    const char *path = NULL;
    for (i = 0; i < nfont_files; i++) {
        int s = score_file(font_files[i], fam, bold, italic);
        if (s > best) { best = s; path = font_files[i]; }
    }
    *best_score = best;
    return path;
}

static const char *find_font_file(const char *family, int bold, int italic)
{
    char fam[128];
    const char *s = family ? family : "sans-serif";
    const char *found = NULL;
    scan_fonts();
    /* try each family of the CSS list in order, then its substitutes */
    while (*s) {
        const char *comma = strchr(s, ',');
        size_t len = comma ? (size_t)(comma - s) : strlen(s);
        char one[128];
        const char **aliases;
        int score, k;
        if (len >= sizeof(one)) len = sizeof(one) - 1;
        memcpy(one, s, len);
        one[len] = 0;
        normalise(one, fam, sizeof(fam));
        if (fam[0]) {
            if (!is_generic_family(fam)) {
                found = best_file_for(fam, bold, italic, &score);
                if (found && score > 0) return found;
            }
            aliases = alias_list(fam);
            for (k = 0; aliases[k]; k++) {
                found = best_file_for(aliases[k], bold, italic, &score);
                if (found && score > 0) return found;
            }
        }
        if (!comma) break;
        s = comma + 1;
    }
    {
        int k, score;
        for (k = 0; sans_list[k]; k++) {
            found = best_file_for(sans_list[k], bold, italic, &score);
            if (found && score > 0) return found;
        }
        /* any font at all, preferring a regular one */
        for (k = 0; sans_list[k]; k++) {
            found = best_file_for(sans_list[k], 0, 0, &score);
            if (found && score > 0) return found;
        }
    }
    return nfont_files > 0 ? font_files[0] : NULL;
}

/* ---- loaded font cache ------------------------------------------------ */

static textfont *cache = NULL;
static int ncache = 0;

static textfont *load_font(const char *path)
{
    int i;
    FILE *f;
    long n;
    textfont *tf;
    for (i = 0; i < ncache; i++) if (!strcmp(cache[i].path, path)) return cache[i].ok ? &cache[i] : NULL;
    cache = (textfont *)realloc(cache, sizeof(textfont) * (size_t)(ncache + 1));
    tf = &cache[ncache++];
    memset(tf, 0, sizeof(*tf));
    tf->path = (char *)malloc(strlen(path) + 1);
    strcpy(tf->path, path);
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    tf->data = (unsigned char *)malloc((size_t)n);
    if (fread(tf->data, 1, (size_t)n, f) != (size_t)n) { fclose(f); return NULL; }
    fclose(f);
    if (!stbtt_InitFont(&tf->info, tf->data, stbtt_GetFontOffsetForIndex(tf->data, 0))) return NULL;
    tf->ok = 1;
    return tf;
}

textfont *textfont_open(const char *family, int bold, int italic, const char *override)
{
    const char *path;
    if (override && override[0]) {
        textfont *tf = load_font(override);
        if (tf) return tf;
    }
    path = find_font_file(family, bold, italic);
    if (!path) return NULL;
    return load_font(path);
}

const char *textfont_path(const textfont *f)
{
    return f ? f->path : "";
}

double textfont_scale(const textfont *f, double em_size)
{
    return f ? stbtt_ScaleForMappingEmToPixels(&f->info, (float)em_size) : 0;
}

double textfont_advance(textfont *f, int codepoint)
{
    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&f->info, codepoint, &adv, &lsb);
    return adv;
}

double textfont_kern(textfont *f, int cp1, int cp2)
{
    return stbtt_GetCodepointKernAdvance(&f->info, cp1, cp2);
}

void textfont_vmetrics(const textfont *f, double *ascent, double *descent, double *linegap)
{
    int a = 0, d = 0, g = 0;
    stbtt_GetFontVMetrics(&f->info, &a, &d, &g);
    *ascent = a; *descent = d; *linegap = g;
}

void textfont_outline(textfont *f, int codepoint, const textfont_sink *sink, void *ud)
{
    stbtt_vertex *v = NULL;
    int n = stbtt_GetCodepointShape(&f->info, codepoint, &v), i, open = 0;
    for (i = 0; i < n; i++) {
        switch (v[i].type) {
        case STBTT_vmove:
            if (open) sink->close(ud);
            sink->move(ud, v[i].x, v[i].y);
            open = 1;
            break;
        case STBTT_vline:
            sink->line(ud, v[i].x, v[i].y);
            break;
        case STBTT_vcurve:
            sink->quad(ud, v[i].cx, v[i].cy, v[i].x, v[i].y);
            break;
        case STBTT_vcubic:
            sink->cubic(ud, v[i].cx, v[i].cy, v[i].cx1, v[i].cy1, v[i].x, v[i].y);
            break;
        default:
            break;
        }
    }
    if (open) sink->close(ud);
    if (v) stbtt_FreeShape(&f->info, v);
}

int textfont_utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    int cp, extra, i;
    if (p[0] < 0x80) { cp = p[0]; extra = 0; }
    else if ((p[0] & 0xE0) == 0xC0) { cp = p[0] & 0x1F; extra = 1; }
    else if ((p[0] & 0xF0) == 0xE0) { cp = p[0] & 0x0F; extra = 2; }
    else if ((p[0] & 0xF8) == 0xF0) { cp = p[0] & 0x07; extra = 3; }
    else { *s += 1; return 0xFFFD; }
    for (i = 1; i <= extra; i++) {
        if ((p[i] & 0xC0) != 0x80) { *s += i; return 0xFFFD; }
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    *s += extra + 1;
    return cp;
}

void textfont_cleanup(void)
{
    int i;
    for (i = 0; i < ncache; i++) { free(cache[i].path); free(cache[i].data); }
    free(cache);
    cache = NULL;
    ncache = 0;
    for (i = 0; i < nfont_files; i++) free(font_files[i]);
    free(font_files);
    font_files = NULL;
    nfont_files = cfont_files = 0;
    files_scanned = 0;
}
