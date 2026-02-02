/*
 * DSP stubs
 */
#include "dolphin/types.h"
#include "dolphin/dsp.h"

void DSPInit(void) {}
void DSPReset(void) {}
void DSPHalt(void) {}
void DSPSendMailToDSP(u32 mail) { (void)mail; }
u32 DSPCheckMailToDSP(void) { return 0; }
u32 DSPCheckMailFromDSP(void) { return 0; }
u32 DSPGetDMAStatus(void) { return 0; }
DSPTaskInfo *DSPAddTask(DSPTaskInfo *task) { (void)task; return NULL; }
void __DSP_exec_task(DSPTaskInfo *curr, DSPTaskInfo *next) { (void)curr; (void)next; }
void __DSP_boot_task(DSPTaskInfo *task) { (void)task; }
void __DSP_insert_task(DSPTaskInfo *task) { (void)task; }
void __DSP_remove_task(DSPTaskInfo *task) { (void)task; }
void __DSP_add_task(DSPTaskInfo *task) { (void)task; }
void __DSP_debug_printf(const char *fmt, ...) { (void)fmt; }
