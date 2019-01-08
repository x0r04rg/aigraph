#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <time.h>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GL3_IMPLEMENTATION
#include <nuklear.h>
#include "nuklear_sdl_gl3.h"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

#include "node_editor.h"
#include "console.h"

struct console_ops {
    struct print_ops ops;
    struct console *console;
};

static void 
printops_print(struct print_ops *ops, char *string)
{
    console_print(((struct console_ops*)ops)->console, string);
}

static void 
printops_printfv(struct print_ops *ops, char *fmt, va_list args)
{
    console_printfv(((struct console_ops*)ops)->console, fmt, args);
}

int main(void)
{
    /* Platform */
    SDL_Window *win;
    SDL_GLContext glContext;
    int win_width, win_height;
    int running = 1;

    /* GUI */
    struct nk_context *ctx;
    struct nk_colorf bg;

    struct node_editor editor;
    struct console console;
    struct console_ops conops;
    struct node_info *infos;

    console_init(&console);

    conops.ops.print = &printops_print;
    conops.ops.printfv = &printops_printfv;
    conops.console = &console;

    /* SDL setup */
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    win = SDL_CreateWindow("aigraph",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_ALLOW_HIGHDPI);
    glContext = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);
    SDL_GetWindowSize(win, &win_width, &win_height);

    /* OpenGL setup */
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glewExperimental = 1;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to setup GLEW\n");
        exit(1);
    }

    {const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version = (const char *)glGetString(GL_VERSION);
    console_printf(&console, "Renderer: %s (%s)", renderer, vendor);
    console_printf(&console, "Driver: %s", version);}

    ctx = nk_sdl_init(win);
    {struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);
    nk_sdl_font_stash_end();}

    node_editor_init(&editor, infos = fill_infos(), &conops.ops);

    bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;
    float time = SDL_GetTicks() / 1000.0f;
    while (running)
    {
        {
            float new_time = SDL_GetTicks() / 1000.0f;
            ctx->delta_time_seconds = new_time - time;
            time = new_time;
        }

        /* Input */
        SDL_Event evt;
        nk_input_begin(ctx);
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_QUIT) goto cleanup;
            if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_GRAVE)
                console.hidden = !console.hidden;
            nk_sdl_handle_event(&evt);
        } nk_input_end(ctx);

        SDL_GetWindowSize(win, &win_width, &win_height);
        
        node_editor_gui(ctx, &editor, nk_rect(0, 0, win_width, win_height), NK_WINDOW_NO_SCROLLBAR);
        console_gui(ctx, &console, &editor, nk_rect(0, 0, win_width, win_height));

        /* Draw */
        glViewport(0, 0, win_width, win_height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(bg.r, bg.g, bg.b, bg.a);
        /* IMPORTANT: `nk_sdl_render` modifies some global OpenGL state
         * with blending, scissor, face culling, depth test and viewport and
         * defaults everything back into a default state.
         * Make sure to either a.) save and restore or b.) reset your own state after
         * rendering the UI. */
        nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
        SDL_GL_SwapWindow(win);
    }

cleanup:
    node_editor_cleanup(&editor);
    free(infos);
    nk_sdl_shutdown();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

