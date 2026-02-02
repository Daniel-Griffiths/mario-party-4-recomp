/*
 * VI (Video Interface) replacement using SDL2
 */
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#include "dolphin/types.h"
#include "dolphin/vi.h"
#include "dolphin/gx/GXStruct.h"
#include "game/wipe.h"
#include "pc_config.h"

/* SDL globals from pc_main.c */
extern SDL_Window *g_pc_window;
extern SDL_Renderer *g_pc_renderer;
extern SDL_Texture *g_pc_texture;

/* Software framebuffer from gx_pc.c */
extern u32 g_gx_framebuffer[];

/* GX internal state (for accessing the live framebuffer) */
#include "dolphin/gx_state.h"

static u32 g_retrace_count = 0;
static int g_black = 1;
static void *g_next_xfb = NULL;
static GXRenderModeObj g_render_mode;

/* Retrace callbacks */
typedef void (*VIRetraceCallback)(u32);
static VIRetraceCallback g_post_retrace_cb = NULL;
static VIRetraceCallback g_pre_retrace_cb = NULL;

/* ---- Standard render mode objects ---- */
GXRenderModeObj GXNtsc240Ds = { 1, 640, 240, 240, 40, 0, 640, 480, 0, 0, 0,
    { {6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6} }, { 0, 0, 21, 22, 21, 0, 0 } };
GXRenderModeObj GXNtsc240DsAa = { 1, 640, 240, 240, 40, 0, 640, 480, 0, 0, 1,
    { {3,2},{9,6},{3,10},{3,2},{9,6},{3,10},{9,2},{3,6},{9,10},{9,2},{3,6},{9,10} }, { 0, 0, 21, 22, 21, 0, 0 } };
GXRenderModeObj GXNtsc240Int = { 0, 640, 240, 240, 40, 0, 640, 480, 0, 1, 0,
    { {6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6} }, { 0, 0, 21, 22, 21, 0, 0 } };
GXRenderModeObj GXNtsc480IntDf = { 0, 640, 480, 480, 40, 0, 640, 480, 1, 0, 0,
    { {6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6} }, { 8, 8, 10, 12, 10, 8, 8 } };
GXRenderModeObj GXNtsc480Prog = { 2, 640, 480, 480, 40, 0, 640, 480, 0, 0, 0,
    { {6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6} }, { 0, 0, 21, 22, 21, 0, 0 } };
GXRenderModeObj GXNtsc480ProgSoft = { 2, 640, 480, 480, 40, 0, 640, 480, 0, 0, 0,
    { {6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6} }, { 8, 8, 10, 12, 10, 8, 8 } };
GXRenderModeObj GXNtsc480ProgAa = { 2, 640, 242, 480, 40, 0, 640, 480, 0, 0, 1,
    { {3,2},{9,6},{3,10},{3,2},{9,6},{3,10},{9,2},{3,6},{9,10},{9,2},{3,6},{9,10} }, { 4, 8, 12, 16, 12, 8, 4 } };

/* PAL modes */
GXRenderModeObj GXPal264Ds = { 5, 640, 264, 264, 40, 23, 640, 528, 0, 0, 0,
    { {6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6} }, { 0, 0, 21, 22, 21, 0, 0 } };
GXRenderModeObj GXPal264Int = { 4, 640, 264, 264, 40, 23, 640, 528, 0, 1, 0,
    { {6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6} }, { 0, 0, 21, 22, 21, 0, 0 } };
GXRenderModeObj GXPal528IntDf = { 4, 640, 528, 528, 40, 23, 640, 528, 1, 0, 0,
    { {6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6} }, { 8, 8, 10, 12, 10, 8, 8 } };
GXRenderModeObj GXMpal480IntDf = { 8, 640, 480, 480, 40, 0, 640, 480, 1, 0, 0,
    { {6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6} }, { 8, 8, 10, 12, 10, 8, 8 } };
GXRenderModeObj GXEurgb60Hz480IntDf = { 20, 640, 480, 480, 40, 0, 640, 480, 1, 0, 0,
    { {6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6},{6,6} }, { 8, 8, 10, 12, 10, 8, 8 } };

void VIInit(void) {
    printf("[VI] VIInit()\n");
    g_retrace_count = 0;
    g_black = 1;
}

void VIConfigure(const GXRenderModeObj *mode) {
    if (mode) {
        memcpy(&g_render_mode, mode, sizeof(g_render_mode));
    }
}

void VIConfigurePan(u16 xOrg, u16 yOrg, u16 displayWidth, u16 displayHeight) {
    (void)xOrg; (void)yOrg; (void)displayWidth; (void)displayHeight;
}

void VIFlush(void) {
    /* Commit pending configuration - no-op on PC */
}

u32 VIGetTvFormat(void) {
    return 0; /* NTSC */
}

u32 VIGetDTVStatus(void) {
    return 0; /* No progressive */
}

u32 VIGetNextField(void) {
    return 1; /* always top field */
}

void VISetNextFrameBuffer(void *fb) {
    g_next_xfb = fb;
}

void VISetBlack(BOOL black) {
    g_black = black;
    if (!black) {
        printf("[VI] VISetBlack(FALSE) - display enabled\n");
    }
}

/* Upload the GX framebuffer (3D content) to the SDL renderer as an
 * alpha-blended overlay. Called between background sprites (draw_no=0x7F)
 * and foreground sprites (draw_no=0) so that 3D models appear on top of
 * background sprites but under foreground UI sprites. */
void GXPCFlushFramebuffer(void) {
    if (g_pc_renderer && g_pc_texture) {
        /* Use the live internal framebuffer (g_gx.framebuffer) which has
         * the current frame's 3D content, not g_gx_framebuffer which is
         * the display copy from the previous GXCopyDisp call. */
        SDL_UpdateTexture(g_pc_texture, NULL, g_gx.framebuffer, 640 * sizeof(u32));
        SDL_RenderCopy(g_pc_renderer, g_pc_texture, NULL, NULL);
    }
}

void VIWaitForRetrace(void) {
    /* Pump SDL events */
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            printf("[VI] SDL_QUIT received, exiting.\n");
            exit(0);
        }
    }

    /* Fire pre-retrace callback */
    if (g_pre_retrace_cb) {
        g_pre_retrace_cb(g_retrace_count);
    }

    /*
     * Compositing order: GX framebuffer (3D background) was drawn at the end
     * of the previous retrace. Sprites (2D) were drawn on top during the frame
     * via SDL_RenderCopyEx in HuSprDispPC. Now present everything, then prepare
     * the GX framebuffer as background for the next frame.
     */
    if (g_pc_renderer && g_pc_texture) {
        /* Reset sprite draw counter */
        extern int g_pc_spr_draw_count;
        g_pc_spr_draw_count = 0;

        /* Draw wipe overlay on top of everything (sprites included).
         * On real hardware, wipe + sprites + 3D all share the same framebuffer.
         * On PC, sprites bypass GX, so we draw the wipe as an SDL overlay. */
        {
            extern WipeState wipeData;
            if (wipeData.mode != 0 && wipeData.color.a > 0) {
                SDL_SetRenderDrawBlendMode(g_pc_renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(g_pc_renderer,
                    wipeData.color.r, wipeData.color.g, wipeData.color.b, wipeData.color.a);
                SDL_RenderFillRect(g_pc_renderer, NULL);
            }
        }

        /* Present this frame (sprites + GX overlay + wipe) */
        SDL_RenderPresent(g_pc_renderer);

        /* Prepare next frame: clear to black */
        SDL_SetRenderDrawColor(g_pc_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_pc_renderer);
        /* GX framebuffer is uploaded mid-frame by GXPCFlushFramebuffer()
         * between background sprites and foreground sprites. */
    }

    g_retrace_count++;

    /* Fire post-retrace callback (reads gamepad input) */
    if (g_post_retrace_cb) {
        g_post_retrace_cb(g_retrace_count);
    }
}

u32 VIGetRetraceCount(void) {
    return g_retrace_count;
}

/* PC-specific extensions */
void VISetWindowTitle(const char *title) {
    if (g_pc_window) {
        SDL_SetWindowTitle(g_pc_window, title);
    }
}

void VISetWindowFullscreen(BOOL fullscreen) {
    if (g_pc_window) {
        SDL_SetWindowFullscreen(g_pc_window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }
}

BOOL VIGetWindowFullscreen(void) {
    if (!g_pc_window) return FALSE;
    return (SDL_GetWindowFlags(g_pc_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback) {
    VIRetraceCallback old = g_post_retrace_cb;
    g_post_retrace_cb = callback;
    return old;
}

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback) {
    VIRetraceCallback old = g_pre_retrace_cb;
    g_pre_retrace_cb = callback;
    return old;
}
