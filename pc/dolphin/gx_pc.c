/*
 * GX (Graphics) replacement for PC — Software Rasterizer
 *
 * Implements vertex arrays, display list recording/replay,
 * modelview/projection transform, triangle rasterization,
 * texture decoding, and simplified TEV evaluation.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "dolphin/types.h"
#include "dolphin/gx.h"
#include "pc_config.h"
#include "dolphin/gx_state.h"

/* Global GX state */
GXState g_gx;

/* Alias for vi_pc.c to access the framebuffer */
u32 g_gx_framebuffer[GX_FB_WIDTH * GX_FB_HEIGHT];

/* GX FIFO object (opaque on GC, we just need something to return) */
static GXFifoObj g_fifo_obj;

/* Debug counters (used across GXCopyDisp and GXCallDisplayList) */
static int g_gx_dl_call_count = 0;
static int g_gx_dl_fail_null = 0;
static int g_gx_dl_fail_magic = 0;
static int g_gx_dl_ok = 0;

/* ---- Internal: PC-side texture object layout ----
 * GXTexObj has 22 u32 on PC. We overlay our own fields. */
typedef struct {
    void *image_ptr;      /* 0: pointer to raw GC texture data */
    u16 width;            /* 8 */
    u16 height;           /* 10 */
    u32 format;           /* 12: GXTexFmt */
    u32 wrap_s;           /* 16: GXTexWrapMode */
    u32 wrap_t;           /* 20: GXTexWrapMode */
    u32 mipmap;           /* 24 */
    u32 _pad[14];         /* fill to 22*4 = 88 bytes */
} GXTexObjPC;

_Static_assert(sizeof(GXTexObjPC) == sizeof(GXTexObj), "GXTexObjPC size mismatch");

/* ================================================================
 * Forward declarations for rasterizer
 * ================================================================ */
static void rasterize_primitives(void);
static void rasterize_triangle(GXSWVertex *v0, GXSWVertex *v1, GXSWVertex *v2);
static void transform_vertex(GXSWVertex *in, float *out_screen, float *out_w);
static void dl_record(DLEntry *entry);

/* ================================================================
 * Texture decoding
 * ================================================================ */
static void gc_rgba8_decode(const u8 *src, u32 *dst, int w, int h) {
    for (int ty = 0; ty < h; ty += 4) {
        for (int tx = 0; tx < w; tx += 4) {
            const u8 *ar = src;
            const u8 *gb = src + 32;
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = tx + x, py = ty + y;
                    if (px < w && py < h) {
                        u8 a = *ar++; u8 r = *ar++;
                        u8 g = *gb++; u8 b = *gb++;
                        dst[py * w + px] = (r << 24) | (g << 16) | (b << 8) | a;
                    } else { ar += 2; gb += 2; }
                }
            }
            src += 64;
        }
    }
}

static void gc_rgb5a3_decode(const u8 *src, u32 *dst, int w, int h) {
    for (int ty = 0; ty < h; ty += 4) {
        for (int tx = 0; tx < w; tx += 4) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = tx + x, py = ty + y;
                    u16 pixel = (src[0] << 8) | src[1]; src += 2;
                    if (px < w && py < h) {
                        u8 r, g, b, a;
                        if (pixel & 0x8000) {
                            r = ((pixel >> 10) & 0x1F) * 255 / 31;
                            g = ((pixel >> 5) & 0x1F) * 255 / 31;
                            b = (pixel & 0x1F) * 255 / 31;
                            a = 255;
                        } else {
                            a = ((pixel >> 12) & 0x7) * 255 / 7;
                            r = ((pixel >> 8) & 0xF) * 255 / 15;
                            g = ((pixel >> 4) & 0xF) * 255 / 15;
                            b = (pixel & 0xF) * 255 / 15;
                        }
                        dst[py * w + px] = (r << 24) | (g << 16) | (b << 8) | a;
                    }
                }
            }
        }
    }
}

static void gc_rgb565_decode(const u8 *src, u32 *dst, int w, int h) {
    for (int ty = 0; ty < h; ty += 4) {
        for (int tx = 0; tx < w; tx += 4) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = tx + x, py = ty + y;
                    u16 pixel = (src[0] << 8) | src[1]; src += 2;
                    if (px < w && py < h) {
                        u8 r = ((pixel >> 11) & 0x1F) * 255 / 31;
                        u8 g = ((pixel >> 5) & 0x3F) * 255 / 63;
                        u8 b = (pixel & 0x1F) * 255 / 31;
                        dst[py * w + px] = (r << 24) | (g << 16) | (b << 8) | 255;
                    }
                }
            }
        }
    }
}

static void gc_i4_decode(const u8 *src, u32 *dst, int w, int h) {
    for (int ty = 0; ty < h; ty += 8) {
        for (int tx = 0; tx < w; tx += 8) {
            for (int y = 0; y < 8; y++) {
                for (int x = 0; x < 8; x += 2) {
                    u8 byte = *src++;
                    for (int n = 0; n < 2; n++) {
                        int px = tx + x + n, py = ty + y;
                        u8 i = (n == 0) ? ((byte >> 4) & 0xF) : (byte & 0xF);
                        i = i * 255 / 15;
                        if (px < w && py < h)
                            dst[py * w + px] = (i << 24) | (i << 16) | (i << 8) | 255;
                    }
                }
            }
        }
    }
}

static void gc_i8_decode(const u8 *src, u32 *dst, int w, int h) {
    for (int ty = 0; ty < h; ty += 4) {
        for (int tx = 0; tx < w; tx += 8) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 8; x++) {
                    int px = tx + x, py = ty + y;
                    u8 i = *src++;
                    if (px < w && py < h)
                        dst[py * w + px] = (i << 24) | (i << 16) | (i << 8) | 255;
                }
            }
        }
    }
}

static void gc_ia4_decode(const u8 *src, u32 *dst, int w, int h) {
    for (int ty = 0; ty < h; ty += 4) {
        for (int tx = 0; tx < w; tx += 8) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 8; x++) {
                    int px = tx + x, py = ty + y;
                    u8 byte = *src++;
                    u8 i = (byte & 0xF) * 255 / 15;
                    u8 a = ((byte >> 4) & 0xF) * 255 / 15;
                    if (px < w && py < h)
                        dst[py * w + px] = (i << 24) | (i << 16) | (i << 8) | a;
                }
            }
        }
    }
}

static void gc_ia8_decode(const u8 *src, u32 *dst, int w, int h) {
    for (int ty = 0; ty < h; ty += 4) {
        for (int tx = 0; tx < w; tx += 4) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = tx + x, py = ty + y;
                    u8 a = *src++;
                    u8 i = *src++;
                    if (px < w && py < h)
                        dst[py * w + px] = (i << 24) | (i << 16) | (i << 8) | a;
                }
            }
        }
    }
}

static void gc_cmpr_decode(const u8 *src, u32 *dst, int w, int h) {
    /* S3TC/DXT1: 4x4 blocks in 2x2 meta-tiles (8x8 total) */
    for (int ty = 0; ty < h; ty += 8) {
        for (int tx = 0; tx < w; tx += 8) {
            /* 4 sub-blocks in a 2x2 arrangement */
            for (int by = 0; by < 2; by++) {
                for (int bx = 0; bx < 2; bx++) {
                    u16 c0 = (src[0] << 8) | src[1];
                    u16 c1 = (src[2] << 8) | src[3];
                    u32 bits = ((u32)src[4] << 24) | ((u32)src[5] << 16) | ((u32)src[6] << 8) | src[7];
                    src += 8;

                    u8 r0 = ((c0 >> 11) & 0x1F) * 255 / 31;
                    u8 g0 = ((c0 >> 5) & 0x3F) * 255 / 63;
                    u8 b0 = (c0 & 0x1F) * 255 / 31;
                    u8 r1 = ((c1 >> 11) & 0x1F) * 255 / 31;
                    u8 g1 = ((c1 >> 5) & 0x3F) * 255 / 63;
                    u8 b1 = (c1 & 0x1F) * 255 / 31;

                    u8 palette[4][4]; /* r,g,b,a */
                    palette[0][0] = r0; palette[0][1] = g0; palette[0][2] = b0; palette[0][3] = 255;
                    palette[1][0] = r1; palette[1][1] = g1; palette[1][2] = b1; palette[1][3] = 255;
                    if (c0 > c1) {
                        palette[2][0] = (2*r0 + r1) / 3; palette[2][1] = (2*g0 + g1) / 3;
                        palette[2][2] = (2*b0 + b1) / 3; palette[2][3] = 255;
                        palette[3][0] = (r0 + 2*r1) / 3; palette[3][1] = (g0 + 2*g1) / 3;
                        palette[3][2] = (b0 + 2*b1) / 3; palette[3][3] = 255;
                    } else {
                        palette[2][0] = (r0 + r1) / 2; palette[2][1] = (g0 + g1) / 2;
                        palette[2][2] = (b0 + b1) / 2; palette[2][3] = 255;
                        palette[3][0] = 0; palette[3][1] = 0; palette[3][2] = 0; palette[3][3] = 0;
                    }

                    for (int y = 0; y < 4; y++) {
                        for (int x = 0; x < 4; x++) {
                            int px = tx + bx * 4 + x;
                            int py = ty + by * 4 + y;
                            int idx = (bits >> 30) & 3;
                            bits <<= 2;
                            if (px < w && py < h) {
                                dst[py * w + px] = (palette[idx][0] << 24) | (palette[idx][1] << 16) |
                                                   (palette[idx][2] << 8) | palette[idx][3];
                            }
                        }
                    }
                }
            }
        }
    }
}

static u32 *decode_texture(void *raw_data, u16 width, u16 height, GXTexFmt fmt) {
    if (!raw_data || width == 0 || height == 0) return NULL;

    u32 *decoded = (u32 *)calloc(width * height, sizeof(u32));
    if (!decoded) return NULL;

    switch (fmt) {
        case GX_TF_RGBA8:  gc_rgba8_decode((const u8 *)raw_data, decoded, width, height); break;
        case GX_TF_RGB5A3: gc_rgb5a3_decode((const u8 *)raw_data, decoded, width, height); break;
        case GX_TF_RGB565: gc_rgb565_decode((const u8 *)raw_data, decoded, width, height); break;
        case GX_TF_I4:     gc_i4_decode((const u8 *)raw_data, decoded, width, height); break;
        case GX_TF_I8:     gc_i8_decode((const u8 *)raw_data, decoded, width, height); break;
        case GX_TF_IA4:    gc_ia4_decode((const u8 *)raw_data, decoded, width, height); break;
        case GX_TF_IA8:    gc_ia8_decode((const u8 *)raw_data, decoded, width, height); break;
        case GX_TF_CMPR:   gc_cmpr_decode((const u8 *)raw_data, decoded, width, height); break;
        default:
            /* Fill magenta for unsupported */
            for (int i = 0; i < width * height; i++)
                decoded[i] = (255u << 24) | (0 << 16) | (255u << 8) | 255u;
            break;
    }
    return decoded;
}

static inline u32 sample_texture(GXTexCacheEntry *tc, float u, float v) {
    if (!tc->decoded) return 0xFFFFFFFF;
    int w = tc->width, h = tc->height;

    /* Wrap mode */
    if (tc->wrap_s == GX_REPEAT) { u = u - floorf(u); }
    else if (tc->wrap_s == GX_MIRROR) { float f = floorf(u); u = ((int)f & 1) ? (1.0f - (u - f)) : (u - f); }
    else { if (u < 0) u = 0; if (u > 1) u = 1; }

    if (tc->wrap_t == GX_REPEAT) { v = v - floorf(v); }
    else if (tc->wrap_t == GX_MIRROR) { float f = floorf(v); v = ((int)f & 1) ? (1.0f - (v - f)) : (v - f); }
    else { if (v < 0) v = 0; if (v > 1) v = 1; }

    int tx = (int)(u * (w - 1) + 0.5f);
    int ty = (int)(v * (h - 1) + 0.5f);
    if (tx < 0) tx = 0; if (tx >= w) tx = w - 1;
    if (ty < 0) ty = 0; if (ty >= h) ty = h - 1;
    return tc->decoded[ty * w + tx];
}

/* ================================================================
 * Initialization
 * ================================================================ */

GXFifoObj *GXInit(void *base, u32 size) {
    (void)base; (void)size;
    printf("[GX] GXInit()\n");

    memset(&g_gx, 0, sizeof(g_gx));
    memset(g_gx_framebuffer, 0, sizeof(g_gx_framebuffer));

    /* Default state */
    g_gx.vp_wd = 640.0f;
    g_gx.vp_ht = 480.0f;
    g_gx.vp_farz = 1.0f;
    g_gx.sc_wd = 640;
    g_gx.sc_ht = 480;
    g_gx.z_enable = GX_TRUE;
    g_gx.z_func = GX_LEQUAL;
    g_gx.z_write = GX_TRUE;
    g_gx.color_update = GX_TRUE;
    g_gx.black_screen = 1;
    g_gx.cull_mode = GX_CULL_BACK;
    g_gx.num_tev_stages = 1;
    g_gx.num_tex_gens = 0;
    g_gx.num_chans = 1;
    g_gx.blend_type = GX_BM_NONE;
    g_gx.blend_src = GX_BL_SRCALPHA;
    g_gx.blend_dst = GX_BL_INVSRCALPHA;
    g_gx.clear_z = 0x00FFFFFF;

    /* Default alpha compare: always pass */
    g_gx.alpha_comp0 = GX_ALWAYS;
    g_gx.alpha_ref0  = 0;
    g_gx.alpha_op    = GX_AOP_AND;
    g_gx.alpha_comp1 = GX_ALWAYS;
    g_gx.alpha_ref1  = 0;

    /* Identity matrices */
    for (int i = 0; i < GX_MAX_POS_MATRICES; i++) {
        g_gx.pos_mtx[i][0][0] = 1.0f;
        g_gx.pos_mtx[i][1][1] = 1.0f;
        g_gx.pos_mtx[i][2][2] = 1.0f;
    }
    for (int i = 0; i < GX_MAX_NRM_MATRICES; i++) {
        g_gx.nrm_mtx[i][0][0] = 1.0f;
        g_gx.nrm_mtx[i][1][1] = 1.0f;
        g_gx.nrm_mtx[i][2][2] = 1.0f;
    }

    /* Identity projection */
    g_gx.projection[0][0] = 1.0f;
    g_gx.projection[1][1] = 1.0f;
    g_gx.projection[2][2] = 1.0f;
    g_gx.projection[3][3] = 1.0f;

    /* Default TEV order: stage 0 → TEXMAP0, COLOR0A0 */
    for (int i = 0; i < GX_MAX_TEV_STAGES; i++) {
        g_gx.tev_order[i].coord = GX_TEXCOORD0;
        g_gx.tev_order[i].map = GX_TEXMAP0;
        g_gx.tev_order[i].color = GX_COLOR0A0;
        g_gx.tev_mode[i] = GX_PASSCLR;
    }

    return &g_fifo_obj;
}

/* ================================================================
 * FIFO
 * ================================================================ */
void GXInitFifoBase(GXFifoObj *fifo, void *base, u32 size) { (void)fifo; (void)base; (void)size; }
void GXInitFifoPtrs(GXFifoObj *fifo, void *readPtr, void *writePtr) { (void)fifo; (void)readPtr; (void)writePtr; }
void GXGetFifoPtrs(GXFifoObj *fifo, void **readPtr, void **writePtr) {
    (void)fifo; if (readPtr) *readPtr = NULL; if (writePtr) *writePtr = NULL;
}
GXFifoObj *GXGetCPUFifo(void) { return &g_fifo_obj; }
GXFifoObj *GXGetGPFifo(void) { return &g_fifo_obj; }
void GXSetCPUFifo(GXFifoObj *fifo) { (void)fifo; }
void GXSetGPFifo(GXFifoObj *fifo) { (void)fifo; }
void GXSaveCPUFifo(GXFifoObj *fifo) { (void)fifo; }
void GXGetFifoStatus(GXFifoObj *fifo, GXBool *overhi, GXBool *underlow, u32 *fifoCount,
                     GXBool *cpu_write, GXBool *gp_read, GXBool *fifowrap) {
    (void)fifo;
    if (overhi) *overhi = GX_FALSE;
    if (underlow) *underlow = GX_FALSE;
    if (fifoCount) *fifoCount = 0;
    if (cpu_write) *cpu_write = GX_TRUE;
    if (gp_read) *gp_read = GX_TRUE;
    if (fifowrap) *fifowrap = GX_FALSE;
}
void GXGetGPStatus(GXBool *overhi, GXBool *underlow, GXBool *readIdle, GXBool *cmdIdle, GXBool *brkpt) {
    if (overhi) *overhi = GX_FALSE;
    if (underlow) *underlow = GX_FALSE;
    if (readIdle) *readIdle = GX_TRUE;
    if (cmdIdle) *cmdIdle = GX_TRUE;
    if (brkpt) *brkpt = GX_FALSE;
}
void GXInitFifoLimits(GXFifoObj *fifo, u32 hiWaterMark, u32 loWaterMark) { (void)fifo; (void)hiWaterMark; (void)loWaterMark; }
GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback cb) { (void)cb; return NULL; }
void GXEnableBreakPt(void *breakPt) { (void)breakPt; }
void GXDisableBreakPt(void) {}

/* ================================================================
 * Transform
 * ================================================================ */
void GXSetProjection(const void *mtx, GXProjectionType type) {
    if (mtx) memcpy(g_gx.projection, mtx, sizeof(float) * 16);
    g_gx.proj_type = type;
}

void GXLoadPosMtxImm(const void *mtx, u32 id) {
    if (id < GX_MAX_POS_MATRICES && mtx) {
        memcpy(g_gx.pos_mtx[id], mtx, sizeof(float) * 12);
    }
}

void GXLoadNrmMtxImm(const void *mtx, u32 id) {
    if (id < GX_MAX_NRM_MATRICES && mtx) {
        memcpy(g_gx.nrm_mtx[id], mtx, sizeof(float) * 12);
    }
}

void GXLoadTexMtxImm(const void *mtx, u32 id, GXTexMtxType type) {
    (void)type;
    u32 idx = (id >= 30) ? (id - 30) : id;
    if (idx < GX_MAX_TEX_MATRICES && mtx) {
        memcpy(g_gx.tex_mtx[idx], mtx, sizeof(float) * 12);
    }
}

void GXSetCurrentMtx(u32 id) {
    g_gx.current_pos_mtx = id;
}

void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz) {
    g_gx.vp_left = left;
    g_gx.vp_top = top;
    g_gx.vp_wd = wd;
    g_gx.vp_ht = ht;
    g_gx.vp_nearz = nearz;
    g_gx.vp_farz = farz;
}

void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field) {
    (void)field;
    GXSetViewport(left, top, wd, ht, nearz, farz);
}

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht) {
    g_gx.sc_left = left;
    g_gx.sc_top = top;
    g_gx.sc_wd = wd;
    g_gx.sc_ht = ht;
}

void GXSetScissorBoxOffset(s32 x_off, s32 y_off) { (void)x_off; (void)y_off; }
void GXSetClipMode(GXClipMode mode) { (void)mode; }

void GXProject(f32 x, f32 y, f32 z, const f32 mtx[3][4], const f32 *pm, const f32 *vp,
               f32 *sx, f32 *sy, f32 *sz) {
    /* Transform through modelview */
    float ex = mtx[0][0]*x + mtx[0][1]*y + mtx[0][2]*z + mtx[0][3];
    float ey = mtx[1][0]*x + mtx[1][1]*y + mtx[1][2]*z + mtx[1][3];
    float ez = mtx[2][0]*x + mtx[2][1]*y + mtx[2][2]*z + mtx[2][3];

    /* Transform through projection (4x4 stored as flat array of 16 floats) */
    const float (*p)[4] = (const float (*)[4])pm;
    float cx = p[0][0]*ex + p[0][1]*ey + p[0][2]*ez + p[0][3];
    float cy = p[1][0]*ex + p[1][1]*ey + p[1][2]*ez + p[1][3];
    float cz = p[2][0]*ex + p[2][1]*ey + p[2][2]*ez + p[2][3];
    float cw = p[3][0]*ex + p[3][1]*ey + p[3][2]*ez + p[3][3];

    if (fabsf(cw) > 1e-6f) {
        cx /= cw; cy /= cw; cz /= cw;
    }

    /* Viewport transform: vp = {left, top, wd, ht, nearz, farz} */
    if (sx) *sx = vp[0] + vp[2] * (cx + 1.0f) * 0.5f;
    if (sy) *sy = vp[1] + vp[3] * (1.0f - cy) * 0.5f;
    if (sz) *sz = vp[4] + (vp[5] - vp[4]) * (cz + 1.0f) * 0.5f;
}

void GXGetViewportv(f32 *vp) {
    if (vp) {
        vp[0] = g_gx.vp_left; vp[1] = g_gx.vp_top;
        vp[2] = g_gx.vp_wd; vp[3] = g_gx.vp_ht;
        vp[4] = g_gx.vp_nearz; vp[5] = g_gx.vp_farz;
    }
}

void GXGetProjectionv(f32 *p) {
    if (p) memcpy(p, g_gx.projection, sizeof(float) * 16);
}

/* ================================================================
 * Geometry / Vertex Format
 * ================================================================ */
void GXSetVtxDesc(GXAttr attr, GXAttrType type) {
    for (int i = 0; i < g_gx.vtx_desc_count; i++) {
        if (g_gx.vtx_desc[i].attr == attr) {
            g_gx.vtx_desc[i].type = type;
            return;
        }
    }
    if (g_gx.vtx_desc_count < GX_MAX_VERTEX_ATTRS) {
        g_gx.vtx_desc[g_gx.vtx_desc_count].attr = attr;
        g_gx.vtx_desc[g_gx.vtx_desc_count].type = type;
        g_gx.vtx_desc_count++;
    }
}

void GXSetVtxDescv(GXVtxDescList *list) {
    g_gx.vtx_desc_count = 0;
    if (!list) return;
    while (list->attr != GX_VA_NULL) {
        if (g_gx.vtx_desc_count < GX_MAX_VERTEX_ATTRS) {
            g_gx.vtx_desc[g_gx.vtx_desc_count] = *list;
            g_gx.vtx_desc_count++;
        }
        list++;
    }
}

void GXClearVtxDesc(void) {
    g_gx.vtx_desc_count = 0;
}

void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac) {
    if (vtxfmt < GX_MAX_VTXFMT && attr < GX_VA_MAX_ATTR) {
        g_gx.vtx_attr_fmt[vtxfmt][attr].cnt = cnt;
        g_gx.vtx_attr_fmt[vtxfmt][attr].type = type;
        g_gx.vtx_attr_fmt[vtxfmt][attr].frac = frac;
    }
}

void GXSetArray(GXAttr attr, const void *data, u8 stride) {
    if (attr < GX_VA_MAX_ATTR) {
        g_gx.vtx_arrays[attr].data = data;
        g_gx.vtx_arrays[attr].stride = stride;
    }
}

void GXSetNumTexGens(u8 nTexGens) { g_gx.num_tex_gens = nTexGens; }

void GXSetTexCoordGen2(GXTexCoordID dst_coord, GXTexGenType func, GXTexGenSrc src_param,
                       u32 mtx, GXBool normalize, u32 postmtx) {
    (void)dst_coord; (void)func; (void)src_param; (void)mtx; (void)normalize; (void)postmtx;
}

void GXSetLineWidth(u8 width, GXTexOffset texOffsets) { (void)width; (void)texOffsets; }
void GXSetPointSize(u8 pointSize, GXTexOffset texOffsets) { (void)pointSize; (void)texOffsets; }
void GXEnableTexOffsets(GXTexCoordID coord, GXBool line_enable, GXBool point_enable) {
    (void)coord; (void)line_enable; (void)point_enable;
}

void GXInvalidateVtxCache(void) {}

void GXGetVtxAttrFmt(GXVtxFmt idx, GXAttr attr, GXCompCnt *compCnt, GXCompType *compType, u8 *shift) {
    if (idx < GX_MAX_VTXFMT && attr < GX_VA_MAX_ATTR) {
        if (compCnt) *compCnt = g_gx.vtx_attr_fmt[idx][attr].cnt;
        if (compType) *compType = g_gx.vtx_attr_fmt[idx][attr].type;
        if (shift) *shift = g_gx.vtx_attr_fmt[idx][attr].frac;
    } else {
        if (compCnt) *compCnt = 0;
        if (compType) *compType = 0;
        if (shift) *shift = 0;
    }
}

/* ================================================================
 * Indexed vertex reading helpers
 * ================================================================ */
static inline float read_comp_float(const u8 *data, GXCompType type, u8 frac) {
    float scale = (frac > 0) ? (1.0f / (float)(1 << frac)) : 1.0f;
    switch (type) {
        case GX_U8:  return (float)(*(const u8 *)data) * scale;
        case GX_S8:  return (float)(*(const s8 *)data) * scale;
        case GX_U16: { u16 v = (data[0] << 8) | data[1]; return (float)v * scale; }
        case GX_S16: { s16 v = (s16)((data[0] << 8) | data[1]); return (float)v * scale; }
        case GX_F32: { float v; memcpy(&v, data, 4); return v; }
        default: return 0.0f;
    }
}

static inline int comp_size(GXCompType type) {
    switch (type) {
        case GX_U8: case GX_S8: return 1;
        case GX_U16: case GX_S16: return 2;
        case GX_F32: return 4;
        default: return 1;
    }
}

/* ================================================================
 * Vertex Submission
 * ================================================================ */
void GXBegin(GXPrimitive type, GXVtxFmt vtxfmt, u16 nverts) {
    if (g_gx.recording_dl) {
        DLEntry e;
        e.cmd = DL_CMD_BEGIN;
        e.begin.prim = (u8)type;
        e.begin.fmt = (u8)vtxfmt;
        e.begin.nverts = nverts;
        dl_record(&e);
        return;
    }
    g_gx.current_prim = type;
    g_gx.current_vtx_fmt = vtxfmt;
    g_gx.current_prim_nverts = nverts;
    g_gx.verts_submitted = 0;
    memset(&g_gx.current_vertex, 0, sizeof(GXSWVertex));
    g_gx.cur_vtx_has_pos = 0;
    g_gx.cur_vtx_has_nrm = 0;
    g_gx.cur_vtx_has_clr = 0;
    g_gx.cur_vtx_texcoord_count = 0;
}

static void submit_vertex(void) {
    if (g_gx.verts_submitted < GX_MAX_PENDING_VERTS) {
        g_gx.vert_buf[g_gx.verts_submitted] = g_gx.current_vertex;
        g_gx.verts_submitted++;
    }
    memset(&g_gx.current_vertex, 0, sizeof(GXSWVertex));
    g_gx.cur_vtx_has_pos = 0;
    g_gx.cur_vtx_has_nrm = 0;
    g_gx.cur_vtx_has_clr = 0;
    g_gx.cur_vtx_texcoord_count = 0;
}

/* Determine if the current vertex is complete based on vtx_desc and auto-submit */
static void maybe_auto_submit(void) {
    /* Check if we have all required attributes.
     * For simplicity, we submit when the last expected attribute arrives.
     * The order is: pos, nrm, clr, texcoord. We check what's described. */
    int need_nrm = 0, need_clr = 0, need_tc = 0;
    for (int i = 0; i < g_gx.vtx_desc_count; i++) {
        GXAttr a = g_gx.vtx_desc[i].attr;
        if (g_gx.vtx_desc[i].type == GX_NONE) continue;
        if (a == GX_VA_NRM || a == GX_VA_NBT) need_nrm = 1;
        if (a == GX_VA_CLR0 || a == GX_VA_CLR1) need_clr = 1;
        if (a >= GX_VA_TEX0 && a <= GX_VA_TEX7) need_tc = 1;
    }

    if (!g_gx.cur_vtx_has_pos) return;

    /* If we need texcoords and have them, or if we don't need them but have everything else */
    if (need_tc && g_gx.cur_vtx_texcoord_count > 0) { submit_vertex(); return; }
    if (!need_tc && need_clr && g_gx.cur_vtx_has_clr) { submit_vertex(); return; }
    if (!need_tc && !need_clr && need_nrm && g_gx.cur_vtx_has_nrm) { submit_vertex(); return; }
    if (!need_tc && !need_clr && !need_nrm && g_gx.cur_vtx_has_pos) { submit_vertex(); return; }
}

void GXPosition3f32(f32 x, f32 y, f32 z) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_POSITION3F32; e.pos.x = x; e.pos.y = y; e.pos.z = z;
        dl_record(&e); return;
    }
    g_gx.current_vertex.pos[0] = x;
    g_gx.current_vertex.pos[1] = y;
    g_gx.current_vertex.pos[2] = z;
    g_gx.cur_vtx_has_pos = 1;
    maybe_auto_submit();
}
void GXPosition3u16(u16 x, u16 y, u16 z) { GXPosition3f32((f32)x, (f32)y, (f32)z); }
void GXPosition3s16(s16 x, s16 y, s16 z) { GXPosition3f32((f32)x, (f32)y, (f32)z); }
void GXPosition3u8(u8 x, u8 y, u8 z) { GXPosition3f32((f32)x, (f32)y, (f32)z); }
void GXPosition3s8(s8 x, s8 y, s8 z) { GXPosition3f32((f32)x, (f32)y, (f32)z); }
void GXPosition2f32(f32 x, f32 y) { GXPosition3f32(x, y, 0.0f); }
void GXPosition2u16(u16 x, u16 y) { GXPosition3f32((f32)x, (f32)y, 0.0f); }
void GXPosition2s16(s16 x, s16 y) { GXPosition3f32((f32)x, (f32)y, 0.0f); }
void GXPosition2u8(u8 x, u8 y) { GXPosition3f32((f32)x, (f32)y, 0.0f); }
void GXPosition2s8(s8 x, s8 y) { GXPosition3f32((f32)x, (f32)y, 0.0f); }

void GXPosition1x16(u16 index) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_POSITION1X16; e.index = index;
        dl_record(&e); return;
    }
    GXVtxArray *arr = &g_gx.vtx_arrays[GX_VA_POS];
    if (!arr->data) { g_gx.cur_vtx_has_pos = 1; return; }
    const u8 *p = (const u8 *)arr->data + index * arr->stride;
    GXVtxAttrFmtEntry *fmt = &g_gx.vtx_attr_fmt[g_gx.current_vtx_fmt][GX_VA_POS];
    int cs = comp_size(fmt->type);
    g_gx.current_vertex.pos[0] = read_comp_float(p, fmt->type, fmt->frac); p += cs;
    g_gx.current_vertex.pos[1] = read_comp_float(p, fmt->type, fmt->frac); p += cs;
    if (fmt->cnt == GX_POS_XYZ)
        g_gx.current_vertex.pos[2] = read_comp_float(p, fmt->type, fmt->frac);
    else
        g_gx.current_vertex.pos[2] = 0.0f;
    g_gx.cur_vtx_has_pos = 1;
    maybe_auto_submit();
}

void GXPosition1x8(u8 index) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_POSITION1X8; e.index = index;
        dl_record(&e); return;
    }
    GXPosition1x16((u16)index);
}

void GXNormal3f32(f32 x, f32 y, f32 z) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_NORMAL3F32; e.pos.x = x; e.pos.y = y; e.pos.z = z;
        dl_record(&e); return;
    }
    g_gx.current_vertex.nrm[0] = x;
    g_gx.current_vertex.nrm[1] = y;
    g_gx.current_vertex.nrm[2] = z;
    g_gx.cur_vtx_has_nrm = 1;
    maybe_auto_submit();
}
void GXNormal3s16(s16 x, s16 y, s16 z) { GXNormal3f32(x/32767.0f, y/32767.0f, z/32767.0f); }
void GXNormal3s8(s8 x, s8 y, s8 z) { GXNormal3f32(x/127.0f, y/127.0f, z/127.0f); }

void GXNormal1x16(u16 index) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_NORMAL1X16; e.index = index;
        dl_record(&e); return;
    }
    GXVtxArray *arr = &g_gx.vtx_arrays[GX_VA_NRM];
    if (!arr->data) { g_gx.cur_vtx_has_nrm = 1; return; }
    const u8 *p = (const u8 *)arr->data + index * arr->stride;
    GXVtxAttrFmtEntry *fmt = &g_gx.vtx_attr_fmt[g_gx.current_vtx_fmt][GX_VA_NRM];
    int cs = comp_size(fmt->type);
    float nx = read_comp_float(p, fmt->type, fmt->frac); p += cs;
    float ny = read_comp_float(p, fmt->type, fmt->frac); p += cs;
    float nz = read_comp_float(p, fmt->type, fmt->frac);
    /* S8 normals need to be divided by 64 (or 127) to normalize */
    if (fmt->type == GX_S8) {
        nx /= 64.0f; ny /= 64.0f; nz /= 64.0f;
    } else if (fmt->type == GX_S16) {
        nx /= 16384.0f; ny /= 16384.0f; nz /= 16384.0f;
    }
    g_gx.current_vertex.nrm[0] = nx;
    g_gx.current_vertex.nrm[1] = ny;
    g_gx.current_vertex.nrm[2] = nz;
    g_gx.cur_vtx_has_nrm = 1;
    maybe_auto_submit();
}

void GXNormal1x8(u8 index) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_NORMAL1X8; e.index = index;
        dl_record(&e); return;
    }
    GXNormal1x16((u16)index);
}

void GXColor4u8(u8 r, u8 g, u8 b, u8 a) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_COLOR4U8;
        e.clr.r = r; e.clr.g = g; e.clr.b = b; e.clr.a = a;
        dl_record(&e); return;
    }
    g_gx.current_vertex.color[0] = r / 255.0f;
    g_gx.current_vertex.color[1] = g / 255.0f;
    g_gx.current_vertex.color[2] = b / 255.0f;
    g_gx.current_vertex.color[3] = a / 255.0f;
    g_gx.cur_vtx_has_clr = 1;
    maybe_auto_submit();
}
void GXColor3u8(u8 r, u8 g, u8 b) { GXColor4u8(r, g, b, 255); }
void GXColor1u32(u32 clr) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_COLOR1U32; e.clr32 = clr;
        dl_record(&e); return;
    }
    GXColor4u8((clr >> 24) & 0xFF, (clr >> 16) & 0xFF, (clr >> 8) & 0xFF, clr & 0xFF);
}
void GXColor1u16(u16 clr) {
    u8 r = ((clr >> 11) & 0x1F) << 3;
    u8 g = ((clr >> 5) & 0x3F) << 2;
    u8 b = (clr & 0x1F) << 3;
    GXColor4u8(r, g, b, 255);
}

void GXColor1x16(u16 index) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_COLOR1X16; e.index = index;
        dl_record(&e); return;
    }
    GXVtxArray *arr = &g_gx.vtx_arrays[GX_VA_CLR0];
    if (!arr->data) { g_gx.cur_vtx_has_clr = 1; return; }
    const u8 *p = (const u8 *)arr->data + index * arr->stride;
    /* Color arrays are typically RGBA8 (4 bytes) or RGB8 (3 bytes) */
    GXVtxAttrFmtEntry *fmt = &g_gx.vtx_attr_fmt[g_gx.current_vtx_fmt][GX_VA_CLR0];
    if (fmt->type == GX_RGBA8) {
        GXColor4u8(p[0], p[1], p[2], p[3]);
    } else if (fmt->type == GX_RGB8) {
        GXColor4u8(p[0], p[1], p[2], 255);
    } else if (fmt->type == GX_RGBX8) {
        GXColor4u8(p[0], p[1], p[2], 255);
    } else if (fmt->type == GX_RGBA4) {
        u16 v = (p[0] << 8) | p[1];
        GXColor4u8(((v>>12)&0xF)*17, ((v>>8)&0xF)*17, ((v>>4)&0xF)*17, (v&0xF)*17);
    } else if (fmt->type == GX_RGBA6) {
        /* 24-bit packed RGBA6 */
        u32 v = ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
        GXColor4u8(((v>>18)&0x3F)*4, ((v>>12)&0x3F)*4, ((v>>6)&0x3F)*4, (v&0x3F)*4);
    } else {
        /* Default: assume RGBA8 */
        GXColor4u8(p[0], p[1], p[2], (arr->stride >= 4) ? p[3] : 255);
    }
}

void GXColor1x8(u8 index) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_COLOR1X8; e.index = index;
        dl_record(&e); return;
    }
    GXColor1x16((u16)index);
}

void GXColor4f32(float r, float g, float b, float a) {
    g_gx.current_vertex.color[0] = r;
    g_gx.current_vertex.color[1] = g;
    g_gx.current_vertex.color[2] = b;
    g_gx.current_vertex.color[3] = a;
    g_gx.cur_vtx_has_clr = 1;
    maybe_auto_submit();
}

void GXTexCoord2f32(f32 s, f32 t) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_TEXCOORD2F32; e.tc.s = s; e.tc.t = t;
        dl_record(&e); return;
    }
    int tc = g_gx.cur_vtx_texcoord_count;
    if (tc < 8) {
        g_gx.current_vertex.texcoord[tc][0] = s;
        g_gx.current_vertex.texcoord[tc][1] = t;
        g_gx.cur_vtx_texcoord_count = tc + 1;
    }
    submit_vertex();
}
void GXTexCoord2u16(u16 s, u16 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord2s16(s16 s, s16 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord2u8(u8 s, u8 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord2s8(s8 s, s8 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord1f32(f32 s, f32 t) { GXTexCoord2f32(s, t); }
void GXTexCoord1u16(u16 s, u16 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord1s16(s16 s, s16 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord1u8(u8 s, u8 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord1s8(s8 s, s8 t) { GXTexCoord2f32((f32)s, (f32)t); }

void GXTexCoord1x16(u16 index) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_TEXCOORD1X16; e.index = index;
        dl_record(&e); return;
    }
    GXVtxArray *arr = &g_gx.vtx_arrays[GX_VA_TEX0];
    if (!arr->data) { submit_vertex(); return; }
    const u8 *p = (const u8 *)arr->data + index * arr->stride;
    GXVtxAttrFmtEntry *fmt = &g_gx.vtx_attr_fmt[g_gx.current_vtx_fmt][GX_VA_TEX0];
    int cs = comp_size(fmt->type);
    float s = read_comp_float(p, fmt->type, fmt->frac); p += cs;
    float t = read_comp_float(p, fmt->type, fmt->frac);
    int tc = g_gx.cur_vtx_texcoord_count;
    if (tc < 8) {
        g_gx.current_vertex.texcoord[tc][0] = s;
        g_gx.current_vertex.texcoord[tc][1] = t;
        g_gx.cur_vtx_texcoord_count = tc + 1;
    }
    submit_vertex();
}

void GXTexCoord1x8(u8 index) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_TEXCOORD1X8; e.index = index;
        dl_record(&e); return;
    }
    GXTexCoord1x16((u16)index);
}

/* ================================================================
 * GXEnd — rasterize collected primitives
 * ================================================================ */
static int g_gx_end_count = 0;
static int g_gx_end_empty = 0;
void GXEnd(void) {
    if (g_gx.recording_dl) {
        DLEntry e; e.cmd = DL_CMD_END;
        dl_record(&e); return;
    }
    g_gx_end_count++;
    if (g_gx.verts_submitted > 0) {
        rasterize_primitives();
    } else {
        g_gx_end_empty++;
        if (g_gx_end_empty <= 3) {
            printf("[GX] GXEnd empty: prim=%d nverts_expected=%d desc_count=%d pos_arr=%p\n",
                   g_gx.current_prim, g_gx.current_prim_nverts,
                   g_gx.vtx_desc_count, g_gx.vtx_arrays[GX_VA_POS].data);
        }
    }
    g_gx.verts_submitted = 0;
}

/* ================================================================
 * Transform Pipeline
 * ================================================================ */
static void transform_vertex(GXSWVertex *in, float *out_screen, float *out_w) {
    u32 mtx_id = g_gx.current_pos_mtx;
    if (mtx_id >= GX_MAX_POS_MATRICES) mtx_id = 0;
    float (*mv)[4] = g_gx.pos_mtx[mtx_id];

    /* Modelview transform (3x4) */
    float ex = mv[0][0]*in->pos[0] + mv[0][1]*in->pos[1] + mv[0][2]*in->pos[2] + mv[0][3];
    float ey = mv[1][0]*in->pos[0] + mv[1][1]*in->pos[1] + mv[1][2]*in->pos[2] + mv[1][3];
    float ez = mv[2][0]*in->pos[0] + mv[2][1]*in->pos[1] + mv[2][2]*in->pos[2] + mv[2][3];

    /* Projection transform (4x4) */
    float (*p)[4] = g_gx.projection;
    float cx = p[0][0]*ex + p[0][1]*ey + p[0][2]*ez + p[0][3];
    float cy = p[1][0]*ex + p[1][1]*ey + p[1][2]*ez + p[1][3];
    float cz = p[2][0]*ex + p[2][1]*ey + p[2][2]*ez + p[2][3];
    float cw = p[3][0]*ex + p[3][1]*ey + p[3][2]*ez + p[3][3];

    /* Perspective divide */
    float inv_w;
    if (fabsf(cw) > 1e-6f) {
        inv_w = 1.0f / cw;
    } else {
        inv_w = 1.0f;
    }
    float ndcx = cx * inv_w;
    float ndcy = cy * inv_w;
    float ndcz = cz * inv_w;

    /* Viewport transform: NDC [-1,1] → screen coords */
    out_screen[0] = g_gx.vp_left + g_gx.vp_wd * (ndcx + 1.0f) * 0.5f;
    out_screen[1] = g_gx.vp_top  + g_gx.vp_ht * (1.0f - ndcy) * 0.5f;
    out_screen[2] = g_gx.vp_nearz + (g_gx.vp_farz - g_gx.vp_nearz) * (ndcz + 1.0f) * 0.5f;
    *out_w = cw;
}

/* ================================================================
 * TEV Evaluation (simplified)
 * ================================================================ */
static inline void tev_evaluate(float *ras_color, float *tex_color, int has_texture, GXTevMode mode,
                                float *out) {
    switch (mode) {
        case GX_PASSCLR:
        default:
            out[0] = ras_color[0]; out[1] = ras_color[1];
            out[2] = ras_color[2]; out[3] = ras_color[3];
            break;
        case GX_MODULATE:
            if (has_texture) {
                out[0] = tex_color[0] * ras_color[0];
                out[1] = tex_color[1] * ras_color[1];
                out[2] = tex_color[2] * ras_color[2];
                out[3] = tex_color[3] * ras_color[3];
            } else {
                out[0] = ras_color[0]; out[1] = ras_color[1];
                out[2] = ras_color[2]; out[3] = ras_color[3];
            }
            break;
        case GX_DECAL:
            if (has_texture) {
                out[0] = tex_color[0]; out[1] = tex_color[1];
                out[2] = tex_color[2]; out[3] = ras_color[3];
            } else {
                out[0] = ras_color[0]; out[1] = ras_color[1];
                out[2] = ras_color[2]; out[3] = ras_color[3];
            }
            break;
        case GX_REPLACE:
            if (has_texture) {
                out[0] = tex_color[0]; out[1] = tex_color[1];
                out[2] = tex_color[2]; out[3] = tex_color[3];
            } else {
                out[0] = ras_color[0]; out[1] = ras_color[1];
                out[2] = ras_color[2]; out[3] = ras_color[3];
            }
            break;
        case GX_BLEND:
            if (has_texture) {
                float a = tex_color[3];
                out[0] = ras_color[0] * (1.0f - a) + tex_color[0] * a;
                out[1] = ras_color[1] * (1.0f - a) + tex_color[1] * a;
                out[2] = ras_color[2] * (1.0f - a) + tex_color[2] * a;
                out[3] = ras_color[3];
            } else {
                out[0] = ras_color[0]; out[1] = ras_color[1];
                out[2] = ras_color[2]; out[3] = ras_color[3];
            }
            break;
    }
}

/* ================================================================
 * Blend factor helpers
 * ================================================================ */
static inline float blend_factor(GXBlendFactor f, float sr, float sg, float sb, float sa,
                                 float dr, float dg, float db, float da, int channel) {
    (void)sr; (void)sg; (void)sb; (void)sa;
    (void)dr; (void)dg; (void)db; (void)da;
    switch (f) {
        case GX_BL_ZERO: return 0.0f;
        case GX_BL_ONE:  return 1.0f;
        case GX_BL_SRCCLR: {
            float c[] = {sr, sg, sb, sa};
            return c[channel];
        }
        case GX_BL_INVSRCCLR: {
            float c[] = {sr, sg, sb, sa};
            return 1.0f - c[channel];
        }
        case GX_BL_SRCALPHA: return sa;
        case GX_BL_INVSRCALPHA: return 1.0f - sa;
        case GX_BL_DSTALPHA: return da;
        case GX_BL_INVDSTALPHA: return 1.0f - da;
        default: return 1.0f;
    }
}

/* ================================================================
 * Depth test
 * ================================================================ */
static inline int depth_test(float z, float zbuf, GXCompare func) {
    switch (func) {
        case GX_NEVER:   return 0;
        case GX_LESS:    return z < zbuf;
        case GX_EQUAL:   return fabsf(z - zbuf) < 1e-6f;
        case GX_LEQUAL:  return z <= zbuf;
        case GX_GREATER: return z > zbuf;
        case GX_NEQUAL:  return fabsf(z - zbuf) >= 1e-6f;
        case GX_GEQUAL:  return z >= zbuf;
        case GX_ALWAYS:  return 1;
        default:         return 1;
    }
}

/* ================================================================
 * Triangle Rasterizer (edge-function based)
 * ================================================================ */
static int g_gx_tri_count = 0;
static int g_gx_pixel_count = 0;
static int g_gx_reject_cull = 0;
static int g_gx_reject_degen = 0;
static int g_gx_reject_oob = 0;
static int g_gx_reject_allclip = 0;
static int g_gx_tri_raster_ok = 0;
static int g_gx_tri_diag_printed = 0;

static void rasterize_triangle(GXSWVertex *v0, GXSWVertex *v1, GXSWVertex *v2) {
    float screen[3][3]; /* [vertex][x,y,z] */
    float w[3];

    g_gx_tri_count++;

    transform_vertex(v0, screen[0], &w[0]);
    transform_vertex(v1, screen[1], &w[1]);
    transform_vertex(v2, screen[2], &w[2]);

    float x0 = screen[0][0], y0 = screen[0][1], z0 = screen[0][2];
    float x1 = screen[1][0], y1 = screen[1][1], z1 = screen[1][2];
    float x2 = screen[2][0], y2 = screen[2][1], z2 = screen[2][2];

    /* Signed area for culling */
    float signed_area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);

    if (g_gx.cull_mode == GX_CULL_BACK && signed_area <= 0) { g_gx_reject_cull++; return; }
    if (g_gx.cull_mode == GX_CULL_FRONT && signed_area >= 0) { g_gx_reject_cull++; return; }
    if (g_gx.cull_mode == GX_CULL_ALL) { g_gx_reject_cull++; return; }

    /* Diagnostic for visible triangles (disabled - set threshold > 0 to re-enable) */
    if (g_gx_tri_diag_printed < 0) {
        printf("[TRI-VIS] #%d screen=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f) area=%.1f\n",
            g_gx_tri_diag_printed,
            x0, y0, x1, y1, x2, y2, signed_area);
        g_gx_tri_diag_printed++;
    }

    if (fabsf(signed_area) < 0.5f) { g_gx_reject_degen++; return; } /* Degenerate triangle */

    float inv_area = 1.0f / signed_area;

    /* Bounding box */
    float fminx = fminf(fminf(x0, x1), x2);
    float fmaxx = fmaxf(fmaxf(x0, x1), x2);
    float fminy = fminf(fminf(y0, y1), y2);
    float fmaxy = fmaxf(fmaxf(y0, y1), y2);

    /* Clamp to scissor */
    int minx = (int)fmaxf(fminx, (float)g_gx.sc_left);
    int maxx = (int)fminf(fmaxx, (float)(g_gx.sc_left + g_gx.sc_wd - 1));
    int miny = (int)fmaxf(fminy, (float)g_gx.sc_top);
    int maxy = (int)fminf(fmaxy, (float)(g_gx.sc_top + g_gx.sc_ht - 1));

    /* Clamp to framebuffer */
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= GX_FB_WIDTH) maxx = GX_FB_WIDTH - 1;
    if (maxy >= GX_FB_HEIGHT) maxy = GX_FB_HEIGHT - 1;

    if (minx > maxx || miny > maxy) { g_gx_reject_oob++; return; }

    g_gx_tri_raster_ok++;

    /* Determine rasterized color source */
    float ras_r, ras_g, ras_b, ras_a;
    int vtx_has_color = (v0->color[3] != 0.0f || v0->color[0] != 0.0f ||
                         v0->color[1] != 0.0f || v0->color[2] != 0.0f);
    if (!vtx_has_color) {
        /* Use material color */
        ras_r = g_gx.chan_mat[0].r / 255.0f;
        ras_g = g_gx.chan_mat[0].g / 255.0f;
        ras_b = g_gx.chan_mat[0].b / 255.0f;
        ras_a = g_gx.chan_mat[0].a / 255.0f;
    } else {
        ras_r = ras_g = ras_b = ras_a = 0; /* will interpolate per-pixel */
    }

    /* TEV: figure out which texture is used by stage 0 */
    GXTexMapID tex_map = g_gx.tev_order[0].map;
    int has_texture = 0;
    GXTexCacheEntry *tc = NULL;
    if (tex_map < GX_MAX_TEXTURES && g_gx.tex_loaded[tex_map] && g_gx.tex_cache[tex_map].decoded) {
        tc = &g_gx.tex_cache[tex_map];
        has_texture = 1;
    }

    GXTevMode tev_mode = g_gx.tev_mode[0];

    /* Perspective-correct interpolation: 1/w per vertex */
    float inv_w0 = (fabsf(w[0]) > 1e-6f) ? 1.0f / w[0] : 1.0f;
    float inv_w1 = (fabsf(w[1]) > 1e-6f) ? 1.0f / w[1] : 1.0f;
    float inv_w2 = (fabsf(w[2]) > 1e-6f) ? 1.0f / w[2] : 1.0f;

    for (int py = miny; py <= maxy; py++) {
        for (int px = minx; px <= maxx; px++) {
            float pxf = px + 0.5f, pyf = py + 0.5f;

            /* Barycentric coordinates */
            float e0 = (x1 - x0) * (pyf - y0) - (y1 - y0) * (pxf - x0);
            float e1 = (x2 - x1) * (pyf - y1) - (y2 - y1) * (pxf - x1);
            float e2 = (x0 - x2) * (pyf - y2) - (y0 - y2) * (pxf - x2);

            /* Winding check */
            if (signed_area > 0) {
                if (e0 < 0 || e1 < 0 || e2 < 0) continue;
            } else {
                if (e0 > 0 || e1 > 0 || e2 > 0) continue;
            }

            float bary2 = e0 * inv_area;
            float bary0 = e1 * inv_area;
            float bary1 = e2 * inv_area;

            /* Interpolate depth */
            float z = bary0 * z0 + bary1 * z1 + bary2 * z2;

            /* Depth test */
            int fb_idx = py * GX_FB_WIDTH + px;
            if (g_gx.z_enable && !depth_test(z, g_gx.zbuffer[fb_idx], g_gx.z_func))
                continue;

            /* Perspective-correct interpolation factor */
            float pc_denom = bary0 * inv_w0 + bary1 * inv_w1 + bary2 * inv_w2;
            float pc_inv = (fabsf(pc_denom) > 1e-10f) ? 1.0f / pc_denom : 1.0f;

            /* Interpolate vertex color */
            float frag_color[4];
            if (vtx_has_color) {
                for (int c = 0; c < 4; c++) {
                    float val = (bary0 * v0->color[c] * inv_w0 +
                                 bary1 * v1->color[c] * inv_w1 +
                                 bary2 * v2->color[c] * inv_w2) * pc_inv;
                    frag_color[c] = fmaxf(0.0f, fminf(1.0f, val));
                }
            } else {
                frag_color[0] = ras_r; frag_color[1] = ras_g;
                frag_color[2] = ras_b; frag_color[3] = ras_a;
            }

            /* Sample texture */
            float tex_color[4] = {1,1,1,1};
            if (has_texture) {
                float u = (bary0 * v0->texcoord[0][0] * inv_w0 +
                           bary1 * v1->texcoord[0][0] * inv_w1 +
                           bary2 * v2->texcoord[0][0] * inv_w2) * pc_inv;
                float v = (bary0 * v0->texcoord[0][1] * inv_w0 +
                           bary1 * v1->texcoord[0][1] * inv_w1 +
                           bary2 * v2->texcoord[0][1] * inv_w2) * pc_inv;
                u32 texel = sample_texture(tc, u, v);
                tex_color[0] = ((texel >> 24) & 0xFF) / 255.0f;
                tex_color[1] = ((texel >> 16) & 0xFF) / 255.0f;
                tex_color[2] = ((texel >> 8) & 0xFF) / 255.0f;
                tex_color[3] = (texel & 0xFF) / 255.0f;
            }

            /* TEV */
            float out_color[4];
            tev_evaluate(frag_color, tex_color, has_texture, tev_mode, out_color);

            /* Alpha blending */
            float final_r = out_color[0], final_g = out_color[1];
            float final_b = out_color[2], final_a = out_color[3];

            if (g_gx.blend_type == GX_BM_BLEND) {
                u32 dst_pixel = g_gx.framebuffer[fb_idx];
                float dr = ((dst_pixel >> 24) & 0xFF) / 255.0f;
                float dg = ((dst_pixel >> 16) & 0xFF) / 255.0f;
                float db = ((dst_pixel >> 8) & 0xFF) / 255.0f;
                float da = (dst_pixel & 0xFF) / 255.0f;

                for (int c = 0; c < 4; c++) {
                    float sc[] = {final_r, final_g, final_b, final_a};
                    float dc[] = {dr, dg, db, da};
                    float sf = blend_factor(g_gx.blend_src, final_r, final_g, final_b, final_a,
                                           dr, dg, db, da, c);
                    float df = blend_factor(g_gx.blend_dst, final_r, final_g, final_b, final_a,
                                           dr, dg, db, da, c);
                    float val = sc[c] * sf + dc[c] * df;
                    if (val < 0) val = 0; if (val > 1) val = 1;
                    if (c == 0) final_r = val;
                    else if (c == 1) final_g = val;
                    else if (c == 2) final_b = val;
                    else final_a = val;
                }
            }

            /* Alpha compare test — discard pixel if it fails */
            {
                u8 a8 = (u8)(final_a * 255.0f);
                int pass0, pass1, pass;
                switch (g_gx.alpha_comp0) {
                    case GX_NEVER:   pass0 = 0; break;
                    case GX_LESS:    pass0 = (a8 <  g_gx.alpha_ref0); break;
                    case GX_EQUAL:   pass0 = (a8 == g_gx.alpha_ref0); break;
                    case GX_LEQUAL:  pass0 = (a8 <= g_gx.alpha_ref0); break;
                    case GX_GREATER: pass0 = (a8 >  g_gx.alpha_ref0); break;
                    case GX_NEQUAL:  pass0 = (a8 != g_gx.alpha_ref0); break;
                    case GX_GEQUAL:  pass0 = (a8 >= g_gx.alpha_ref0); break;
                    default:         pass0 = 1; break; /* GX_ALWAYS */
                }
                switch (g_gx.alpha_comp1) {
                    case GX_NEVER:   pass1 = 0; break;
                    case GX_LESS:    pass1 = (a8 <  g_gx.alpha_ref1); break;
                    case GX_EQUAL:   pass1 = (a8 == g_gx.alpha_ref1); break;
                    case GX_LEQUAL:  pass1 = (a8 <= g_gx.alpha_ref1); break;
                    case GX_GREATER: pass1 = (a8 >  g_gx.alpha_ref1); break;
                    case GX_NEQUAL:  pass1 = (a8 != g_gx.alpha_ref1); break;
                    case GX_GEQUAL:  pass1 = (a8 >= g_gx.alpha_ref1); break;
                    default:         pass1 = 1; break; /* GX_ALWAYS */
                }
                switch (g_gx.alpha_op) {
                    case GX_AOP_AND:  pass = pass0 && pass1; break;
                    case GX_AOP_OR:   pass = pass0 || pass1; break;
                    case GX_AOP_XOR:  pass = pass0 ^ pass1; break;
                    case GX_AOP_XNOR: pass = !(pass0 ^ pass1); break;
                    default:          pass = 1; break;
                }
                if (!pass) goto skip_pixel;
            }

            /* Write to framebuffer */
            if (g_gx.color_update) {
                u8 rb = (u8)(final_r * 255.0f);
                u8 gb = (u8)(final_g * 255.0f);
                u8 bb = (u8)(final_b * 255.0f);
                /* On GC, framebuffer alpha wasn't used for display (XFB is YUV).
                 * On PC we use alpha for compositing with sprites, so any rendered
                 * pixel must be opaque (255) unless TEV explicitly outputs transparency. */
                u8 ab = g_gx.alpha_update ? (u8)(final_a * 255.0f) : 0xFF;
                g_gx.framebuffer[fb_idx] = ((u32)rb << 24) | ((u32)gb << 16) | ((u32)bb << 8) | ab;
                g_gx_pixel_count++;
            }

            /* Write depth */
            if (g_gx.z_write) {
                g_gx.zbuffer[fb_idx] = z;
            }
            skip_pixel: ;
        }
    }
}

/* ================================================================
 * Primitive Assembly
 * ================================================================ */
static void rasterize_primitives(void) {
    int n = g_gx.verts_submitted;
    GXSWVertex *vb = g_gx.vert_buf;

    switch (g_gx.current_prim) {
        case GX_TRIANGLES:
            for (int i = 0; i + 2 < n; i += 3) {
                rasterize_triangle(&vb[i], &vb[i+1], &vb[i+2]);
            }
            break;
        case GX_QUADS:
            for (int i = 0; i + 3 < n; i += 4) {
                rasterize_triangle(&vb[i], &vb[i+1], &vb[i+2]);
                rasterize_triangle(&vb[i], &vb[i+2], &vb[i+3]);
            }
            break;
        case GX_TRIANGLESTRIP:
            for (int i = 0; i + 2 < n; i++) {
                if (i & 1)
                    rasterize_triangle(&vb[i+1], &vb[i], &vb[i+2]);
                else
                    rasterize_triangle(&vb[i], &vb[i+1], &vb[i+2]);
            }
            break;
        case GX_TRIANGLEFAN:
            for (int i = 1; i + 1 < n; i++) {
                rasterize_triangle(&vb[0], &vb[i], &vb[i+1]);
            }
            break;
        default:
            break;
    }
}

/* ================================================================
 * Lighting
 * ================================================================ */
void GXSetNumChans(u8 nChans) { g_gx.num_chans = nChans; }
void GXSetChanCtrl(GXChannelID chan, GXBool enable, GXColorSrc amb_src, GXColorSrc mat_src,
                   u32 light_mask, GXDiffuseFn diff_fn, GXAttnFn attn_fn) {
    (void)chan; (void)enable; (void)amb_src; (void)mat_src;
    (void)light_mask; (void)diff_fn; (void)attn_fn;
}
void GXSetChanAmbColor(GXChannelID chan, GXColor color) {
    int idx = (chan == GX_COLOR0A0 || chan == GX_COLOR0) ? 0 : 1;
    g_gx.chan_amb[idx] = color;
}
void GXSetChanMatColor(GXChannelID chan, GXColor color) {
    int idx = (chan == GX_COLOR0A0 || chan == GX_COLOR0) ? 0 : 1;
    g_gx.chan_mat[idx] = color;
}
void GXInitLightSpot(GXLightObj *lt_obj, f32 cutoff, GXSpotFn spot_func) { (void)lt_obj; (void)cutoff; (void)spot_func; }
void GXInitLightDistAttn(GXLightObj *lt_obj, f32 ref_distance, f32 ref_brightness, GXDistAttnFn dist_func) {
    (void)lt_obj; (void)ref_distance; (void)ref_brightness; (void)dist_func;
}
void GXInitLightPos(GXLightObj *lt_obj, f32 x, f32 y, f32 z) { (void)lt_obj; (void)x; (void)y; (void)z; }
void GXInitLightDir(GXLightObj *lt_obj, f32 nx, f32 ny, f32 nz) { (void)lt_obj; (void)nx; (void)ny; (void)nz; }
void GXInitLightColor(GXLightObj *lt_obj, GXColor color) { (void)lt_obj; (void)color; }
void GXInitLightAttn(GXLightObj *lt_obj, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2) {
    (void)lt_obj; (void)a0; (void)a1; (void)a2; (void)k0; (void)k1; (void)k2;
}
void GXInitLightAttnA(GXLightObj *lt_obj, f32 a0, f32 a1, f32 a2) { (void)lt_obj; (void)a0; (void)a1; (void)a2; }
void GXInitLightAttnK(GXLightObj *lt_obj, f32 k0, f32 k1, f32 k2) { (void)lt_obj; (void)k0; (void)k1; (void)k2; }
void GXLoadLightObjImm(GXLightObj *lt_obj, GXLightID light) { (void)lt_obj; (void)light; }
void GXGetLightPos(const GXLightObj *lt_obj, f32 *x, f32 *y, f32 *z) {
    (void)lt_obj; if (x) *x = 0; if (y) *y = 0; if (z) *z = 0;
}
void GXGetLightColor(const GXLightObj *lt_obj, GXColor *color) {
    (void)lt_obj; if (color) { color->r = 255; color->g = 255; color->b = 255; color->a = 255; }
}

/* ================================================================
 * TEV (Texture Environment)
 * ================================================================ */
void GXSetTevOp(GXTevStageID id, GXTevMode mode) {
    if (id < GX_MAX_TEV_STAGES) g_gx.tev_mode[id] = mode;
}
void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d) {
    (void)stage; (void)a; (void)b; (void)c; (void)d;
}
void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d) {
    (void)stage; (void)a; (void)b; (void)c; (void)d;
}
void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg) {
    (void)stage; (void)op; (void)bias; (void)scale; (void)clamp; (void)out_reg;
}
void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg) {
    (void)stage; (void)op; (void)bias; (void)scale; (void)clamp; (void)out_reg;
}
void GXSetTevColor(GXTevRegID id, GXColor color) { (void)id; (void)color; }
void GXSetTevColorS10(GXTevRegID id, GXColorS10 color) { (void)id; (void)color; }
void GXSetTevKColor(GXTevKColorID id, GXColor color) { (void)id; (void)color; }
void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel) { (void)stage; (void)sel; }
void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel) { (void)stage; (void)sel; }
void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel ras_sel, GXTevSwapSel tex_sel) { (void)stage; (void)ras_sel; (void)tex_sel; }
void GXSetTevSwapModeTable(GXTevSwapSel table, GXTevColorChan red, GXTevColorChan green, GXTevColorChan blue, GXTevColorChan alpha) {
    (void)table; (void)red; (void)green; (void)blue; (void)alpha;
}
void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1) {
    g_gx.alpha_comp0 = comp0;
    g_gx.alpha_ref0  = ref0;
    g_gx.alpha_op    = op;
    g_gx.alpha_comp1 = comp1;
    g_gx.alpha_ref1  = ref1;
}
void GXSetZTexture(GXZTexOp op, GXTexFmt fmt, u32 bias) { (void)op; (void)fmt; (void)bias; }
void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color) {
    if (stage < GX_MAX_TEV_STAGES) {
        g_gx.tev_order[stage].coord = coord;
        g_gx.tev_order[stage].map = map;
        g_gx.tev_order[stage].color = color;
    }
}
void GXSetNumTevStages(u8 nStages) { g_gx.num_tev_stages = nStages; }

/* ================================================================
 * Textures
 * ================================================================ */
u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, u8 mipmap, u8 max_lod) {
    (void)mipmap; (void)max_lod;
    u32 bpp = 32;
    switch (format) {
        case GX_TF_I4: case GX_TF_C4: bpp = 4; break;
        case GX_TF_I8: case GX_TF_IA4: case GX_TF_C8: bpp = 8; break;
        case GX_TF_IA8: case GX_TF_RGB565: case GX_TF_RGB5A3: case GX_TF_C14X2: bpp = 16; break;
        case GX_TF_RGBA8: bpp = 32; break;
        case GX_TF_CMPR: bpp = 4; break;
        default: bpp = 32; break;
    }
    return (width * height * bpp + 7) / 8;
}

void GXInitTexObj(GXTexObj *obj, void *image_ptr, u16 width, u16 height, GXTexFmt format,
                  GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, u8 mipmap) {
    if (!obj) return;
    memset(obj, 0, sizeof(GXTexObj));
    GXTexObjPC *pc = (GXTexObjPC *)obj;
    pc->image_ptr = image_ptr;
    pc->width = width;
    pc->height = height;
    pc->format = (u32)format;
    pc->wrap_s = (u32)wrap_s;
    pc->wrap_t = (u32)wrap_t;
    pc->mipmap = mipmap;
}
void GXInitTexObjCI(GXTexObj *obj, void *image_ptr, u16 width, u16 height, GXCITexFmt format,
                    GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, u8 mipmap, u32 tlut_name) {
    /* Treat CI textures as regular for now (will show wrong colors but won't crash) */
    (void)tlut_name;
    GXInitTexObj(obj, image_ptr, width, height, (GXTexFmt)format, wrap_s, wrap_t, mipmap);
}
void GXInitTexObjLOD(GXTexObj *obj, GXTexFilter min_filt, GXTexFilter mag_filt,
                     f32 min_lod, f32 max_lod, f32 lod_bias, GXBool bias_clamp,
                     GXBool do_edge_lod, GXAnisotropy max_aniso) {
    (void)obj; (void)min_filt; (void)mag_filt; (void)min_lod; (void)max_lod;
    (void)lod_bias; (void)bias_clamp; (void)do_edge_lod; (void)max_aniso;
}
void GXInitTexObjData(GXTexObj *obj, void *image_ptr) {
    if (obj) ((GXTexObjPC *)obj)->image_ptr = image_ptr;
}
void GXInitTexObjWrapMode(GXTexObj *obj, GXTexWrapMode s, GXTexWrapMode t) {
    if (obj) { ((GXTexObjPC *)obj)->wrap_s = s; ((GXTexObjPC *)obj)->wrap_t = t; }
}
void GXInitTexObjTlut(GXTexObj *obj, u32 tlut_name) { (void)obj; (void)tlut_name; }
void GXInitTexObjUserData(GXTexObj *obj, void *user_data) { (void)obj; (void)user_data; }
void *GXGetTexObjUserData(const GXTexObj *obj) { (void)obj; return NULL; }
void GXLoadTexObjPreLoaded(GXTexObj *obj, GXTexRegion *region, GXTexMapID id) { (void)obj; (void)region; (void)id; }

void GXLoadTexObj(GXTexObj *obj, GXTexMapID id) {
    if (id >= GX_MAX_TEXTURES || !obj) return;
    g_gx.tex_obj[id] = *obj;
    g_gx.tex_loaded[id] = 1;

    /* Decode texture into cache */
    GXTexObjPC *pc = (GXTexObjPC *)obj;
    GXTexCacheEntry *tc = &g_gx.tex_cache[id];

    /* Check if already decoded for this data pointer */
    if (tc->decoded && tc->raw_data == pc->image_ptr &&
        tc->width == pc->width && tc->height == pc->height) {
        /* Already cached */
        tc->wrap_s = (GXTexWrapMode)pc->wrap_s;
        tc->wrap_t = (GXTexWrapMode)pc->wrap_t;
        return;
    }

    /* Free old decoded data */
    if (tc->decoded) {
        free(tc->decoded);
        tc->decoded = NULL;
    }

    tc->raw_data = pc->image_ptr;
    tc->width = pc->width;
    tc->height = pc->height;
    tc->format = (GXTexFmt)pc->format;
    tc->wrap_s = (GXTexWrapMode)pc->wrap_s;
    tc->wrap_t = (GXTexWrapMode)pc->wrap_t;

    if (pc->image_ptr && pc->width > 0 && pc->height > 0) {
        tc->decoded = decode_texture(pc->image_ptr, pc->width, pc->height, tc->format);
    }
}

void GXInitTlutObj(GXTlutObj *tlut_obj, void *lut, GXTlutFmt fmt, u16 n_entries) {
    if (tlut_obj) memset(tlut_obj, 0, sizeof(GXTlutObj));
    (void)lut; (void)fmt; (void)n_entries;
}
void GXLoadTlut(GXTlutObj *tlut_obj, u32 tlut_name) { (void)tlut_obj; (void)tlut_name; }
void GXInitTexCacheRegion(GXTexRegion *region, u8 is_32b_mipmap, u32 tmem_even,
                          GXTexCacheSize size_even, u32 tmem_odd, GXTexCacheSize size_odd) {
    (void)region; (void)is_32b_mipmap; (void)tmem_even; (void)size_even; (void)tmem_odd; (void)size_odd;
}
void GXInitTexPreLoadRegion(GXTexRegion *region, u32 tmem_even, u32 size_even, u32 tmem_odd, u32 size_odd) {
    (void)region; (void)tmem_even; (void)size_even; (void)tmem_odd; (void)size_odd;
}
void GXInitTlutRegion(GXTlutRegion *region, u32 tmem_addr, GXTlutSize tlut_size) {
    (void)region; (void)tmem_addr; (void)tlut_size;
}
void GXInvalidateTexRegion(GXTexRegion *region) { (void)region; }
void GXInvalidateTexAll(void) {
    /* Clear texture cache */
    for (int i = 0; i < GX_MAX_TEXTURES; i++) {
        if (g_gx.tex_cache[i].decoded) {
            free(g_gx.tex_cache[i].decoded);
            g_gx.tex_cache[i].decoded = NULL;
        }
        g_gx.tex_cache[i].raw_data = NULL;
        g_gx.tex_loaded[i] = 0;
    }
}
GXTexRegionCallback GXSetTexRegionCallback(GXTexRegionCallback f) { (void)f; return NULL; }
GXTlutRegionCallback GXSetTlutRegionCallback(GXTlutRegionCallback f) { (void)f; return NULL; }
void GXPreLoadEntireTexture(GXTexObj *tex_obj, GXTexRegion *region) { (void)tex_obj; (void)region; }
void GXSetTexCoordScaleManually(GXTexCoordID coord, u8 enable, u16 ss, u16 ts) {
    (void)coord; (void)enable; (void)ss; (void)ts;
}
void GXSetTexCoordCylWrap(GXTexCoordID coord, u8 s_enable, u8 t_enable) { (void)coord; (void)s_enable; (void)t_enable; }
void GXSetTexCoordBias(GXTexCoordID coord, u8 s_enable, u8 t_enable) { (void)coord; (void)s_enable; (void)t_enable; }

GXBool GXGetTexObjMipMap(const GXTexObj *obj) { return obj ? ((GXTexObjPC *)obj)->mipmap : GX_FALSE; }
GXTexFmt GXGetTexObjFmt(const GXTexObj *obj) { return obj ? (GXTexFmt)((GXTexObjPC *)obj)->format : GX_TF_RGBA8; }
u16 GXGetTexObjHeight(const GXTexObj *obj) { return obj ? ((GXTexObjPC *)obj)->height : 0; }
u16 GXGetTexObjWidth(const GXTexObj *obj) { return obj ? ((GXTexObjPC *)obj)->width : 0; }
GXTexWrapMode GXGetTexObjWrapS(const GXTexObj *obj) { return obj ? (GXTexWrapMode)((GXTexObjPC *)obj)->wrap_s : GX_CLAMP; }
GXTexWrapMode GXGetTexObjWrapT(const GXTexObj *obj) { return obj ? (GXTexWrapMode)((GXTexObjPC *)obj)->wrap_t : GX_CLAMP; }
void *GXGetTexObjData(const GXTexObj *obj) { return obj ? ((GXTexObjPC *)obj)->image_ptr : NULL; }

void GXDestroyTexObj(GXTexObj *obj) { if (obj) memset(obj, 0, sizeof(GXTexObj)); }
void GXDestroyTlutObj(GXTlutObj *obj) { if (obj) memset(obj, 0, sizeof(GXTlutObj)); }

/* ================================================================
 * Pixel / Blend / Z
 * ================================================================ */
void GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color) {
    g_gx.fog_type = type; g_gx.fog_startz = startz; g_gx.fog_endz = endz;
    g_gx.fog_nearz = nearz; g_gx.fog_farz = farz; g_gx.fog_color = color;
}
void GXSetFogColor(GXColor color) { g_gx.fog_color = color; }
void GXSetBlendMode(GXBlendMode type, GXBlendFactor src_factor, GXBlendFactor dst_factor, GXLogicOp op) {
    (void)op;
    g_gx.blend_type = type;
    g_gx.blend_src = src_factor;
    g_gx.blend_dst = dst_factor;
}
void GXSetColorUpdate(GXBool update_enable) { g_gx.color_update = update_enable; }
void GXSetAlphaUpdate(GXBool update_enable) { g_gx.alpha_update = update_enable; }
void GXSetZMode(GXBool compare_enable, GXCompare func, GXBool update_enable) {
    g_gx.z_enable = compare_enable;
    g_gx.z_func = func;
    g_gx.z_write = update_enable;
}
void GXSetZCompLoc(GXBool before_tex) { (void)before_tex; }
void GXSetDither(GXBool dither) { (void)dither; }
void GXSetDstAlpha(GXBool enable, u8 alpha) { (void)enable; (void)alpha; }
void GXSetFieldMode(u8 field_mode, u8 half_aspect_ratio) { (void)field_mode; (void)half_aspect_ratio; }
void GXSetCullMode(GXCullMode mode) { g_gx.cull_mode = mode; }
void GXSetCoPlanar(GXBool enable) { (void)enable; }

/* ================================================================
 * Framebuffer
 * ================================================================ */
void GXSetCopyClear(GXColor clear_clr, u32 clear_z) {
    g_gx.clear_color = clear_clr;
    g_gx.clear_z = clear_z;
}

void GXCopyDisp(void *dest, GXBool clear) {
    static int copy_count = 0;
    g_gx.xfb_ptr = dest;

    /* Copy software framebuffer to the external display buffer */
    memcpy(g_gx_framebuffer, g_gx.framebuffer, sizeof(g_gx_framebuffer));

    if (copy_count % 60 == 0) {
        /* Count non-clear pixels */
        u32 clr = ((u32)g_gx.clear_color.r << 24) | ((u32)g_gx.clear_color.g << 16) |
                  ((u32)g_gx.clear_color.b << 8) | (u32)g_gx.clear_color.a;
        int non_clear = 0;
        for (int i = 0; i < GX_FB_WIDTH * GX_FB_HEIGHT; i += 100) {
            if (g_gx.framebuffer[i] != clr) non_clear++;
        }
        printf("[GX] CopyDisp #%d: tris=%d pixels=%d non_clear=%d/%d clear=0x%08x DL(call=%d ok=%d null=%d magic=%d)\n",
               copy_count, g_gx_tri_count, g_gx_pixel_count, non_clear, GX_FB_WIDTH*GX_FB_HEIGHT/100, clr,
               g_gx_dl_call_count, g_gx_dl_ok, g_gx_dl_fail_null, g_gx_dl_fail_magic);
        printf("[GX]   reject: cull=%d degen=%d oob=%d | raster_ok=%d\n",
               g_gx_reject_cull, g_gx_reject_degen, g_gx_reject_oob, g_gx_tri_raster_ok);
        g_gx_tri_count = 0;
        g_gx_pixel_count = 0;
        g_gx_reject_cull = 0;
        g_gx_reject_degen = 0;
        g_gx_reject_oob = 0;
        g_gx_tri_raster_ok = 0;
        g_gx_tri_diag_printed = 0;
        g_gx_dl_call_count = 0;
        g_gx_dl_ok = 0;
        g_gx_dl_fail_null = 0;
        g_gx_dl_fail_magic = 0;
    }
    copy_count++;

    if (clear) {
        /* Force alpha=0 so cleared pixels are transparent when composited
         * via SDL alpha blending. This lets background sprites show through
         * areas where no 3D geometry was rendered. */
        u32 clr = ((u32)g_gx.clear_color.r << 24) | ((u32)g_gx.clear_color.g << 16) |
                  ((u32)g_gx.clear_color.b << 8) | 0x00;
        for (int i = 0; i < GX_FB_WIDTH * GX_FB_HEIGHT; i++) {
            g_gx.framebuffer[i] = clr;
            g_gx.zbuffer[i] = 1.0f;
        }
    }
}

void GXAdjustForOverscan(GXRenderModeObj *rmin, GXRenderModeObj *rmout, u16 hor, u16 ver) {
    if (rmin && rmout) {
        memcpy(rmout, rmin, sizeof(GXRenderModeObj));
        rmout->viXOrigin += hor;
        rmout->viYOrigin += ver;
        rmout->viWidth -= hor * 2;
        rmout->viHeight -= ver * 2;
    }
}

void GXSetDispCopyGamma(GXGamma gamma) { (void)gamma; }
void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) { (void)left; (void)top; (void)wd; (void)ht; }
void GXSetDispCopyDst(u16 wd, u16 ht) { (void)wd; (void)ht; }
f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    if (efbHeight == 0) return 1.0f;
    return (f32)xfbHeight / (f32)efbHeight;
}
u32 GXSetDispCopyYScale(f32 vscale) { (void)vscale; return 0; }
void GXSetCopyFilter(GXBool aa, u8 sample_pattern[12][2], GXBool vf, u8 vfilter[7]) {
    (void)aa; (void)sample_pattern; (void)vf; (void)vfilter;
}
void GXSetPixelFmt(GXPixelFmt pix_fmt, GXZFmt16 z_fmt) { (void)pix_fmt; (void)z_fmt; }
void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht) { (void)left; (void)top; (void)wd; (void)ht; }
void GXSetTexCopyDst(u16 wd, u16 ht, GXTexFmt fmt, GXBool mipmap) { (void)wd; (void)ht; (void)fmt; (void)mipmap; }
void GXCopyTex(void *dest, GXBool clear) { (void)dest; (void)clear; }

/* ================================================================
 * Management / Sync
 * ================================================================ */
void GXDrawDone(void) {}
void GXSetDrawDone(void) {}
void GXFlush(void) {}
void GXPixModeSync(void) {}
void GXSetMisc(GXMiscToken token, u32 val) { (void)token; (void)val; }
GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb) { (void)cb; return NULL; }
void GXSetDrawSync(u16 token) { (void)token; }
GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback callback) { (void)callback; return NULL; }

/* ================================================================
 * Performance counters (stubs)
 * ================================================================ */
void GXSetGPMetric(GXPerf0 perf0, GXPerf1 perf1) { (void)perf0; (void)perf1; }
void GXClearGPMetric(void) {}
void GXReadXfRasMetric(u32 *xfWaitIn, u32 *xfWaitOut, u32 *rasBusy, u32 *clocks) {
    if (xfWaitIn) *xfWaitIn = 0; if (xfWaitOut) *xfWaitOut = 0;
    if (rasBusy) *rasBusy = 0; if (clocks) *clocks = 0;
}
void GXReadGPMetric(u32 *count0, u32 *count1) { if (count0) *count0 = 0; if (count1) *count1 = 0; }
u32 GXReadGP0Metric(void) { return 0; }
u32 GXReadGP1Metric(void) { return 0; }
void GXReadMemMetric(u32 *cpReq, u32 *tcReq, u32 *cpuReadReq, u32 *cpuWriteReq,
                     u32 *dspReq, u32 *ioReq, u32 *viReq, u32 *peReq, u32 *rfReq, u32 *fiReq) {
    if (cpReq) *cpReq = 0; if (tcReq) *tcReq = 0;
    if (cpuReadReq) *cpuReadReq = 0; if (cpuWriteReq) *cpuWriteReq = 0;
    if (dspReq) *dspReq = 0; if (ioReq) *ioReq = 0;
    if (viReq) *viReq = 0; if (peReq) *peReq = 0;
    if (rfReq) *rfReq = 0; if (fiReq) *fiReq = 0;
}
void GXClearMemMetric(void) {}
void GXReadPixMetric(u32 *topIn, u32 *topOut, u32 *bottomIn, u32 *bottomOut, u32 *clearIn, u32 *copyClocks) {
    if (topIn) *topIn = 0; if (topOut) *topOut = 0;
    if (bottomIn) *bottomIn = 0; if (bottomOut) *bottomOut = 0;
    if (clearIn) *clearIn = 0; if (copyClocks) *copyClocks = 0;
}
void GXClearPixMetric(void) {}
void GXSetVCacheMetric(GXVCachePerf attr) { (void)attr; }
void GXReadVCacheMetric(u32 *check, u32 *miss, u32 *stall) {
    if (check) *check = 0; if (miss) *miss = 0; if (stall) *stall = 0;
}
void GXClearVCacheMetric(void) {}
void GXInitXfRasMetric(void) {}
u32 GXReadClksPerVtx(void) { return 0; }

/* ================================================================
 * Indirect Textures (bump mapping)
 * ================================================================ */
void GXSetTevDirect(GXTevStageID tev_stage) { (void)tev_stage; }
void GXSetNumIndStages(u8 nIndStages) { g_gx.num_ind_stages = nIndStages; }
void GXSetIndTexMtx(GXIndTexMtxID mtx_sel, const void *offset, s8 scale_exp) {
    (void)mtx_sel; (void)offset; (void)scale_exp;
}
void GXSetIndTexOrder(GXIndTexStageID ind_stage, GXTexCoordID tex_coord, GXTexMapID tex_map) {
    (void)ind_stage; (void)tex_coord; (void)tex_map;
}
void GXSetTevIndirect(GXTevStageID tev_stage, GXIndTexStageID ind_stage, GXIndTexFormat format,
                      GXIndTexBiasSel bias_sel, GXIndTexMtxID matrix_sel, GXIndTexWrap wrap_s,
                      GXIndTexWrap wrap_t, GXBool add_prev, GXBool ind_lod, GXIndTexAlphaSel alpha_sel) {
    (void)tev_stage; (void)ind_stage; (void)format; (void)bias_sel; (void)matrix_sel;
    (void)wrap_s; (void)wrap_t; (void)add_prev; (void)ind_lod; (void)alpha_sel;
}
void GXSetTevIndTile(GXTevStageID tev_stage, GXIndTexStageID ind_stage, u16 tilesize_s, u16 tilesize_t,
                     u16 tilespacing_s, u16 tilespacing_t, GXIndTexFormat format, GXIndTexMtxID matrix_sel,
                     GXIndTexBiasSel bias_sel, GXIndTexAlphaSel alpha_sel) {
    (void)tev_stage; (void)ind_stage; (void)tilesize_s; (void)tilesize_t;
    (void)tilespacing_s; (void)tilespacing_t; (void)format; (void)matrix_sel;
    (void)bias_sel; (void)alpha_sel;
}
void GXSetIndTexCoordScale(GXIndTexStageID ind_state, GXIndTexScale scale_s, GXIndTexScale scale_t) {
    (void)ind_state; (void)scale_s; (void)scale_t;
}

/* ================================================================
 * Display Lists
 * ================================================================ */
static void dl_record(DLEntry *entry) {
    if (!g_gx.recording_dl || !g_gx.dl_buf) return;
    u32 max_entries = g_gx.dl_buf_size / sizeof(DLEntry);
    if (g_gx.dl_count < max_entries) {
        g_gx.dl_buf[g_gx.dl_count++] = *entry;
    }
}

/* Display list handle stored in game's buffer.
 * The game allocates a small buffer expecting GC FIFO commands (~4 bytes each).
 * Our DLEntry is much larger (~16 bytes), so we malloc our own buffer and store
 * a handle in the game's buffer that GXCallDisplayList can look up. */
typedef struct {
    u32 magic;        /* 0xDEADDL01 */
    u32 count;
    DLEntry *entries;
} DLHandle;
#define DL_HANDLE_MAGIC 0xDEAD0101

#define DL_INTERNAL_MAX 16384

static int g_gx_dl_create_count = 0;
void GXBeginDisplayList(void *list, u32 size) {
    g_gx.recording_dl = 1;
    /* Allocate our own internal buffer for DL entries */
    g_gx.dl_buf = (DLEntry *)malloc(DL_INTERNAL_MAX * sizeof(DLEntry));
    g_gx.dl_buf_size = DL_INTERNAL_MAX * sizeof(DLEntry);
    g_gx.dl_count = 0;
    /* Remember where the game's buffer is so we can store the handle */
    g_gx.dl_game_buf = list;
    g_gx_dl_create_count++;
}

u32 GXEndDisplayList(void) {
    g_gx.recording_dl = 0;
    /* Shrink internal buffer to actual size */
    DLEntry *final = (DLEntry *)realloc(g_gx.dl_buf, g_gx.dl_count * sizeof(DLEntry));
    if (!final) final = g_gx.dl_buf;
    /* Store handle in game's buffer */
    DLHandle *handle = (DLHandle *)g_gx.dl_game_buf;
    handle->magic = DL_HANDLE_MAGIC;
    handle->count = g_gx.dl_count;
    handle->entries = final;
    if (g_gx_dl_create_count <= 5) {
        printf("[GX] DL created #%d: buf=%p count=%u entries=%p handle_size=%zu\n",
               g_gx_dl_create_count, (void*)g_gx.dl_game_buf, g_gx.dl_count, (void*)final, sizeof(DLHandle));
    }
    g_gx.dl_buf = NULL;
    /* Return the size of the handle (what the game thinks is the DL size) */
    return (u32)sizeof(DLHandle);
}

void GXCallDisplayList(const void *list, u32 nbytes) {
    g_gx_dl_call_count++;
    if (!list || nbytes == 0) { g_gx_dl_fail_null++; return; }
    const DLHandle *handle = (const DLHandle *)list;
    if (handle->magic != DL_HANDLE_MAGIC || !handle->entries) {
        g_gx_dl_fail_magic++;
        if (g_gx_dl_fail_magic <= 5) {
            printf("[GX] DL magic fail: ptr=%p nbytes=%u magic=0x%08x (expected 0x%08x)\n",
                   list, nbytes, handle->magic, DL_HANDLE_MAGIC);
        }
        return;
    }
    g_gx_dl_ok++;
    const DLEntry *entries = handle->entries;
    u32 count = handle->count;

    for (u32 i = 0; i < count; i++) {
        const DLEntry *e = &entries[i];
        switch (e->cmd) {
            case DL_CMD_BEGIN:
                GXBegin((GXPrimitive)e->begin.prim, (GXVtxFmt)e->begin.fmt, e->begin.nverts);
                break;
            case DL_CMD_END:
                GXEnd();
                break;
            case DL_CMD_POSITION1X16:
                GXPosition1x16(e->index);
                break;
            case DL_CMD_POSITION1X8:
                GXPosition1x8((u8)e->index);
                break;
            case DL_CMD_NORMAL1X16:
                GXNormal1x16(e->index);
                break;
            case DL_CMD_NORMAL1X8:
                GXNormal1x8((u8)e->index);
                break;
            case DL_CMD_COLOR1X16:
                GXColor1x16(e->index);
                break;
            case DL_CMD_COLOR1X8:
                GXColor1x8((u8)e->index);
                break;
            case DL_CMD_TEXCOORD1X16:
                GXTexCoord1x16(e->index);
                break;
            case DL_CMD_TEXCOORD1X8:
                GXTexCoord1x8((u8)e->index);
                break;
            case DL_CMD_POSITION3F32:
                GXPosition3f32(e->pos.x, e->pos.y, e->pos.z);
                break;
            case DL_CMD_NORMAL3F32:
                GXNormal3f32(e->pos.x, e->pos.y, e->pos.z);
                break;
            case DL_CMD_TEXCOORD2F32:
                GXTexCoord2f32(e->tc.s, e->tc.t);
                break;
            case DL_CMD_COLOR4U8:
                GXColor4u8(e->clr.r, e->clr.g, e->clr.b, e->clr.a);
                break;
            case DL_CMD_COLOR1U32:
                GXColor1u32(e->clr32);
                break;
            default:
                break;
        }
    }

    /* Flush any pending vertices.
     * On real GC hardware, GXEnd() is a no-op — the hardware processes vertices
     * as they arrive. Display lists never contain GXEnd commands. We must
     * manually flush after replaying all DL entries. */
    if (g_gx.verts_submitted > 0) {
        rasterize_primitives();
        g_gx.verts_submitted = 0;
    }
}

/* ================================================================
 * Misc
 * ================================================================ */
void GXDrawSphere(u8 numMajor, u8 numMinor) { (void)numMajor; (void)numMinor; }
void GXSetVerifyLevel(u32 level) { (void)level; }
