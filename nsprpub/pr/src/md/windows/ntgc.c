/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#if defined(_X86_) && !defined(__MINGW32__)
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
