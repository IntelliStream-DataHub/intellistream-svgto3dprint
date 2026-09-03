/* Nuklear backend for SDL3 + OpenGL 3.2/3.3 core. */
#ifndef LOGO3D_NK_SDL_GL3_H
#define LOGO3D_NK_SDL_GL3_H

#include "nk_config.h"
#include <SDL3/SDL.h>

struct nk_context *nk_sdl_init(SDL_Window *win);
void nk_sdl_font_stash_begin(struct nk_font_atlas **atlas);
void nk_sdl_font_stash_end(void);
/* Feed an SDL event; returns 1 when the event was consumed by the UI. */
int nk_sdl_handle_event(const SDL_Event *evt);
/* Call after nk_input_end and building the UI. */
void nk_sdl_render(enum nk_anti_aliasing aa, int max_vertex_buffer, int max_element_buffer);
void nk_sdl_shutdown(void);

#endif
