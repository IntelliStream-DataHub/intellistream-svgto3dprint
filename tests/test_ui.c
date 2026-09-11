/* Headless checks of the Nuklear widgets the GUI relies on, run without a
 * window: input is fed by hand and the widgets are laid out in a frame.
 * Exit status is non-zero when a check fails. */
#include "nk_config.h"
#include "panel_undo.h"

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

static void st(panel_undo_state *s, int n)
{
    memset(s, 0, sizeof *s);
    s->params.width_mm = (double)n;
    s->bed_w = (float)n;
}

static void test_panel_undo_hotkey(void)
{
    CHECK(panel_undo_hotkey(1, 0, 0, 'z') == 1);
    CHECK(panel_undo_hotkey(1, 1, 0, 'z') == 2);
    CHECK(panel_undo_hotkey(1, 0, 0, 'y') == 2);
    CHECK(panel_undo_hotkey(0, 0, 0, 'z') == 0);
    CHECK(panel_undo_hotkey(1, 0, 1, 'z') == 0);
    CHECK(panel_undo_hotkey(1, 1, 0, 'y') == 0);
    CHECK(panel_undo_hotkey(1, 0, 0, 'x') == 0);
}

static void test_panel_undo_stack(void)
{
    panel_undo u;
    panel_undo_state a;
    const panel_undo_state *s;
    panel_undo_reset(&u, NULL);
    CHECK(!panel_undo_can_undo(&u) && !panel_undo_can_redo(&u));

    st(&a, 10); panel_undo_record(&u, &a, 0);
    CHECK(u.n == 1 && u.cur == 0);
    CHECK(!panel_undo_can_undo(&u));

    st(&a, 20); panel_undo_record(&u, &a, 0);
    st(&a, 30); panel_undo_record(&u, &a, 0);
    CHECK(u.n == 3 && u.cur == 2);
    CHECK(panel_undo_can_undo(&u) && !panel_undo_can_redo(&u));

    s = panel_undo_undo(&u);
    CHECK(s && s->params.width_mm == 20 && s->bed_w == 20);
    s = panel_undo_undo(&u);
    CHECK(s && s->params.width_mm == 10);
    CHECK(!panel_undo_can_undo(&u) && panel_undo_can_redo(&u));
    CHECK(panel_undo_undo(&u) == NULL);

    s = panel_undo_redo(&u);
    CHECK(s && s->params.width_mm == 20);
    s = panel_undo_redo(&u);
    CHECK(s && s->params.width_mm == 30);
    CHECK(panel_undo_redo(&u) == NULL);

    /* a new edit after undo drops the redo branch */
    panel_undo_undo(&u);
    st(&a, 99); panel_undo_record(&u, &a, 0);
    CHECK(u.n == 3 && u.cur == 2);
    CHECK(u.hist[2].params.width_mm == 99);
    CHECK(!panel_undo_can_redo(&u));
}

static void test_panel_undo_coalesce(void)
{
    panel_undo u;
    panel_undo_state a;
    const panel_undo_state *s;
    st(&a, 1); panel_undo_reset(&u, &a);
    st(&a, 2); panel_undo_record(&u, &a, 1);   /* start a drag */
    st(&a, 3); panel_undo_record(&u, &a, 1);
    st(&a, 4); panel_undo_record(&u, &a, 1);
    CHECK(u.n == 2 && u.cur == 1);
    CHECK(u.hist[0].params.width_mm == 1);
    CHECK(u.hist[1].params.width_mm == 4);
    s = panel_undo_undo(&u);
    CHECK(s && s->params.width_mm == 1);

    /* a click (no coalesce) is its own step */
    st(&a, 5); panel_undo_record(&u, &a, 0);
    st(&a, 6); panel_undo_record(&u, &a, 0);
    CHECK(u.n == 3 && u.cur == 2);
    s = panel_undo_undo(&u);
    CHECK(s && s->params.width_mm == 5);
}

/* Gestures: one step per drag or per field commit, never one step for two fields. */
static void test_panel_undo_gestures(void)
{
    panel_undo u;
    panel_undo_state a;
    st(&a, 1); panel_undo_reset(&u, &a);
    /* typing into field A commits on the click that opens field B: two steps */
    st(&a, 2); panel_undo_record(&u, &a, 0x80000001u);
    st(&a, 3); panel_undo_record(&u, &a, 0x80000002u);
    CHECK(u.n == 3 && u.cur == 2);
    /* the same gesture coalesces, a different one does not */
    st(&a, 4); panel_undo_record(&u, &a, 0x80000002u);
    CHECK(u.n == 3 && u.hist[2].params.width_mm == 4);
    /* a drag, a release with nothing changed, another drag: two steps */
    st(&a, 5); panel_undo_record(&u, &a, 1);
    st(&a, 6); panel_undo_record(&u, &a, 1);
    panel_undo_record(&u, &a, 0);
    st(&a, 7); panel_undo_record(&u, &a, 1);
    CHECK(u.n == 5 && u.hist[3].params.width_mm == 6 && u.hist[4].params.width_mm == 7);
    /* recording the state an undo just restored keeps the redo branch */
    panel_undo_undo(&u);
    st(&a, 6); panel_undo_record(&u, &a, 0);
    CHECK(u.n == 5 && u.cur == 3 && panel_undo_can_redo(&u));
}

static void test_panel_undo_overflow(void)
{
    panel_undo u;
    panel_undo_state a;
    int i;
    st(&a, 0); panel_undo_reset(&u, &a);
    for (i = 1; i <= PANEL_UNDO_MAX + 5; i++) { st(&a, i); panel_undo_record(&u, &a, 0); }
    CHECK(u.n == PANEL_UNDO_MAX);
    CHECK(u.cur == PANEL_UNDO_MAX - 1);
    CHECK(u.hist[0].params.width_mm == (double)(5 + 1)); /* oldest dropped */
    CHECK(u.hist[u.cur].params.width_mm == (double)(PANEL_UNDO_MAX + 5));
}

int main(void)
{
    test_color_picker_drag();
    test_panel_undo_hotkey();
    test_panel_undo_stack();
    test_panel_undo_coalesce();
    test_panel_undo_gestures();
    test_panel_undo_overflow();
    printf("test_ui: %d checks, %d failed\n", ncheck, nfail);
    return nfail ? 1 : 0;
}
