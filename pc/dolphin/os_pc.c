/*
 * OS replacement for PC - replaces all of src/dolphin/os/
 * Provides heap management, timing, reporting, and stubs for
 * hardware-specific OS functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "dolphin/types.h"
#include "dolphin/os.h"
#include "dolphin/gx/GXStruct.h"
#include "pc_config.h"

/* ---- Clock globals ---- */
u32 __OSBusClock  = PC_BUS_CLOCK;
u32 __OSCoreClock = PC_CORE_CLOCK;

/* ---- Arena (simulated GC memory layout) ---- */
#define ARENA_SIZE (24 * 1024 * 1024)  /* 24 MB */
static u8 *arena_base = NULL;
static void *arena_lo = NULL;
static void *arena_hi = NULL;

/* ---- Heap management ---- */
#define MAX_HEAPS 16

typedef struct heap_block {
    struct heap_block *next;
    u32 size;
    /* data follows */
} HeapBlock;

typedef struct {
    void *start;
    void *end;
    HeapBlock *free_list;
    HeapBlock *used_list;
} HeapInfo;

static HeapInfo heaps[MAX_HEAPS];
static int num_heaps = 0;
volatile OSHeapHandle __OSCurrHeap = -1;

/* ---- OSInit ---- */
void OSInit(void) {
    printf("[OS] OSInit()\n");
    if (!arena_base) {
        arena_base = (u8 *)malloc(ARENA_SIZE);
        if (!arena_base) {
            fprintf(stderr, "[OS] Failed to allocate arena!\n");
            abort();
        }
        memset(arena_base, 0, ARENA_SIZE);
        arena_lo = arena_base;
        arena_hi = arena_base + ARENA_SIZE;
    }
}

/* ---- Arena ---- */
void *OSGetArenaHi(void) { return arena_hi; }
void *OSGetArenaLo(void) { return arena_lo; }
void OSSetArenaHi(void *addr) { arena_hi = addr; }
void OSSetArenaLo(void *addr) { arena_lo = addr; }

void *OSAllocFromArenaLo(u32 size, u32 align) {
    uintptr_t lo = ((uintptr_t)arena_lo + align - 1) & ~(uintptr_t)(align - 1);
    void *ptr = (void *)lo;
    arena_lo = (void *)(lo + size);
    return ptr;
}

/* ---- Heap allocation ---- */
void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps) {
    (void)maxHeaps;
    num_heaps = 0;
    memset(heaps, 0, sizeof(heaps));
    /* Return advanced arenaStart (space for heap table - we don't need it but game expects advancement) */
    return (void *)((uintptr_t)arenaStart + sizeof(HeapInfo) * MAX_HEAPS);
}

OSHeapHandle OSCreateHeap(void *start, void *end) {
    if (num_heaps >= MAX_HEAPS) return -1;
    int h = num_heaps++;
    heaps[h].start = start;
    heaps[h].end = end;
    heaps[h].free_list = NULL;
    heaps[h].used_list = NULL;
    return h;
}

void OSDestroyHeap(OSHeapHandle heap) {
    if (heap >= 0 && heap < MAX_HEAPS) {
        heaps[heap].start = NULL;
        heaps[heap].end = NULL;
    }
}

OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap) {
    OSHeapHandle old = __OSCurrHeap;
    __OSCurrHeap = heap;
    return old;
}

void OSAddToHeap(OSHeapHandle heap, void *start, void *end) {
    (void)heap; (void)start; (void)end;
}

/* Simple malloc-backed allocation (ignores heap boundaries for PC) */
void *OSAllocFromHeap(OSHeapHandle heap, u32 size) {
    (void)heap;
    void *ptr = malloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

void OSFreeToHeap(OSHeapHandle heap, void *ptr) {
    (void)heap;
    free(ptr);
}

long OSCheckHeap(OSHeapHandle heap) {
    if (heap < 0 || heap >= MAX_HEAPS || !heaps[heap].start) return 0;
    return (long)((uintptr_t)heaps[heap].end - (uintptr_t)heaps[heap].start);
}

void OSDumpHeap(OSHeapHandle heap) {
    printf("[OS] OSDumpHeap(%d)\n", heap);
}

u32 OSReferentSize(void *ptr) {
    (void)ptr;
    return 0;
}

void OSVisitAllocated(void (*visitor)(void *, u32)) {
    (void)visitor;
}

void *OSAllocFixed(void **rstart, void **rend) {
    (void)rstart; (void)rend;
    return NULL;
}

/* ---- Reporting ---- */
void OSReport(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    fflush(stdout);
}

static int panic_count = 0;

void OSPanic(const char *file, int line, const char *msg, ...) {
    va_list args;
    fprintf(stderr, "[PANIC] %s:%d: ", file, line);
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    panic_count++;
    if (panic_count > 200) {
        fprintf(stderr, "[PC] Too many panics (%d), aborting.\n", panic_count);
        abort();
    }
    /* On PC, continue execution instead of aborting.
     * Missing data files will produce panics but the game loop can still run. */
}

void OSFatal(GXColor fg, GXColor bg, const char *msg) {
    (void)fg; (void)bg;
    fprintf(stderr, "[FATAL] %s\n", msg);
    abort();
}

/* ---- Time ---- */
static struct timespec g_start_time;
static int g_time_inited = 0;

static void ensure_time_init(void) {
    if (!g_time_inited) {
        clock_gettime(CLOCK_MONOTONIC, &g_start_time);
        g_time_inited = 1;
    }
}

OSTime OSGetTime(void) {
    ensure_time_init();
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    u64 ns = (u64)(now.tv_sec - g_start_time.tv_sec) * 1000000000ULL +
             (u64)(now.tv_nsec - g_start_time.tv_nsec);
    /* Convert to timer ticks: OS_TIMER_CLOCK ticks per second */
    return (OSTime)(ns * (PC_BUS_CLOCK / 4) / 1000000000ULL);
}

OSTick OSGetTick(void) {
    return (OSTick)(OSGetTime() & 0xFFFFFFFF);
}

OSTime OSCalendarTimeToTicks(OSCalendarTime *td) {
    (void)td;
    return 0;
}

void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime *td) {
    (void)ticks;
    memset(td, 0, sizeof(*td));
}

/* ---- Console info ---- */
u32 OSGetConsoleType(void) {
    return OS_CONSOLE_RETAIL;
}

u32 OSGetPhysicalMemSize(void) {
    return ARENA_SIZE;
}

u32 OSGetConsoleSimulatedMemSize(void) {
    return ARENA_SIZE;
}

/* ---- Sound / display mode ---- */
static u32 g_sound_mode = 1; /* stereo */
static u32 g_progressive = 0;
static u8  g_language = 0; /* English */
static u32 g_eurgb60 = 0;

u32 OSGetSoundMode(void) { return g_sound_mode; }
void OSSetSoundMode(u32 mode) { g_sound_mode = mode; }
u32 OSGetProgressiveMode(void) { return g_progressive; }
void OSSetProgressiveMode(u32 on) { g_progressive = on; }
u8 OSGetLanguage(void) { return g_language; }
void OSSetLanguage(u8 language) { g_language = language; }
u32 OSGetEuRgb60Mode(void) { return g_eurgb60; }
void OSSetEuRgb60Mode(u32 on) { g_eurgb60 = on; }

void OSRegisterVersion(const char *id) {
    printf("[OS] Version: %s\n", id);
}

/* ---- Interrupts (no-ops on PC) ---- */
BOOL OSDisableInterrupts(void) { return 0; }
BOOL OSEnableInterrupts(void) { return 0; }
BOOL OSRestoreInterrupts(BOOL level) { (void)level; return 0; }

/* ---- Module linking (static on PC, no-ops) ---- */
void OSSetStringTable(const void *stringTable) { (void)stringTable; }
BOOL OSLink(OSModuleInfo *newModule, void *bss) { (void)newModule; (void)bss; return TRUE; }
BOOL OSLinkFixed(OSModuleInfo *newModule, void *bss) { (void)newModule; (void)bss; return TRUE; }
BOOL OSUnlink(OSModuleInfo *oldModule) { (void)oldModule; return TRUE; }
OSModuleInfo *OSSearchModule(void *ptr, u32 *section, u32 *offset) {
    (void)ptr; (void)section; (void)offset;
    return NULL;
}
void OSNotifyLink(void) {}
void OSNotifyUnlink(void) {}

/* ---- Stopwatch (simple stubs) ---- */
void OSInitStopwatch(OSStopwatch *sw, char *name) {
    memset(sw, 0, sizeof(*sw));
    sw->name = name;
}
void OSStartStopwatch(OSStopwatch *sw) { sw->running = TRUE; sw->last = OSGetTime(); }
void OSStopStopwatch(OSStopwatch *sw) {
    if (sw->running) {
        OSTime elapsed = OSGetTime() - sw->last;
        sw->total += elapsed;
        sw->hits++;
        sw->running = FALSE;
    }
}
OSTime OSCheckStopwatch(OSStopwatch *sw) { return sw->total; }
void OSResetStopwatch(OSStopwatch *sw) { sw->total = 0; sw->hits = 0; }
void OSDumpStopwatch(OSStopwatch *sw) { printf("[OS] Stopwatch '%s': %lld\n", sw->name, (long long)sw->total); }

/* ---- Thread stubs ---- */
static OSThread g_main_thread;
void OSInitThreadQueue(OSThreadQueue *queue) { if (queue) { queue->head = NULL; queue->tail = NULL; } }
OSThread *OSGetCurrentThread(void) { return &g_main_thread; }
BOOL OSIsThreadSuspended(OSThread *thread) { (void)thread; return FALSE; }
BOOL OSIsThreadTerminated(OSThread *thread) { (void)thread; return FALSE; }
s32 OSDisableScheduler(void) { return 0; }
s32 OSEnableScheduler(void) { return 0; }
void OSYieldThread(void) {}
BOOL OSCreateThread(OSThread *thread, void *(*func)(void *), void *param,
                    void *stack, u32 stackSize, OSPriority priority, u16 attr) {
    (void)thread; (void)func; (void)param; (void)stack;
    (void)stackSize; (void)priority; (void)attr;
    return TRUE;
}
void OSExitThread(void *val) { (void)val; }
void OSCancelThread(OSThread *thread) { (void)thread; }
BOOL OSJoinThread(OSThread *thread, void **val) { (void)thread; (void)val; return TRUE; }
void OSDetachThread(OSThread *thread) { (void)thread; }
s32 OSResumeThread(OSThread *thread) { (void)thread; return 0; }
s32 OSSuspendThread(OSThread *thread) { (void)thread; return 0; }
BOOL OSSetThreadPriority(OSThread *thread, OSPriority priority) { (void)thread; (void)priority; return TRUE; }
OSPriority OSGetThreadPriority(OSThread *thread) { (void)thread; return 16; }
void OSSleepThread(OSThreadQueue *queue) { (void)queue; }
void OSWakeupThread(OSThreadQueue *queue) { (void)queue; }
OSThread *OSSetIdleFunction(void (*idleFunction)(void *), void *param, void *stack, u32 stackSize) {
    (void)idleFunction; (void)param; (void)stack; (void)stackSize;
    return NULL;
}

/* ---- Context stubs ---- */
u32 OSGetStackPointer(void) { return 0; }
void OSDumpContext(OSContext *context) { (void)context; }
u32 OSSaveContext(OSContext *context) { (void)context; return 0; }
void OSLoadContext(OSContext *context) { (void)context; }
void OSClearContext(OSContext *context) { if (context) memset(context, 0, sizeof(*context)); }
static OSContext g_current_context;
OSContext *OSGetCurrentContext(void) { return &g_current_context; }
void OSSetCurrentContext(OSContext *context) { (void)context; }
void OSSaveFPUContext(OSContext *context) { (void)context; }
void OSInitContext(OSContext *context, u32 pc, u32 newsp) { (void)context; (void)pc; (void)newsp; }

/* ---- Error handler stubs ---- */
OSErrorHandler OSSetErrorHandler(OSError code, OSErrorHandler handler) {
    (void)code; (void)handler;
    return NULL;
}

/* ---- Interrupt handler stubs ---- */
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler) {
    (void)interrupt; (void)handler;
    return NULL;
}
__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt) {
    (void)interrupt;
    return NULL;
}
void __OSDispatchInterrupt(__OSException exception, OSContext *context) {
    (void)exception; (void)context;
}
u32 OSGetInterruptMask(void) { return 0; }
u32 OSSetInterruptMask(u32 mask) { (void)mask; return 0; }
u32 __OSMaskInterrupts(u32 mask) { (void)mask; return 0; }
u32 __OSUnmaskInterrupts(u32 mask) { (void)mask; return 0; }

/* ---- Reset stubs ---- */
void OSRegisterResetFunction(OSResetFunctionInfo *info) { (void)info; }
u32 OSGetResetCode(void) { return 0; }
BOOL OSGetResetButtonState(void) { return FALSE; }
BOOL OSGetResetSwitchState(void) { return FALSE; }
OSResetCallback OSSetResetCallback(OSResetCallback callback) { (void)callback; return NULL; }

/* ---- Message queue stubs ---- */
void OSInitMessageQueue(OSMessageQueue *mq, OSMessage *msgArray, s32 msgCount) {
    (void)mq; (void)msgArray; (void)msgCount;
}
BOOL OSSendMessage(OSMessageQueue *mq, OSMessage msg, s32 flags) {
    (void)mq; (void)msg; (void)flags;
    return TRUE;
}
BOOL OSJamMessage(OSMessageQueue *mq, OSMessage msg, s32 flags) {
    (void)mq; (void)msg; (void)flags;
    return TRUE;
}
BOOL OSReceiveMessage(OSMessageQueue *mq, OSMessage *msg, s32 flags) {
    (void)mq; (void)msg; (void)flags;
    return FALSE;
}

/* ---- Mutex stubs ---- */
void OSInitMutex(OSMutex *mutex) { (void)mutex; }
void OSLockMutex(OSMutex *mutex) { (void)mutex; }
void OSUnlockMutex(OSMutex *mutex) { (void)mutex; }
BOOL OSTryLockMutex(OSMutex *mutex) { (void)mutex; return TRUE; }
void OSInitCond(OSCond *cond) { (void)cond; }
void OSWaitCond(OSCond *cond, OSMutex *mutex) { (void)cond; (void)mutex; }
void OSSignalCond(OSCond *cond) { (void)cond; }

/* ---- Alarm stubs ---- */
void OSInitAlarm(void) {}
void OSCreateAlarm(OSAlarm *alarm) { (void)alarm; }
void OSSetAlarm(OSAlarm *alarm, OSTime tick, void (*handler)(OSAlarm *, OSContext *)) {
    (void)alarm; (void)tick; (void)handler;
}
void OSSetPeriodicAlarm(OSAlarm *alarm, OSTime start, OSTime period, void (*handler)(OSAlarm *, OSContext *)) {
    (void)alarm; (void)start; (void)period; (void)handler;
}
void OSCancelAlarm(OSAlarm *alarm) { (void)alarm; }
void OSCancelAlarms(u32 tag) { (void)tag; }

/* ---- Font stubs ---- */
u16 OSGetFontEncode(void) { return 0; }
BOOL OSInitFont(OSFontHeader *fontData) { (void)fontData; return TRUE; }
u32 OSLoadFont(OSFontHeader *fontData, void *temp) { (void)fontData; (void)temp; return 0; }
char *OSGetFontTexture(char *string, void **image, s32 *x, s32 *y, s32 *width) {
    (void)string; (void)image; (void)x; (void)y; (void)width;
    return NULL;
}
char *OSGetFontWidth(char *string, s32 *width) {
    (void)string; (void)width;
    return NULL;
}
char *OSGetFontTexel(char *string, void *image, s32 pos, s32 stride, s32 *width) {
    (void)string; (void)image; (void)pos; (void)stride; (void)width;
    return NULL;
}

/* ---- Memory protection (no-op) ---- */
void OSProtectRange(u32 chan, void *addr, u32 nBytes, u32 control) {
    (void)chan; (void)addr; (void)nBytes; (void)control;
}

/* ---- PPC arch stubs ---- */
u32 PPCMfmsr(void) { return 0; }
void PPCMtmsr(u32 newMSR) { (void)newMSR; }
u32 PPCOrMsr(u32 value) { (void)value; return 0; }
u32 PPCMfhid0(void) { return 0; }
void PPCMthid0(u32 newHID0) { (void)newHID0; }
u32 PPCMfl2cr(void) { return 0; }
void PPCMtl2cr(u32 newL2cr) { (void)newL2cr; }
void PPCMtdec(u32 newDec) { (void)newDec; }
void PPCSync(void) {}
void PPCHalt(void) { abort(); }
u32 PPCMffpscr(void) { return 0; }
void PPCMtfpscr(u32 newFPSCR) { (void)newFPSCR; }
u32 PPCMfhid2(void) { return 0; }
void PPCMthid2(u32 newhid2) { (void)newhid2; }
u32 PPCMfwpar(void) { return 0; }
void PPCMtwpar(u32 newwpar) { (void)newwpar; }
void PPCEnableSpeculation(void) {}
void PPCDisableSpeculation(void) {}
void PPCSetFpIEEEMode(void) {}
void PPCSetFpNonIEEEMode(void) {}

/* ---- DB stubs ---- */
void DBInit(void) {}
void DBInitComm(int *inputFlagPtr, int *mtrCallback) { (void)inputFlagPtr; (void)mtrCallback; }
void DBPrintf(char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/* ---- Exception stubs ---- */
void *OSSetExceptionHandler(u32 exception, void *handler) {
    (void)exception; (void)handler;
    return NULL;
}
