/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * GC related routines
 *
 */
#include <windows.h>
#include "primpl.h"

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np) 
{
#if defined(_X86_)
    CONTEXT context;
    context.ContextFlags = CONTEXT_INTEGER;

    if (_PR_IS_NATIVE_THREAD(t)) {
        context.ContextFlags |= CONTEXT_CONTROL;
        if (GetThreadContext(t->md.handle, &context)) {
            t->md.gcContext[0] = context.Eax;
            t->md.gcContext[1] = context.Ebx;
            t->md.gcContext[2] = context.Ecx;
            t->md.gcContext[3] = context.Edx;
            t->md.gcContext[4] = context.Esi;
            t->md.gcContext[5] = context.Edi;
            t->md.gcContext[6] = context.Esp;
            t->md.gcContext[7] = context.Ebp;
            *np = PR_NUM_GCREGS;
        } else {
            PR_ASSERT(0);/* XXX */
        }
    } else {
        /* WARNING WARNING WARNING WARNING WARNING WARNING WARNING
         *
         * This code is extremely machine dependant and completely 
         * undocumented by MS.  Its only known to work experimentally.  
         * Ready for a walk on the wild * side?
         *
         * WARNING WARNING WARNING WARNING WARNING WARNING WARNING */

#if !defined WIN95 // Win95 does not have fibers
        int *fiberData = t->md.fiber_id;

        /* I found these offsets by disassembling SwitchToFiber().
         * Are your palms sweating yet?
         */

        /* 
        ** EAX is on the stack (ESP+0)
        ** EDX is on the stack (ESP+4)
        ** ECX is on the stack (ESP+8)
        */
        t->md.gcContext[0] = 0;                /* context.Eax */
        t->md.gcContext[1] = fiberData[0x2e];  /* context.Ebx */
        t->md.gcContext[2] = 0;                /* context.Ecx */
        t->md.gcContext[2] = 0;                /* context.Edx */
        t->md.gcContext[4] = fiberData[0x2d];  /* context.Esi */
        t->md.gcContext[5] = fiberData[0x2c];  /* context.Edi */
        t->md.gcContext[6] = fiberData[0x36];  /* context.Esp */
        t->md.gcContext[7] = fiberData[0x32];  /* context.Ebp */
        *np = PR_NUM_GCREGS;
#endif
    }
    return (PRWord *)&t->md.gcContext;
#elif defined(_ALPHA_)
#endif /* defined(_X86_) */
}

/* This function is not used right now, but is left as a reference.
 * If you ever need to get the fiberID from the currently running fiber, 
 * this is it.
 */
void *
GetMyFiberID()
{
#if defined(_X86_)
    void *fiberData;

    /* A pointer to our tib entry is found at FS:[18]
     * At offset 10h is the fiberData pointer.  The context of the 
     * fiber is stored in there.  
     */
    __asm {
        mov    EDX, FS:[18h]
        mov    EAX, DWORD PTR [EDX+10h]
        mov    [fiberData], EAX
    }
  
    return fiberData;
#elif defined(_ALPHA_)
#endif /* defined(_X86_) */
}
