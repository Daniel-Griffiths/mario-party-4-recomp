/*
 * THP (Video playback) stubs
 */
#include <stdio.h>
#include "dolphin/types.h"

/* THPPlayer stubs */
void THPPlayerInit(s32 audioTrack) { (void)audioTrack; }
void THPPlayerQuit(void) {}
BOOL THPPlayerOpen(const char *fileName, BOOL onMemory) { (void)fileName; (void)onMemory; return FALSE; }
BOOL THPPlayerClose(void) { return TRUE; }
BOOL THPPlayerPlay(void) { return FALSE; }
void THPPlayerStop(void) {}
BOOL THPPlayerPause(void) { return FALSE; }
BOOL THPPlayerPrepare(s32 frame, u32 flag, s32 audioTrack) {
    (void)frame; (void)flag; (void)audioTrack; return FALSE;
}
s32 THPPlayerDrawCurrentFrame(void *gxobj, u32 x, u32 y, u32 polygonW, u32 polygonH) {
    (void)gxobj; (void)x; (void)y; (void)polygonW; (void)polygonH; return 0;
}
void THPPlayerSetVolume(s32 vol, s32 duration) { (void)vol; (void)duration; }
u32 THPPlayerGetTotalFrame(void) { return 0; }
s32 THPPlayerGetState(void) { return 0; }
void THPPlayerDrawDone(void) {}
void THPPlayerPostDrawDone(void) {}
void THPPlayerSetBuffer(void *buffer) { (void)buffer; }
u32 THPPlayerCalcNeedMemory(void) { return 0; }

/* THPSimple stubs */
void THPSimpleInit(void) { printf("[THP] THPSimpleInit() - video stubbed\n"); }
void THPSimpleQuit(void) {}
BOOL THPSimpleOpen(const char *fileName) { (void)fileName; return FALSE; }
BOOL THPSimpleClose(void) { return TRUE; }
u32 THPSimpleCalcNeedMemory(void) { return 0; }
void THPSimpleSetBuffer(void *buffer) { (void)buffer; }
s32 THPSimplePreLoad(void) { return 0; }
void THPSimpleAudioStart(void) {}
void THPSimpleAudioStop(void) {}
void THPSimpleLoadStop(void) {}
s32 THPSimpleDecode(void) { return 0; }
s32 THPSimpleDrawCurrentFrame(void *gxobj, u32 x, u32 y, u32 w, u32 h) {
    (void)gxobj; (void)x; (void)y; (void)w; (void)h; return 0;
}
void THPSimpleSetVolume(s32 vol, s32 duration) { (void)vol; (void)duration; }
u32 THPSimpleGetTotalFrame(void) { return 0; }

/* Low-level THP */
void THPInit(void) {}
s32 THPVideoDecode(void *file, void *tileY, void *tileU, void *tileV, void *work) {
    (void)file; (void)tileY; (void)tileU; (void)tileV; (void)work; return 0;
}
s32 THPAudioDecode(void *buffer, void *audioBuffer, s32 flag) {
    (void)buffer; (void)audioBuffer; (void)flag; return 0;
}
void THPGXRestore(void) {}
void THPGXYuv2RgbSetup(u32 dispWidth, u32 dispHeight) { (void)dispWidth; (void)dispHeight; }
void THPGXYuv2RgbDraw(void *y, void *u, void *v, s16 x, s16 y_pos, s16 textureWidth,
                       s16 textureHeight, s16 polygonX, s16 polygonY, s16 polygonW, s16 polygonH) {
    (void)y; (void)u; (void)v; (void)x; (void)y_pos;
    (void)textureWidth; (void)textureHeight;
    (void)polygonX; (void)polygonY; (void)polygonW; (void)polygonH;
}
