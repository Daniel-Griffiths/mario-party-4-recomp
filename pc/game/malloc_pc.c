/*
 * PC replacement for src/game/malloc.c
 * Replaces PPC asm (mflr) with __builtin_return_address
 */
#include "game/memory.h"
#include "game/init.h"
#include "dolphin/os.h"

static u32 HeapSizeTbl[HEAP_MAX] = { 0x240000, 0x140000, 0xA80000, 0x580000, 0 };
static void *HeapTbl[HEAP_MAX];

void HuMemInitAll(void)
{
    s32 i;
    void *ptr;
    u32 free_size;
    for (i = 0; i < 4; i++) {
        ptr = OSAlloc(HeapSizeTbl[i]);
        if (ptr == NULL) {
            OSReport("HuMem> Failed OSAlloc Size:%d\n", HeapSizeTbl[i]);
            return;
        }
        HeapTbl[i] = HuMemInit(ptr, HeapSizeTbl[i]);
    }
    free_size = OSCheckHeap(currentHeapHandle);
    OSReport("HuMem> left memory space %dKB(%d)\n", free_size / 1024, free_size);
    ptr = OSAlloc(free_size);
    if (ptr == NULL) {
        OSReport("HuMem> Failed OSAlloc left space\n");
        return;
    }
    HeapTbl[4] = HuMemInit(ptr, free_size);
    HeapSizeTbl[4] = free_size;
}

void *HuMemInit(void *ptr, s32 size)
{
    return HuMemHeapInit(ptr, size);
}

void HuMemDCFlushAll(void)
{
    HuMemDCFlush(2);
    HuMemDCFlush(0);
}

void HuMemDCFlush(HeapID heap)
{
    DCFlushRangeNoSync(HeapTbl[heap], HeapSizeTbl[heap]);
}

void *HuMemDirectMalloc(HeapID heap, s32 size)
{
    u32 retaddr = (u32)(uintptr_t)__builtin_return_address(0);
    if (heap >= HEAP_MAX || !HeapTbl[heap]) {
        OSReport("HuMem>DirectMalloc NULL heap %d size=%d call=%08x\n", heap, size, retaddr);
        return NULL;
    }
    size = (size + 31) & 0xFFFFFFE0;
    return HuMemMemoryAlloc(HeapTbl[heap], size, retaddr);
}

void *HuMemDirectMallocNum(HeapID heap, s32 size, u32 num)
{
    u32 retaddr = (u32)(uintptr_t)__builtin_return_address(0);
    if (heap >= HEAP_MAX || !HeapTbl[heap]) {
        OSReport("HuMem>DirectMallocNum NULL heap %d size=%d num=%08x call=%08x\n", heap, size, num, retaddr);
        return NULL;
    }
    size = (size + 31) & 0xFFFFFFE0;
    return HuMemMemoryAllocNum(HeapTbl[heap], size, num, retaddr);
}

void HuMemDirectFree(void *ptr)
{
    u32 retaddr = (u32)(uintptr_t)__builtin_return_address(0);
    HuMemMemoryFree(ptr, retaddr);
}

void HuMemDirectFreeNum(HeapID heap, u32 num)
{
    u32 retaddr = (u32)(uintptr_t)__builtin_return_address(0);
    HuMemMemoryFreeNum(HeapTbl[heap], num, retaddr);
}

s32 HuMemUsedMallocSizeGet(HeapID heap)
{
    return HuMemUsedMemorySizeGet(HeapTbl[heap]);
}

s32 HuMemUsedMallocBlockGet(HeapID heap)
{
    return HuMemUsedMemoryBlockGet(HeapTbl[heap]);
}

u32 HuMemHeapSizeGet(HeapID heap)
{
    return HeapSizeTbl[heap];
}

void *HuMemHeapPtrGet(HeapID heap)
{
    return HeapTbl[heap];
}
