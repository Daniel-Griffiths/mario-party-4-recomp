/* Minimal MusyX stub for PC build */
#ifndef _MUSYX_MUSYX_H
#define _MUSYX_MUSYX_H

#include "dolphin/types.h"

typedef u32 SND_OUTPUTMODE;
typedef u32 SND_VOICEID;
typedef u32 SND_FXID;
typedef u32 SND_GROUPID;
typedef u32 SND_SONGID;
typedef u32 SND_STREAMID;
typedef u32 SND_SEQID;

#define SND_OUTPUTMODE_MONO 0
#define SND_OUTPUTMODE_STEREO 1
#define SND_OUTPUTMODE_SURROUND 2

/* AUX effect structs - sized to match MSM_AUX union (0x1E0 bytes) */
typedef struct {
    u8 _pad[0x1E0];
} SND_AUX_REVERBHI;

typedef struct {
    u8 _pad[0x1E0];
} SND_AUX_REVERBSTD;

typedef struct {
    u8 _pad[0x1E0];
} SND_AUX_CHORUS;

typedef struct {
    u8 _pad[0x1E0];
} SND_AUX_DELAY;

#endif /* _MUSYX_MUSYX_H */
