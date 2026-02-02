/*
 * Audio Interface stubs
 */
#include "dolphin/types.h"
#include "dolphin/ai.h"

AIDCallback AIRegisterDMACallback(AIDCallback callback) { (void)callback; return NULL; }
void AIInitDMA(u32 start_addr, u32 length) { (void)start_addr; (void)length; }
BOOL AIGetDMAEnableFlag(void) { return FALSE; }
void AIStartDMA(void) {}
void AIStopDMA(void) {}
u32 AIGetDMABytesLeft(void) { return 0; }
u32 AIGetDMAStartAddr(void) { return 0; }
u32 AIGetDMALength(void) { return 0; }
u32 AIGetDSPSampleRate(void) { return 1; }
void AISetDSPSampleRate(u32 rate) { (void)rate; }
AISCallback AIRegisterStreamCallback(AISCallback callback) { (void)callback; return NULL; }
u32 AIGetStreamSampleCount(void) { return 0; }
void AIResetStreamSampleCount(void) {}
void AISetStreamTrigger(u32 trigger) { (void)trigger; }
u32 AIGetStreamTrigger(void) { return 0; }
void AISetStreamPlayState(u32 state) { (void)state; }
u32 AIGetStreamPlayState(void) { return 0; }
void AISetStreamSampleRate(u32 rate) { (void)rate; }
u32 AIGetStreamSampleRate(void) { return 0; }
void AISetStreamVolLeft(u8 vol) { (void)vol; }
void AISetStreamVolRight(u8 vol) { (void)vol; }
u8 AIGetStreamVolLeft(void) { return 0; }
u8 AIGetStreamVolRight(void) { return 0; }
void AIInit(u8 *stack) { (void)stack; }
BOOL AICheckInit(void) { return TRUE; }
void AIReset(void) {}
