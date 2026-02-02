/*
 * PC implementation of gclongjmp (the resume/start half of the coroutine system).
 *
 * gcsetjmp is a macro in jmp_pc.h that expands to sigsetjmp.
 * gclongjmp either:
 *   - Starts a new coroutine (lr changed) via makecontext/setcontext
 *   - Resumes a saved coroutine (lr unchanged) via siglongjmp
 */
#ifdef __APPLE__
#define _XOPEN_SOURCE
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>

#include "game/jmp.h"

/* Stack size for coroutines */
#define COROUTINE_STACK_SIZE (256 * 1024)

/* Global to pass the jump pointer to coroutine_entry */
static volatile jmp_buf *g_new_coroutine = NULL;

/* Coroutine entry wrapper - called via makecontext for new coroutines */
static void coroutine_entry(void) {
    jmp_buf *jump = (jmp_buf *)g_new_coroutine;
    g_new_coroutine = NULL;

    if (jump && jump->lr) {
        void (*func)(void) = (void (*)(void))(uintptr_t)jump->lr;
        func();
    }

    /* If the function returns, we're stuck - shouldn't happen in game code */
    fprintf(stderr, "[JMP] Coroutine function returned unexpectedly!\n");
    abort();
}

s32 gclongjmp(jmp_buf *jump, s32 status) {
    if (jump->lr != jump->lr_at_save && jump->lr != 0) {
        /*
         * lr was changed - start a new coroutine.
         * Use makecontext/setcontext to create and switch to it.
         */
        g_new_coroutine = jump;

        ucontext_t new_ctx;
        getcontext(&new_ctx);

        void *stack = malloc(COROUTINE_STACK_SIZE);
        new_ctx.uc_stack.ss_sp = stack;
        new_ctx.uc_stack.ss_size = COROUTINE_STACK_SIZE;
        new_ctx.uc_stack.ss_flags = 0;
        new_ctx.uc_link = NULL;

        makecontext(&new_ctx, coroutine_entry, 0);
        jump->lr_at_save = jump->lr;
        jump->ctx_valid = 0; /* no sigsetjmp save yet for this new context */

        /* Switch to the new coroutine - does not return */
        setcontext(&new_ctx);

        fprintf(stderr, "[JMP] setcontext failed!\n");
        abort();
    }

    if (jump->ctx_valid) {
        /* Resume a previously saved context via siglongjmp */
        siglongjmp(jump->sjbuf, status ? status : 1);
        /* Should not return */
    }

    fprintf(stderr, "[JMP] gclongjmp: no valid context to resume (lr=%p, lr_at_save=%p, ctx_valid=%d)!\n",
            (void *)jump->lr, (void *)jump->lr_at_save, jump->ctx_valid);
    return 0;
}
