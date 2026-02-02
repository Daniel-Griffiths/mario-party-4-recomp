/*
 * Audio system stubs - all HuAud* and msm* functions return 0/no-op
 */
#include <stdio.h>
#include <string.h>
#include "dolphin/types.h"
#include "dolphin/dvd.h"
#include "game/audio.h"

/* ---- Global audio state (referenced by game code) ---- */
float Snd3DBackSurDisOffset = 0.0f;
float Snd3DFrontSurDisOffset = 0.0f;
float Snd3DStartDisOffset = 0.0f;
float Snd3DSpeedOffset = 0.0f;
float Snd3DDistOffset = 0.0f;
s32 musicOffF = 0;
u8 fadeStat = 0;

/* ---- HuAud functions ---- */
void HuAudInit(void) {
    printf("[AUD] HuAudInit() - audio stubbed\n");
}

s32 HuAudStreamPlay(char *name, BOOL flag) { (void)name; (void)flag; return 0; }
void HuAudStreamVolSet(s16 vol) { (void)vol; }
void HuAudStreamPauseOn(void) {}
void HuAudStreamPauseOff(void) {}
void HuAudStreamFadeOut(s32 streamNo) { (void)streamNo; }
void HuAudAllStop(void) {}
void HuAudFadeOut(s32 speed) { (void)speed; }

int HuAudFXPlay(int seId) { (void)seId; return -1; }
int HuAudFXPlayVol(int seId, s16 vol) { (void)seId; (void)vol; return -1; }
int HuAudFXPlayVolPan(int seId, s16 vol, s16 pan) { (void)seId; (void)vol; (void)pan; return -1; }
void HuAudFXStop(int seNo) { (void)seNo; }
void HuAudFXAllStop(void) {}
void HuAudFXFadeOut(int seNo, s32 speed) { (void)seNo; (void)speed; }
void HuAudFXPanning(int seNo, s16 pan) { (void)seNo; (void)pan; }
void HuAudFXListnerSet(Vec *pos, Vec *heading, float sndDist, float sndSpeed) {
    (void)pos; (void)heading; (void)sndDist; (void)sndSpeed;
}
void HuAudFXListnerSetEX(Vec *pos, Vec *heading, float sndDist, float sndSpeed,
                          float startDis, float frontSurDis, float backSurDis) {
    (void)pos; (void)heading; (void)sndDist; (void)sndSpeed;
    (void)startDis; (void)frontSurDis; (void)backSurDis;
}
void HuAudFXListnerUpdate(Vec *pos, Vec *heading) { (void)pos; (void)heading; }
int HuAudFXEmiterPlay(int seId, Vec *pos) { (void)seId; (void)pos; return -1; }
void HuAudFXEmiterUpDate(int seNo, Vec *pos) { (void)seNo; (void)pos; }
void HuAudFXListnerKill(void) {}
void HuAudFXPauseAll(s32 pause) { (void)pause; }
s32 HuAudFXStatusGet(int seNo) { (void)seNo; return 0; }
s32 HuAudFXPitchSet(int seNo, s16 pitch) { (void)seNo; (void)pitch; return 0; }
s32 HuAudFXVolSet(int seNo, s16 vol) { (void)seNo; (void)vol; return 0; }

s32 HuAudSeqPlay(s16 musId) { (void)musId; return -1; }
void HuAudSeqStop(s32 musNo) { (void)musNo; }
void HuAudSeqFadeOut(s32 musNo, s32 speed) { (void)musNo; (void)speed; }
void HuAudSeqAllFadeOut(s32 speed) { (void)speed; }
void HuAudSeqAllStop(void) {}
void HuAudSeqPauseAll(s32 pause) { (void)pause; }
void HuAudSeqPause(s32 musNo, s32 pause, s32 speed) { (void)musNo; (void)pause; (void)speed; }
s32 HuAudSeqMidiCtrlGet(s32 musNo, s8 channel, s8 ctrl) { (void)musNo; (void)channel; (void)ctrl; return 0; }

s32 HuAudSStreamPlay(s16 streamId) { (void)streamId; return -1; }
void HuAudSStreamStop(s32 seNo) { (void)seNo; }
void HuAudSStreamFadeOut(s32 seNo, s32 speed) { (void)seNo; (void)speed; }
void HuAudSStreamAllFadeOut(s32 speed) { (void)speed; }
void HuAudSStreamAllStop(void) {}
s32 HuAudSStreamStatGet(s32 seNo) { (void)seNo; return 0; }

void HuAudDllSndGrpSet(u16 ovl) { (void)ovl; }
void HuAudSndGrpSetSet(s16 dataSize) { (void)dataSize; }
void HuAudSndGrpSet(s16 grpId) { (void)grpId; }
void HuAudSndCommonGrpSet(s16 grpId, s32 groupCheck) { (void)grpId; (void)groupCheck; }
void HuAudAUXSet(s32 auxA, s32 auxB) { (void)auxA; (void)auxB; }
void HuAudAUXVolSet(s8 auxA, s8 auxB) { (void)auxA; (void)auxB; }
void HuAudVoiceInit(s16 ovl) { (void)ovl; }
s32 HuAudPlayerVoicePlay(s16 player, s16 seId) { (void)player; (void)seId; return -1; }
s32 HuAudPlayerVoicePlayPos(s16 player, s16 seId, Vec *pos) { (void)player; (void)seId; (void)pos; return -1; }
void HuAudPlayerVoicePlayEntry(s16 player, s16 seId) { (void)player; (void)seId; }
s32 HuAudCharVoicePlay(s16 charNo, s16 seId) { (void)charNo; (void)seId; return -1; }
s32 HuAudCharVoicePlayPos(s16 charNo, s16 seId, Vec *pos) { (void)charNo; (void)seId; (void)pos; return -1; }
void HuAudCharVoicePlayEntry(s16 charNo, s16 seId) { (void)charNo; (void)seId; }

/* ---- HuAR stubs (ARAM management for audio) ---- */
void HuARInit(void) {
    printf("[AUD] HuARInit() - stubbed\n");
}

/* ---- HuCard stubs ---- */
void HuCardInit(void) {
    printf("[CARD] HuCardInit() - stubbed\n");
}

/* ---- msm stubs ---- */
void msmMusFdoutEnd(void) {}
void msmMusPeriodicProc(void) {}
s32 msmMusGetMidiCtrl(int musNo, s32 channel, s32 ctrl) { (void)musNo; (void)channel; (void)ctrl; return 0; }
s32 msmMusGetNumPlay(BOOL baseGrp) { (void)baseGrp; return 0; }
s32 msmMusGetStatus(int musNo) { (void)musNo; return 0; }
void msmMusSetMasterVolume(s32 arg0) { (void)arg0; }
s32 msmMusSetParam(s32 arg0, void *arg1) { (void)arg0; (void)arg1; return 0; }
void msmMusPauseAll(BOOL pause, s32 speed) { (void)pause; (void)speed; }
s32 msmMusPause(int musNo, BOOL pause, s32 speed) { (void)musNo; (void)pause; (void)speed; return 0; }
void msmMusStopAll(BOOL checkGrp, s32 speed) { (void)checkGrp; (void)speed; }
s32 msmMusStop(int musNo, s32 speed) { (void)musNo; (void)speed; return 0; }
int msmMusPlay(int musId, void *musParam) { (void)musId; (void)musParam; return -1; }
s32 msmMusInit(void *arg0, DVDFileInfo *arg1) { (void)arg0; (void)arg1; return 0; }

void msmSePeriodicProc(void) {}
void *msmSeGetIndexPtr(s32 arg0) { (void)arg0; return NULL; }
void msmSeDelListener(void) {}
s32 msmSeUpdataListener(Vec *pos, Vec *heading) { (void)pos; (void)heading; return 0; }
s32 msmSeSetListener(Vec *pos, Vec *heading, float sndDist, float sndSpeed, void *listener) {
    (void)pos; (void)heading; (void)sndDist; (void)sndSpeed; (void)listener; return 0;
}
s32 msmSeGetEntryID(s32 seId, int *seNo) { (void)seId; (void)seNo; return 0; }
s32 msmSeGetNumPlay(BOOL baseGrp) { (void)baseGrp; return 0; }
s32 msmSeGetStatus(int seNo) { (void)seNo; return 0; }
void msmSeSetMasterVolume(s32 arg0) { (void)arg0; }
s32 msmSeSetParam(int seNo, void *param) { (void)seNo; (void)param; return 0; }
void msmSePauseAll(BOOL pause, s32 speed) { (void)pause; (void)speed; }
void msmSeStopAll(BOOL checkGrp, s32 speed) { (void)checkGrp; (void)speed; }
s32 msmSeStop(int seNo, s32 speed) { (void)seNo; (void)speed; return 0; }
int msmSePlay(int seId, void *param) { (void)seId; (void)param; return -1; }
s32 msmSeInit(void *arg0, DVDFileInfo *arg1) { (void)arg0; (void)arg1; return 0; }

s32 msmStreamGetStatus(int streamNo) { (void)streamNo; return 0; }
void msmStreamSetMasterVolume(s32 arg0) { (void)arg0; }
void msmStreamStopAll(s32 speed) { (void)speed; }
s32 msmStreamStop(int streamNo, s32 speed) { (void)streamNo; (void)speed; return 0; }
int msmStreamPlay(int streamId, void *streamParam) { (void)streamId; (void)streamParam; return -1; }
void msmStreamPeriodicProc(void) {}
void msmStreamSetOutputMode(s32 arg0) { (void)arg0; }
void msmStreamAmemFree(void) {}
s32 msmStreamAmemAlloc(void) { return 0; }
s32 msmStreamInit(char *arg0) { (void)arg0; return 0; }

s32 msmSysSearchGroupStack(s32 arg0, s32 arg1) { (void)arg0; (void)arg1; return 0; }
s32 msmSysGroupInit(DVDFileInfo *arg0) { (void)arg0; return 0; }
void msmSysIrqDisable(void) {}
void msmSysIrqEnable(void) {}
BOOL msmSysCheckBaseGroup(s32 arg0) { (void)arg0; return FALSE; }
void *msmSysGetGroupDataPtr(s32 arg0) { (void)arg0; return NULL; }
BOOL msmSysCheckLoadGroupID(s32 arg0) { (void)arg0; return FALSE; }
void msmSysRegularProc(void) {}
s32 msmSysGetOutputMode(void) { return 0; }
BOOL msmSysSetOutputMode(s32 mode) { (void)mode; return TRUE; }
s32 msmSysGetSampSize(BOOL baseGrp) { (void)baseGrp; return 0; }
s32 msmSysDelGroupAll(void) { return 0; }
s32 msmSysDelGroupBase(s32 grpNum) { (void)grpNum; return 0; }
s32 msmSysLoadGroupBase(s32 arg0, void *arg1) { (void)arg0; (void)arg1; return 0; }
s32 msmSysLoadGroupSet(s32 arg0, void *arg1) { (void)arg0; (void)arg1; return 0; }
void msmSysCheckInit(void) {}
s32 msmSysInit(void *init, void *aram) { (void)init; (void)aram; return 0; }
void msmSysSetAux(s32 busNo, s32 auxNo, s32 vol) { (void)busNo; (void)auxNo; (void)vol; }

BOOL msmFioClose(DVDFileInfo *fileInfo) { (void)fileInfo; return TRUE; }
BOOL msmFioRead(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset) {
    (void)fileInfo; (void)addr; (void)length; (void)offset; return TRUE;
}
BOOL msmFioOpen(s32 entrynum, DVDFileInfo *fileInfo) { (void)entrynum; (void)fileInfo; return TRUE; }
void msmFioInit(void *open, void *read, void *close) { (void)open; (void)read; (void)close; }

void msmMemFree(void *ptr) { (void)ptr; }
void *msmMemAlloc(u32 sze) { (void)sze; return NULL; }
void msmMemInit(void *ptr, u32 size) { (void)ptr; (void)size; }
