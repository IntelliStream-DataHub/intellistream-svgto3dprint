#include "nk_sdl_gl3.h"
#include "glapi.h"

#include <stdlib.h>
#include <string.h>

struct nk_sdl_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
};

struct nk_sdl_device {
    struct nk_buffer cmds;
    struct nk_draw_null_texture tex_null;
    GLuint vbo, vao, ebo;
    GLuint prog;
    GLuint vert_shdr;
    GLuint frag_shdr;
    GLint attrib_pos;
    GLint attrib_uv;
    GLint attrib_col;
    GLint uniform_tex;
    GLint uniform_proj;
    GLuint font_tex;
};

static struct nk_sdl {
    SDL_Window *win;
    struct nk_sdl_device ogl;
    struct nk_context ctx;
    struct nk_font_atlas atlas;
} sdl;

static const char *vertex_shader =
    "#version 150\n"
    "uniform mat4 ProjMtx;\n"
    "in vec2 Position;\n"
    "in vec2 TexCoord;\n"
    "in vec4 Color;\n"
    "out vec2 Frag_UV;\n"
    "out vec4 Frag_Color;\n"
    "void main() {\n"
    "   Frag_UV = TexCoord;\n"
    "   Frag_Color = Color;\n"
    "   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
    "}\n";

static const char *fragment_shader =
    "#version 150\n"
    "precision mediump float;\n"
    "uniform sampler2D Texture;\n"
    "in vec2 Frag_UV;\n"
    "in vec4 Frag_Color;\n"
    "out vec4 Out_Color;\n"
    "void main(){\n"
    "   Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
    "}\n";

static void device_create(void)
{
    struct nk_sdl_device *dev = &sdl.ogl;
    GLint status;
    nk_buffer_init_default(&dev->cmds);
    dev->prog = glCreateProgram();
    dev->vert_shdr = glCreateShader(GL_VERTEX_SHADER);
    dev->frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(dev->vert_shdr, 1, &vertex_shader, 0);
    glShaderSource(dev->frag_shdr, 1, &fragment_shader, 0);
    glCompileShader(dev->vert_shdr);
    glCompileShader(dev->frag_shdr);
    glGetShaderiv(dev->vert_shdr, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) { char log[1024]; glGetShaderInfoLog(dev->vert_shdr, sizeof(log), NULL, log); SDL_Log("UI vertex shader: %s", log); }
    glGetShaderiv(dev->frag_shdr, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) { char log[1024]; glGetShaderInfoLog(dev->frag_shdr, sizeof(log), NULL, log); SDL_Log("UI fragment shader: %s", log); }
    glAttachShader(dev->prog, dev->vert_shdr);
    glAttachShader(dev->prog, dev->frag_shdr);
    glLinkProgram(dev->prog);
    glGetProgramiv(dev->prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) { char log[1024]; glGetProgramInfoLog(dev->prog, sizeof(log), NULL, log); SDL_Log("UI program: %s", log); }

    dev->uniform_tex = glGetUniformLocation(dev->prog, "Texture");
    dev->uniform_proj = glGetUniformLocation(dev->prog, "ProjMtx");
    dev->attrib_pos = glGetAttribLocation(dev->prog, "Position");
    dev->attrib_uv = glGetAttribLocation(dev->prog, "TexCoord");
    dev->attrib_col = glGetAttribLocation(dev->prog, "Color");

    {
        GLsizei vs = sizeof(struct nk_sdl_vertex);
        size_t vp = offsetof(struct nk_sdl_vertex, position);
        size_t vt = offsetof(struct nk_sdl_vertex, uv);
        size_t vc = offsetof(struct nk_sdl_vertex, col);
        glGenBuffers(1, &dev->vbo);
        glGenBuffers(1, &dev->ebo);
        glGenVertexArrays(1, &dev->vao);
        glBindVertexArray(dev->vao);
        glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);
        glEnableVertexAttribArray((GLuint)dev->attrib_pos);
        glEnableVertexAttribArray((GLuint)dev->attrib_uv);
        glEnableVertexAttribArray((GLuint)dev->attrib_col);
        glVertexAttribPointer((GLuint)dev->attrib_pos, 2, GL_FLOAT, GL_FALSE, vs, (void *)vp);
        glVertexAttribPointer((GLuint)dev->attrib_uv, 2, GL_FLOAT, GL_FALSE, vs, (void *)vt);
        glVertexAttribPointer((GLuint)dev->attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (void *)vc);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void device_upload_atlas(const void *image, int width, int height)
{
    struct nk_sdl_device *dev = &sdl.ogl;
    glGenTextures(1, &dev->font_tex);
    glBindTexture(GL_TEXTURE_2D, dev->font_tex);
    /* the atlas is drawn 1:1, nearest filtering keeps glyph edges sharp */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
}

static void device_destroy(void)
{
    struct nk_sdl_device *dev = &sdl.ogl;
    glDeleteShader(dev->vert_shdr);
    glDeleteShader(dev->frag_shdr);
    glDeleteProgram(dev->prog);
    glDeleteTextures(1, &dev->font_tex);
    glDeleteBuffers(1, &dev->vbo);
    glDeleteBuffers(1, &dev->ebo);
    glDeleteVertexArrays(1, &dev->vao);
    nk_buffer_free(&dev->cmds);
}

void nk_sdl_render(enum nk_anti_aliasing aa, int max_vertex_buffer, int max_element_buffer)
{
    struct nk_sdl_device *dev = &sdl.ogl;
    int width, height, display_width, display_height;
    struct nk_vec2 scale;
    GLfloat ortho[4][4] = {
        {2.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, -2.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f, 1.0f},
    };
    SDL_GetWindowSize(sdl.win, &width, &height);
    SDL_GetWindowSizeInPixels(sdl.win, &display_width, &display_height);
    if (width <= 0 || height <= 0) return;
    ortho[0][0] /= (GLfloat)width;
    ortho[1][1] /= (GLfloat)height;
    scale.x = (float)display_width / (float)width;
    scale.y = (float)display_height / (float)height;

    glViewport(0, 0, display_width, display_height);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    glUseProgram(dev->prog);
    glUniform1i(dev->uniform_tex, 0);
    glUniformMatrix4fv(dev->uniform_proj, 1, GL_FALSE, &ortho[0][0]);
    {
        const struct nk_draw_command *cmd;
        void *vertices, *elements;
        const nk_draw_index *offset = NULL;
        struct nk_buffer vbuf, ebuf;

        glBindVertexArray(dev->vao);
        glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

        vertices = malloc((size_t)max_vertex_buffer);
        elements = malloc((size_t)max_element_buffer);
        {
            struct nk_convert_config config;
            static const struct nk_draw_vertex_layout_element vertex_layout[] = {
                {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_sdl_vertex, position)},
                {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_sdl_vertex, uv)},
                {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_sdl_vertex, col)},
                {NK_VERTEX_LAYOUT_END}
            };
            memset(&config, 0, sizeof(config));
            config.vertex_layout = vertex_layout;
            config.vertex_size = sizeof(struct nk_sdl_vertex);
            config.vertex_alignment = NK_ALIGNOF(struct nk_sdl_vertex);
            config.tex_null = dev->tex_null;
            config.circle_segment_count = 22;
            config.curve_segment_count = 22;
            config.arc_segment_count = 22;
            config.global_alpha = 1.0f;
            config.shape_AA = aa;
            config.line_AA = aa;
            nk_buffer_init_fixed(&vbuf, vertices, (nk_size)max_vertex_buffer);
            nk_buffer_init_fixed(&ebuf, elements, (nk_size)max_element_buffer);
            nk_convert(&sdl.ctx, &dev->cmds, &vbuf, &ebuf, &config);
        }
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vbuf.allocated, vertices, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)ebuf.allocated, elements, GL_STREAM_DRAW);
        free(vertices);
        free(elements);

        nk_draw_foreach(cmd, &sdl.ctx, &dev->cmds) {
            if (!cmd->elem_count) continue;
            glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
            glScissor((GLint)(cmd->clip_rect.x * scale.x),
                      (GLint)(((float)height - (cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
                      (GLint)(cmd->clip_rect.w * scale.x),
                      (GLint)(cmd->clip_rect.h * scale.y));
            glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
            offset += cmd->elem_count;
        }
        nk_clear(&sdl.ctx);
        nk_buffer_clear(&dev->cmds);
    }
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
}

static void clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    char *text = SDL_GetClipboardText();
    (void)usr;
    if (text && text[0]) nk_textedit_paste(edit, text, nk_strlen(text));
    SDL_free(text);
}

static void clipboard_copy(nk_handle usr, const char *text, int len)
{
    char *str;
    (void)usr;
    if (!len) return;
    str = (char *)malloc((size_t)len + 1);
    if (!str) return;
    memcpy(str, text, (size_t)len);
    str[len] = 0;
    SDL_SetClipboardText(str);
    free(str);
}

struct nk_context *nk_sdl_init(SDL_Window *win)
{
    sdl.win = win;
    nk_init_default(&sdl.ctx, 0);
    sdl.ctx.clip.copy = clipboard_copy;
    sdl.ctx.clip.paste = clipboard_paste;
    sdl.ctx.clip.userdata = nk_handle_ptr(0);
    device_create();
    SDL_StartTextInput(win);
    return &sdl.ctx;
}

void nk_sdl_font_stash_begin(struct nk_font_atlas **atlas)
{
    nk_font_atlas_init_default(&sdl.atlas);
    nk_font_atlas_begin(&sdl.atlas);
    *atlas = &sdl.atlas;
}

void nk_sdl_font_stash_end(void)
{
    const void *image;
    int w, h;
    image = nk_font_atlas_bake(&sdl.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    device_upload_atlas(image, w, h);
    nk_font_atlas_end(&sdl.atlas, nk_handle_id((int)sdl.ogl.font_tex), &sdl.ogl.tex_null);
    if (sdl.atlas.default_font) nk_style_set_font(&sdl.ctx, &sdl.atlas.default_font->handle);
}

int nk_sdl_handle_event(const SDL_Event *evt)
{
    struct nk_context *ctx = &sdl.ctx;
    switch (evt->type) {
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_KEY_DOWN: {
        int down = evt->type == SDL_EVENT_KEY_DOWN;
        SDL_Keycode sym = evt->key.key;
        int ctrl = (evt->key.mod & SDL_KMOD_CTRL) != 0;
        if (sym == SDLK_RSHIFT || sym == SDLK_LSHIFT) nk_input_key(ctx, NK_KEY_SHIFT, down);
        else if (sym == SDLK_DELETE) nk_input_key(ctx, NK_KEY_DEL, down);
        else if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) nk_input_key(ctx, NK_KEY_ENTER, down);
        else if (sym == SDLK_TAB) nk_input_key(ctx, NK_KEY_TAB, down);
        else if (sym == SDLK_BACKSPACE) nk_input_key(ctx, NK_KEY_BACKSPACE, down);
        else if (sym == SDLK_HOME) { nk_input_key(ctx, NK_KEY_TEXT_START, down); nk_input_key(ctx, NK_KEY_SCROLL_START, down); }
        else if (sym == SDLK_END) { nk_input_key(ctx, NK_KEY_TEXT_END, down); nk_input_key(ctx, NK_KEY_SCROLL_END, down); }
        else if (sym == SDLK_PAGEDOWN) nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down);
        else if (sym == SDLK_PAGEUP) nk_input_key(ctx, NK_KEY_SCROLL_UP, down);
        else if (sym == SDLK_Z) nk_input_key(ctx, NK_KEY_TEXT_UNDO, down && ctrl);
        else if (sym == SDLK_R) nk_input_key(ctx, NK_KEY_TEXT_REDO, down && ctrl);
        else if (sym == SDLK_C) nk_input_key(ctx, NK_KEY_COPY, down && ctrl);
        else if (sym == SDLK_V) nk_input_key(ctx, NK_KEY_PASTE, down && ctrl);
        else if (sym == SDLK_X) nk_input_key(ctx, NK_KEY_CUT, down && ctrl);
        else if (sym == SDLK_B) nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down && ctrl);
        else if (sym == SDLK_E) nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down && ctrl);
        else if (sym == SDLK_A) nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL, down && ctrl);
        else if (sym == SDLK_UP) nk_input_key(ctx, NK_KEY_UP, down);
        else if (sym == SDLK_DOWN) nk_input_key(ctx, NK_KEY_DOWN, down);
        else if (sym == SDLK_LEFT) {
            if (ctrl) nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down);
            else nk_input_key(ctx, NK_KEY_LEFT, down);
        } else if (sym == SDLK_RIGHT) {
            if (ctrl) nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
            else nk_input_key(ctx, NK_KEY_RIGHT, down);
        } else return 0;
        return 1;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        int down = evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        int x = (int)evt->button.x, y = (int)evt->button.y;
        if (evt->button.button == SDL_BUTTON_LEFT) {
            if (evt->button.clicks > 1) nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, down);
            nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);
        } else if (evt->button.button == SDL_BUTTON_MIDDLE) nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down);
        else if (evt->button.button == SDL_BUTTON_RIGHT) nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down);
        return 1;
    }
    case SDL_EVENT_MOUSE_MOTION:
        if (ctx->input.mouse.grabbed) {
            int x = (int)ctx->input.mouse.prev.x, y = (int)ctx->input.mouse.prev.y;
            nk_input_motion(ctx, x + (int)evt->motion.xrel, y + (int)evt->motion.yrel);
        } else nk_input_motion(ctx, (int)evt->motion.x, (int)evt->motion.y);
        return 1;
    case SDL_EVENT_TEXT_INPUT: {
        nk_glyph glyph;
        memcpy(glyph, evt->text.text, NK_UTF_SIZE);
        nk_input_glyph(ctx, glyph);
        return 1;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        nk_input_scroll(ctx, nk_vec2(evt->wheel.x, evt->wheel.y));
        return 1;
    default:
        return 0;
    }
}

void nk_sdl_shutdown(void)
{
    nk_font_atlas_clear(&sdl.atlas);
    nk_free(&sdl.ctx);
    device_destroy();
    memset(&sdl, 0, sizeof(sdl));
}
