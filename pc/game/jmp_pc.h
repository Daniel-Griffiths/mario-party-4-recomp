#ifndef _GAME_JMP_PC_H
#define _GAME_JMP_PC_H

/*
 * PC replacement for the PPC jmp_buf / gcsetjmp / gclongjmp system.
 *
 * The game's process scheduler manipulates lr (return address) and sp (stack pointer)
 * directly in the jmp_buf to create coroutines. On PC we use a combination of
 * sigsetjmp/siglongjmp (for resuming) and ucontext (for starting new coroutines).
 *
 * CRITICAL: gcsetjmp MUST be a macro (not a function) so that sigsetjmp executes
 * in the caller's stack frame. The C standard requires the function containing
 * setjmp/sigsetjmp to still be active when longjmp/siglongjmp is called.
 *
 * We forward-declare sigsetjmp/siglongjmp instead of including <setjmp.h>
 * to avoid a naming conflict between the standard library's jmp_buf typedef
 * and the game's jmp_buf typedef.
 */
#include "dolphin/types.h"

#ifdef __APPLE__
#define _XOPEN_SOURCE
#endif
#include <ucontext.h>

/* Forward-declare sigsetjmp/siglongjmp to avoid including <setjmp.h>.
 * On macOS ARM64, sigjmp_buf is int[49] (196 bytes). */
#define _GAME_SIGJMP_BUF_INTS 49
typedef int game_sigjmp_buf[_GAME_SIGJMP_BUF_INTS];
int sigsetjmp(game_sigjmp_buf, int);
void siglongjmp(game_sigjmp_buf, int) __attribute__((noreturn));

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

    /* PC-specific fields */
    ucontext_t uctx;           /* ucontext for starting new coroutines */
    uintptr_t lr_at_save;     /* lr value when gcsetjmp was called */
    int ctx_valid;             /* 1 if sjbuf contains a valid sigsetjmp save */
    game_sigjmp_buf sjbuf;    /* sigsetjmp save buffer */
} jmp_buf;

/*
 * gcsetjmp - save context for later resume via gclongjmp.
 * Must be a macro so sigsetjmp runs in the caller's frame.
 * Returns 0 on first call, non-zero status from gclongjmp on resume.
 */
#define gcsetjmp(jump) \
    ((jump)->lr_at_save = (jump)->lr, \
     (jump)->ctx_valid = 1, \
     sigsetjmp((jump)->sjbuf, 0))

s32 gclongjmp(jmp_buf *jump, s32 status);

#endif /* _GAME_JMP_PC_H */
