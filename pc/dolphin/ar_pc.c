/*
 * ARAM (Auxiliary RAM) stubs
 */
#include <stdlib.h>
#include <string.h>
#include "dolphin/types.h"
#include "dolphin/ar.h"
#include "dolphin/arq.h"

/* Simulate a 16MB ARAM */
#define ARAM_SIZE (16 * 1024 * 1024)
static u8 *g_aram = NULL;
static u32 g_aram_alloc_ptr = 0;

ARCallback ARRegisterDMACallback(ARCallback callback) { (void)callback; return NULL; }
u32 ARGetDMAStatus(void) { return 0; }

void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length) {
    if (!g_aram) return;
    if (type == ARAM_DIR_MRAM_TO_ARAM) {
        if (aram_addr + length <= ARAM_SIZE)
            memcpy(g_aram + aram_addr, (void *)(uintptr_t)mainmem_addr, length);
    } else {
        if (aram_addr + length <= ARAM_SIZE)
            memcpy((void *)(uintptr_t)mainmem_addr, g_aram + aram_addr, length);
    }
}

u32 ARInit(u32 *stack_index_addr, u32 num_entries) {
    (void)stack_index_addr; (void)num_entries;
    if (!g_aram) {
        g_aram = (u8 *)calloc(1, ARAM_SIZE);
    }
    g_aram_alloc_ptr = 0;
    return 0;
}

u32 ARGetBaseAddress(void) { return 0; }
BOOL ARCheckInit(void) { return g_aram != NULL; }
void ARReset(void) { g_aram_alloc_ptr = 0; }

u32 ARAlloc(u32 length) {
    u32 ptr = g_aram_alloc_ptr;
    g_aram_alloc_ptr += (length + 31) & ~31u;
    return ptr;
}

u32 ARFree(u32 *length) { (void)length; return 0; }
u32 ARGetSize(void) { return ARAM_SIZE; }
u32 ARGetInternalSize(void) { return ARAM_SIZE; }
void ARSetSize(void) {}
void ARClear(u32 flag) { (void)flag; if (g_aram) memset(g_aram, 0, ARAM_SIZE); }
void __ARClearInterrupt(void) {}
u16 __ARGetInterruptStatus(void) { return 0; }

/* ARQ stubs */
void ARQInit(void) {}
void ARQReset(void) {}
void ARQPostRequest(ARQRequest *task, u32 owner, u32 type, u32 priority,
                    u32 source, u32 dest, u32 length, ARQCallback callback) {
    (void)task; (void)owner; (void)priority;
    /* Execute DMA synchronously */
    ARStartDMA(type, source, dest, length);
    if (callback) callback((u32)(uintptr_t)task);
}
void ARQRemoveRequest(ARQRequest *task) { (void)task; }
void ARQRemoveOwnerRequest(u32 owner) { (void)owner; }
void ARQFlushQueue(void) {}
void ARQSetChunkSize(u32 size) { (void)size; }
u32 ARQGetChunkSize(void) { return 4096; }
BOOL ARQCheckInit(void) { return TRUE; }
