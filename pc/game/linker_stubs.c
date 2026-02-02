/**
 * linker_stubs.c - Stub implementations for undefined symbols
 *
 * Auto-generated stubs to satisfy the linker for the Mario Party 4 PC port.
 * These provide minimal implementations that allow the project to link.
 * Each stub should be replaced with a real implementation over time.
 */

#include "dolphin/types.h"
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * Global variable stubs
 * ======================================================================== */

void* boardBowserHook = NULL;
void* boardMainProc = NULL;
void* boardObjMan = NULL;
void* boardStarGiveHook = NULL;
void* boardStarShowNextHook = NULL;
void* boardTurnFunc = NULL;
void* boardTurnStartFunc = NULL;
void* boardTutorialData = NULL;
s32 boardTutorialF = 0;
s16 boardPlayerMdl[4] = { -1, -1, -1, -1 };
s32 sndGrpTable = 0;
s32 UnMountCnt = 0;

/* ========================================================================
 * Board Audio
 * ======================================================================== */

void BoardAudSeqFadeOut(void) {}
void BoardAudSeqFadeOutAll(void) {}
void BoardAudSeqFadeOutFast(void) {}
void BoardAudSeqPause(void) {}

/* ========================================================================
 * Board Boo House / Bowser Suit
 * ======================================================================== */

void BoardBooHouseHostSet(void) {}
void BoardBooHouseTutorialExec(void) {}
s32 BoardBowserSuitModelGet(void) { return -1; }
s32 BoardBowserSuitPlayerModelGet(void) { return -1; }

/* ========================================================================
 * Board Camera
 * ======================================================================== */

s32 BoardCameraDirGet(void) { return 0; }
s32 BoardCameraMotionIsDone(void) { return 0; }
void BoardCameraMotionStart(void) {}
void BoardCameraMotionStartEx(void) {}
void BoardCameraMotionWait(void) {}
void BoardCameraMoveSet(void) {}
void BoardCameraNearFarSet(void) {}
void BoardCameraOffsetSet(void) {}
void BoardCameraPosCalcFuncSet(void) {}
void BoardCameraPosGet(void) {}
void BoardCameraQuakeReset(void) {}
void BoardCameraQuakeSet(void) {}
void BoardCameraRotGet(void) {}
void BoardCameraRotSet(void) {}
void BoardCameraTargetGet(void) {}
void BoardCameraTargetModelSet(void) {}
void BoardCameraTargetPlayerSet(void) {}
void BoardCameraTargetSet(void) {}
void BoardCameraTargetSpaceSet(void) {}
void BoardCameraViewSet(void) {}
void BoardCameraXRotZoomSet(void) {}
void BoardCameraZoomSet(void) {}

/* ========================================================================
 * Board Character / Coin / Com / Confetti
 * ======================================================================== */

void BoardCharWheelTargetSet(void) {}
s32 BoardCoinChgCreate(void) { return 0; }
s32 BoardCoinChgExist(void) { return 0; }
void BoardComKeySetDown(void) {}
void BoardComKeySetLeft(void) {}
void BoardComKeySetRight(void) {}
void BoardComKeySetUp(void) {}
s32 BoardComPreferItemCheck(void) { return 0; }
s32 BoardComPreferItemGet(void) { return 0; }
void BoardConfettiCreate(void) {}
void BoardConfettiKill(void) {}
void BoardConfettiStop(void) {}

/* ========================================================================
 * Board Math / Angle
 * ======================================================================== */

f32 BoardDAngleCalc(void) { return 0.0f; }
f32 BoardDAngleCalcRange(void) { return 0.0f; }

/* ========================================================================
 * Board Event Flags
 * ======================================================================== */

s32 BoardEventFlagGet(void) { return 0; }
void BoardEventFlagReset(void) {}
void BoardEventFlagSet(void) {}

/* ========================================================================
 * Board Filter / Fade
 * ======================================================================== */

void BoardFilterFadeInit(void) {}
void BoardFilterFadeOut(void) {}
s32 BoardFilterFadePauseCheck(void) { return 0; }

/* ========================================================================
 * Board Kill / Item / Junction / Light
 * ======================================================================== */

s32 BoardIsKill(void) { return 0; }
s32 BoardItemModelGet(void) { return -1; }
s32 BoardItemPrevGet(void) { return 0; }
void BoardItemPrevSet(void) {}
void BoardJunctionMaskReset(void) {}
void BoardJunctionMaskSet(void) {}
void BoardKill(void) {}
void BoardLightHookSet(void) {}
void BoardLightResetExec(void) {}
void BoardLightSetExec(void) {}

/* ========================================================================
 * Board Lottery
 * ======================================================================== */

void BoardLotteryHostSet(void) {}
void BoardLotteryTutorialExec(void) {}

/* ========================================================================
 * Board Minigame
 * ======================================================================== */

s32 BoardMGDoneFlagGet(void) { return 0; }
void BoardMGDoneFlagSet(void) {}
void BoardMGExit(void) {}
void BoardMGSetupTutorialExec(void) {}

/* ========================================================================
 * Board Model
 * ======================================================================== */

void BoardModelAlphaSet(void) {}
void BoardModelAttrReset(void) {}
void BoardModelAttrSet(void) {}
s32 BoardModelCreate(void) { return 0; }
s32 BoardModelExistDupe(void) { return 0; }
void BoardModelHideSetAll(void) {}
void BoardModelHookObjReset(void) {}
void BoardModelHookReset(void) {}
void BoardModelHookSet(void) {}
s32 BoardModelIDGet(void) { return 0; }
void BoardModelKill(void) {}
void BoardModelLayerSet(void) {}
s32 BoardModelMotionCreate(void) { return 0; }
s32 BoardModelMotionEndCheck(void) { return 0; }
void BoardModelMotionKill(void) {}
f32 BoardModelMotionMaxTimeGet(void) { return 0.0f; }
void BoardModelMotionShiftSet(void) {}
f32 BoardModelMotionSpeedGet(void) { return 0.0f; }
void BoardModelMotionSpeedSet(void) {}
void BoardModelMotionStart(void) {}
void BoardModelMotionStartEndSet(void) {}
f32 BoardModelMotionTimeGet(void) { return 0.0f; }
void BoardModelMotionTimeSet(void) {}
void BoardModelMotionUpdateSet(void) {}
void BoardModelMtxSet(void) {}
void BoardModelPassSet(void) {}
void BoardModelPosGet(void) {}
void BoardModelPosSet(void) {}
void BoardModelPosSetV(void) {}
void BoardModelRotGet(void) {}
void BoardModelRotSet(void) {}
void BoardModelRotSetV(void) {}
f32 BoardModelRotYGet(void) { return 0.0f; }
void BoardModelRotYSet(void) {}
void BoardModelScaleGet(void) {}
void BoardModelScaleSet(void) {}
void BoardModelScaleSetV(void) {}
s32 BoardModelVisibilityGet(void) { return 0; }
void BoardModelVisibilitySet(void) {}

/* ========================================================================
 * Board MTX / Music
 * ======================================================================== */

void BoardMTXCalcLookAt(void) {}
void BoardMusStart(void) {}
void BoardMusStartBoard(void) {}
s32 BoardMusStatusGet(void) { return 0; }

/* ========================================================================
 * Board Misc
 * ======================================================================== */

void BoardNextOvlSet(void) {}
void BoardPartyConfigSet(void) {}
s32 BoardPauseActiveCheck(void) { return 0; }

/* ========================================================================
 * Board Player
 * ======================================================================== */

f32 BoardPlayerAutoSizeGet(void) { return 1.0f; }
void BoardPlayerCoinsAdd(void) {}
s32 BoardPlayerCoinsGet(void) { return 0; }
void BoardPlayerCopyEyeMat(void) {}
s32 BoardPlayerDiceJumpCheck(void) { return 0; }
void BoardPlayerDiceJumpStart(void) {}
s32 BoardPlayerExistCheck(void) { return 0; }
s32 BoardPlayerGetCharMess(void) { return 0; }
void BoardPlayerIdleSet(void) {}
void BoardPlayerItemAdd(void) {}
s32 BoardPlayerItemCount(void) { return 0; }
void BoardPlayerItemRemove(void) {}
void BoardPlayerModelAttrReset(void) {}
s32 BoardPlayerMotBlendCheck(void) { return 0; }
void BoardPlayerMotBlendSet(void) {}
s32 BoardPlayerMotionCreate(void) { return 0; }
s32 BoardPlayerMotionEndCheck(void) { return 0; }
void BoardPlayerMotionEndWait(void) {}
void BoardPlayerMotionKill(void) {}
f32 BoardPlayerMotionMaxTimeGet(void) { return 0.0f; }
void BoardPlayerMotionShiftSet(void) {}
void BoardPlayerMotionSpeedSet(void) {}
void BoardPlayerMotionStart(void) {}
f32 BoardPlayerMotionTimeGet(void) { return 0.0f; }
void BoardPlayerMotionTimeSet(void) {}
void BoardPlayerMoveAwayStartCurr(void) {}
void BoardPlayerMoveBetween(void) {}
void BoardPlayerMoveToAsync(void) {}
void BoardPlayerMtxSet(void) {}
void BoardPlayerPosGet(void) {}
void BoardPlayerPosLerpStart(void) {}
void BoardPlayerPosSet(void) {}
void BoardPlayerPosSetV(void) {}
void BoardPlayerPostTurnHookSet(void) {}
void BoardPlayerPreTurnHookSet(void) {}
void BoardPlayerRankCalc(void) {}
void BoardPlayerResizeAnimExec(void) {}
void BoardPlayerRotGet(void) {}
void BoardPlayerRotSet(void) {}
void BoardPlayerRotSetV(void) {}
f32 BoardPlayerRotYGet(void) { return 0.0f; }
void BoardPlayerRotYSet(void) {}
void BoardPlayerScaleGet(void) {}
void BoardPlayerScaleSetV(void) {}
f32 BoardPlayerSizeGet(void) { return 1.0f; }
void BoardPlayerSizeSet(void) {}
void BoardPlayerStarsAdd(void) {}
void BoardPlayerVoiceEnableSet(void) {}

/* ========================================================================
 * Board Random
 * ======================================================================== */

s32 BoardRand(void) { return 0; }
f32 BoardRandFloat(void) { return 0.0f; }
s32 BoardRandMod(void) { return 0; }

/* ========================================================================
 * Board Roll
 * ======================================================================== */

void BoardRollCreate(void) {}
void BoardRollDispSet(void) {}
void BoardRollTutorialSet(void) {}
void BoardRollUpdateSet(void) {}

/* ========================================================================
 * Board Save / Shop
 * ======================================================================== */

void BoardSaveInit(void) {}
void BoardShopHostSet(void) {}
void BoardShopTutorialExec(void) {}

/* ========================================================================
 * Board Space
 * ======================================================================== */

void BoardSpaceAttrReset(void) {}
void BoardSpaceAttrSet(void) {}
void BoardSpaceCornerPosGet(void) {}
s32 BoardSpaceCountGet(void) { return 0; }
void BoardSpaceDestroy(void) {}
s32 BoardSpaceFlagGet(void) { return 0; }
void BoardSpaceFlagPosGet(void) {}
s32 BoardSpaceFlagSearch(void) { return -1; }
void* BoardSpaceGet(void) { return NULL; }
void BoardSpaceHide(void) {}
void BoardSpaceInit(void) {}
void BoardSpaceLandEventFuncSet(void) {}
s32 BoardSpaceLinkFlagSearch(void) { return -1; }
void BoardSpaceLinkTargetListGet(void) {}
void BoardSpacePosGet(void) {}
void BoardSpaceRead(void) {}
s32 BoardSpaceStarGet(void) { return -1; }
s32 BoardSpaceStarGetRandom(void) { return -1; }
void BoardSpaceStarSetIndex(void) {}
s32 BoardSpaceTypeGet(void) { return 0; }
void BoardSpaceTypeSet(void) {}
void BoardSpaceWalkEventFuncSet(void) {}
void BoardSpaceWalkMiniEventFuncSet(void) {}

/* ========================================================================
 * Board Star
 * ======================================================================== */

s32 BoardStarHostMdlGet(void) { return -1; }
void BoardStarHostSet(void) {}

/* ========================================================================
 * Board Status
 * ======================================================================== */

void BoardStatusCreate(void) {}
void BoardStatusItemSet(void) {}
void BoardStatusKill(void) {}
void BoardStatusPosGet(void) {}
void BoardStatusPosSet(void) {}
void BoardStatusShowSet(void) {}
void BoardStatusShowSetAll(void) {}
void BoardStatusShowSetForce(void) {}
s32 BoardStatusStopCheck(void) { return 0; }
void BoardStatusTargetPosSet(void) {}

/* ========================================================================
 * Board Story / Tutorial
 * ======================================================================== */

void BoardStoryConfigSet(void) {}
void BoardTutorialBlockSetPos(void) {}
void BoardTutorialDirInputSet(void) {}
void BoardTutorialHookSet(void) {}
void BoardTutorialHostSet(void) {}
void BoardTutorialItemSet(void) {}

/* ========================================================================
 * Board Vector / View / Window
 * ======================================================================== */

f32 BoardVecDistXZCalc(void) { return 0.0f; }
s32 BoardVecMaxDistXZCheck(void) { return 0; }
s32 BoardVecMinDistCheck(void) { return 0; }
void BoardViewFocusGetPos(void) {}
void BoardViewFocusSet(void) {}
void BoardViewMapExec(void) {}
s32 BoardViewMoveCheck(void) { return 0; }
void BoardViewMoveEnd(void) {}
void BoardWinAttrSet(void) {}
void BoardWinChoiceDisable(void) {}
s32 BoardWinChoiceGet(void) { return 0; }
s32 BoardWinCreate(void) { return 0; }
s32 BoardWinCreateChoice(void) { return 0; }
void BoardWinInsertMesSet(void) {}
void BoardWinKill(void) {}
void BoardWinPlayerSet(void) {}
void BoardWinWait(void) {}

/* ========================================================================
 * Math helpers
 * ======================================================================== */

f64 fabs2(f64 x) {
    return fabs(x);
}

f32 LinearInterpolation(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

/* ========================================================================
 * Unnamed decomp symbols (fn_* / lbl_*)
 * ======================================================================== */

void fn_1_4C74_inline(void) {}
void fn_1_52B8_inline(void) {}
s32 fn_8006DDE8(void) { return 0; }

/* ========================================================================
 * Misc game functions
 * ======================================================================== */

s32 get_current_board(void) { return 0; }

/* ========================================================================
 * GX Graphics stubs
 * ======================================================================== */

void GXInitSpecularDir(void) {}
void GXResetWriteGatherPipe(void) {}
void GXSetTevIndWarp(void) {}
u16 GXUnknownu16 = 0;
void GXWaitDrawDone(void) {}

/* ========================================================================
 * Hu3D Light stubs
 * ======================================================================== */

void Hu3DLightColorSet(void) {}
s32 Hu3DLightCreateV(void) { return 0; }
void Hu3DLightInfinitytSet(void) {}
void Hu3DLightPointSet(void) {}
void Hu3DLightPosAimSetV(void) {}
void Hu3DLightPosSet(void) {}
void Hu3DLightPosSetV(void) {}
void Hu3DLightSpotSet(void) {}
void Hu3DLightStaticSet(void) {}

/* ========================================================================
 * HuAMem / HuAR (ARAM) stubs
 * ======================================================================== */

void HuAMemDump(void) {}

/*
 * PC ARAM replacement: bypass ARAM entirely, load files directly from disk.
 * HuAR_DVDtoARAM "loads" a directory — on PC just record that it's available.
 * HuAR_ARAMtoMRAMFileRead loads a specific file — on PC use HuDataSelHeapReadNum.
 */
#include "game/data.h"
#include "game/memory.h"

/*
 * On PC there is no ARAM. HuARDirCheck always returns 0 so
 * HuDataDirReadNum always takes the direct DVD path instead of
 * the ARAM copy-back path. The other functions are harmless no-ops.
 */
u32 HuAR_DVDtoARAM(u32 dir) { (void)dir; return 1; }
void *HuAR_ARAMtoMRAMFileRead(u32 dir, u32 num, s32 heap) {
    return HuDataSelHeapReadNum(dir, num, heap);
}
u32 HuAR_MRAMtoARAM(s32 dir) { (void)dir; return 1; }
void HuAR_ARAMtoMRAM(u32 src) { (void)src; }
void *HuAR_ARAMtoMRAMNum(u32 src, s32 num) { (void)src; (void)num; return NULL; }
u32 HuARDirCheck(u32 dir) { (void)dir; return 0; }
s32 HuARDMACheck(void) { return 0; }
void HuARFree(u32 amemptr) { (void)amemptr; }

/* ========================================================================
 * HuCard (Memory Card) stubs
 * ======================================================================== */

s32 HuCardClose(void) { return 0; }
s32 HuCardCreate(void) { return 0; }
s32 HuCardDelete(void) { return 0; }
s32 HuCardFormat(void) { return 0; }
s32 HuCardFreeSpaceGet(void) { return 0; }
s32 HuCardMount(void) { return 0; }
s32 HuCardOpen(void) { return -1; }
s32 HuCardRead(void) { return 0; }
s32 HuCardSectorSizeGet(void) { return 0; }
s32 HuCardSlotCheck(void) { return 0; }
void HuCardUnMount(void) {}
s32 HuCardWrite(void) { return 0; }

/* ========================================================================
 * HuPerf (Performance Profiling) stubs
 * ======================================================================== */

void HuPerfBegin(void) {}
void HuPerfCreate(void) {}
void HuPerfEnd(void) {}
void HuPerfInit(void) {}
void HuPerfZero(void) {}

/* ========================================================================
 * HuVec helpers
 * ======================================================================== */

void HuSetVecF(void) {}
void HuSubVecF(void) {}

/* ========================================================================
 * HuTHP (Video Playback) stubs
 * ======================================================================== */

void HuTHPClose(void) {}
s32 HuTHPEndCheck(void) { return 1; } /* Video "finished" immediately */
s32 HuTHPFrameGet(void) { return 0; }
void HuTHPRestart(void) {}
s32 HuTHPSprCreate(void) { return 0; }
s32 HuTHPSprCreateVol(void) { return 0; }
void HuTHPStop(void) {}

/* ========================================================================
 * Audio system (msm*) stubs
 * ======================================================================== */

s32 msmSysLoadGroup(void) { return 0; }

/* ========================================================================
 * Setup / Misc stubs
 * ======================================================================== */

void SetBlendMode(void) {}
void SetDominationDataStuff(void) {}
void SetupGX(void) {}

/* ========================================================================
 * VI (Video Interface) stubs
 * ======================================================================== */

/* VISetPostRetraceCallback moved to vi_pc.c */

/* ==============================
 * REL modules missing ObjectSetup
 * ============================== */
void mentDll__ObjectSetup(void) {}
void mstory4Dll__ObjectSetup(void) {}
void safDll__ObjectSetup(void) {}
void w01Dll__ObjectSetup(void) {}
void w02Dll__ObjectSetup(void) {}
void w03Dll__ObjectSetup(void) {}
void w04Dll__ObjectSetup(void) {}
void w05Dll__ObjectSetup(void) {}
void w06Dll__ObjectSetup(void) {}
void w10Dll__ObjectSetup(void) {}
void w20Dll__ObjectSetup(void) {}
void w21Dll__ObjectSetup(void) {}
