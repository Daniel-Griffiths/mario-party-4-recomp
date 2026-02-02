/*
 * Serial Interface stubs
 */
#include "dolphin/types.h"
#include "dolphin/os/OSSerial.h"

u32 SIProbe(s32 chan) { (void)chan; return 0x09000000u; /* Standard controller */ }
char *SIGetTypeString(u32 type) { (void)type; return "Standard Controller"; }
void SIRefreshSamplingRate(void) {}
void SISetSamplingRate(u32 msec) { (void)msec; }
