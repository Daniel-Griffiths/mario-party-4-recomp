/*
 * PC implementation of gcsetjmp/gclongjmp using ucontext.
 *
 * The game's process system does:
 *   1. gcsetjmp(&proc->jump) - save current context
 *   2. proc->jump.lr = (u32)new_func - set new coroutine entry
 *   3. proc->jump.sp = (u32)stack_top - set new stack
 *   4. gclongjmp(&proc->jump, 1) - start/resume coroutine
 *
 * If lr changed since the save, we need to create a new coroutine context.
 * If lr hasn't changed, we resume the saved context.
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

/* Return value passed through longjmp */
static volatile s32 g_longjmp_val = 0;
static volatile jmp_buf *g_setjmp_target = NULL;

s32 gcsetjmp(jmp_buf *jump) {
    jump->lr_at_save = jump->lr;
    jump->ctx_valid = 1;

    /* Save the current context */
    if (getcontext(&jump->uctx) == -1) {
        fprintf(stderr, "[JMP] getcontext failed\n");
        return 0;
    }

    /* Check if we're returning from a longjmp */
    if (g_setjmp_target == jump) {
        s32 val = g_longjmp_val;
        g_setjmp_target = NULL;
        return val;
    }

    return 0;
}

/* Coroutine entry wrapper */
static void coroutine_entry(void) {
    /* The game expects lr to be a function pointer: void (*)(void) */
    jmp_buf *jump = (jmp_buf *)g_setjmp_target;
    g_setjmp_target = NULL;

    if (jump && jump->lr) {
        void (*func)(void) = (void (*)(void))(uintptr_t)jump->lr;
        func();
    }

    /* If the function returns, we're stuck - shouldn't happen in game code */
    fprintf(stderr, "[JMP] Coroutine function returned unexpectedly!\n");
    abort();
}

s32 gclongjmp(jmp_buf *jump, s32 status) {
    g_longjmp_val = status;
    g_setjmp_target = jump;

    if (jump->lr != jump->lr_at_save && jump->lr != 0) {
        /*
         * lr was changed - this means we need to start a new coroutine.
         * Create a new context with makecontext.
         */
        ucontext_t new_ctx;
        getcontext(&new_ctx);

        /* Allocate stack for the new coroutine */
        void *stack;
        if (jump->sp) {
            /*
             * The game sets sp to a stack it allocated.
             * On PPC, sp points to the top of the stack (grows down).
             * We use it directly as the stack base but allocate our own
             * since ucontext needs a contiguous region we control.
             */
            stack = malloc(COROUTINE_STACK_SIZE);
        } else {
            stack = malloc(COROUTINE_STACK_SIZE);
        }

        new_ctx.uc_stack.ss_sp = stack;
        new_ctx.uc_stack.ss_size = COROUTINE_STACK_SIZE;
        new_ctx.uc_stack.ss_flags = 0;
        new_ctx.uc_link = NULL;

        makecontext(&new_ctx, coroutine_entry, 0);
        jump->uctx = new_ctx;
        jump->ctx_valid = 1;
        jump->lr_at_save = jump->lr;
    }

    if (jump->ctx_valid) {
        setcontext(&jump->uctx);
        /* Should not return */
        fprintf(stderr, "[JMP] setcontext failed!\n");
        abort();
    }

    return 0;
}
