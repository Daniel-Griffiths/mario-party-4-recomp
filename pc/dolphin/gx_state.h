#ifndef GX_STATE_H
#define GX_STATE_H

/*
 * Internal GX pipeline state for the software renderer.
 */

#include "dolphin/types.h"
#include "dolphin/gx/GXEnum.h"
#include "dolphin/gx/GXStruct.h"

#define GX_MAX_POS_MATRICES  10
#define GX_MAX_NRM_MATRICES  10
#define GX_MAX_TEX_MATRICES  10
#define GX_MAX_TEV_STAGES    16
#define GX_MAX_TEXTURES      8
#define GX_MAX_LIGHTS        8
#define GX_MAX_VERTEX_ATTRS  32
#define GX_MAX_PENDING_VERTS 65536

/* Software framebuffer */
#define GX_FB_WIDTH  640
#define GX_FB_HEIGHT 480

typedef struct {
    float pos[3];
    float nrm[3];
    float color[4];
    float texcoord[8][2];
} GXSWVertex;

/* Display list command types */
typedef enum {
    DL_CMD_BEGIN,
    DL_CMD_END,
    DL_CMD_POSITION1X16,
    DL_CMD_NORMAL1X16,
    DL_CMD_COLOR1X16,
    DL_CMD_TEXCOORD1X16,
    DL_CMD_POSITION3F32,
    DL_CMD_NORMAL3F32,
    DL_CMD_TEXCOORD2F32,
    DL_CMD_COLOR4U8,
    DL_CMD_COLOR1U32,
    DL_CMD_POSITION1X8,
    DL_CMD_NORMAL1X8,
    DL_CMD_COLOR1X8,
    DL_CMD_TEXCOORD1X8,
} DLCommand;

typedef struct {
    u8 cmd;
    union {
        u16 index;
        struct { float x, y, z; } pos;
        struct { float s, t; } tc;
        struct { u8 r, g, b, a; } clr;
        u32 clr32;
        struct { u8 prim; u8 fmt; u16 nverts; } begin;
    };
} DLEntry;

/* Vertex array descriptor */
typedef struct {
    const void *data;
    u8 stride;
} GXVtxArray;

/* Vertex attribute format per vtxfmt/attr */
typedef struct {
    GXCompCnt cnt;
    GXCompType type;
    u8 frac;
} GXVtxAttrFmtEntry;

/* Decoded texture cache entry */
typedef struct {
    u32 *decoded;      /* linear RGBA8 pixels */
    u16 width, height;
    GXTexFmt format;
    void *raw_data;    /* original GC data pointer (cache key) */
    GXTexWrapMode wrap_s, wrap_t;
} GXTexCacheEntry;

/* TEV stage order (which texmap + channel) */
typedef struct {
    GXTexCoordID coord;
    GXTexMapID map;
    GXChannelID color;
} GXTevOrderEntry;

typedef struct {
    /* Projection */
    float projection[4][4];
    int   proj_type; /* GX_PERSPECTIVE or GX_ORTHOGRAPHIC */

    /* Position/normal matrices */
    float pos_mtx[GX_MAX_POS_MATRICES][3][4];
    float nrm_mtx[GX_MAX_NRM_MATRICES][3][4];
    float tex_mtx[GX_MAX_TEX_MATRICES][3][4];
    u32   current_pos_mtx;

    /* Viewport / scissor */
    float vp_left, vp_top, vp_wd, vp_ht, vp_nearz, vp_farz;
    u32   sc_left, sc_top, sc_wd, sc_ht;

    /* Vertex format state */
    GXVtxDescList vtx_desc[GX_MAX_VERTEX_ATTRS];
    int           vtx_desc_count;
    GXVtxFmt      current_vtx_fmt;

    /* Vertex arrays (indexed by GXAttr) */
    GXVtxArray vtx_arrays[GX_VA_MAX_ATTR];

    /* Vertex attribute format [vtxfmt][attr] */
    GXVtxAttrFmtEntry vtx_attr_fmt[GX_MAX_VTXFMT][GX_VA_MAX_ATTR];

    /* TEV */
    u8 num_tev_stages;
    u8 num_tex_gens;
    u8 num_chans;
    u8 num_ind_stages;

    /* TEV stage ops (simplified - mode per stage) */
    GXTevMode tev_mode[GX_MAX_TEV_STAGES];

    /* TEV stage order */
    GXTevOrderEntry tev_order[GX_MAX_TEV_STAGES];

    /* Textures */
    GXTexObj  tex_obj[GX_MAX_TEXTURES];
    int       tex_loaded[GX_MAX_TEXTURES]; /* 1 if loaded */

    /* Decoded texture cache */
    GXTexCacheEntry tex_cache[GX_MAX_TEXTURES];

    /* Channel colors */
    GXColor chan_amb[2];
    GXColor chan_mat[2];

    /* Channel control (from GXSetChanCtrl) */
    struct {
        GXBool     enable;     /* lighting enable */
        GXColorSrc amb_src;    /* GX_SRC_REG or GX_SRC_VTX */
        GXColorSrc mat_src;    /* GX_SRC_REG or GX_SRC_VTX */
        u32        light_mask;
    } chan_ctrl[2];

    /* Blend / Z */
    GXBlendMode   blend_type;
    GXBlendFactor  blend_src;
    GXBlendFactor  blend_dst;
    GXBool         z_enable;
    GXCompare      z_func;
    GXBool         z_write;
    GXBool         color_update;
    GXBool         alpha_update;

    /* Alpha compare (alpha test) */
    GXCompare      alpha_comp0;
    u8             alpha_ref0;
    GXAlphaOp      alpha_op;
    GXCompare      alpha_comp1;
    u8             alpha_ref1;

    /* Fog */
    GXFogType fog_type;
    float     fog_startz, fog_endz, fog_nearz, fog_farz;
    GXColor   fog_color;

    /* Cull mode */
    GXCullMode cull_mode;

    /* Current primitive being collected */
    GXPrimitive current_prim;
    u16         current_prim_nverts;
    u32         verts_submitted;
    GXSWVertex  vert_buf[GX_MAX_PENDING_VERTS];

    /* Incoming vertex accumulator */
    GXSWVertex  current_vertex;
    int         cur_vtx_has_pos;
    int         cur_vtx_has_nrm;
    int         cur_vtx_has_clr;
    int         cur_vtx_texcoord_count;

    /* Display list recording */
    int recording_dl;
    DLEntry *dl_buf;
    u32 dl_buf_size;    /* max bytes */
    u32 dl_count;       /* entries recorded */
    void *dl_game_buf;  /* game's original buffer for storing DL handle */

    /* Software framebuffer (RGBA8) */
    u32 framebuffer[GX_FB_WIDTH * GX_FB_HEIGHT];
    float zbuffer[GX_FB_WIDTH * GX_FB_HEIGHT];

    /* Display state */
    int  black_screen;
    void *xfb_ptr; /* External framebuffer pointer from game */

    /* Misc */
    GXColor clear_color;
    u32     clear_z;
} GXState;

extern GXState g_gx;

#endif /* GX_STATE_H */
