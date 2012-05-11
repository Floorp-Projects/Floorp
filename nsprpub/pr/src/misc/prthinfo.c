/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlog.h"
#include "prthread.h"
#include "private/pprthred.h"
#include "primpl.h"

PR_IMPLEMENT(PRWord *)
PR_GetGCRegisters(PRThread *t, int isCurrent, int *np)
{
    return _MD_HomeGCRegisters(t, isCurrent, np);
}

PR_IMPLEMENT(PRStatus)
PR_ThreadScanStackPointers(PRThread* t,
                           PRScanStackFun scanFun, void* scanClosure)
{
    PRThread* current = PR_GetCurrentThread();
    PRWord *sp, *esp, *p0;
    int n;
    void **ptd;
    PRStatus status;
    PRUint32 index;
    int stack_end;

    /*
    ** Store the thread's registers in the thread structure so the GC
    ** can scan them. Then scan them.
    */
    p0 = _MD_HomeGCRegisters(t, t == current, &n);
    status = scanFun(t, (void**)p0, n, scanClosure);
    if (status != PR_SUCCESS)
        return status;

    /* Scan the C stack for pointers into the GC heap */
#if defined(XP_PC) && defined(WIN16)
    /*
    ** Under WIN16, the stack of the current thread is always mapped into
    ** the "task stack" (at SS:xxxx).  So, if t is the current thread, scan
    ** the "task stack".  Otherwise, scan the "cached stack" of the inactive
    ** thread...
    */
    if (t == current) {
        sp  = (PRWord*) &stack_end;
        esp = (PRWord*) _pr_top_of_task_stack;

        PR_ASSERT(sp <= esp);
    } else {
        sp  = (PRWord*) PR_GetSP(t);
        esp = (PRWord*) t->stack->stackTop;

        PR_ASSERT((t->stack->stackSize == 0) ||
                  ((sp >  (PRWord*)t->stack->stackBottom) &&
                   (sp <= (PRWord*)t->stack->stackTop)));
    }
#else   /* ! WIN16 */
#ifdef HAVE_STACK_GROWING_UP
    if (t == current) {
        esp = (PRWord*) &stack_end;
    } else {
        esp = (PRWord*) PR_GetSP(t);
    }
    sp = (PRWord*) t->stack->stackTop;
    if (t->stack->stackSize) {
        PR_ASSERT((esp > (PRWord*)t->stack->stackTop) &&
                  (esp < (PRWord*)t->stack->stackBottom));
    }
#else   /* ! HAVE_STACK_GROWING_UP */
    if (t == current) {
        sp = (PRWord*) &stack_end;
    } else {
        sp = (PRWord*) PR_GetSP(t);
    }
    esp = (PRWord*) t->stack->stackTop;
    if (t->stack->stackSize) {
        PR_ASSERT((sp > (PRWord*)t->stack->stackBottom) &&
                  (sp < (PRWord*)t->stack->stackTop));
    }
#endif  /* ! HAVE_STACK_GROWING_UP */
#endif  /* ! WIN16 */

#if defined(WIN16)
    {
        prword_t scan;
        prword_t limit;
        
        scan = (prword_t) sp;
        limit = (prword_t) esp;
        while (scan < limit) {
            prword_t *test;

            test = *((prword_t **)scan);
            status = scanFun(t, (void**)&test, 1, scanClosure);
            if (status != PR_SUCCESS)
                return status;
            scan += sizeof(char);
        }
    }
#else
    if (sp < esp) {
        status = scanFun(t, (void**)sp, esp - sp, scanClosure);
        if (status != PR_SUCCESS)
            return status;
    }
#endif

    /*
    ** Mark all of the per-thread-data items attached to this thread
    **
    ** The execution environment better be accounted for otherwise it
    ** will be collected
    */
    status = scanFun(t, (void**)&t->environment, 1, scanClosure);
    if (status != PR_SUCCESS)
        return status;

    /* if thread is not allocated on stack, this is redundant. */
    ptd = t->privateData;
    for (index = 0; index < t->tpdLength; index++, ptd++) {
        status = scanFun(t, (void**)ptd, 1, scanClosure);
        if (status != PR_SUCCESS)
            return status;
    }
    
    return PR_SUCCESS;
}

/* transducer for PR_EnumerateThreads */
typedef struct PRScanStackData {
    PRScanStackFun      scanFun;
    void*               scanClosure;
} PRScanStackData;

static PRStatus PR_CALLBACK
pr_ScanStack(PRThread* t, int i, void* arg)
{
    PRScanStackData* data = (PRScanStackData*)arg;
    return PR_ThreadScanStackPointers(t, data->scanFun, data->scanClosure);
}

PR_IMPLEMENT(PRStatus)
PR_ScanStackPointers(PRScanStackFun scanFun, void* scanClosure)
{
    PRScanStackData data;
    data.scanFun = scanFun;
    data.scanClosure = scanClosure;
    return PR_EnumerateThreads(pr_ScanStack, &data);
}

PR_IMPLEMENT(PRUword)
PR_GetStackSpaceLeft(PRThread* t)
{
    PRThread *current = PR_GetCurrentThread();
    PRWord *sp, *esp;
    int stack_end;

#if defined(WIN16)
    /*
    ** Under WIN16, the stack of the current thread is always mapped into
    ** the "task stack" (at SS:xxxx).  So, if t is the current thread, scan
    ** the "task stack".  Otherwise, scan the "cached stack" of the inactive
    ** thread...
    */
    if (t == current) {
        sp  = (PRWord*) &stack_end;
        esp = (PRWord*) _pr_top_of_task_stack;

        PR_ASSERT(sp <= esp);
    } else {
        sp  = (PRWord*) PR_GetSP(t);
        esp = (PRWord*) t->stack->stackTop;

	PR_ASSERT((t->stack->stackSize == 0) ||
                 ((sp >  (PRWord*)t->stack->stackBottom) &&
		  (sp <= (PRWord*)t->stack->stackTop)));
    }
#else   /* ! WIN16 */
#ifdef HAVE_STACK_GROWING_UP
    if (t == current) {
        esp = (PRWord*) &stack_end;
    } else {
        esp = (PRWord*) PR_GetSP(t);
    }
    sp = (PRWord*) t->stack->stackTop;
    if (t->stack->stackSize) {
        PR_ASSERT((esp > (PRWord*)t->stack->stackTop) &&
                  (esp < (PRWord*)t->stack->stackBottom));
    }
#else   /* ! HAVE_STACK_GROWING_UP */
    if (t == current) {
        sp = (PRWord*) &stack_end;
    } else {
        sp = (PRWord*) PR_GetSP(t);
    }
    esp = (PRWord*) t->stack->stackTop;
    if (t->stack->stackSize) {
	PR_ASSERT((sp > (PRWord*)t->stack->stackBottom) &&
		  (sp < (PRWord*)t->stack->stackTop));
    }
#endif  /* ! HAVE_STACK_GROWING_UP */
#endif  /* ! WIN16 */
    return (PRUword)t->stack->stackSize - ((PRWord)esp - (PRWord)sp);
}
