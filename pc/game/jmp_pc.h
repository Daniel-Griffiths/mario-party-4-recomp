#ifndef _GAME_JMP_PC_H
#define _GAME_JMP_PC_H

/*
 * PC replacement for the PPC jmp_buf / gcsetjmp / gclongjmp system.
 *
 * The game's process scheduler manipulates lr (return address) and sp (stack pointer)
 * directly in the jmp_buf to create coroutines. On PC we use ucontext_t for this.
 *
 * We keep the lr/sp fields for API compatibility - game code reads/writes them.
 * The actual context switching is done via ucontext.
 */
#include "dolphin/types.h"

#ifdef __APPLE__
#define _XOPEN_SOURCE
#endif
#include <ucontext.h>

typedef struct jump_buf {
    /* Fields the game code directly accesses.
     * On GC these are u32, but on PC (64-bit) lr/sp hold pointers. */
    uintptr_t lr;       /* "return address" / function pointer */
    u32 cr;             /* condition register (unused on PC) */
    uintptr_t sp;       /* stack pointer */
    u32 r2;             /* r2 (unused on PC) */
    u32 pad;            /* padding */
    u32 regs[19];       /* GPRs (unused on PC) */
    double flt_regs[19]; /* FPRs (unused on PC) */

    /* PC-specific: ucontext for actual context switching */
    ucontext_t uctx;
    uintptr_t lr_at_save;  /* lr value when gcsetjmp was called */
    int ctx_valid;         /* 1 if uctx is valid */
} jmp_buf;

s32 gcsetjmp(jmp_buf *jump);
s32 gclongjmp(jmp_buf *jump, s32 status);

#endif /* _GAME_JMP_PC_H */
