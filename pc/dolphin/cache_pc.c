/*
 * Cache operations - all no-ops on PC
 */
#include "dolphin/types.h"
#include "dolphin/os/OSCache.h"

void DCInvalidateRange(void *addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCFlushRange(void *addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCStoreRange(void *addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCFlushRangeNoSync(void *addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCStoreRangeNoSync(void *addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCZeroRange(void *addr, u32 nBytes) { (void)addr; (void)nBytes; }
void DCTouchRange(void *addr, u32 nBytes) { (void)addr; (void)nBytes; }

void ICInvalidateRange(void *addr, u32 nBytes) { (void)addr; (void)nBytes; }
void ICFlashInvalidate(void) {}
void ICEnable(void) {}
void ICDisable(void) {}
void ICFreeze(void) {}
void ICUnfreeze(void) {}
void ICBlockInvalidate(void *addr) { (void)addr; }
void ICSync(void) {}

/* Locked cache stubs */
void LCEnable(void) {}
void LCDisable(void) {}
void LCLoadBlocks(void *destTag, void *srcAddr, u32 numBlocks) {
    (void)destTag; (void)srcAddr; (void)numBlocks;
}
void LCStoreBlocks(void *destAddr, void *srcTag, u32 numBlocks) {
    (void)destAddr; (void)srcTag; (void)numBlocks;
}
u32 LCLoadData(void *destAddr, void *srcAddr, u32 nBytes) {
    (void)destAddr; (void)srcAddr; (void)nBytes; return 0;
}
u32 LCStoreData(void *destAddr, void *srcAddr, u32 nBytes) {
    (void)destAddr; (void)srcAddr; (void)nBytes; return 0;
}
u32 LCQueueLength(void) { return 0; }
void LCQueueWait(u32 len) { (void)len; }
void LCFlushQueue(void) {}
