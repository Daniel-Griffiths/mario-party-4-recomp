/*
 * PC entry point - initializes SDL2 then calls the game's main()
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "pc_config.h"

/* The game declares main(void) - we rename it via the build system */
extern void game_main(void);

/* SDL globals used by vi_pc.c */
SDL_Window *g_pc_window = NULL;
SDL_Renderer *g_pc_renderer = NULL;
SDL_Texture *g_pc_texture = NULL;

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("[PC] Mario Party 4 PC Port starting...\n");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "[PC] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    g_pc_window = SDL_CreateWindow(
        "Mario Party 4",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        PC_SCREEN_WIDTH, PC_SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!g_pc_window) {
        fprintf(stderr, "[PC] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    g_pc_renderer = SDL_CreateRenderer(g_pc_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_pc_renderer) {
        /* Fall back to software */
        g_pc_renderer = SDL_CreateRenderer(g_pc_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_pc_renderer) {
        fprintf(stderr, "[PC] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_pc_window);
        SDL_Quit();
        return 1;
    }

    g_pc_texture = SDL_CreateTexture(g_pc_renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
        PC_SCREEN_WIDTH, PC_SCREEN_HEIGHT);
    /* Alpha blend so cleared pixels (alpha=0) are transparent and
     * background sprites show through areas with no 3D geometry. */
    SDL_SetTextureBlendMode(g_pc_texture, SDL_BLENDMODE_BLEND);
    if (!g_pc_texture) {
        fprintf(stderr, "[PC] SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(g_pc_renderer);
        SDL_DestroyWindow(g_pc_window);
        SDL_Quit();
        return 1;
    }
    /* Enable alpha blending so cleared pixels (alpha=0) are transparent.
     * This lets the GX framebuffer (3D content) overlay on top of
     * background sprites without hiding them. */
    SDL_SetTextureBlendMode(g_pc_texture, SDL_BLENDMODE_BLEND);

    printf("[PC] SDL initialized, calling game_main()...\n");
    game_main();

    /* Cleanup (game loops forever, but just in case) */
    SDL_DestroyTexture(g_pc_texture);
    SDL_DestroyRenderer(g_pc_renderer);
    SDL_DestroyWindow(g_pc_window);
    SDL_Quit();
    return 0;
}
