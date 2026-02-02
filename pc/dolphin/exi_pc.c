/*
 * EXI (External Interface) stubs
 */
#include "dolphin/types.h"

typedef void (*EXICallback)(s32 chan, void *context);

/* From OSExpansion.h */
BOOL EXIProbe(s32 chan) { (void)chan; return FALSE; }
s32 EXIProbeEx(s32 chan) { (void)chan; return 0; }
s32 EXIGetType(s32 chan, u32 dev, u32 *type) { (void)chan; (void)dev; (void)type; return 0; }
char *EXIGetTypeString(u32 type) { (void)type; return "None"; }
u32 EXIClearInterrupts(s32 chan, BOOL exi, BOOL tc, BOOL ext) {
    (void)chan; (void)exi; (void)tc; (void)ext; return 0;
}
s32 EXIGetID(s32 chan, u32 dev, u32 *id) { (void)chan; (void)dev; (void)id; return 0; }

/* From exi.h */
void EXIInit(void) {}
BOOL EXILock(s32 channel, u32 device, void *callback) { (void)channel; (void)device; (void)callback; return TRUE; }
BOOL EXIUnlock(s32 channel) { (void)channel; return TRUE; }
BOOL EXISelect(s32 channel, u32 device, u32 frequency) { (void)channel; (void)device; (void)frequency; return TRUE; }
BOOL EXIDeselect(s32 channel) { (void)channel; return TRUE; }
BOOL EXIImm(s32 channel, void *buffer, s32 length, u32 type, void *callback) {
    (void)channel; (void)buffer; (void)length; (void)type; (void)callback; return TRUE;
}
BOOL EXIImmEx(s32 channel, void *buffer, s32 length, u32 type) {
    (void)channel; (void)buffer; (void)length; (void)type; return TRUE;
}
BOOL EXIDma(s32 channel, void *buffer, s32 length, u32 type, void *callback) {
    (void)channel; (void)buffer; (void)length; (void)type; (void)callback; return TRUE;
}
BOOL EXISync(s32 channel) { (void)channel; return TRUE; }
BOOL EXIAttach(s32 channel, void *callback) { (void)channel; (void)callback; return TRUE; }
BOOL EXIDetach(s32 channel) { (void)channel; return TRUE; }
u32 EXIGetState(s32 channel) { (void)channel; return 0; }
void EXIProbeReset(void) {}
void *EXISetExiCallback(s32 channel, void *callback) { (void)channel; (void)callback; return NULL; }
