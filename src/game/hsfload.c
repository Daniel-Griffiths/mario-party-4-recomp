#include "game/hsfload.h"
#include "game/EnvelopeExec.h"
#include "string.h"
#include "ctype.h"

#ifdef TARGET_PC
#include <stdint.h>
#include <stdlib.h>
/* On 64-bit, pointer arithmetic must use uintptr_t instead of u32 */
#define PTRCAST (uintptr_t)
#else
#define PTRCAST (u32)
#endif

#ifdef TARGET_PC
/* ---- Big-endian read helpers for HSF binary parsing ---- */
static inline u32 hsf_be32(const void *p) {
    const u8 *b = (const u8 *)p;
    return ((u32)b[0]<<24)|((u32)b[1]<<16)|((u32)b[2]<<8)|b[3];
}
static inline s32 hsf_bes32(const void *p) { return (s32)hsf_be32(p); }
static inline u16 hsf_be16(const void *p) {
    const u8 *b = (const u8 *)p;
    return ((u16)b[0]<<8)|b[1];
}
static inline s16 hsf_bes16(const void *p) { return (s16)hsf_be16(p); }
static inline float hsf_bef(const void *p) {
    u32 v = hsf_be32(p);
    float f; memcpy(&f, &v, 4); return f;
}

/* Byte-swap an array of u32 in-place (BE to native LE) */
static void hsf_swap32_array(void *data, int count) {
    u32 *p = (u32 *)data;
    int i;
    for (i = 0; i < count; i++) {
        u8 *b = (u8 *)&p[i];
        p[i] = ((u32)b[0]<<24)|((u32)b[1]<<16)|((u32)b[2]<<8)|b[3];
    }
}

/* GC file struct sizes (32-bit pointers, 4-byte alignment) */
#define HSF_F_BUF    12   /* HsfBuffer: name(4)+count(4)+data(4) */
#define HSF_F_SCENE  16   /* HsfScene: fogType(4)+start(4)+end(4)+color(4) */
#define HSF_F_PAL    16   /* HsfPalette: name(4)+unk(4)+palSize(4)+data(4) */
#define HSF_F_BMP    32   /* HsfBitmap: see layout below */
#define HSF_F_MAT    60   /* HsfMaterial: see layout below */
#define HSF_F_ATTR  132   /* HsfAttribute: see layout below */
#define HSF_F_OBJ   324   /* HsfObject with HsfObjectData union */
#define HSF_F_FACE   48   /* HsfFace: type(2)+mat(2)+union(32)+nbt(12) */
#define HSF_F_SKEL   40   /* HsfSkeleton: name(4)+transform(36) */
#define HSF_F_MOTION 16   /* HsfMotion: name(4)+numTracks(4)+track(4)+len(4) */
#define HSF_F_TRACK  16   /* HsfTrack: type(1)+start(1)+target(2)+unk04(4)+curveType(2)+numKeyframes(2)+data/value(4) */
#define HSF_F_MATRIX 16   /* HsfMatrix: count(4)+pad(4)+data(4)+pad(4) */

/* Read HsfTransform (9 floats = 36 bytes) from big-endian data */
static void hsf_read_transform(const u8 *src, HsfTransform *dst) {
    dst->pos.x = hsf_bef(src+0);  dst->pos.y = hsf_bef(src+4);  dst->pos.z = hsf_bef(src+8);
    dst->rot.x = hsf_bef(src+12); dst->rot.y = hsf_bef(src+16); dst->rot.z = hsf_bef(src+20);
    dst->scale.x = hsf_bef(src+24); dst->scale.y = hsf_bef(src+28); dst->scale.z = hsf_bef(src+32);
}

static HsfData *LoadHSF_PC(void *data);
#endif /* TARGET_PC */

#define AS_S16(field) (*((s16 *)&(field)))
#define AS_U16(field) (*((u16 *)&(field)))

GXColor rgba[100];
HsfHeader head;
HsfData Model;

static BOOL MotionOnly;
static HsfData *MotionModel;
static void *VertexDataTop;
static void *NormalDataTop;
void *fileptr;
char *StringTable;
char *DicStringTable;
void **NSymIndex;
HsfObject *objtop;
HsfBuffer *vtxtop;
HsfCluster *ClusterTop;
HsfAttribute *AttributeTop;
HsfMaterial *MaterialTop;

static void FileLoad(void *data);
static HsfData *SetHsfModel(void);
static void MaterialLoad(void);
static void AttributeLoad(void);
static void SceneLoad(void);
static void ColorLoad(void);
static void VertexLoad(void);
static void NormalLoad(void);
static void STLoad(void);
static void FaceLoad(void);
static void ObjectLoad(void);
static void CenvLoad(void);
static void SkeletonLoad(void);
static void PartLoad(void);
static void ClusterLoad(void);
static void ShapeLoad(void);
static void MapAttrLoad(void);
static void PaletteLoad(void);
static void BitmapLoad(void);
static void MotionLoad(void);
static void MatrixLoad(void);

static s32 SearchObjectSetName(HsfData *data, char *name);
static HsfBuffer *SearchVertexPtr(s32 id);
static HsfBuffer *SearchNormalPtr(s32 id);
static HsfBuffer *SearchStPtr(s32 id);
static HsfBuffer *SearchColorPtr(s32 id);
static HsfBuffer *SearchFacePtr(s32 id);
static HsfCenv *SearchCenvPtr(s32 id);
static HsfPart *SearchPartPtr(s32 id);
static HsfPalette *SearchPalettePtr(s32 id);

static HsfBitmap *SearchBitmapPtr(s32 id);
static char *GetString(u32 *str_ofs);
static char *GetMotionString(u16 *str_ofs);

HsfData *LoadHSF(void *data)
{
    HsfData *hsf;
#ifdef TARGET_PC
    hsf = LoadHSF_PC(data);
    return hsf;
#else
    Model.root = NULL;
    objtop = NULL;
    FileLoad(data);
    SceneLoad();
    ColorLoad();
    PaletteLoad();
    BitmapLoad();
    MaterialLoad();
    AttributeLoad();
    VertexLoad();
    NormalLoad();
    STLoad();
    FaceLoad();
    ObjectLoad();
    CenvLoad();
    SkeletonLoad();
    PartLoad();
    ClusterLoad();
    ShapeLoad();
    MapAttrLoad();
    MotionLoad();
    MatrixLoad();
    hsf = SetHsfModel();
    InitEnvelope(hsf);
    objtop = NULL;
    return hsf;
#endif
}

void ClusterAdjustObject(HsfData *model, HsfData *src_model)
{
    HsfCluster *cluster;
    s32 i;
    if(!src_model) {
        return;
    }
    if(src_model->clusterCnt == 0) {
        return;
    }
    cluster = src_model->cluster;
    if(cluster->adjusted) {
        return;
    }
    cluster->adjusted = 1;
    for(i=0; i<src_model->clusterCnt; i++, cluster++) {
        char *name = cluster->targetName;
        cluster->target = SearchObjectSetName(model, name);
    }
}

static void FileLoad(void *data)
{
    fileptr = data;
    memcpy(&head, fileptr, sizeof(HsfHeader));
#ifdef TARGET_PC
    {
        /* Byte-swap all HsfSection entries in the header (big-endian → little-endian) */
        s32 *fields = (s32 *)&head.scene;
        int num_fields = (sizeof(HsfHeader) - 8) / sizeof(s32); /* skip 8-byte magic */
        int i;
        for (i = 0; i < num_fields; i++) {
            u8 *b = (u8 *)&fields[i];
            fields[i] = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
        }
        /* Bulk byte-swap all 32-bit words in non-string data sections in-place.
         * The HSF format stores all ints/floats/offsets as big-endian 32-bit.
         * We swap everything except the string table (raw text). */
        {
            HsfSection *sections[] = {
                &head.scene, &head.color, &head.material, &head.attribute,
                &head.vertex, &head.normal, &head.st, &head.face,
                &head.object, &head.bitmap, &head.palette, &head.motion,
                &head.cenv, &head.skeleton, &head.part, &head.cluster,
                &head.shape, &head.mapAttr, &head.matrix, &head.symbol,
                NULL
            };
            /* We also need to find the size of each section. Use next section's offset
             * or total file length. For simplicity, swap from section start to string table
             * start (which is typically last before strings), or just swap the entire
             * data area minus the string table. */
            /* Simpler: swap all u32 words from after the header to the string table,
             * then from after string table to end of file. Actually, let's just figure out
             * what NOT to swap: the string table is pure text. Everything else is 32-bit. */
            u8 *base = (u8 *)fileptr;
            s32 str_start = head.string.ofs;
            /* Find extent of string data - it's typically at the end, but let's find the
             * next section after it or assume it goes to the end of symbol table. */
            /* Swap everything from byte 8 (after magic) to string start */
            u32 *wp = (u32 *)(base + 8);
            u32 *wp_end = (u32 *)(base + str_start);
            for (; wp < wp_end; wp++) {
                u8 *b = (u8 *)wp;
                *wp = ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];
            }
            /* Don't swap the string table - it's ASCII text */
        }
    }
#endif
    memset(&Model, 0, sizeof(HsfData));
    NSymIndex = (void **)(PTRCAST fileptr+head.symbol.ofs);
    StringTable = (char *)(PTRCAST fileptr+head.string.ofs);
    ClusterTop = (HsfCluster *)(PTRCAST fileptr+head.cluster.ofs);
    AttributeTop = (HsfAttribute *)(PTRCAST fileptr+head.attribute.ofs);
    MaterialTop = (HsfMaterial *)(PTRCAST fileptr+head.material.ofs);
}

static HsfData *SetHsfModel(void)
{
    HsfData *data = fileptr;
    data->scene = Model.scene;
    data->sceneCnt = Model.sceneCnt;
    data->attribute = Model.attribute;
    data->attributeCnt = Model.attributeCnt;
    data->bitmap = Model.bitmap;
    data->bitmapCnt = Model.bitmapCnt;
    data->cenv = Model.cenv;
    data->cenvCnt = Model.cenvCnt;
    data->skeleton = Model.skeleton;
    data->skeletonCnt = Model.skeletonCnt;
    data->face = Model.face;
    data->faceCnt = Model.faceCnt;
    data->material = Model.material;
    data->materialCnt = Model.materialCnt;
    data->motion = Model.motion;
    data->motionCnt = Model.motionCnt;
    data->normal = Model.normal;
    data->normalCnt = Model.normalCnt;
    data->root = Model.root;
    data->objectCnt = Model.objectCnt;
    data->object = objtop;
    data->matrix = Model.matrix;
    data->matrixCnt = Model.matrixCnt;
    data->palette = Model.palette;
    data->paletteCnt = Model.paletteCnt;
    data->st = Model.st;
    data->stCnt = Model.stCnt;
    data->vertex = Model.vertex;
    data->vertexCnt = Model.vertexCnt;
    data->cenv = Model.cenv;
    data->cenvCnt = Model.cenvCnt;
    data->cluster = Model.cluster;
    data->clusterCnt = Model.clusterCnt;
    data->part = Model.part;
    data->partCnt = Model.partCnt;
    data->shape = Model.shape;
    data->shapeCnt = Model.shapeCnt;
    data->mapAttr = Model.mapAttr;
    data->mapAttrCnt = Model.mapAttrCnt;
    return data;
}

char *SetName(u32 *str_ofs)
{
    char *ret = GetString(str_ofs);
    return ret;
}

static inline char *SetMotionName(u16 *str_ofs)
{
    char *ret = GetMotionString(str_ofs);
    return ret;
}

static void MaterialLoad(void)
{
    s32 i;
    s32 j;
    if(head.material.count) {
        HsfMaterial *file_mat = (HsfMaterial *)(PTRCAST fileptr+head.material.ofs);
        HsfMaterial *curr_mat;
        HsfMaterial *new_mat;
        for(i=0; i<head.material.count; i++) {
            curr_mat = &file_mat[i];
        }
        new_mat = file_mat;
        Model.material = new_mat;
        Model.materialCnt = head.material.count;
        file_mat = (HsfMaterial *)(PTRCAST fileptr+head.material.ofs);
        for(i=0; i<head.material.count; i++, new_mat++) {
            curr_mat = &file_mat[i];
            new_mat->name = SetName((u32 *)&curr_mat->name);
            new_mat->pass = curr_mat->pass;
            new_mat->vtxMode = curr_mat->vtxMode;
            new_mat->litColor[0] = curr_mat->litColor[0];
            new_mat->litColor[1] = curr_mat->litColor[1];
            new_mat->litColor[2] = curr_mat->litColor[2];
            new_mat->color[0] = curr_mat->color[0];
            new_mat->color[1] = curr_mat->color[1];
            new_mat->color[2] = curr_mat->color[2];
            new_mat->shadowColor[0] = curr_mat->shadowColor[0];
            new_mat->shadowColor[1] = curr_mat->shadowColor[1];
            new_mat->shadowColor[2] = curr_mat->shadowColor[2];
            new_mat->hilite_scale = curr_mat->hilite_scale;
            new_mat->unk18 = curr_mat->unk18;
            new_mat->invAlpha = curr_mat->invAlpha;
            new_mat->unk20[0] = curr_mat->unk20[0];
            new_mat->unk20[1] = curr_mat->unk20[1];
            new_mat->refAlpha = curr_mat->refAlpha;
            new_mat->unk2C = curr_mat->unk2C;
            new_mat->numAttrs = curr_mat->numAttrs;
            new_mat->attrs = (s32 *)(NSymIndex+(PTRCAST curr_mat->attrs));
            rgba[i].r = new_mat->litColor[0];
            rgba[i].g = new_mat->litColor[1];
            rgba[i].b = new_mat->litColor[2];
            rgba[i].a = 255;
            for(j=0; j<new_mat->numAttrs; j++) {
                new_mat->attrs[j] = new_mat->attrs[j];
            }
        }
    }
}

static void AttributeLoad(void)
{
    HsfAttribute *file_attr;
    HsfAttribute *new_attr;
    HsfAttribute *temp_attr;
    s32 i;
    if(head.attribute.count) {
        temp_attr = file_attr = (HsfAttribute *)(PTRCAST fileptr+head.attribute.ofs);
        new_attr = temp_attr;
        Model.attribute = new_attr;
        Model.attributeCnt = head.attribute.count;
        for(i=0; i<head.attribute.count; i++, new_attr++) {
            if(PTRCAST file_attr[i].name != -1) {
                new_attr->name = SetName((u32 *)&file_attr[i].name);
            } else {
                new_attr->name = NULL;
            }
            new_attr->bitmap = SearchBitmapPtr((s32)file_attr[i].bitmap);
        }
    }
}

static void SceneLoad(void)
{
    HsfScene *file_scene;
    HsfScene *new_scene;
    if(head.scene.count) {
        file_scene = (HsfScene *)(PTRCAST fileptr+head.scene.ofs);
        new_scene = file_scene;
        new_scene->end = file_scene->end;
        new_scene->start = file_scene->start;
        Model.scene = new_scene;
        Model.sceneCnt = head.scene.count;
    }
}

static void ColorLoad(void)
{
    s32 i;
    HsfBuffer *file_color;
    HsfBuffer *new_color;
    void *data;
    void *color_data;
    HsfBuffer *temp_color;
    
    if(head.color.count) {
        temp_color = file_color = (HsfBuffer *)(PTRCAST fileptr+head.color.ofs);
        data = &file_color[head.color.count];
        for(i=0; i<head.color.count; i++, file_color++);
        new_color = temp_color;
        Model.color = new_color;
        Model.colorCnt = head.color.count;
        file_color = (HsfBuffer *)(PTRCAST fileptr+head.color.ofs);
        data = &file_color[head.color.count];
        for(i=0; i<head.color.count; i++, new_color++, file_color++) {
            color_data = file_color->data;
            new_color->name = SetName((u32 *)&file_color->name);
            new_color->data = (void *)(PTRCAST data+PTRCAST color_data);
        }
    }
}

static void VertexLoad(void)
{
    s32 i, j;
    HsfBuffer *file_vertex;
    HsfBuffer *new_vertex;
    void *data;
    HsfVector3f *data_elem;
    void *temp_data;
    
    if(head.vertex.count) {
        vtxtop = file_vertex = (HsfBuffer *)(PTRCAST fileptr+head.vertex.ofs);
        data = (void *)&file_vertex[head.vertex.count];
        for(i=0; i<head.vertex.count; i++, file_vertex++) {
            for(j=0; j<PTRCAST file_vertex->count; j++) {
                data_elem = (HsfVector3f *)((PTRCAST data)+(PTRCAST file_vertex->data)+(j*sizeof(HsfVector3f)));
            }
        }
        new_vertex = vtxtop;
        Model.vertex = new_vertex;
        Model.vertexCnt = head.vertex.count;
        file_vertex = (HsfBuffer *)(PTRCAST fileptr+head.vertex.ofs);
        VertexDataTop = data = (void *)&file_vertex[head.vertex.count];
        for(i=0; i<head.vertex.count; i++, new_vertex++, file_vertex++) {
            temp_data = file_vertex->data;
            new_vertex->count = file_vertex->count;
            new_vertex->name = SetName((u32 *)&file_vertex->name);
            new_vertex->data = (void *)(PTRCAST data+PTRCAST temp_data);
            for(j=0; j<new_vertex->count; j++) {
                data_elem = (HsfVector3f *)((PTRCAST data)+(PTRCAST temp_data)+(j*sizeof(HsfVector3f)));
                ((HsfVector3f *)new_vertex->data)[j].x = data_elem->x;
                ((HsfVector3f *)new_vertex->data)[j].y = data_elem->y;
                ((HsfVector3f *)new_vertex->data)[j].z = data_elem->z;
            }
        }
    }
}

static void NormalLoad(void)
{
    s32 i, j;
    void *temp_data;
    HsfBuffer *file_normal;
    HsfBuffer *new_normal;
    HsfBuffer *temp_normal;
    void *data;
    
    
    if(head.normal.count) {
        s32 cenv_count = head.cenv.count;
        temp_normal = file_normal = (HsfBuffer *)(PTRCAST fileptr+head.normal.ofs);
        data = (void *)&file_normal[head.normal.count];
        new_normal = temp_normal;
        Model.normal = new_normal;
        Model.normalCnt = head.normal.count;
        file_normal = (HsfBuffer *)(PTRCAST fileptr+head.normal.ofs);
        NormalDataTop = data = (void *)&file_normal[head.normal.count];
        for(i=0; i<head.normal.count; i++, new_normal++, file_normal++) {
            temp_data = file_normal->data;
            new_normal->count = file_normal->count;
            new_normal->name = SetName((u32 *)&file_normal->name);
            new_normal->data = (void *)(PTRCAST data+PTRCAST temp_data);
        }
    }
}

static void STLoad(void)
{
    s32 i, j;
    HsfBuffer *file_st;
    HsfBuffer *temp_st;
    HsfBuffer *new_st;
    void *data;
    HsfVector2f *data_elem;
    void *temp_data;
    
    if(head.st.count) {
        temp_st = file_st = (HsfBuffer *)(PTRCAST fileptr+head.st.ofs);
        data = (void *)&file_st[head.st.count];
        for(i=0; i<head.st.count; i++, file_st++) {
            for(j=0; j<PTRCAST file_st->count; j++) {
                data_elem = (HsfVector2f *)((PTRCAST data)+(PTRCAST file_st->data)+(j*sizeof(HsfVector2f)));
            }
        }
        new_st = temp_st;
        Model.st = new_st;
        Model.stCnt = head.st.count;
        file_st = (HsfBuffer *)(PTRCAST fileptr+head.st.ofs);
        data = (void *)&file_st[head.st.count];
        for(i=0; i<head.st.count; i++, new_st++, file_st++) {
            temp_data = file_st->data;
            new_st->count = file_st->count;
            new_st->name = SetName((u32 *)&file_st->name);
            new_st->data = (void *)(PTRCAST data+PTRCAST temp_data);
            for(j=0; j<new_st->count; j++) {
                data_elem = (HsfVector2f *)((PTRCAST data)+(PTRCAST temp_data)+(j*sizeof(HsfVector2f)));
                ((HsfVector2f *)new_st->data)[j].x = data_elem->x;
                ((HsfVector2f *)new_st->data)[j].y = data_elem->y;
            }
        }
    }
}

static void FaceLoad(void)
{
    HsfBuffer *file_face;
    HsfBuffer *new_face;
    HsfBuffer *temp_face;
    HsfFace *temp_data;
    HsfFace *data;
    HsfFace *file_face_strip;
    HsfFace *new_face_strip;
    u8 *strip;
    s32 i;
    s32 j;
    
    if(head.face.count) {
        temp_face = file_face = (HsfBuffer *)(PTRCAST fileptr+head.face.ofs);
        data = (HsfFace *)&file_face[head.face.count];
        new_face = temp_face;
        Model.face = new_face;
        Model.faceCnt = head.face.count;
        file_face = (HsfBuffer *)(PTRCAST fileptr+head.face.ofs);
        data = (HsfFace *)&file_face[head.face.count];
        for(i=0; i<head.face.count; i++, new_face++, file_face++) {
            temp_data = file_face->data;
            new_face->name = SetName((u32 *)&file_face->name);
            new_face->count = file_face->count;
            new_face->data = (void *)(PTRCAST data+PTRCAST temp_data);
            strip = (u8 *)(&((HsfFace *)new_face->data)[new_face->count]);
        }
        new_face = temp_face;
        for(i=0; i<head.face.count; i++, new_face++) {
            file_face_strip = new_face_strip = new_face->data;
            for(j=0; j<new_face->count; j++, new_face_strip++, file_face_strip++) {
                if(AS_U16(file_face_strip->type) == 4) {
                    new_face_strip->strip.data = (s16 *)(strip+PTRCAST file_face_strip->strip.data*(sizeof(s16)*4));
                }
            }
        }
    }
}

static void DispObject(HsfObject *parent, HsfObject *object)
{
    u32 i;
    HsfObject *child_obj;
    HsfObject *temp_object;
    struct {
        HsfObject *parent;
        HsfBuffer *shape;
        HsfCluster *cluster;
    } temp;
    
    temp.parent = parent;
    object->type = object->type;
    switch(object->type) {
        case HSF_OBJ_MESH:
        {
            HsfObjectData *data;
            HsfObject *new_object;
            
            data = &object->data;
            new_object = temp_object = object;
            new_object->data.childrenCount = data->childrenCount;
            new_object->data.children = (HsfObject **)&NSymIndex[PTRCAST data->children];
            for(i=0; i<new_object->data.childrenCount; i++) {
                child_obj = &objtop[PTRCAST new_object->data.children[i]];
                new_object->data.children[i] = child_obj;
            }
            new_object->data.parent = parent;
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            new_object->type = HSF_OBJ_MESH;
            new_object->data.vertex = SearchVertexPtr((s32)data->vertex);
            new_object->data.normal = SearchNormalPtr((s32)data->normal);
            new_object->data.st = SearchStPtr((s32)data->st);
            new_object->data.color = SearchColorPtr((s32)data->color);
            new_object->data.face = SearchFacePtr((s32)data->face);
            new_object->data.vertexShape = (HsfBuffer **)&NSymIndex[PTRCAST data->vertexShape];
            for(i=0; i<new_object->data.vertexShapeCnt; i++) {
                temp.shape = &vtxtop[PTRCAST new_object->data.vertexShape[i]];
                new_object->data.vertexShape[i] = temp.shape;
            }
            new_object->data.cluster = (HsfCluster **)&NSymIndex[PTRCAST data->cluster];
            for(i=0; i<new_object->data.clusterCnt; i++) {
                temp.cluster = &ClusterTop[PTRCAST new_object->data.cluster[i]];
                new_object->data.cluster[i] = temp.cluster;
            }
            new_object->data.cenv = SearchCenvPtr((s32)data->cenv);
            new_object->data.material = Model.material;
            if((s32)data->attribute >= 0) {
                new_object->data.attribute = Model.attribute;
            } else {
                new_object->data.attribute = NULL;
            }
            new_object->data.file[0] = (void *)(PTRCAST fileptr+PTRCAST data->file[0]);
            new_object->data.file[1] = (void *)(PTRCAST fileptr+PTRCAST data->file[1]);
            new_object->data.base.pos.x = data->base.pos.x;
            new_object->data.base.pos.y = data->base.pos.y;
            new_object->data.base.pos.z = data->base.pos.z;
            new_object->data.base.rot.x = data->base.rot.x;
            new_object->data.base.rot.y = data->base.rot.y;
            new_object->data.base.rot.z = data->base.rot.z;
            new_object->data.base.scale.x = data->base.scale.x;
            new_object->data.base.scale.y = data->base.scale.y;
            new_object->data.base.scale.z = data->base.scale.z;
            new_object->data.mesh.min.x = data->mesh.min.x;
            new_object->data.mesh.min.y = data->mesh.min.y;
            new_object->data.mesh.min.z = data->mesh.min.z;
            new_object->data.mesh.max.x = data->mesh.max.x;
            new_object->data.mesh.max.y = data->mesh.max.y;
            new_object->data.mesh.max.z = data->mesh.max.z;
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->data.children[i]);
            }
        }
        break;
            
        case HSF_OBJ_NULL1:
        {
            HsfObjectData *data;
            HsfObject *new_object;
            data = &object->data;
            new_object = temp_object = object;
            new_object->data.parent = parent;
            new_object->data.childrenCount = data->childrenCount;
            new_object->data.children = (HsfObject **)&NSymIndex[PTRCAST data->children];
            for(i=0; i<new_object->data.childrenCount; i++) {
                child_obj = &objtop[PTRCAST new_object->data.children[i]];
                new_object->data.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->data.children[i]);
            }
        }
        break;
        
        case HSF_OBJ_REPLICA:
        {
            HsfObjectData *data;
            HsfObject *new_object;
            data = &object->data;
            new_object = temp_object = object;
            new_object->data.parent = parent;
            new_object->data.childrenCount = data->childrenCount;
            new_object->data.children = (HsfObject **)&NSymIndex[PTRCAST data->children];
            for(i=0; i<new_object->data.childrenCount; i++) {
                child_obj = &objtop[PTRCAST new_object->data.children[i]];
                new_object->data.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            new_object->data.replica = &objtop[PTRCAST new_object->data.replica];
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->data.children[i]);
            }
        }
        break;

        case HSF_OBJ_ROOT:
        {
            HsfObjectData *data;
            HsfObject *new_object;
            data = &object->data;
            new_object = temp_object = object;
            new_object->data.parent = parent;
            new_object->data.childrenCount = data->childrenCount;
            new_object->data.children = (HsfObject **)&NSymIndex[PTRCAST data->children];
            for(i=0; i<new_object->data.childrenCount; i++) {
                child_obj = &objtop[PTRCAST new_object->data.children[i]];
                new_object->data.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->data.children[i]);
            }
        }
        break;
        
        case HSF_OBJ_JOINT:
        {
            HsfObjectData *data;
            HsfObject *new_object;
            data = &object->data;
            new_object = temp_object = object;
            new_object->data.parent = parent;
            new_object->data.childrenCount = data->childrenCount;
            new_object->data.children = (HsfObject **)&NSymIndex[PTRCAST data->children];
            for(i=0; i<new_object->data.childrenCount; i++) {
                child_obj = &objtop[PTRCAST new_object->data.children[i]];
                new_object->data.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->data.children[i]);
            }
        }
        break;
        
        case HSF_OBJ_NULL2:
        {
            HsfObjectData *data;
            HsfObject *new_object;
            data = &object->data;
            new_object = temp_object = object;
            new_object->data.parent = parent;
            new_object->data.childrenCount = data->childrenCount;
            new_object->data.children = (HsfObject **)&NSymIndex[PTRCAST data->children];
            for(i=0; i<new_object->data.childrenCount; i++) {
                child_obj = &objtop[PTRCAST new_object->data.children[i]];
                new_object->data.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->data.children[i]);
            }
        }
        break;
        
        case HSF_OBJ_MAP:
        {
            HsfObjectData *data;
            HsfObject *new_object;
            data = &object->data;
            new_object = temp_object = object;
            new_object->data.parent = parent;
            new_object->data.childrenCount = data->childrenCount;
            new_object->data.children = (HsfObject **)&NSymIndex[PTRCAST data->children];
            for(i=0; i<new_object->data.childrenCount; i++) {
                child_obj = &objtop[PTRCAST new_object->data.children[i]];
                new_object->data.children[i] = child_obj;
            }
            if(Model.root == NULL) {
                Model.root = temp_object;
            }
            for(i=0; i<data->childrenCount; i++) {
                DispObject(new_object, new_object->data.children[i]);
            }
        }
        break;
        
        default:
            break;
    }
}

static inline void FixupObject(HsfObject *object)
{
    HsfObjectData *objdata_8;
    HsfObjectData *objdata_7;
    
    s32 obj_type = object->type;
    switch(obj_type) {
        case 8:
        {
            objdata_8 = &object->data;
            object->type = HSF_OBJ_NONE2;
        }
        break;
        
        case 7:
        {
            objdata_7 = &object->data;
            object->type = HSF_OBJ_NONE1;
        }
        break;
        
        default:
            break;
            
    }
}

static void ObjectLoad(void)
{
    s32 i;
    HsfObject *object;
    HsfObject *new_object;
    s32 obj_type;

    if(head.object.count) {
        objtop = object = (HsfObject *)(PTRCAST fileptr+head.object.ofs);
        for(i=0; i<head.object.count; i++, object++) {
            new_object = object;
            new_object->name = SetName((u32 *)&object->name);
        }
        object = objtop;
        for(i=0; i<head.object.count; i++, object++) {
            if((s32)object->data.parent == -1) {
                break;
            }
        }
        DispObject(NULL, object);
        Model.objectCnt = head.object.count;
        object = objtop;
        for(i=0; i<head.object.count; i++, object++) {
            FixupObject(object);
        }
    }
}

static void CenvLoad(void)
{
    HsfCenvMulti *multi_file;
    HsfCenvMulti *multi_new;
    HsfCenvSingle *single_new;
    HsfCenvSingle *single_file;
    HsfCenvDual *dual_file;
    HsfCenvDual *dual_new;

    HsfCenv *cenv_new;
    HsfCenv *cenv_file;
    void *data_base;
    void *weight_base;
    
    s32 j;
    s32 i;
    
    if(head.cenv.count) {
        cenv_file = (HsfCenv *)(PTRCAST fileptr+head.cenv.ofs);
        data_base = &cenv_file[head.cenv.count];
        weight_base = data_base;
        cenv_new = cenv_file;
        Model.cenvCnt = head.cenv.count;
        Model.cenv = cenv_file;
        for(i=0; i<head.cenv.count; i++) {
            cenv_new[i].singleData = (HsfCenvSingle *)(PTRCAST cenv_file[i].singleData+PTRCAST data_base);
            cenv_new[i].dualData = (HsfCenvDual *)(PTRCAST cenv_file[i].dualData+PTRCAST data_base);
            cenv_new[i].multiData = (HsfCenvMulti *)(PTRCAST cenv_file[i].multiData+PTRCAST data_base);
            cenv_new[i].singleCount = cenv_file[i].singleCount;
            cenv_new[i].dualCount = cenv_file[i].dualCount;
            cenv_new[i].multiCount = cenv_file[i].multiCount;
            cenv_new[i].copyCount = cenv_file[i].copyCount;
            cenv_new[i].vtxCount = cenv_file[i].vtxCount;
            weight_base = (void *)(PTRCAST weight_base+(cenv_new[i].singleCount*sizeof(HsfCenvSingle)));
            weight_base = (void *)(PTRCAST weight_base+(cenv_new[i].dualCount*sizeof(HsfCenvDual)));
            weight_base = (void *)(PTRCAST weight_base+(cenv_new[i].multiCount*sizeof(HsfCenvMulti)));
        }
        for(i=0; i<head.cenv.count; i++) {
            single_new = single_file = cenv_new[i].singleData;
            for(j=0; j<cenv_new[i].singleCount; j++) {
                single_new[j].target = single_file[j].target;
                single_new[j].posCnt = single_file[j].posCnt;
                single_new[j].pos = single_file[j].pos;
                single_new[j].normalCnt = single_file[j].normalCnt;
                single_new[j].normal = single_file[j].normal;
                
            }
            dual_new = dual_file = cenv_new[i].dualData;
            for(j=0; j<cenv_new[i].dualCount; j++) {
                dual_new[j].target1 = dual_file[j].target1;
                dual_new[j].target2 = dual_file[j].target2;
                dual_new[j].weightCnt = dual_file[j].weightCnt;
                dual_new[j].weight = (HsfCenvDualWeight *)(PTRCAST weight_base+PTRCAST dual_file[j].weight);
            }
            multi_new = multi_file = cenv_new[i].multiData;
            for(j=0; j<cenv_new[i].multiCount; j++) {
                multi_new[j].weightCnt = multi_file[j].weightCnt;
                multi_new[j].pos = multi_file[j].pos;
                multi_new[j].posCnt = multi_file[j].posCnt;
                multi_new[j].normal = multi_file[j].normal;
                multi_new[j].normalCnt = multi_file[j].normalCnt;
                multi_new[j].weight = (HsfCenvMultiWeight *)(PTRCAST weight_base+PTRCAST multi_file[j].weight);
            }
            dual_new = dual_file = cenv_new[i].dualData;
            for(j=0; j<cenv_new[i].dualCount; j++) {
                HsfCenvDualWeight *discard = dual_new[j].weight;
            }
            multi_new = multi_file = cenv_new[i].multiData;
            for(j=0; j<cenv_new[i].multiCount; j++) {
                HsfCenvMultiWeight *weight = multi_new[j].weight;
                s32 k;
                for(k=0; k<multi_new[j].weightCnt; k++, weight++);
            }
        }
    }
}

static void SkeletonLoad(void)
{
    HsfSkeleton *skeleton_file;
    HsfSkeleton *skeleton_new;
    s32 i;
    
    if(head.skeleton.count) {
        skeleton_new = skeleton_file = (HsfSkeleton *)(PTRCAST fileptr+head.skeleton.ofs);
        Model.skeletonCnt = head.skeleton.count;
        Model.skeleton = skeleton_file;
        for(i=0; i<head.skeleton.count; i++) {
            skeleton_new[i].name = SetName((u32 *)&skeleton_file[i].name);
            skeleton_new[i].transform.pos.x = skeleton_file[i].transform.pos.x;
            skeleton_new[i].transform.pos.y = skeleton_file[i].transform.pos.y;
            skeleton_new[i].transform.pos.z = skeleton_file[i].transform.pos.z;
            skeleton_new[i].transform.rot.x = skeleton_file[i].transform.rot.x;
            skeleton_new[i].transform.rot.y = skeleton_file[i].transform.rot.y;
            skeleton_new[i].transform.rot.z = skeleton_file[i].transform.rot.z;
            skeleton_new[i].transform.scale.x = skeleton_file[i].transform.scale.x;
            skeleton_new[i].transform.scale.y = skeleton_file[i].transform.scale.y;
            skeleton_new[i].transform.scale.z = skeleton_file[i].transform.scale.z;
        }
    }
}

static void PartLoad(void)
{
    HsfPart *part_file;
    HsfPart *part_new;
    
    u16 *data;
    s32 i, j;
    
    if(head.part.count) {
        part_new = part_file = (HsfPart *)(PTRCAST fileptr+head.part.ofs);
        Model.partCnt = head.part.count;
        Model.part = part_file;
        data = (u16 *)&part_file[head.part.count];
        for(i=0; i<head.part.count; i++, part_new++) {
            part_new->name = SetName((u32 *)&part_file[i].name);
            part_new->count = part_file[i].count;
            part_new->vertex = &data[PTRCAST part_file[i].vertex];
            for(j=0; j<part_new->count; j++) {
                part_new->vertex[j] = part_new->vertex[j];
            }
        }
    }
}

static void ClusterLoad(void)
{
    HsfCluster *cluster_file;
    HsfCluster *cluster_new;
    
    s32 i, j;
    
    if(head.cluster.count) {
        cluster_new = cluster_file = (HsfCluster *)(PTRCAST fileptr+head.cluster.ofs);
        Model.clusterCnt = head.cluster.count;
        Model.cluster = cluster_file;
        for(i=0; i<head.cluster.count; i++) {
            HsfBuffer *vertex;
            u32 vertexSym;
            cluster_new[i].name[0] = SetName((u32 *)&cluster_file[i].name[0]);
            cluster_new[i].name[1] = SetName((u32 *)&cluster_file[i].name[1]);
            cluster_new[i].targetName = SetName((u32 *)&cluster_file[i].targetName);
            cluster_new[i].part = SearchPartPtr((s32)cluster_file[i].part);
            cluster_new[i].unk95 = cluster_file[i].unk95;
            cluster_new[i].type = cluster_file[i].type;
            cluster_new[i].vertexCnt = cluster_file[i].vertexCnt;
            vertexSym = PTRCAST cluster_file[i].vertex;
            cluster_new[i].vertex = (HsfBuffer **)&NSymIndex[vertexSym];
            for(j=0; j<cluster_new[i].vertexCnt; j++) {
                vertex = SearchVertexPtr((s32)cluster_new[i].vertex[j]);
                cluster_new[i].vertex[j] = vertex;
            }
        }
    }
}

static void ShapeLoad(void)
{
    s32 i, j;
    HsfShape *shape_new;
    HsfShape *shape_file;

    if(head.shape.count) {
        shape_new = shape_file = (HsfShape *)(PTRCAST fileptr+head.shape.ofs);
        Model.shapeCnt = head.shape.count;
        Model.shape = shape_file;
        for(i=0; i<Model.shapeCnt; i++) {
            u32 vertexSym;
            HsfBuffer *vertex;

            shape_new[i].name = SetName((u32 *)&shape_file[i].name);
            shape_new[i].count16[0] = shape_file[i].count16[0];
            shape_new[i].count16[1] = shape_file[i].count16[1];
            vertexSym = PTRCAST shape_file[i].vertex;
            shape_new[i].vertex = (HsfBuffer **)&NSymIndex[vertexSym];
            for(j=0; j<shape_new[i].count16[1]; j++) {
                vertex = &vtxtop[PTRCAST shape_new[i].vertex[j]];
                shape_new[i].vertex[j] = vertex;
            }
        }
    }
}

static void MapAttrLoad(void)
{
    s32 i;
    HsfMapAttr *mapattr_base;
    HsfMapAttr *mapattr_file;
    HsfMapAttr *mapattr_new;
    u16 *data;
    
    if(head.mapAttr.count) {
        mapattr_file = mapattr_base = (HsfMapAttr *)(PTRCAST fileptr+head.mapAttr.ofs);
        mapattr_new = mapattr_base;
        Model.mapAttrCnt = head.mapAttr.count;
        Model.mapAttr = mapattr_base;
        data = (u16 *)&mapattr_base[head.mapAttr.count];
        for(i=0; i<head.mapAttr.count; i++, mapattr_file++, mapattr_new++) {
            mapattr_new->data = &data[PTRCAST mapattr_file->data];
        }
    }
}

static void BitmapLoad(void)
{
    HsfBitmap *bitmap_file;
    HsfBitmap *bitmap_temp;
    HsfBitmap *bitmap_new;
    HsfPalette *palette;
    void *data;
    s32 i;
    
    if(head.bitmap.count) {
        bitmap_temp = bitmap_file = (HsfBitmap *)(PTRCAST fileptr+head.bitmap.ofs);
        data = &bitmap_file[head.bitmap.count];
        for(i=0; i<head.bitmap.count; i++, bitmap_file++);
        bitmap_new = bitmap_temp;
        Model.bitmap = bitmap_file;
        Model.bitmapCnt = head.bitmap.count;
        bitmap_file = (HsfBitmap *)(PTRCAST fileptr+head.bitmap.ofs);
        data = &bitmap_file[head.bitmap.count];
        for(i=0; i<head.bitmap.count; i++, bitmap_file++, bitmap_new++) {
            bitmap_new->name = SetName((u32 *)&bitmap_file->name);
            bitmap_new->dataFmt = bitmap_file->dataFmt;
            bitmap_new->pixSize = bitmap_file->pixSize;
            bitmap_new->sizeX = bitmap_file->sizeX;
            bitmap_new->sizeY = bitmap_file->sizeY;
            bitmap_new->palSize = bitmap_file->palSize;
            palette = SearchPalettePtr(PTRCAST bitmap_file->palData);
            if(palette) {
                bitmap_new->palData = palette->data;
            }
            bitmap_new->data = (void *)(PTRCAST data+PTRCAST bitmap_file->data);
        }
    }
}

static void PaletteLoad(void)
{
    s32 i;
    s32 j;
    HsfPalette *palette_file;
    HsfPalette *palette_temp;
    HsfPalette *palette_new;
    
    void *data_base;
    u16 *temp_data;
    u16 *data;
    
    if(head.palette.count) {
        palette_temp = palette_file = (HsfPalette *)(PTRCAST fileptr+head.palette.ofs);
        data_base = (u16 *)&palette_file[head.palette.count];
        for(i=0; i<head.palette.count; i++, palette_file++) {
            temp_data = (u16 *)(PTRCAST data_base+PTRCAST palette_file->data);
        }
        Model.palette = palette_temp;
        Model.paletteCnt = head.palette.count;
        palette_new = palette_temp;
        palette_file = (HsfPalette *)(PTRCAST fileptr+head.palette.ofs);
        data_base = (u16 *)&palette_file[head.palette.count];
        for(i=0; i<head.palette.count; i++, palette_file++, palette_new++) {
            temp_data = (u16 *)(PTRCAST data_base+PTRCAST palette_file->data);
            data = temp_data;
            palette_new->name = SetName((u32 *)&palette_file->name);
            palette_new->data = data;
            palette_new->palSize = palette_file->palSize;
            for(j=0; j<palette_file->palSize; j++) {
                data[j] = data[j];
            }
        }
    }
}

char *MakeObjectName(char *name)
{
    static char buf[768];
    s32 index, num_minus;
    char *temp_name;
    num_minus = 0;
    index = 0;
    temp_name = name;
    while(*temp_name) {
        if(*temp_name == '-') {
            name = temp_name+1;
            break;
        }
        temp_name++;
    }
    while(*name) {
        if(num_minus != 0) {
            break;
        }
        if(*name == '_' && !isalpha(name[1])) {
            num_minus++;
            break;
        }
        buf[index] = *name;
        name++;
        index++;
    }
    buf[index] = '\0';
    return buf;
}

s32 CmpObjectName(char *name1, char *name2)
{
    s32 temp = 0;
    return strcmp(name1, name2);
}

static inline char *MotionGetName(HsfTrack *track)
{
    char *ret;
    if(DicStringTable) {
        ret = &DicStringTable[track->target];
    } else {
        ret = GetMotionString(&track->target);
    }
    return ret;
}

static inline s32 FindObjectName(char *name)
{
    s32 i;
    HsfObject *object;
    
    object = objtop;
    for(i=0; i<head.object.count; i++, object++) {
        if(!CmpObjectName(object->name, name)) {
            return i;
        }
    }
    return -1;
}

static inline s32 FindClusterName(char *name)
{
    s32 i;
    HsfCluster *cluster;
    
    cluster = ClusterTop;
    for(i=0; i<head.cluster.count; i++, cluster++) {
        if(!strcmp(cluster->name[0], name)) {
            return i;
        }
    }
    return -1;
}

static inline s32 FindMotionClusterName(char *name)
{
    s32 i;
    HsfCluster *cluster;
    
    cluster = MotionModel->cluster;
    for(i=0; i<MotionModel->clusterCnt; i++, cluster++) {
        if(!strcmp(cluster->name[0], name)) {
            return i;
        }
    }
    return -1;
}

static inline s32 FindAttributeName(char *name)
{
    s32 i;
    HsfAttribute *attribute;
    
    attribute = AttributeTop;
    for(i=0; i<head.attribute.count; i++, attribute++) {
        if(!attribute->name) {
            continue;
        }
        if(!strcmp(attribute->name, name)) {
            return i;
        }
    }
    return -1;
}

static inline s32 FindMotionAttributeName(char *name)
{
    s32 i;
    HsfAttribute *attribute;
    
    attribute = MotionModel->attribute;
    for(i=0; i<MotionModel->attributeCnt; i++, attribute++) {
        if(!attribute->name) {
            continue;
        }
        if(!strcmp(attribute->name, name)) {
            return i;
        }
    }
    return -1;
}

static inline void MotionLoadTransform(HsfTrack *track, void *data)
{
    float *step_data;
    float *linear_data;
    float *bezier_data;
    HsfTrack *out_track;
    char *name;
    s32 numKeyframes;
    out_track = track;
    name = MotionGetName(track);
    if(objtop) {
        out_track->target = FindObjectName(name);
    }
    numKeyframes = AS_S16(track->numKeyframes);
    switch(track->curveType) {
        case HSF_CURVE_STEP:
        {
            step_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = step_data;
        }
        break;
        
        case HSF_CURVE_LINEAR:
        {
            linear_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = linear_data;
        }
        break;
        
        case HSF_CURVE_BEZIER:
        {
            bezier_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = bezier_data;
        }
        break;
        
        case HSF_CURVE_CONST:
            break;
    }
}

static inline void MotionLoadCluster(HsfTrack *track, void *data)
{
    s32 numKeyframes;
    float *step_data;
    float *linear_data;
    float *bezier_data;
    HsfTrack *out_track;
    char *name;
    
    out_track = track;
    name = SetMotionName(&track->target);
    if(!MotionOnly) {
        AS_S16(out_track->target) = FindClusterName(name);
    } else {
        AS_S16(out_track->target) = FindMotionClusterName(name);
    }
    numKeyframes = AS_S16(track->numKeyframes);
    (void)out_track;
    switch(track->curveType) {
        case HSF_CURVE_STEP:
        {
            step_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = step_data;
        }
        break;
        
        case HSF_CURVE_LINEAR:
        {
            linear_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = linear_data;
        }
        break;
        
        case HSF_CURVE_BEZIER:
        {
            bezier_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = bezier_data;
        }
        break;
        
        case HSF_CURVE_CONST:
            break;
    }
}

static inline void MotionLoadClusterWeight(HsfTrack *track, void *data)
{
    s32 numKeyframes;
    float *step_data;
    float *linear_data;
    float *bezier_data;
    HsfTrack *out_track;
    char *name;
    
    out_track = track;
    name = SetMotionName(&track->target);
    if(!MotionOnly) {
        AS_S16(out_track->target) = FindClusterName(name);
    } else {
        AS_S16(out_track->target) = FindMotionClusterName(name);
    }
    numKeyframes = AS_S16(track->numKeyframes);
    (void)out_track;
    switch(track->curveType) {
        case HSF_CURVE_STEP:
        {
            step_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = step_data;
        }
        break;
        
        case HSF_CURVE_LINEAR:
        {
            linear_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = linear_data;
        }
        break;
        
        case HSF_CURVE_BEZIER:
        {
            bezier_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = bezier_data;
        }
        break;
        
        case HSF_CURVE_CONST:
            break;
    }
}

static inline void MotionLoadMaterial(HsfTrack *track, void *data)
{
    float *step_data;
    float *linear_data;
    float *bezier_data;
    s32 numKeyframes;
    HsfTrack *out_track;
    out_track = track;
    numKeyframes = AS_S16(track->numKeyframes);
    switch(track->curveType) {
        case HSF_CURVE_STEP:
        {
            step_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = step_data;
        }
        break;
        
        case HSF_CURVE_LINEAR:
        {
            linear_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = linear_data;
        }
        break;
        
        case HSF_CURVE_BEZIER:
        {
            bezier_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = bezier_data;
        }
        break;
        
        case HSF_CURVE_CONST:
            break;
    }
}

static inline void MotionLoadAttribute(HsfTrack *track, void *data)
{
    HsfBitmapKey *file_frame;
    HsfBitmapKey *new_frame;
    s32 i;
    float *step_data;
    float *linear_data;
    float *bezier_data;
    HsfTrack *out_track;
    char *name;
    out_track = track;
    if(AS_S16(out_track->target) != -1) {
        name = SetMotionName(&track->target);
        if(!MotionOnly) {
            AS_S16(out_track->param) = FindAttributeName(name);
        } else {
            AS_S16(out_track->param) = FindMotionAttributeName(name);
        }
    }
    
    switch(track->curveType) {
        case HSF_CURVE_STEP:
        {
            step_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = step_data;
        }
        break;
        
        case HSF_CURVE_LINEAR:
        {
            linear_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = linear_data;
        }
        break;
        
        case HSF_CURVE_BEZIER:
        {
            bezier_data = (float *)(PTRCAST data+PTRCAST track->data);
            out_track->data = bezier_data;
        }
        break;
        
        case HSF_CURVE_BITMAP:
        {
            new_frame = file_frame = (HsfBitmapKey *)(PTRCAST data+PTRCAST track->data);
            out_track->data = file_frame;
            for(i=0; i<out_track->numKeyframes; i++, file_frame++, new_frame++) {
                new_frame->data = SearchBitmapPtr((s32)file_frame->data);
            }
        }
        break;
        case HSF_CURVE_CONST:
            break;
    }
}

static void MotionLoad(void)
{
    HsfMotion *file_motion;
    HsfMotion *temp_motion;
    HsfMotion *new_motion;
    HsfTrack *track_base;
    void *track_data;
    s32 i;
    
    MotionOnly = FALSE;
    MotionModel = NULL;
    if(head.motion.count) {
        temp_motion = file_motion = (HsfMotion *)(PTRCAST fileptr+head.motion.ofs);
        new_motion = temp_motion;
        Model.motion = new_motion;
        Model.motionCnt = file_motion->numTracks;
        track_base = (HsfTrack *)&file_motion[head.motion.count];
        track_data = &track_base[file_motion->numTracks];
        new_motion->track = track_base;
        for(i=0; i<(s32)file_motion->numTracks; i++) {
            switch(track_base[i].type) {
                case HSF_TRACK_TRANSFORM:
                case HSF_TRACK_MORPH:
                    MotionLoadTransform(&track_base[i], track_data);
                    break;
                    
                case HSF_TRACK_CLUSTER:
                    MotionLoadCluster(&track_base[i], track_data);
                    break;
                    
                case HSF_TRACK_CLUSTER_WEIGHT:
                    MotionLoadClusterWeight(&track_base[i], track_data);
                    break;
                    
                case HSF_TRACK_MATERIAL:
                    MotionLoadMaterial(&track_base[i], track_data);
                    break;
                    
                case HSF_TRACK_ATTRIBUTE:
                    MotionLoadAttribute(&track_base[i], track_data);
                    break;
                    
                default:
                    break;
            }
        }
    }
    //HACK: Bump register of i to r31
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
    (void)i;
}

static void MatrixLoad(void)
{
    HsfMatrix *matrix_file;
    
    if(head.matrix.count) {
        matrix_file = (HsfMatrix *)(PTRCAST fileptr+head.matrix.ofs);
        matrix_file->data = (Mtx *)(PTRCAST fileptr+head.matrix.ofs+sizeof(HsfMatrix));
        Model.matrix = matrix_file;
        Model.matrixCnt = head.matrix.count;
    }
}

static s32 SearchObjectSetName(HsfData *data, char *name)
{
    HsfObject *object = data->object;
    s32 i;
    for(i=0; i<data->objectCnt; i++, object++) {
        if(!CmpObjectName(object->name, name)) {
            return i;
        }
    }
    OSReport("Search Object Error %s\n", name);
    return -1;
}

static HsfBuffer *SearchVertexPtr(s32 id)
{
    HsfBuffer *vertex; 
    if(id == -1) {
        return NULL;
    }
    vertex = (HsfBuffer *)(PTRCAST fileptr+head.vertex.ofs);
    vertex += id;
    return vertex;
}

static HsfBuffer *SearchNormalPtr(s32 id)
{
    HsfBuffer *normal; 
    if(id == -1) {
        return NULL;
    }
    normal = (HsfBuffer *)(PTRCAST fileptr+head.normal.ofs);
    normal += id;
    return normal;
}

static HsfBuffer *SearchStPtr(s32 id)
{
    HsfBuffer *st; 
    if(id == -1) {
        return NULL;
    }
    st = (HsfBuffer *)(PTRCAST fileptr+head.st.ofs);
    st += id;
    return st;
}

static HsfBuffer *SearchColorPtr(s32 id)
{
    HsfBuffer *color; 
    if(id == -1) {
        return NULL;
    }
    color = (HsfBuffer *)(PTRCAST fileptr+head.color.ofs);
    color += id;
    return color;
}

static HsfBuffer *SearchFacePtr(s32 id)
{
    HsfBuffer *face; 
    if(id == -1) {
        return NULL;
    }
    face = (HsfBuffer *)(PTRCAST fileptr+head.face.ofs);
    face += id;
    return face;
}

static HsfCenv *SearchCenvPtr(s32 id)
{
    HsfCenv *cenv; 
    if(id == -1) {
        return NULL;
    }
    cenv = (HsfCenv *)(PTRCAST fileptr+head.cenv.ofs);
    cenv += id;
    return cenv;
}

static HsfPart *SearchPartPtr(s32 id)
{
    HsfPart *part; 
    if(id == -1) {
        return NULL;
    }
    part = (HsfPart *)(PTRCAST fileptr+head.part.ofs);
    part += id;
    return part;
}

static HsfPalette *SearchPalettePtr(s32 id)
{
    HsfPalette *palette; 
    if(id == -1) {
        return NULL;
    }
    palette = Model.palette;
    palette += id;
    return palette;
}

static HsfBitmap *SearchBitmapPtr(s32 id)
{
    HsfBitmap *bitmap; 
    if(id == -1) {
        return NULL;
    }
    bitmap = (HsfBitmap *)(PTRCAST fileptr+head.bitmap.ofs);
    bitmap += id;
    return bitmap;
}

static char *GetString(u32 *str_ofs)
{
    char *ret = &StringTable[*str_ofs];
    return ret;
}

static char *GetMotionString(u16 *str_ofs)
{
    char *ret = &StringTable[*str_ofs];
    return ret;
}

#ifdef TARGET_PC
/*
 * PC-specific HSF loader.
 * Parses the big-endian HSF binary at known byte offsets and populates
 * freshly allocated native structs (which have 8-byte pointers on 64-bit).
 * The file buffer must remain alive since bitmap/palette raw data is referenced.
 */
static HsfData *LoadHSF_PC(void *data)
{
    u8 *base = (u8 *)data;
    s32 i, j;

    /* --- Parse header (all s32 fields after 8-byte magic) --- */
    HsfHeader hdr;
    memcpy(hdr.magic, base, 8);
    {
        u8 *raw = base + 8;
        s32 *dst = (s32 *)&hdr.scene;
        int nf = (sizeof(HsfHeader) - 8) / sizeof(s32);
        for (i = 0; i < nf; i++) {
            dst[i] = hsf_bes32(raw + i * 4);
        }
    }

    char *strings = (char *)(base + hdr.string.ofs);

    /* Set global StringTable so runtime functions (SearchObjectIndex, SetName,
     * GetMotionString) can resolve string offsets from motion tracks.
     * On GC, FileLoad sets this; on PC we must do it here. */
    StringTable = strings;

    /* --- Byte-swap symbol table in-place (array of u32 indices) --- */
    u32 *sym = NULL;
    if (hdr.symbol.count > 0) {
        sym = (u32 *)(base + hdr.symbol.ofs);
        hsf_swap32_array(sym, hdr.symbol.count);
    }

    /* --- Allocate result --- */
    HsfData *hsf = (HsfData *)calloc(1, sizeof(HsfData));
    memcpy(hsf->magic, hdr.magic, 8);

    /* ================================================================
     *  SCENE
     * ================================================================ */
    if (hdr.scene.count > 0) {
        u8 *s = base + hdr.scene.ofs;
        HsfScene *scene = (HsfScene *)calloc(1, sizeof(HsfScene));
        scene->fogType = (GXFogType)hsf_be32(s + 0);
        scene->start = hsf_bef(s + 4);
        scene->end = hsf_bef(s + 8);
        scene->color.r = s[12]; scene->color.g = s[13];
        scene->color.b = s[14]; scene->color.a = s[15];
        hsf->scene = scene;
        hsf->sceneCnt = hdr.scene.count;
    }

    /* ================================================================
     *  PALETTES
     * ================================================================ */
    HsfPalette *palettes = NULL;
    if (hdr.palette.count > 0) {
        u8 *sec = base + hdr.palette.ofs;
        u8 *pal_data_base = sec + hdr.palette.count * HSF_F_PAL;
        palettes = (HsfPalette *)calloc(hdr.palette.count, sizeof(HsfPalette));
        for (i = 0; i < hdr.palette.count; i++) {
            u8 *e = sec + i * HSF_F_PAL;
            palettes[i].name = &strings[hsf_be32(e + 0)];
            palettes[i].unk = hsf_bes32(e + 4);
            palettes[i].palSize = hsf_be32(e + 8);
            u32 dofs = hsf_be32(e + 12);
            palettes[i].data = (u16 *)(pal_data_base + dofs);
            /* Palette data stays in big-endian byte order;
             * texture decoders read it as (pal[i*2]<<8|pal[i*2+1]). */
        }
        hsf->palette = palettes;
        hsf->paletteCnt = hdr.palette.count;
    }

    /* ================================================================
     *  BITMAPS
     * ================================================================ */
    if (hdr.bitmap.count > 0) {
        u8 *sec = base + hdr.bitmap.ofs;
        u8 *bmp_data_base = sec + hdr.bitmap.count * HSF_F_BMP;
        HsfBitmap *bmps = (HsfBitmap *)calloc(hdr.bitmap.count, sizeof(HsfBitmap));
        for (i = 0; i < hdr.bitmap.count; i++) {
            u8 *e = sec + i * HSF_F_BMP;
            bmps[i].name     = &strings[hsf_be32(e + 0)];
            bmps[i].maxLod   = hsf_be32(e + 4);
            bmps[i].dataFmt  = e[8];
            bmps[i].pixSize  = e[9];
            bmps[i].sizeX    = hsf_bes16(e + 10);
            bmps[i].sizeY    = hsf_bes16(e + 12);
            bmps[i].palSize  = hsf_bes16(e + 14);
            bmps[i].tint.r = e[16]; bmps[i].tint.g = e[17];
            bmps[i].tint.b = e[18]; bmps[i].tint.a = e[19];
            s32 pal_id       = hsf_bes32(e + 20);
            bmps[i].unk      = hsf_be32(e + 24);
            u32 dofs         = hsf_be32(e + 28);
            bmps[i].data = (void *)(bmp_data_base + dofs);
            /* Resolve palette reference */
            if (pal_id >= 0 && palettes && pal_id < hdr.palette.count) {
                bmps[i].palData = palettes[pal_id].data;
            } else {
                bmps[i].palData = NULL;
            }
        }
        hsf->bitmap = bmps;
        hsf->bitmapCnt = hdr.bitmap.count;
    }

    /* ================================================================
     *  MATERIALS
     * ================================================================ */
    if (hdr.material.count > 0) {
        u8 *sec = base + hdr.material.ofs;
        HsfMaterial *mats = (HsfMaterial *)calloc(hdr.material.count, sizeof(HsfMaterial));
        for (i = 0; i < hdr.material.count; i++) {
            u8 *e = sec + i * HSF_F_MAT;
            mats[i].name = &strings[hsf_be32(e + 0)];
            memcpy(mats[i].unk4, e + 4, 4);
            mats[i].pass = hsf_be16(e + 8);
            mats[i].vtxMode = e[10];
            mats[i].litColor[0] = e[11]; mats[i].litColor[1] = e[12]; mats[i].litColor[2] = e[13];
            mats[i].color[0] = e[14]; mats[i].color[1] = e[15]; mats[i].color[2] = e[16];
            mats[i].shadowColor[0] = e[17]; mats[i].shadowColor[1] = e[18]; mats[i].shadowColor[2] = e[19];
            mats[i].hilite_scale = hsf_bef(e + 20);
            mats[i].unk18 = hsf_bef(e + 24);
            mats[i].invAlpha = hsf_bef(e + 28);
            mats[i].unk20[0] = hsf_bef(e + 32);
            mats[i].unk20[1] = hsf_bef(e + 36);
            mats[i].refAlpha = hsf_bef(e + 40);
            mats[i].unk2C = hsf_bef(e + 44);
            mats[i].flags = hsf_be32(e + 48);
            mats[i].numAttrs = hsf_be32(e + 52);
            /* attrs: array of s32 attribute indices from symbol table */
            u32 attr_sym_ofs = hsf_be32(e + 56);
            if (sym && mats[i].numAttrs > 0) {
                s32 *attr_indices = (s32 *)calloc(mats[i].numAttrs, sizeof(s32));
                for (j = 0; j < (s32)mats[i].numAttrs; j++) {
                    attr_indices[j] = (s32)sym[attr_sym_ofs + j];
                }
                mats[i].attrs = attr_indices;
            }
        }
        hsf->material = mats;
        hsf->materialCnt = hdr.material.count;
    }

    /* ================================================================
     *  ATTRIBUTES
     * ================================================================ */
    if (hdr.attribute.count > 0) {
        u8 *sec = base + hdr.attribute.ofs;
        HsfAttribute *attrs = (HsfAttribute *)calloc(hdr.attribute.count, sizeof(HsfAttribute));
        for (i = 0; i < hdr.attribute.count; i++) {
            u8 *e = sec + i * HSF_F_ATTR;
            u32 name_ofs = hsf_be32(e + 0);
            attrs[i].name = (name_ofs != 0xFFFFFFFF) ? &strings[name_ofs] : NULL;
            attrs[i].unk04 = NULL;
            memcpy(attrs[i].unk8, e + 8, 4);
            attrs[i].unk0C = hsf_bef(e + 12);
            memcpy(attrs[i].unk10, e + 16, 4);
            attrs[i].unk14 = hsf_bef(e + 20);
            memcpy(attrs[i].unk18, e + 24, 8);
            attrs[i].unk20 = hsf_bef(e + 32);
            memcpy(attrs[i].unk24, e + 36, 4);
            attrs[i].unk28 = hsf_bef(e + 40);
            attrs[i].unk2C = hsf_bef(e + 44);
            attrs[i].unk30 = hsf_bef(e + 48);
            attrs[i].unk34 = hsf_bef(e + 52);
            memcpy(attrs[i].unk38, e + 56, 44);
            attrs[i].wrap_s = hsf_be32(e + 100);
            attrs[i].wrap_t = hsf_be32(e + 104);
            memcpy(attrs[i].unk6C, e + 108, 12);
            attrs[i].unk78 = hsf_be32(e + 120);
            attrs[i].flag = hsf_be32(e + 124);
            /* Bitmap reference */
            s32 bmp_id = hsf_bes32(e + 128);
            if (bmp_id >= 0 && bmp_id < hdr.bitmap.count && hsf->bitmap) {
                attrs[i].bitmap = &hsf->bitmap[bmp_id];
            } else {
                attrs[i].bitmap = NULL;
            }
        }
        hsf->attribute = attrs;
        hsf->attributeCnt = hdr.attribute.count;
    }

    /* ================================================================
     *  VERTEX BUFFERS
     * ================================================================ */
    if (hdr.vertex.count > 0) {
        u8 *sec = base + hdr.vertex.ofs;
        u8 *data_base = sec + hdr.vertex.count * HSF_F_BUF;
        HsfBuffer *bufs = (HsfBuffer *)calloc(hdr.vertex.count, sizeof(HsfBuffer));
        for (i = 0; i < hdr.vertex.count; i++) {
            u8 *e = sec + i * HSF_F_BUF;
            bufs[i].name = &strings[hsf_be32(e + 0)];
            bufs[i].count = hsf_bes32(e + 4);
            u32 dofs = hsf_be32(e + 8);
            int cnt = bufs[i].count;
            if (cnt > 0) {
                HsfVector3f *vdata = (HsfVector3f *)malloc(cnt * sizeof(HsfVector3f));
                u8 *src = data_base + dofs;
                for (j = 0; j < cnt; j++) {
                    vdata[j].x = hsf_bef(src + j * 12 + 0);
                    vdata[j].y = hsf_bef(src + j * 12 + 4);
                    vdata[j].z = hsf_bef(src + j * 12 + 8);
                }
                bufs[i].data = vdata;
            }
        }
        hsf->vertex = bufs;
        hsf->vertexCnt = hdr.vertex.count;
    }

    /* ================================================================
     *  NORMAL BUFFERS
     * ================================================================ */
    if (hdr.normal.count > 0) {
        u8 *sec = base + hdr.normal.ofs;
        u8 *data_base = sec + hdr.normal.count * HSF_F_BUF;
        HsfBuffer *bufs = (HsfBuffer *)calloc(hdr.normal.count, sizeof(HsfBuffer));
        for (i = 0; i < hdr.normal.count; i++) {
            u8 *e = sec + i * HSF_F_BUF;
            bufs[i].name = &strings[hsf_be32(e + 0)];
            bufs[i].count = hsf_bes32(e + 4);
            u32 dofs = hsf_be32(e + 8);
            int cnt = bufs[i].count;
            if (cnt > 0) {
                HsfVector3f *ndata = (HsfVector3f *)malloc(cnt * sizeof(HsfVector3f));
                u8 *src = data_base + dofs;
                for (j = 0; j < cnt; j++) {
                    ndata[j].x = hsf_bef(src + j * 12 + 0);
                    ndata[j].y = hsf_bef(src + j * 12 + 4);
                    ndata[j].z = hsf_bef(src + j * 12 + 8);
                }
                bufs[i].data = ndata;
            }
        }
        hsf->normal = bufs;
        hsf->normalCnt = hdr.normal.count;
    }

    /* ================================================================
     *  ST (TEXCOORD) BUFFERS
     * ================================================================ */
    if (hdr.st.count > 0) {
        u8 *sec = base + hdr.st.ofs;
        u8 *data_base = sec + hdr.st.count * HSF_F_BUF;
        HsfBuffer *bufs = (HsfBuffer *)calloc(hdr.st.count, sizeof(HsfBuffer));
        for (i = 0; i < hdr.st.count; i++) {
            u8 *e = sec + i * HSF_F_BUF;
            bufs[i].name = &strings[hsf_be32(e + 0)];
            bufs[i].count = hsf_bes32(e + 4);
            u32 dofs = hsf_be32(e + 8);
            int cnt = bufs[i].count;
            if (cnt > 0) {
                HsfVector2f *sdata = (HsfVector2f *)malloc(cnt * sizeof(HsfVector2f));
                u8 *src = data_base + dofs;
                for (j = 0; j < cnt; j++) {
                    sdata[j].x = hsf_bef(src + j * 8 + 0);
                    sdata[j].y = hsf_bef(src + j * 8 + 4);
                }
                bufs[i].data = sdata;
            }
        }
        hsf->st = bufs;
        hsf->stCnt = hdr.st.count;
    }

    /* ================================================================
     *  COLOR BUFFERS (byte-addressed RGBA, no swap needed)
     * ================================================================ */
    if (hdr.color.count > 0) {
        u8 *sec = base + hdr.color.ofs;
        u8 *data_base = sec + hdr.color.count * HSF_F_BUF;
        HsfBuffer *bufs = (HsfBuffer *)calloc(hdr.color.count, sizeof(HsfBuffer));
        for (i = 0; i < hdr.color.count; i++) {
            u8 *e = sec + i * HSF_F_BUF;
            bufs[i].name = &strings[hsf_be32(e + 0)];
            bufs[i].count = hsf_bes32(e + 4);
            u32 dofs = hsf_be32(e + 8);
            bufs[i].data = (void *)(data_base + dofs);
        }
        hsf->color = bufs;
        hsf->colorCnt = hdr.color.count;
    }

    /* ================================================================
     *  FACE BUFFERS
     * ================================================================ */
    if (hdr.face.count > 0) {
        u8 *sec = base + hdr.face.ofs;
        u8 *face_data_base = sec + hdr.face.count * HSF_F_BUF;
        HsfBuffer *bufs = (HsfBuffer *)calloc(hdr.face.count, sizeof(HsfBuffer));

        /* First pass: parse buffer headers and find global strip base.
         * Strip data lives after ALL face entries across all buffers. */
        u32 max_face_end = 0;
        for (i = 0; i < hdr.face.count; i++) {
            u8 *e = sec + i * HSF_F_BUF;
            bufs[i].name = &strings[hsf_be32(e + 0)];
            bufs[i].count = hsf_bes32(e + 4);
            u32 dofs = hsf_be32(e + 8);
            u32 end = dofs + bufs[i].count * HSF_F_FACE;
            if (end > max_face_end) max_face_end = end;
            /* Stash dofs temporarily in data pointer for second pass */
            bufs[i].data = (void *)(uintptr_t)dofs;
        }
        u8 *strip_base = face_data_base + max_face_end;

        /* Second pass: parse face data */
        for (i = 0; i < hdr.face.count; i++) {
            u32 dofs = (u32)(uintptr_t)bufs[i].data;
            int cnt = bufs[i].count;
            bufs[i].data = NULL;
            if (cnt > 0) {
                HsfFace *faces = (HsfFace *)calloc(cnt, sizeof(HsfFace));
                u8 *fsrc = face_data_base + dofs;
                for (j = 0; j < cnt; j++) {
                    u8 *f = fsrc + j * HSF_F_FACE;
                    faces[j].type = hsf_bes16(f + 0);
                    faces[j].mat = hsf_bes16(f + 2);
                    if (faces[j].type == 4) {
                        /* Strip face: indices[3][4] + count + data offset */
                        int k;
                        for (k = 0; k < 12; k++) {
                            faces[j].strip.indices[k / 4][k % 4] = hsf_bes16(f + 4 + k * 2);
                        }
                        faces[j].strip.count = hsf_be32(f + 28);
                        u32 strip_ofs = hsf_be32(f + 32);
                        /* Strip data: array of s16 groups at global strip_base + ofs * 8.
                         * Copy and byte-swap into a new allocation to avoid corrupting file data. */
                        u32 strip_words = faces[j].strip.count * 4;
                        s16 *strip_copy = (s16 *)malloc(strip_words * sizeof(s16));
                        u8 *sp = strip_base + strip_ofs * 8;
                        u32 sw;
                        for (sw = 0; sw < strip_words; sw++) {
                            strip_copy[sw] = hsf_bes16(sp + sw * 2);
                        }
                        faces[j].strip.data = strip_copy;
                    } else {
                        /* Regular face: indices[4][4] = 16 s16 values */
                        int k;
                        for (k = 0; k < 16; k++) {
                            faces[j].indices[k / 4][k % 4] = hsf_bes16(f + 4 + k * 2);
                        }
                    }
                    /* Normal/binormal/tangent vector */
                    faces[j].nbt.x = hsf_bef(f + 36);
                    faces[j].nbt.y = hsf_bef(f + 40);
                    faces[j].nbt.z = hsf_bef(f + 44);
                }
                bufs[i].data = faces;
            }
        }
        hsf->face = bufs;
        hsf->faceCnt = hdr.face.count;
    }

    /* ================================================================
     *  OBJECTS
     * ================================================================ */
    HsfObject *objects = NULL;
    if (hdr.object.count > 0) {
        u8 *sec = base + hdr.object.ofs;
        objects = (HsfObject *)calloc(hdr.object.count, sizeof(HsfObject));

        /* First pass: parse names, types, flags */
        for (i = 0; i < hdr.object.count; i++) {
            u8 *e = sec + i * HSF_F_OBJ;
            objects[i].name = &strings[hsf_be32(e + 0)];
            objects[i].type = hsf_be32(e + 4);
            objects[i].constData = NULL;
            objects[i].flags = hsf_be32(e + 12);
        }

        /* Second pass: parse object data */
        s32 root_idx = -1;
        for (i = 0; i < hdr.object.count; i++) {
            u8 *e = sec + i * HSF_F_OBJ;
            u8 *d = e + 16; /* Union data starts at object + 16 in file */

            /* Camera objects (type 7): parse union as HsfCamera directly.
             * On 64-bit PC, HsfObjectData has different offsets due to pointer
             * sizes, so the union overlay with HsfCamera doesn't align correctly
             * if we parse as HsfObjectData. */
            if (objects[i].type == 7) {
                HsfCamera *cam = &objects[i].camera;
                cam->target.x = hsf_bef(d + 0);
                cam->target.y = hsf_bef(d + 4);
                cam->target.z = hsf_bef(d + 8);
                cam->pos.x    = hsf_bef(d + 12);
                cam->pos.y    = hsf_bef(d + 16);
                cam->pos.z    = hsf_bef(d + 20);
                cam->aspect_dupe = hsf_bef(d + 24);
                cam->fov      = hsf_bef(d + 28);
                cam->near     = hsf_bef(d + 32);
                cam->far      = hsf_bef(d + 36);
                continue;
            }

            /* Light objects (type 8): parse union as HsfLight directly. */
            if (objects[i].type == 8) {
                HsfLight *light = &objects[i].light;
                light->pos.x    = hsf_bef(d + 0);
                light->pos.y    = hsf_bef(d + 4);
                light->pos.z    = hsf_bef(d + 8);
                light->target.x = hsf_bef(d + 12);
                light->target.y = hsf_bef(d + 16);
                light->target.z = hsf_bef(d + 20);
                light->type     = d[24];
                light->r        = d[25];
                light->g        = d[26];
                light->b        = d[27];
                light->unk2C        = hsf_bef(d + 28);
                light->ref_distance  = hsf_bef(d + 32);
                light->ref_brightness = hsf_bef(d + 36);
                light->cutoff   = hsf_bef(d + 40);
                continue;
            }

            HsfObjectData *od = &objects[i].data;

            s32 parent_val = hsf_bes32(d + 0);
            u32 child_count = hsf_be32(d + 4);
            u32 child_sym = hsf_be32(d + 8);

            /* Root object has parent == -1 */
            if (parent_val == -1 && root_idx == -1) {
                root_idx = i;
            }

            od->childrenCount = child_count;
            od->parent = NULL; /* resolved below */

            /* Allocate and resolve children array from symbol table */
            if (child_count > 0 && sym && child_sym < (u32)hdr.symbol.count) {
                HsfObject **children = (HsfObject **)calloc(child_count, sizeof(HsfObject *));
                for (j = 0; j < (s32)child_count; j++) {
                    u32 sym_idx = child_sym + j;
                    if (sym_idx < (u32)hdr.symbol.count) {
                        s32 cidx = (s32)sym[sym_idx];
                        if (cidx >= 0 && cidx < hdr.object.count) {
                            children[j] = &objects[cidx];
                        }
                    }
                }
                od->children = children;
            } else if (child_count > 0) {
                /* Invalid symbol index - allocate empty children array */
                od->children = (HsfObject **)calloc(child_count, sizeof(HsfObject *));
            }

            /* Transforms */
            hsf_read_transform(d + 12, &od->base);
            hsf_read_transform(d + 48, &od->curr);

            /* Mesh bounding box + morph weights */
            od->mesh.min.x = hsf_bef(d + 84);
            od->mesh.min.y = hsf_bef(d + 88);
            od->mesh.min.z = hsf_bef(d + 92);
            od->mesh.max.x = hsf_bef(d + 96);
            od->mesh.max.y = hsf_bef(d + 100);
            od->mesh.max.z = hsf_bef(d + 104);
            od->mesh.baseMorph = hsf_bef(d + 108);
            for (j = 0; j < 33; j++) {
                od->mesh.morphWeight[j] = hsf_bef(d + 112 + j * 4);
            }

            /* Buffer references (index into respective buffer arrays, -1 = none) */
            s32 face_id = hsf_bes32(d + 244);
            s32 vert_id = hsf_bes32(d + 248);
            s32 norm_id = hsf_bes32(d + 252);
            s32 clr_id  = hsf_bes32(d + 256);
            s32 st_id   = hsf_bes32(d + 260);

            od->face   = (face_id >= 0 && face_id < hdr.face.count   && hsf->face)   ? &hsf->face[face_id]   : NULL;
            od->vertex = (vert_id >= 0 && vert_id < hdr.vertex.count && hsf->vertex) ? &hsf->vertex[vert_id] : NULL;
            od->normal = (norm_id >= 0 && norm_id < hdr.normal.count && hsf->normal) ? &hsf->normal[norm_id] : NULL;
            od->color  = (clr_id  >= 0 && clr_id  < hdr.color.count  && hsf->color)  ? &hsf->color[clr_id]   : NULL;
            od->st     = (st_id   >= 0 && st_id   < hdr.st.count     && hsf->st)     ? &hsf->st[st_id]       : NULL;

            /* Material: always set to array base (face.mat selects index) */
            od->material = hsf->material;

            /* Attribute: set to array base if valid, else NULL */
            s32 attr_val = hsf_bes32(d + 268);
            od->attribute = (attr_val >= 0 && hsf->attribute) ? hsf->attribute : NULL;

            /* Misc bytes */
            od->unk120[0] = d[272]; od->unk120[1] = d[273];
            od->shapeType = d[274];
            od->unk123 = d[275];

            /* VertexShape, cluster, cenv - leave as zero for initial port */
            od->vertexShapeCnt = 0;
            od->vertexShape = NULL;
            od->clusterCnt = 0;
            od->cluster = NULL;
            od->cenvCnt = 0;
            od->cenv = NULL;

            /* File data pointers (offsets into file buffer) */
            u32 file0_ofs = hsf_be32(d + 300);
            u32 file1_ofs = hsf_be32(d + 304);
            od->file[0] = (file0_ofs != 0) ? (void *)(base + file0_ofs) : NULL;
            od->file[1] = (file1_ofs != 0) ? (void *)(base + file1_ofs) : NULL;
        }

        /* Resolve parent pointers from children relationships.
         * Skip camera (type 7) and light (type 8) objects — they don't
         * have HsfObjectData fields (children, parent). */
        for (i = 0; i < hdr.object.count; i++) {
            if (objects[i].type == 7 || objects[i].type == 8) continue;
            for (j = 0; j < (s32)objects[i].data.childrenCount; j++) {
                if (objects[i].data.children && objects[i].data.children[j]) {
                    objects[i].data.children[j]->data.parent = &objects[i];
                }
            }
        }

        /* Fixup object types (7 -> NONE1, 8 -> NONE2) */
        for (i = 0; i < hdr.object.count; i++) {
            if (objects[i].type == 8) objects[i].type = HSF_OBJ_NONE2;
            else if (objects[i].type == 7) objects[i].type = HSF_OBJ_NONE1;
        }

        /* Resolve replica pointers */
        for (i = 0; i < hdr.object.count; i++) {
            if (objects[i].type == HSF_OBJ_REPLICA) {
                u8 *d = sec + i * HSF_F_OBJ + 16;
                /* Replica object index is stored at mesh.min offset (d+84) */
                s32 rep_id = hsf_bes32(d + 84);
                if (rep_id >= 0 && rep_id < hdr.object.count) {
                    objects[i].data.replica = &objects[rep_id];
                }
            }
        }

        hsf->object = objects;
        hsf->objectCnt = hdr.object.count;
        hsf->root = (root_idx >= 0) ? &objects[root_idx] : NULL;
    }

    /* ================================================================
     *  SKELETON (needed for InitEnvelope checks)
     * ================================================================ */
    if (hdr.skeleton.count > 0) {
        u8 *sec = base + hdr.skeleton.ofs;
        HsfSkeleton *skels = (HsfSkeleton *)calloc(hdr.skeleton.count, sizeof(HsfSkeleton));
        for (i = 0; i < hdr.skeleton.count; i++) {
            u8 *e = sec + i * HSF_F_SKEL;
            skels[i].name = &strings[hsf_be32(e + 0)];
            hsf_read_transform(e + 4, &skels[i].transform);
        }
        hsf->skeleton = skels;
        hsf->skeletonCnt = hdr.skeleton.count;
    }

    /* ================================================================
     *  MATRIX (skinning matrices)
     * ================================================================ */
    if (hdr.matrix.count > 0) {
        u8 *sec = base + hdr.matrix.ofs;
        /* File layout: HsfMatrix header (base_idx:u32, count:u32, data_ofs:u32, pad:u32)
         * followed by count 3x4 matrices (each 48 bytes, big-endian floats). */
        HsfMatrix *mtx = (HsfMatrix *)calloc(1, sizeof(HsfMatrix));
        mtx->base_idx = hsf_be32(sec + 0);
        mtx->count    = hsf_be32(sec + 4);
        u32 data_ofs  = hsf_be32(sec + 8);
        /* Matrix data follows the header */
        u8 *mtx_data_raw = sec + HSF_F_MATRIX;
        Mtx *mtx_arr = (Mtx *)calloc(mtx->count, sizeof(Mtx));
        for (i = 0; i < (s32)mtx->count; i++) {
            u8 *m = mtx_data_raw + i * 48; /* 3x4 float = 48 bytes */
            for (j = 0; j < 3; j++) {
                for (int k = 0; k < 4; k++) {
                    mtx_arr[i][j][k] = hsf_bef(m + (j * 4 + k) * 4);
                }
            }
        }
        mtx->data = mtx_arr;
        hsf->matrix = mtx;
        hsf->matrixCnt = hdr.matrix.count;
    }

    /* ================================================================
     *  MOTION (animation data)
     * ================================================================ */
    if (hdr.motion.count > 0) {
        u8 *sec = base + hdr.motion.ofs;

        /* Parse motion header (first record only, matching GC behavior) */
        HsfMotion *motion = (HsfMotion *)calloc(1, sizeof(HsfMotion));
        motion->name      = &strings[hsf_be32(sec + 0)];
        s32 numTracks     = hsf_bes32(sec + 4);
        /* sec+8 is unused track pointer in file */
        motion->len       = hsf_bef(sec + 12);
        motion->numTracks = numTracks;

        /* Tracks follow after all motion headers */
        u8 *track_sec = sec + hdr.motion.count * HSF_F_MOTION;
        /* Keyframe data follows after all tracks */
        u8 *kf_data = track_sec + numTracks * HSF_F_TRACK;

        HsfTrack *tracks = (HsfTrack *)calloc(numTracks, sizeof(HsfTrack));
        for (i = 0; i < numTracks; i++) {
            u8 *t = track_sec + i * HSF_F_TRACK;
            tracks[i].type         = t[0];
            tracks[i].start        = t[1];
            tracks[i].target       = hsf_be16(t + 2);
            tracks[i].unk04        = hsf_bes32(t + 4);
            tracks[i].curveType    = hsf_be16(t + 8);
            tracks[i].numKeyframes = hsf_be16(t + 10);
            u32 data_ofs           = hsf_be32(t + 12);

            /* Resolve target name → object/cluster/attribute index.
             * For TRANSFORM/MORPH: only resolve if HSF has objects.
             * Motion-only files (Hu3DMotionCreate) leave targets as string
             * offsets so JointModel_Motion can resolve them at runtime
             * against a different model's object list. This matches the GC
             * behavior where MotionLoadTransform skips if(objtop==NULL). */
            switch (tracks[i].type) {
                case HSF_TRACK_TRANSFORM:
                case HSF_TRACK_MORPH: {
                    if (hdr.object.count > 0) {
                        /* Model+motion combo: resolve target to object index */
                        char *tname = &strings[tracks[i].target];
                        s32 obj_idx = -1;
                        for (j = 0; j < hsf->objectCnt; j++) {
                            if (strcmp(hsf->object[j].name, tname) == 0) {
                                obj_idx = j;
                                break;
                            }
                        }
                        tracks[i].target = (u16)obj_idx;
                    }
                    /* else: leave target as string table offset for runtime resolution */
                    break;
                }
                case HSF_TRACK_CLUSTER:
                case HSF_TRACK_CLUSTER_WEIGHT: {
                    /* target is string offset → resolve to cluster index
                     * Clusters not loaded yet, store -1 for now */
                    tracks[i].target_s16 = -1;
                    break;
                }
                case HSF_TRACK_MATERIAL: {
                    /* param is string offset → resolve to material index */
                    char *mname = &strings[tracks[i].param_u16];
                    s32 mat_idx = -1;
                    for (j = 0; j < hsf->materialCnt; j++) {
                        if (strcmp(hsf->material[j].name, mname) == 0) {
                            mat_idx = j;
                            break;
                        }
                    }
                    tracks[i].param = (s16)mat_idx;
                    break;
                }
                case HSF_TRACK_ATTRIBUTE: {
                    /* param is string offset → resolve to attribute index */
                    char *aname = &strings[tracks[i].param_u16];
                    s32 attr_idx = -1;
                    for (j = 0; j < hsf->attributeCnt; j++) {
                        if (strcmp(hsf->attribute[j].name, aname) == 0) {
                            attr_idx = j;
                            break;
                        }
                    }
                    tracks[i].param = (s16)attr_idx;
                    break;
                }
            }

            /* Fix up keyframe data pointer and byte-swap floats */
            if (tracks[i].curveType == HSF_CURVE_CONST) {
                /* Value is stored inline as float at offset 12 in the track */
                tracks[i].value = hsf_bef(t + 12);
            } else if (tracks[i].curveType == HSF_CURVE_BITMAP) {
                /* Bitmap keyframes: (float time, u32 bitmap_idx) pairs
                 * Skip for now — bitmap animation not critical */
                tracks[i].data = NULL;
            } else {
                /* STEP, LINEAR, BEZIER: data_ofs is relative to kf_data base */
                u8 *raw = kf_data + data_ofs;
                int nkf = tracks[i].numKeyframes;
                int floats_per_kf;
                switch (tracks[i].curveType) {
                    case HSF_CURVE_STEP:    floats_per_kf = 2; break; /* time, value */
                    case HSF_CURVE_LINEAR:  floats_per_kf = 2; break; /* time, value */
                    case HSF_CURVE_BEZIER:  floats_per_kf = 4; break; /* time, value, cp1, cp2 */
                    default:                floats_per_kf = 2; break;
                }
                /* Byte-swap floats in place */
                int total_floats = nkf * floats_per_kf;
                hsf_swap32_array(raw, total_floats);
                tracks[i].data = (void *)raw;
            }
        }

        motion->track = tracks;
        hsf->motion = motion;
        hsf->motionCnt = numTracks;

        {
            static int mot_diag = 0;
            if (mot_diag < 5) {
                /* Count tracks by type */
                int cnt[16] = {0};
                for (int d = 0; d < numTracks; d++) {
                    if (tracks[d].type < 16) cnt[tracks[d].type]++;
                }
                printf("[HSF-MOT] Loaded motion '%s': %d tracks, len=%.1f\n",
                    motion->name, numTracks, motion->len);
                printf("[HSF-MOT]   by type: TRANSFORM=%d MORPH=%d CLUSTER=%d CW=%d MAT=%d ATTR=%d\n",
                    cnt[2], cnt[3], cnt[5], cnt[6], cnt[9], cnt[10]);
                mot_diag++;
            }
        }
    }

    /* Remaining sections left as zero/NULL for now:
     * cenv, cluster, part, shape, mapAttr
     * These are needed for skinning but not basic model/animation rendering. */

    /* InitEnvelope is safe to call - it early-outs when cenvCnt == 0 */
    InitEnvelope(hsf);

    return hsf;
}
#endif /* TARGET_PC */