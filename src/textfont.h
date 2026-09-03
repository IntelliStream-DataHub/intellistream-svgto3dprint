/* TrueType/OpenType fonts for SVG <text>: system font lookup and glyph outlines. */
#ifndef LOGO3D_TEXTFONT_H
#define LOGO3D_TEXTFONT_H

typedef struct textfont textfont;

/* Find and load a font for a CSS font-family list ("Helvetica, Arial, sans-serif"),
 * weight and style.  `override` (may be NULL) is a font file used for everything.
 * Returns NULL when no usable font exists.  Fonts are cached; do not free. */
textfont *textfont_open(const char *family, int bold, int italic, const char *override);
const char *textfont_path(const textfont *f);
/* Factor from font units to an em size in user units. */
double textfont_scale(const textfont *f, double em_size);
double textfont_advance(textfont *f, int codepoint);          /* font units */
double textfont_kern(textfont *f, int cp1, int cp2);          /* font units */
void textfont_vmetrics(const textfont *f, double *ascent, double *descent, double *linegap);

typedef struct {
    void (*move)(void *ud, double x, double y);
    void (*line)(void *ud, double x, double y);
    void (*quad)(void *ud, double cx, double cy, double x, double y);
    void (*cubic)(void *ud, double c1x, double c1y, double c2x, double c2y, double x, double y);
    void (*close)(void *ud);
} textfont_sink;

/* Glyph outline in font units (y up); contours are reported closed. */
void textfont_outline(textfont *f, int codepoint, const textfont_sink *sink, void *ud);

/* Decode one UTF-8 code point, advancing *s.  Invalid bytes yield U+FFFD. */
int textfont_utf8_next(const char **s);

/* Release every cached font (at exit). */
void textfont_cleanup(void);

#endif
