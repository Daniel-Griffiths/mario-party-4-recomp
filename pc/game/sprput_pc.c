/*
 * PC Sprite Renderer - Renders 2D sprites using SDL2
 * Replaces GX-based HuSprDisp/HuSprTexLoad on PC
 */
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dolphin/types.h"
#include "game/sprite.h"
#include "game/animdata.h"

extern SDL_Renderer *g_pc_renderer;

/* Texture cache to avoid recreating SDL textures every frame */
#define SPR_TEX_CACHE_SIZE 512
typedef struct {
    void *data_ptr;     /* key: pointer to AnimBmpData.data */
    SDL_Texture *tex;
    int w, h;
} SprTexCacheEntry;

static SprTexCacheEntry g_tex_cache[SPR_TEX_CACHE_SIZE];
static int g_tex_cache_count = 0;

/* Convert GC tile-ordered pixels to linear RGBA for SDL */
static void gc_rgba8_to_linear(const u8 *src, u8 *dst, int w, int h) {
    /* GC RGBA8: stored in 4x4 tiles, each tile has AR then GB halves */
    for (int ty = 0; ty < h; ty += 4) {
        for (int tx = 0; tx < w; tx += 4) {
            /* Each tile: 32 bytes AR, then 32 bytes GB */
            const u8 *ar = src;
            const u8 *gb = src + 32;
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = tx + x;
                    int py = ty + y;
                    if (px < w && py < h) {
                        int idx = (py * w + px) * 4;
                        u8 a = *ar++;
                        u8 r = *ar++;
                        u8 g = *gb++;
                        u8 b = *gb++;
                        dst[idx + 0] = r;
                        dst[idx + 1] = g;
                        dst[idx + 2] = b;
                        dst[idx + 3] = a;
                    } else {
                        ar += 2;
                        gb += 2;
                    }
                }
            }
            src += 64;
        }
    }
}

/* Convert GC RGB5A3 tile-ordered pixels to linear RGBA */
static void gc_rgb5a3_to_linear(const u8 *src, u8 *dst, int w, int h) {
    /* GC RGB5A3: 4x4 tiles, 2 bytes per pixel (big-endian) */
    for (int ty = 0; ty < h; ty += 4) {
        for (int tx = 0; tx < w; tx += 4) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = tx + x;
                    int py = ty + y;
                    u16 pixel = (src[0] << 8) | src[1];
                    src += 2;
                    if (px < w && py < h) {
                        int idx = (py * w + px) * 4;
                        if (pixel & 0x8000) {
                            /* RGB555: 1RRRRRGGGGGBBBBB */
                            dst[idx + 0] = ((pixel >> 10) & 0x1F) * 255 / 31;
                            dst[idx + 1] = ((pixel >> 5) & 0x1F) * 255 / 31;
                            dst[idx + 2] = (pixel & 0x1F) * 255 / 31;
                            dst[idx + 3] = 255;
                        } else {
                            /* ARGB3444: 0AAARGGGGGBBBBBB -> actually 0AARRRRGGGGBBBB */
                            dst[idx + 3] = ((pixel >> 12) & 0x7) * 255 / 7;
                            dst[idx + 0] = ((pixel >> 8) & 0xF) * 255 / 15;
                            dst[idx + 1] = ((pixel >> 4) & 0xF) * 255 / 15;
                            dst[idx + 2] = (pixel & 0xF) * 255 / 15;
                        }
                    }
                }
            }
        }
    }
}

/* Convert GC C8 (8-bit indexed) tile-ordered pixels to linear RGBA */
static void gc_c8_to_linear(const u8 *src, const u8 *pal, int pal_count, u8 *dst, int w, int h) {
    /* GC C8: 8x4 tiles, 1 byte per pixel. Palette is RGB5A3 format (big-endian). */
    for (int ty = 0; ty < h; ty += 4) {
        for (int tx = 0; tx < w; tx += 8) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 8; x++) {
                    int px = tx + x;
                    int py = ty + y;
                    u8 ci = *src++;
                    if (px < w && py < h) {
                        int idx = (py * w + px) * 4;
                        if (pal && ci < pal_count) {
                            u16 pixel = (pal[ci * 2] << 8) | pal[ci * 2 + 1];
                            if (pixel & 0x8000) {
                                dst[idx + 0] = ((pixel >> 10) & 0x1F) * 255 / 31;
                                dst[idx + 1] = ((pixel >> 5) & 0x1F) * 255 / 31;
                                dst[idx + 2] = (pixel & 0x1F) * 255 / 31;
                                dst[idx + 3] = 255;
                            } else {
                                dst[idx + 3] = ((pixel >> 12) & 0x7) * 255 / 7;
                                dst[idx + 0] = ((pixel >> 8) & 0xF) * 255 / 15;
                                dst[idx + 1] = ((pixel >> 4) & 0xF) * 255 / 15;
                                dst[idx + 2] = (pixel & 0xF) * 255 / 15;
                            }
                        } else {
                            dst[idx + 0] = dst[idx + 1] = dst[idx + 2] = ci;
                            dst[idx + 3] = 255;
                        }
                    }
                }
            }
        }
    }
}

/* Convert GC IA8 tile-ordered pixels to linear RGBA */
static void gc_ia8_to_linear(const u8 *src, u8 *dst, int w, int h) {
    /* GC IA8: 4x4 tiles, 2 bytes per pixel (intensity + alpha) */
    for (int ty = 0; ty < h; ty += 4) {
        for (int tx = 0; tx < w; tx += 4) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = tx + x;
                    int py = ty + y;
                    u8 a = *src++;
                    u8 i = *src++;
                    if (px < w && py < h) {
                        int idx = (py * w + px) * 4;
                        dst[idx + 0] = i;
                        dst[idx + 1] = i;
                        dst[idx + 2] = i;
                        dst[idx + 3] = a;
                    }
                }
            }
        }
    }
}

static SDL_Texture *create_texture_from_bmp(AnimBmpData *bmp) {
    if (!bmp || !bmp->data || !g_pc_renderer) return NULL;

    int w = bmp->sizeX;
    int h = bmp->sizeY;
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048) return NULL;

    u8 *pixels = (u8 *)calloc(w * h * 4, 1);
    if (!pixels) return NULL;

    u8 fmt = bmp->dataFmt & ANIM_BMP_FMTMASK;
    switch (fmt) {
        case ANIM_BMP_RGBA8:
            gc_rgba8_to_linear(bmp->data, pixels, w, h);
            break;
        case ANIM_BMP_RGB5A3:
        case ANIM_BMP_RGB5A3_DUPE:
            gc_rgb5a3_to_linear(bmp->data, pixels, w, h);
            break;
        case ANIM_BMP_C8:
            gc_c8_to_linear(bmp->data, bmp->palData, bmp->palNum, pixels, w, h);
            break;
        case ANIM_BMP_IA8:
            gc_ia8_to_linear(bmp->data, pixels, w, h);
            break;
        default:
            /* Unsupported format - fill with magenta for debug */
            for (int i = 0; i < w * h; i++) {
                pixels[i*4+0] = 255;
                pixels[i*4+1] = 0;
                pixels[i*4+2] = 255;
                pixels[i*4+3] = 128;
            }
            break;
    }

    SDL_Texture *tex = SDL_CreateTexture(g_pc_renderer,
        SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (tex) {
        SDL_UpdateTexture(tex, NULL, pixels, w * 4);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }
    free(pixels);
    return tex;
}

static SDL_Texture *get_cached_texture(AnimBmpData *bmp) {
    if (!bmp || !bmp->data) return NULL;

    /* Look up in cache */
    for (int i = 0; i < g_tex_cache_count; i++) {
        if (g_tex_cache[i].data_ptr == bmp->data) {
            return g_tex_cache[i].tex;
        }
    }

    /* Create and cache */
    SDL_Texture *tex = create_texture_from_bmp(bmp);
    if (tex && g_tex_cache_count < SPR_TEX_CACHE_SIZE) {
        g_tex_cache[g_tex_cache_count].data_ptr = bmp->data;
        g_tex_cache[g_tex_cache_count].tex = tex;
        g_tex_cache[g_tex_cache_count].w = bmp->sizeX;
        g_tex_cache[g_tex_cache_count].h = bmp->sizeY;
        g_tex_cache_count++;
    }
    return tex;
}

int g_pc_spr_draw_count = 0;

/* Called by HuSprDisp on PC to render a sprite */
void HuSprDispPC(HuSprite *sprite) {
    if (!sprite || !sprite->data || !g_pc_renderer) return;
    g_pc_spr_draw_count++;

    AnimData *anim = sprite->data;
    AnimPatData *pat = sprite->pat_data;
    if (!pat || !pat->layer) {
        return;
    }

    int bmpNum = anim->bmpNum & ANIM_BMP_NUM_MASK;

    /* Render each layer back to front */
    for (int i = pat->layerNum - 1; i >= 0; i--) {
        AnimLayerData *layer = &pat->layer[i];

        if (layer->bmpNo < 0 || layer->bmpNo >= bmpNum) continue;
        AnimBmpData *bmp = &anim->bmp[layer->bmpNo];

        SDL_Texture *tex = get_cached_texture(bmp);
        if (!tex) continue;

        /* Set alpha */
        u8 alpha = (u8)((sprite->a * layer->alpha) / 255.0f);
        SDL_SetTextureAlphaMod(tex, alpha);

        /* Set color modulation */
        SDL_SetTextureColorMod(tex, sprite->r, sprite->g, sprite->b);

        /* Calculate source rect (texture region) */
        SDL_Rect src_rect;
        src_rect.x = layer->startX;
        src_rect.y = layer->startY;
        src_rect.w = layer->sizeX;
        src_rect.h = layer->sizeY;

        /* Calculate destination rect using vertex positions */
        float cx = pat->centerX;
        float cy = pat->centerY;
        float x0 = layer->vtx[0] - cx;
        float y0 = layer->vtx[1] - cy;
        float x1 = layer->vtx[2] - cx;
        float y1 = layer->vtx[5] - cy;

        /* Apply sprite transform */
        float dx = sprite->x + x0 * sprite->scale_x;
        float dy = sprite->y + y0 * sprite->scale_y;
        float dw = (x1 - x0) * sprite->scale_x;
        float dh = (y1 - y0) * sprite->scale_y;

        /* Apply group matrix position offset if available */
        if (sprite->group_mtx) {
            dx += (*sprite->group_mtx)[0][3];
            dy += (*sprite->group_mtx)[1][3];
        }

        SDL_Rect dst_rect;
        dst_rect.x = (int)dx;
        dst_rect.y = (int)dy;
        dst_rect.w = (int)dw;
        dst_rect.h = (int)dh;

        /* Handle flipping */
        SDL_RendererFlip flip = SDL_FLIP_NONE;
        if (layer->flip & ANIM_LAYER_FLIPX) flip |= SDL_FLIP_HORIZONTAL;
        if (layer->flip & ANIM_LAYER_FLIPY) flip |= SDL_FLIP_VERTICAL;

        SDL_RenderCopyEx(g_pc_renderer, tex, &src_rect, &dst_rect,
                         sprite->z_rot, NULL, flip);
    }
}

/* Clear texture cache (call on scene change) */
void HuSprTexCacheClear(void) {
    for (int i = 0; i < g_tex_cache_count; i++) {
        if (g_tex_cache[i].tex) {
            SDL_DestroyTexture(g_tex_cache[i].tex);
        }
    }
    g_tex_cache_count = 0;
}
