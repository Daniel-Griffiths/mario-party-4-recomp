#include "game/sprite.h"
#include "game/memory.h"
#include "game/init.h"

#include "dolphin/mtx.h"

#define SPRITE_DIRTY_ATTR 0x1
#define SPRITE_DIRTY_XFORM 0x2
#define SPRITE_DIRTY_COLOR 0x4

typedef struct sprite_order {
    u16 group;
    u16 sprite;
    u16 prio;
    u16 next;
} SpriteOrder;

HuSprite HuSprData[HUSPR_MAX];
HuSprGrp HuSprGrpData[HUSPR_GRP_MAX];
static SpriteOrder HuSprOrder[HUSPR_MAX*2];

static s16 HuSprOrderNum;
static s16 HuSprOrderNo;
static BOOL HuSprPauseF;

static void HuSprOrderEntry(s16 group, s16 sprite);


void HuSprInit(void)
{
    s16 i;
    HuSprite *sprite;
    HuSprGrp *group;
    for(sprite = &HuSprData[1], i=1; i<HUSPR_MAX; i++, sprite++) {
        sprite->data = NULL;
    }
    for(group = HuSprGrpData, i=0; i<HUSPR_GRP_MAX; i++, group++) {
        group->capacity = 0;
    }
    sprite = &HuSprData[0];
    sprite->prio = 0;
    sprite->data = (void *)1;
    HuSprPauseF = FALSE;
}

void HuSprClose(void)
{
    s16 i;
    HuSprGrp *group;
    HuSprite *sprite;
    
    for(group = HuSprGrpData, i=0; i<HUSPR_GRP_MAX; i++, group++) {
        if(group->capacity != 0) {
            HuSprGrpKill(i);
        }
    }
    for(sprite = &HuSprData[1], i=1; i<HUSPR_MAX; i++, sprite++) {
        if(sprite->data) {
            HuSprKill(i);
        }
    }
    HuSprPauseF = FALSE;
}

void HuSprExec(s16 draw_no)
{
    HuSprite *sprite;
#ifdef TARGET_PC
    static int spr_diag_frame = 0;
    int spr_total = 0, spr_drawn = 0, spr_skip_data = 0, spr_skip_dispoff = 0, spr_skip_drawno = 0;
#endif
    while(sprite = HuSprCall()) {
#ifdef TARGET_PC
        spr_total++;
        if(!(sprite->attr & HUSPR_ATTR_FUNC) && (!sprite->data || (uintptr_t)sprite->data < 0x10000)) {
            spr_skip_data++;
            continue;
        }
#endif
        if(!(sprite->attr & HUSPR_ATTR_DISPOFF) && sprite->draw_no == draw_no) {
            HuSprDisp(sprite);
#ifdef TARGET_PC
            spr_drawn++;
#endif
        }
#ifdef TARGET_PC
        else if(sprite->attr & HUSPR_ATTR_DISPOFF) {
            spr_skip_dispoff++;
        } else {
            spr_skip_drawno++;
        }
#endif
    }
#ifdef TARGET_PC
    if (spr_diag_frame < 600 && spr_diag_frame % 120 == 0) {
        printf("[SPR] HuSprExec(draw_no=%d): total=%d drawn=%d skip_data=%d skip_dispoff=%d skip_drawno=%d\n",
            draw_no, spr_total, spr_drawn, spr_skip_data, spr_skip_dispoff, spr_skip_drawno);
    }
    spr_diag_frame++;
#endif
}

void HuSprBegin(void)
{
    Mtx temp, rot;
    s16 i, j;
    Vec axis = {0, 0, 1};
    HuSprGrp *group;
    group = HuSprGrpData;
    HuSprOrderNum = 1;
    HuSprOrder[0].next = 0;
    HuSprOrder[0].prio = -1;
    for(i=0; i<HUSPR_GRP_MAX; i++, group++) {
        if(group->capacity != 0) {
            MTXTrans(temp, group->center_x*group->scale_x, group->center_y*group->scale_y, 0.0f);
            MTXRotAxisDeg(rot, &axis, group->z_rot);
            MTXConcat(rot, temp, group->mtx);
            MTXScale(temp, group->scale_x, group->scale_y, 1.0f);
            MTXConcat(group->mtx, temp, group->mtx);
            mtxTransCat(group->mtx, group->x, group->y, 0);
            for(j=0; j<group->capacity; j++) {
                if(group->members[j] != -1) {
                    HuSprOrderEntry(i, group->members[j]);
                }
            }
        }
    }
    HuSprOrderNo = 0;

#ifdef TARGET_PC
    {
        static int begin_diag = 0;
        if (begin_diag >= 900 && begin_diag < 910 && HuSprOrderNum > 1) {
            printf("[SPR-BEGIN] %d sprites in order list:\n", HuSprOrderNum - 1);
            s16 cur = HuSprOrder[0].next;
            int count = 0;
            while (cur != 0 && count < 20) {
                SpriteOrder *o = &HuSprOrder[cur];
                HuSprite *s = &HuSprData[o->sprite];
                printf("[SPR-BEGIN]   #%d: grp=%d spr=%d draw_no=%d prio=%d attr=0x%x data=%p\n",
                    count, o->group, o->sprite, s->draw_no, s->prio, s->attr, (void*)s->data);
                cur = o->next;
                count++;
            }
        }
        begin_diag++;
    }
#endif
}

static void HuSprOrderEntry(s16 group, s16 sprite)
{
    SpriteOrder *order = &HuSprOrder[HuSprOrderNum];
    s16 prio = HuSprData[sprite].prio;
    s16 prev, next;
    if(HuSprOrderNum >= HUSPR_MAX*2) {
        OSReport("Order Max Over!\n");
        return;
    }
    next = HuSprOrder[0].next;
    for(prev = 0; next != 0; prev = next, next = HuSprOrder[next].next) {
        if(HuSprOrder[next].prio < prio) {
            break;
        }
    }
    order->next = HuSprOrder[prev].next;
    HuSprOrder[prev].next = HuSprOrderNum;
    order->prio = prio;
    order->group = group;
    order->sprite = sprite;
    HuSprOrderNum++;
}

HuSprite *HuSprCall(void)
{
    HuSprOrderNo = HuSprOrder[HuSprOrderNo].next;
    if(HuSprOrderNo != 0) {
        SpriteOrder *order = &HuSprOrder[HuSprOrderNo];
        HuSprite *sprite = &HuSprData[order->sprite];
        sprite->group_mtx = &HuSprGrpData[order->group].mtx;
        if(sprite->attr & HUSPR_ATTR_FUNC) {
            return sprite;
        }
#ifdef TARGET_PC
        if(!sprite->data || (uintptr_t)sprite->data < 0x10000) {
            return sprite;
        }
#endif
        sprite->frame_data = &sprite->data->bank[sprite->bank].frame[sprite->frame];
        sprite->pat_data = &sprite->data->pat[sprite->frame_data->pat];
        return sprite;
    } else {
        return NULL;
    }
}

static inline void SpriteCalcFrame(HuSprite *sprite, AnimBankData *bank, AnimFrameData **frame, s16 loop)
{
    if(sprite->time >= (*frame)->time) {
        sprite->frame++;
        sprite->time -= (*frame)->time;
        if(sprite->frame >= bank->timeNum || (*frame)[1].time == -1) {
            if(loop) {
                sprite->frame = 0;
            } else {
                sprite->frame = bank->timeNum-1;
            }
        }
        *frame = &bank->frame[sprite->frame];
    } else if(sprite->time < 0) {
        sprite->frame--;
        if(sprite->frame < 0) {
            if(loop) {
                sprite->frame = bank->timeNum-1;
            } else {
                sprite->frame = 0;
            }
        }
        *frame = &bank->frame[sprite->frame];
        sprite->time += (*frame)->time;
    }
}

void HuSprFinish(void)
{
    AnimData *anim;
    AnimBankData *bank;
    AnimFrameData *frame;
    HuSprite *sprite;
    s16 i;
    s16 j;
    s16 loop;
    s16 dir;
    
    for(sprite = &HuSprData[1], i=1; i<HUSPR_MAX; i++, sprite++) {
        if(sprite->data && !(sprite->attr & HUSPR_ATTR_FUNC)) {
            if(!HuSprPauseF || (sprite->attr & HUSPR_ATTR_NOPAUSE)) {
                anim = sprite->data;
                bank = &anim->bank[sprite->bank];
                frame = &bank->frame[sprite->frame];
                loop = (sprite->attr & HUSPR_ATTR_LOOP) ? 0 : 1;
                if(!(sprite->attr & HUSPR_ATTR_NOANIM)) {
                    dir = (sprite->attr & HUSPR_ATTR_REVERSE) ? -1 : 1;
                    for(j=0; j<(s32)sprite->speed*minimumVcount; j++) {
                        sprite->time += dir;
                        SpriteCalcFrame(sprite, bank, &frame, loop);
                    }
                    sprite->time += (sprite->speed*(float)minimumVcount)-j;
                    SpriteCalcFrame(sprite, bank, &frame, loop);
                }
                sprite->dirty_flag = 0;
            }
        }
    }
}

void HuSprPauseSet(BOOL value)
{
    HuSprPauseF = value;
}

#ifdef TARGET_PC

/* ---- PC: Parse 32-bit GC binary layout with byte-swap ---- */

#define PC_ANIM_MAGIC 0xABCD1234u

/* GC struct sizes (32-bit, with PPC alignment padding) */
#define GC_ANIMDATA_SIZE       20  /* 4*s16(8) + 3*u32(12) */
#define GC_ANIMBANKDATA_SIZE    8  /* 2*s16(4) + u32(4) */
#define GC_ANIMPATDATA_SIZE    16  /* 5*s16(10) + 2pad + u32(4) */
#define GC_ANIMBMPDATA_SIZE    20  /* u8+u8+3*s16(8) + u32(4) + 2*u32(8) */

static inline s16 _anim_bes16(const u8 *p) { return (s16)((p[0] << 8) | p[1]); }
static inline u32 _anim_beu32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }

AnimData *HuSprAnimRead(void *data)
{
    s16 i, j;
    u8 *buf = (u8 *)data;

    if (!data) {
        return NULL;
    }

    /* Check if already processed (native AnimData with real pointers) */
    AnimData *check = (AnimData *)data;
    if (check->_pc_magic == PC_ANIM_MAGIC) {
        check->useNum++;
        return check;
    }

    /* Parse AnimData header from GC 32-bit layout (big-endian) */
    s16 bankNum = _anim_bes16(buf + 0);
    s16 patNum  = _anim_bes16(buf + 2);
    s16 bmpNum  = _anim_bes16(buf + 4);
    u32 bank_off = _anim_beu32(buf + 8);
    u32 pat_off  = _anim_beu32(buf + 12);
    u32 bmp_off  = _anim_beu32(buf + 16);

    /* Allocate native-layout AnimData + sub-arrays in one block */
    size_t total = sizeof(AnimData)
        + bankNum * sizeof(AnimBankData)
        + patNum  * sizeof(AnimPatData)
        + bmpNum  * sizeof(AnimBmpData);

    u8 *block = (u8 *)HuMemDirectMalloc(HEAP_DATA, total);
    AnimData *anim       = (AnimData *)block;
    AnimBankData *banks   = (AnimBankData *)(block + sizeof(AnimData));
    AnimPatData  *pats    = (AnimPatData *)((u8 *)banks + bankNum * sizeof(AnimBankData));
    AnimBmpData  *bmps    = (AnimBmpData *)((u8 *)pats  + patNum  * sizeof(AnimPatData));

    anim->bankNum      = bankNum;
    anim->patNum       = patNum;
    anim->bmpNum       = bmpNum;
    anim->useNum       = 0;
    anim->bank         = banks;
    anim->pat          = pats;
    anim->bmp          = bmps;
    anim->_pc_magic    = PC_ANIM_MAGIC;
    anim->_pc_raw_data = data;  /* original buffer, freed later */

    /* Parse AnimBankData array (GC: 8 bytes each) */
    for (i = 0; i < bankNum; i++) {
        u8 *p = buf + bank_off + i * GC_ANIMBANKDATA_SIZE;
        banks[i].timeNum = _anim_bes16(p + 0);
        banks[i].unk     = _anim_bes16(p + 2);
        u32 frame_off    = _anim_beu32(p + 4);
        banks[i].frame   = (AnimFrameData *)(buf + frame_off);
    }

    /* Byte-swap AnimFrameData arrays in-place (12 bytes each, all s16) */
    for (i = 0; i < bankNum; i++) {
        AnimFrameData *frame = banks[i].frame;
        for (j = 0; j < banks[i].timeNum; j++) {
            u8 *fp = (u8 *)&frame[j];
            frame[j].pat    = _anim_bes16(fp + 0);
            frame[j].time   = _anim_bes16(fp + 2);
            frame[j].shiftX = _anim_bes16(fp + 4);
            frame[j].shiftY = _anim_bes16(fp + 6);
            frame[j].flip   = _anim_bes16(fp + 8);
            frame[j].pad    = _anim_bes16(fp + 10);
        }
    }

    /* Parse AnimPatData array (GC: 16 bytes each) */
    for (i = 0; i < patNum; i++) {
        u8 *p = buf + pat_off + i * GC_ANIMPATDATA_SIZE;
        pats[i].layerNum = _anim_bes16(p + 0);
        pats[i].centerX  = _anim_bes16(p + 2);
        pats[i].centerY  = _anim_bes16(p + 4);
        pats[i].sizeX    = _anim_bes16(p + 6);
        pats[i].sizeY    = _anim_bes16(p + 8);
        /* 2 bytes padding at p+10 */
        u32 layer_off    = _anim_beu32(p + 12);
        pats[i].layer    = (AnimLayerData *)(buf + layer_off);
    }

    /* Byte-swap AnimLayerData arrays in-place (32 bytes each) */
    for (i = 0; i < patNum; i++) {
        AnimLayerData *layer = pats[i].layer;
        for (j = 0; j < pats[i].layerNum; j++) {
            u8 *lp = (u8 *)&layer[j];
            /* alpha (u8) and flip (u8) at offsets 0-1: no swap needed */
            layer[j].bmpNo  = _anim_bes16(lp + 2);
            layer[j].startX = _anim_bes16(lp + 4);
            layer[j].startY = _anim_bes16(lp + 6);
            layer[j].sizeX  = _anim_bes16(lp + 8);
            layer[j].sizeY  = _anim_bes16(lp + 10);
            layer[j].shiftX = _anim_bes16(lp + 12);
            layer[j].shiftY = _anim_bes16(lp + 14);
            {
                s16 k;
                for (k = 0; k < 8; k++) {
                    layer[j].vtx[k] = _anim_bes16(lp + 16 + k * 2);
                }
            }
        }
    }

    /* Parse AnimBmpData array (GC: 20 bytes each) */
    for (i = 0; i < bmpNum; i++) {
        u8 *p = buf + bmp_off + i * GC_ANIMBMPDATA_SIZE;
        bmps[i].pixSize  = p[0];
        bmps[i].dataFmt  = p[1];
        bmps[i].palNum   = _anim_bes16(p + 2);
        bmps[i].sizeX    = _anim_bes16(p + 4);
        bmps[i].sizeY    = _anim_bes16(p + 6);
        bmps[i].dataSize = _anim_beu32(p + 8);
        u32 pal_off      = _anim_beu32(p + 12);
        u32 data_off     = _anim_beu32(p + 16);
        bmps[i].palData  = (void *)(buf + pal_off);
        bmps[i].data     = (void *)(buf + data_off);
    }

    return anim;
}

#else

AnimData *HuSprAnimRead(void *data)
{
    s16 i;
    AnimBmpData *bmp;
    AnimBankData *bank;
    AnimPatData *pat;

    AnimData *anim = (AnimData *)data;
    if((u32)anim->bank & 0xFFFF0000) {
        anim->useNum++;
        return anim;
    }
    bank = (AnimBankData *)((u32)anim->bank+(u32)data);
    anim->bank = bank;
    pat = (AnimPatData *)((u32)anim->pat+(u32)data);
    anim->pat = pat;
    bmp = (AnimBmpData *)((u32)anim->bmp+(u32)data);
    anim->bmp = bmp;
    for(i=0; i<anim->bankNum; i++, bank++) {
        bank->frame = (AnimFrameData *)((u32)bank->frame+(u32)data);
    }
    for(i=0; i<anim->patNum; i++, pat++) {
        pat->layer = (AnimLayerData *)((u32)pat->layer+(u32)data);
    }
    for(i=0; i<anim->bmpNum; i++, bmp++) {
        bmp->palData = (void *)((u32)bmp->palData+(u32)data);
        bmp->data = (void *)((u32)bmp->data+(u32)data);
    }
    anim->useNum = 0;
    return anim;
}

#endif

void HuSprAnimLock(AnimData *anim)
{
#ifdef TARGET_PC
    if (!anim) return;
#endif
    anim->useNum++;
}

s16 HuSprCreate(AnimData *anim, s16 prio, s16 bank)
{
    HuSprite *sprite;
    s16 i;
    for(sprite = &HuSprData[1], i=1; i<HUSPR_MAX; i++, sprite++) {
        if(!sprite->data) {
            break;
        }
    }
    if(i == HUSPR_MAX) {
        return HUSPR_NONE;
    }
    sprite->data = anim;
    sprite->speed = 1.0f;
    sprite->frame = 0;
    sprite->bank = bank;
    sprite->time = 0.0f;
    sprite->attr = 0;
    sprite->draw_no = 0;
    sprite->r = sprite->g = sprite->b = sprite->a = 255;
    sprite->x = sprite->y = sprite->z_rot = 0.0f;
    sprite->prio = prio;
    sprite->scale_x = sprite->scale_y = 1.0f;
    sprite->wrap_s = sprite->wrap_t = GX_CLAMP;
    sprite->tex_scale_x = sprite->tex_scale_y = 1;
    sprite->bg = NULL;
    sprite->scissor_x = sprite->scissor_y = 0;
    sprite->scissor_w = 640;
    sprite->scissor_h = 480;
    if(anim) {
        HuSprAnimLock(anim);
    }
    return i;
}

s16 HuSprFuncCreate(HuSprFunc func, s16 prio)
{
    HuSprite *sprite;
    s16 index = HuSprCreate(NULL, prio, 0);
    if(index == HUSPR_NONE) {
        return HUSPR_NONE;
    }
    sprite = &HuSprData[index];
    sprite->func = func;
    sprite->attr |= HUSPR_ATTR_FUNC;
    return index;
}

s16 HuSprGrpCreate(s16 capacity)
{
    HuSprGrp *group;
    s16 i, j;
    for(group = HuSprGrpData, i=0; i<HUSPR_GRP_MAX; i++, group++) {
        if(group->capacity == 0) {
            break;
        }
    }
    if(i == HUSPR_GRP_MAX) {
        return HUSPR_GRP_NONE;
    }
    group->members = HuMemDirectMalloc(HEAP_SYSTEM, sizeof(s16)*capacity);
    for(j=0; j<capacity; j++) {
        group->members[j] = HUSPR_NONE;
    }
    group->capacity = capacity;
    group->x = group->y = group->z_rot = group->center_x = group->center_y = 0.0f;
    group->scale_x = group->scale_y = 1.0f;
    return i;
}

s16 HuSprGrpCopy(s16 group)
{
    HuSprGrp *new_group_ptr;
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    s16 new_group = HuSprGrpCreate(group_ptr->capacity);
    s16 i;
    if(new_group == HUSPR_GRP_NONE) {
        return HUSPR_GRP_NONE;
    }
    new_group_ptr = &HuSprGrpData[new_group];
    new_group_ptr->x = group_ptr->x;
    new_group_ptr->y = group_ptr->y;
    new_group_ptr->z_rot = group_ptr->z_rot;
    new_group_ptr->scale_x = group_ptr->scale_x;
    new_group_ptr->scale_y = group_ptr->scale_y;
    new_group_ptr->center_x = group_ptr->center_x;
    new_group_ptr->center_y = group_ptr->center_y;
    for(i=0; i<group_ptr->capacity; i++) {
        if(group_ptr->members[i] != HUSPR_NONE) {
            HuSprite *old_sprite = &HuSprData[group_ptr->members[i]];
            s16 new_sprite = HuSprCreate(old_sprite->data, old_sprite->prio, old_sprite->bank);
            HuSprData[new_sprite] = *old_sprite;
            HuSprGrpMemberSet(new_group, i, new_sprite);
        }
    }
    return new_group;
}

void HuSprGrpMemberSet(s16 group, s16 member, s16 sprite)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    HuSprite *sprite_ptr = &HuSprData[sprite];
    if(group_ptr->capacity == 0 || group_ptr->capacity <= member || group_ptr->members[member] != HUSPR_NONE) {
        return;
    }
    group_ptr->members[member] = sprite;
}

void HuSprGrpMemberKill(s16 group, s16 member)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    if(group_ptr->capacity == 0 || group_ptr->capacity <= member || group_ptr->members[member] == HUSPR_NONE) {
        return;
    }
    HuSprKill(group_ptr->members[member]);
    group_ptr->members[member] = HUSPR_NONE;
}

void HuSprGrpKill(s16 group)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    s16 i;
    for(i=0; i<group_ptr->capacity; i++) {
        if(group_ptr->members[i] != HUSPR_NONE) {
            HuSprKill(group_ptr->members[i]);
        }
    }
    group_ptr->capacity = 0;
    HuMemDirectFree(group_ptr->members);
}

void HuSprKill(s16 sprite)
{
    HuSprite *sprite_ptr = &HuSprData[sprite];
    if(!sprite_ptr->data) {
        return;
    }
    if(!(sprite_ptr->attr & HUSPR_ATTR_FUNC)) {
        HuSprAnimKill(sprite_ptr->data);
        if(sprite_ptr->bg) {
            HuSprAnimKill(sprite_ptr->bg);
            sprite_ptr->bg = NULL;
        }
    }
    sprite_ptr->data = NULL;
}

void HuSprAnimKill(AnimData *anim)
{
#ifdef TARGET_PC
    if (!anim || (uintptr_t)anim < 0x10000) return;
#endif
    if(--anim->useNum <= 0) {
        if(anim->bmpNum & ANIM_BMP_ALLOC) {
            if(anim->bmp->data) {
                HuMemDirectFree(anim->bmp->data);
            }
            if(anim->bmp->palData) {
                HuMemDirectFree(anim->bmp->palData);
            }
        }
#ifdef TARGET_PC
        if(anim->_pc_raw_data) {
            HuMemDirectFree(anim->_pc_raw_data);
        }
#endif
        HuMemDirectFree(anim);
    }
}

void HuSprAttrSet(s16 group, s16 member, s32 attr)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    HuSprite *sprite_ptr;
    if(group_ptr->capacity == 0 || group_ptr->capacity <= member || group_ptr->members[member] == HUSPR_NONE) {
        return;
    }
    sprite_ptr = &HuSprData[group_ptr->members[member]];
    sprite_ptr->attr |= attr;
    sprite_ptr->dirty_flag |= SPRITE_DIRTY_ATTR;
}

void HuSprAttrReset(s16 group, s16 member, s32 attr)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    HuSprite *sprite_ptr;
    if(group_ptr->capacity == 0 || group_ptr->capacity <= member || group_ptr->members[member] == HUSPR_NONE) {
        return;
    }
    sprite_ptr = &HuSprData[group_ptr->members[member]];
    sprite_ptr->attr &= ~attr;
    sprite_ptr->dirty_flag |= SPRITE_DIRTY_ATTR;
}

void HuSprPosSet(s16 group, s16 member, float x, float y)
{
    HuSprite *sprite_ptr = &HuSprData[HuSprGrpData[group].members[member]];
    sprite_ptr->x = x;
    sprite_ptr->y = y;
    sprite_ptr->dirty_flag |= SPRITE_DIRTY_XFORM;
}

void HuSprZRotSet(s16 group, s16 member, float z_rot)
{
    HuSprite *sprite_ptr = &HuSprData[HuSprGrpData[group].members[member]];
    sprite_ptr->z_rot = z_rot;
    sprite_ptr->dirty_flag |= SPRITE_DIRTY_XFORM;
}

void HuSprScaleSet(s16 group, s16 member, float x, float y)
{
    HuSprite *sprite_ptr = &HuSprData[HuSprGrpData[group].members[member]];
    sprite_ptr->scale_x = x;
    sprite_ptr->scale_y = y;
    sprite_ptr->dirty_flag |= SPRITE_DIRTY_XFORM;
}

void HuSprTPLvlSet(s16 group, s16 member, float tp_lvl)
{
    HuSprite *sprite_ptr = &HuSprData[HuSprGrpData[group].members[member]];
    sprite_ptr->a = tp_lvl*255;
    sprite_ptr->dirty_flag |= SPRITE_DIRTY_COLOR;
}

void HuSprColorSet(s16 group, s16 member, u8 r, u8 g, u8 b)
{
    HuSprite *sprite_ptr = &HuSprData[HuSprGrpData[group].members[member]];
    sprite_ptr->r = r;
    sprite_ptr->g = g;
    sprite_ptr->b = b;
    sprite_ptr->dirty_flag |= SPRITE_DIRTY_COLOR;
}

void HuSprSpeedSet(s16 group, s16 member, float speed)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    HuSprData[group_ptr->members[member]].speed = speed;
}

void HuSprBankSet(s16 group, s16 member, s16 bank)
{
    HuSprite *sprite_ptr = &HuSprData[HuSprGrpData[group].members[member]];
    AnimData *anim = sprite_ptr->data;
    AnimBankData *bank_ptr = &anim->bank[sprite_ptr->bank];
    AnimFrameData *frame_ptr = &bank_ptr->frame[sprite_ptr->frame];
    sprite_ptr->bank = bank;
    if(sprite_ptr->attr & HUSPR_ATTR_REVERSE) {
        sprite_ptr->frame = bank_ptr->timeNum-1;
        frame_ptr = &bank_ptr->frame[sprite_ptr->frame];
        sprite_ptr->time = frame_ptr->time;
    } else {
        sprite_ptr->time = 0;
        sprite_ptr->frame = 0;
    }
}

void HuSprGrpPosSet(s16 group, float x, float y)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    s16 i;
    group_ptr->x = x;
    group_ptr->y = y;
    for(i=0; i<group_ptr->capacity; i++) {
        if(group_ptr->members[i] != -1) {
            HuSprData[group_ptr->members[i]].dirty_flag |= SPRITE_DIRTY_XFORM;
        }
    }
}

void HuSprGrpCenterSet(s16 group, float x, float y)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    s16 i;
    group_ptr->center_x = x;
    group_ptr->center_y = y;
    for(i=0; i<group_ptr->capacity; i++) {
        if(group_ptr->members[i] != HUSPR_NONE) {
            HuSprData[group_ptr->members[i]].dirty_flag |= SPRITE_DIRTY_XFORM;
        }
    }
}

void HuSprGrpZRotSet(s16 group, float z_rot)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    s16 i;
    group_ptr->z_rot = z_rot;
    for(i=0; i<group_ptr->capacity; i++) {
        if(group_ptr->members[i] != HUSPR_NONE) {
            HuSprData[group_ptr->members[i]].dirty_flag |= SPRITE_DIRTY_XFORM;
        }
    }
}

void HuSprGrpScaleSet(s16 group, float x, float y)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    s16 i;
    group_ptr->scale_x = x;
    group_ptr->scale_y = y;
    for(i=0; i<group_ptr->capacity; i++) {
        if(group_ptr->members[i] != HUSPR_NONE) {
            HuSprData[group_ptr->members[i]].dirty_flag |= SPRITE_DIRTY_XFORM;
        }
    }
}

void HuSprGrpTPLvlSet(s16 group, float tp_lvl)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    s16 i;
    for(i=0; i<group_ptr->capacity; i++) {
        if(group_ptr->members[i] != HUSPR_NONE) {
            HuSprData[group_ptr->members[i]].a = tp_lvl*255;
            HuSprData[group_ptr->members[i]].dirty_flag |= SPRITE_DIRTY_COLOR;
        }
    }
}

void HuSprGrpDrawNoSet(s16 group, s32 draw_no)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    s16 i;
    for(i=0; i<group_ptr->capacity; i++) {
        if(group_ptr->members[i] != HUSPR_NONE) {
            HuSprData[group_ptr->members[i]].draw_no = draw_no;
        }
    }
}

void HuSprDrawNoSet(s16 group, s16 member, s32 draw_no)
{
    HuSprite *sprite_ptr = &HuSprData[HuSprGrpData[group].members[member]];
    sprite_ptr->draw_no = draw_no;
}

void HuSprPriSet(s16 group, s16 member, s16 prio)
{
    HuSprite *sprite_ptr = &HuSprData[HuSprGrpData[group].members[member]];
    sprite_ptr->prio = prio;
}

void HuSprGrpScissorSet(s16 group, s16 x, s16 y, s16 w, s16 h)
{
    HuSprGrp *group_ptr = &HuSprGrpData[group];
    s16 i;
    for(i=0; i<group_ptr->capacity; i++) {
        if(group_ptr->members[i] != HUSPR_NONE) {
            HuSprScissorSet(group, i, x, y, w, h);
        }
    }
}

void HuSprScissorSet(s16 group, s16 member, s16 x, s16 y, s16 w, s16 h)
{
    HuSprite *sprite_ptr = &HuSprData[HuSprGrpData[group].members[member]];
    sprite_ptr->scissor_x = x;
    sprite_ptr->scissor_y = y;
    sprite_ptr->scissor_w = w;
    sprite_ptr->scissor_h = h;
}

static s16 bitSizeTbl[11] = { 32, 24, 16, 8, 4, 16, 8, 8, 4, 8, 4 };

AnimData *HuSprAnimMake(s16 sizeX, s16 sizeY, s16 dataFmt)
{
    AnimLayerData *layer;
    AnimBmpData *bmp;
    AnimData *anim;
    AnimPatData *pat;
    AnimFrameData *frame;
    void *temp;
    AnimBankData *bank;
    AnimData *new_anim;

    anim = new_anim = HuMemDirectMalloc(HEAP_DATA, sizeof(AnimData)+sizeof(AnimBankData)+sizeof(AnimFrameData)
                                            +sizeof(AnimPatData)+sizeof(AnimLayerData)+sizeof(AnimBmpData));

    bank = temp = &new_anim[1];
    anim->bank = bank;
    frame = temp = ((char *)temp+sizeof(AnimBankData));
    bank->frame = frame;
    pat = temp = ((char *)temp+sizeof(AnimFrameData));
    anim->pat = pat;
    layer = temp = ((char *)temp+sizeof(AnimPatData));
    pat->layer = layer;
    bmp = temp = ((char *)temp+sizeof(AnimLayerData));
    anim->bmp = bmp;
    anim->useNum = 0;
#ifdef TARGET_PC
    anim->_pc_magic = PC_ANIM_MAGIC;
    anim->_pc_raw_data = NULL;
#endif
    anim->bankNum = 1;
    anim->patNum = 1;
    anim->bmpNum = (1|ANIM_BMP_ALLOC);
    bank->timeNum = 1;
    bank->unk = 10;
    frame->pat = 0;
    frame->time = 10;
    frame->shiftX = frame->shiftY = frame->flip = 0;
    pat->layerNum = 1;
    pat->centerX = sizeX/2;
    pat->centerY = sizeY/2;
    pat->sizeX = sizeX;
    pat->sizeY = sizeY;
    layer->alpha = 255;
    layer->flip = 0;
    layer->bmpNo = 0;
    layer->startX = layer->startY = 0;
    layer->sizeX = sizeX;
    layer->sizeY = sizeY;
    layer->shiftX = layer->shiftY = 0;
    layer->vtx[0] = layer->vtx[1] = 0;
    layer->vtx[2] = sizeX;
    layer->vtx[3] = 0;
    layer->vtx[4] = sizeX;
    layer->vtx[5] = sizeY;
    layer->vtx[6] = 0;
    layer->vtx[7] = sizeY;
    bmp->pixSize = bitSizeTbl[dataFmt];
    bmp->dataFmt = dataFmt;
    bmp->palNum = 0;
    bmp->sizeX = sizeX;
    bmp->sizeY = sizeY;
    bmp->dataSize = sizeX*sizeY*bitSizeTbl[dataFmt]/8;
    bmp->palData = NULL;
    bmp->data = NULL;
    return anim;
}

void HuSprBGSet(s16 group, s16 member,  AnimData *bg, s16 bg_bank)
{
    s16 sprite = HuSprGrpData[group].members[member];
    HuSprSprBGSet(sprite, bg, bg_bank);
}

void HuSprSprBGSet(s16 sprite, AnimData *bg, s16 bg_bank)
{
    HuSprite *sprite_ptr = &HuSprData[sprite];
    sprite_ptr->bg = bg;
    sprite_ptr->bg_bank = bg_bank;
    sprite_ptr->wrap_t = sprite_ptr->wrap_s = GX_REPEAT;
    sprite_ptr->attr &= ~HUSPR_ATTR_LINEAR;
}

void AnimDebug(AnimData *anim)
{
    AnimPatData *pat;
    AnimLayerData *layer;
    s16 i;
    s16 j;
    AnimFrameData *frame;
    AnimBankData *bank;
    AnimBmpData *bmp;
    
    OSReport("patNum %d,bankNum %d,bmpNum %d\n", anim->patNum, anim->bankNum, anim->bmpNum & ANIM_BMP_NUM_MASK);
    pat = anim->pat;
    for(i=0; i<anim->patNum; i++) {
        OSReport("PATTERN%d:\n", i);
        OSReport("\tlayerNum %d,center (%d,%d),size (%d,%d)\n", pat->layerNum, pat->centerX, pat->centerX, pat->sizeX, pat->sizeY);
        layer = pat->layer;
        for(j=0; j<pat->layerNum; j++) {
            OSReport("\t\tfileNo %d,flip %x\n", layer->bmpNo, layer->flip);
            OSReport("\t\tstart (%d,%d),size (%d,%d),shift (%d,%d)\n", layer->startX, layer->startY, layer->sizeX, layer->sizeY, layer->shiftX, layer->shiftY);
            if(j != pat->layerNum-1) {
                OSReport("\n");
            }
            layer++;
        }
        pat++;
    }
    bank = anim->bank;
    for(i=0; i<anim->bankNum; i++) {
        OSReport("BANK%d:\n", i);
        OSReport("\ttimeNum %d\n", bank->timeNum);
        frame = bank->frame;
        for(j=0; j<bank->timeNum; j++) {
            OSReport("\t\tpat %d,time %d,shift(%d,%d),flip %x\n", frame->pat, frame->time, frame->shiftX, frame->shiftY, frame->flip);
            frame++;
        }
        bank++;
    }
    bmp = anim->bmp;
    for(i=0; i<anim->bmpNum & ANIM_BMP_NUM_MASK; i++) {
        OSReport("BMP%d:\n", i);
        OSReport("\tpixSize %d,palNum %d,size (%d,%d)\n", bmp->pixSize, bmp->palNum, bmp->sizeX, bmp->sizeY);
        bmp++;
    }
}