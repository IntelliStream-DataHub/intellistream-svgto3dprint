/* Headless checks of the Nuklear widgets the GUI relies on, run without a
 * window: input is fed by hand and the widgets are laid out in a frame.
 * Exit status is non-zero when a check fails. */
#include "nk_config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int nfail = 0, ncheck = 0;

static void check_(int ok, const char *what, int line)
{
    ncheck++;
    if (!ok) { nfail++; fprintf(stderr, "FAIL test_ui.c:%d: %s\n", line, what); }
}
#define CHECK(c) check_((c) != 0, #c, __LINE__)

static float text_width(nk_handle h, float height, const char *s, int len)
{
    (void)h; (void)s;
    return height * 0.5f * (float)len;
}

/* One frame of a window holding a colour picker, with the mouse at (x, y)
 * and the left button in the given state. */
static struct nk_colorf picker_frame(struct nk_context *ctx, struct nk_colorf c, float x, float y, int down, int event)
{
    nk_input_begin(ctx);
    nk_input_motion(ctx, (int)x, (int)y);
    if (event) nk_input_button(ctx, NK_BUTTON_LEFT, (int)x, (int)y, down ? nk_true : nk_false);
    nk_input_end(ctx);
    if (nk_begin(ctx, "picker", nk_rect(0, 0, 400, 400), NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, 200, 1);
        c = nk_color_picker(ctx, c, NK_RGB);
    }
    nk_end(ctx);
    nk_clear(ctx);
    return c;
}

static void test_color_picker_drag(void)
{
    struct nk_context ctx;
    struct nk_user_font font;
    struct nk_colorf c = {0.2f, 0.3f, 0.4f, 1.0f}, c1;
    memset(&font, 0, sizeof font);
    font.height = 14;
    font.width = text_width;
    CHECK(nk_init_default(&ctx, &font));

    /* press in the middle of the colour matrix: some colour of that hue */
    c1 = picker_frame(&ctx, c, 60, 60, 1, 1);
    CHECK(fabsf(c1.r - c.r) > 0.01f || fabsf(c1.g - c.g) > 0.01f || fabsf(c1.b - c.b) > 0.01f);
    CHECK(!(c1.r > 0.99f && c1.g > 0.99f && c1.b > 0.99f));
    /* drag past the matrix' top-left corner with the button held: clamped to
     * saturation 0, value 1, which is pure white */
    c1 = picker_frame(&ctx, c1, -40, -40, 1, 0);
    CHECK(c1.r > 0.999f && c1.g > 0.999f && c1.b > 0.999f);
    /* drag past the bottom edge: value 0, black */
    c1 = picker_frame(&ctx, c1, 60, 900, 1, 0);
    CHECK(c1.r < 0.001f && c1.g < 0.001f && c1.b < 0.001f);
    /* released outside: the mouse moving around must not change anything */
    c1 = picker_frame(&ctx, c1, 60, 900, 0, 1);
    c1 = picker_frame(&ctx, c1, 60, 60, 0, 0);
    CHECK(c1.r < 0.001f && c1.g < 0.001f && c1.b < 0.001f);
    /* a press that started outside the matrix does not drag it */
    c1.r = 0.5f; c1.g = 0.5f; c1.b = 0.5f;
    c1 = picker_frame(&ctx, c1, 390, 390, 1, 1);
    c1 = picker_frame(&ctx, c1, 60, 60, 1, 0);
    CHECK(fabsf(c1.r - 0.5f) < 0.001f && fabsf(c1.g - 0.5f) < 0.001f && fabsf(c1.b - 0.5f) < 0.001f);
    nk_free(&ctx);
}

int main(void)
{
    test_color_picker_drag();
    printf("test_ui: %d checks, %d failed\n", ncheck, nfail);
    return nfail ? 1 : 0;
}
