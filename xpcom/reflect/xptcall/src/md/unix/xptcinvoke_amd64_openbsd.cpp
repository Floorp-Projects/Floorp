/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// Platform specific code to invoke XPCOM methods on native objects

#include "xptcprivate.h"

// 6 integral parameters are passed in registers
const PRUint32 GPR_COUNT = 6;

// 8 floating point parameters are passed in SSE registers
const PRUint32 FPR_COUNT = 8;

// Remember that these 'words' are 64-bit long
static inline void
invoke_count_words(PRUint32 paramCount, nsXPTCVariant * s,
                   PRUint32 & nr_gpr, PRUint32 & nr_fpr, PRUint32 & nr_stack)
{
    nr_gpr = 1; // skip one GP register for 'that'
    nr_fpr = 0;
    nr_stack = 0;

    /* Compute number of eightbytes of class MEMORY.  */
    for (uint32 i = 0; i < paramCount; i++, s++) {
        if (!s->IsPtrData()
            && (s->type == nsXPTType::T_FLOAT || s->type == nsXPTType::T_DOUBLE)) {
            if (nr_fpr < FPR_COUNT)
                nr_fpr++;
            else
                nr_stack++;
        }
        else {
            if (nr_gpr < GPR_COUNT)
                nr_gpr++;
            else
                nr_stack++;
        }
    }
}

static void
invoke_copy_to_stack(PRUint64 * d, PRUint32 paramCount, nsXPTCVariant * s,
                     PRUint64 * gpregs, double * fpregs)
{
    PRUint32 nr_gpr = 1; // skip one GP register for 'that'
    PRUint32 nr_fpr = 0;
    PRUint64 value;

    for (uint32 i = 0; i < paramCount; i++, s++) {
        if (s->IsPtrData())
            value = (PRUint64) s->ptr;
        else {
            switch (s->type) {
            case nsXPTType::T_FLOAT:                                break;
            case nsXPTType::T_DOUBLE:                               break;
            case nsXPTType::T_I8:     value = s->val.i8;            break;
            case nsXPTType::T_I16:    value = s->val.i16;           break;
            case nsXPTType::T_I32:    value = s->val.i32;           break;
            case nsXPTType::T_I64:    value = s->val.i64;           break;
            case nsXPTType::T_U8:     value = s->val.u8;            break;
            case nsXPTType::T_U16:    value = s->val.u16;           break;
            case nsXPTType::T_U32:    value = s->val.u32;           break;
            case nsXPTType::T_U64:    value = s->val.u64;           break;
            case nsXPTType::T_BOOL:   value = s->val.b;             break;
            case nsXPTType::T_CHAR:   value = s->val.c;             break;
            case nsXPTType::T_WCHAR:  value = s->val.wc;            break;
            default:                  value = (PRUint64) s->val.p;  break;
            }
        }

        if (!s->IsPtrData() && s->type == nsXPTType::T_DOUBLE) {
            if (nr_fpr < FPR_COUNT)
                fpregs[nr_fpr++] = s->val.d;
            else {
                *((double *)d) = s->val.d;
                d++;
            }
        }
        else if (!s->IsPtrData() && s->type == nsXPTType::T_FLOAT) {
            if (nr_fpr < FPR_COUNT)
                // The value in %xmm register is already prepared to
                // be retrieved as a float. Therefore, we pass the
                // value verbatim, as a double without conversion.
                fpregs[nr_fpr++] = s->val.d;
            else {
                *((float *)d) = s->val.f;
                d++;
            }
        }
        else {
            if (nr_gpr < GPR_COUNT)
                gpregs[nr_gpr++] = value;
            else
                *d++ = value;
        }
    }
}

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex_P(nsISupports * that, PRUint32 methodIndex,
                 PRUint32 paramCount, nsXPTCVariant * params)
{
    PRUint32 nr_gpr, nr_fpr, nr_stack;
    invoke_count_words(paramCount, params, nr_gpr, nr_fpr, nr_stack);
    
    // Stack, if used, must be 16-bytes aligned
    if (nr_stack)
        nr_stack = (nr_stack + 1) & ~1;

    // Load parameters to stack, if necessary
    PRUint64 *stack = (PRUint64 *) __builtin_alloca(nr_stack * 8);
    PRUint64 gpregs[GPR_COUNT];
    double fpregs[FPR_COUNT];
    invoke_copy_to_stack(stack, paramCount, params, gpregs, fpregs);

    // Load FPR registers from fpregs[]
    register double d0 asm("xmm0");
    register double d1 asm("xmm1");
    register double d2 asm("xmm2");
    register double d3 asm("xmm3");
    register double d4 asm("xmm4");
    register double d5 asm("xmm5");
    register double d6 asm("xmm6");
    register double d7 asm("xmm7");

    switch (nr_fpr) {
#define ARG_FPR(N) \
    case N+1: d##N = fpregs[N];
        ARG_FPR(7);
        ARG_FPR(6);
        ARG_FPR(5);
        ARG_FPR(4);
        ARG_FPR(3);
        ARG_FPR(2);
        ARG_FPR(1);
        ARG_FPR(0);
    case 0:;
#undef ARG_FPR
    }
    
    // Load GPR registers from gpregs[]
    register PRUint64 a0 asm("rdi");
    register PRUint64 a1 asm("rsi");
    register PRUint64 a2 asm("rdx");
    register PRUint64 a3 asm("rcx");
    register PRUint64 a4 asm("r8");
    register PRUint64 a5 asm("r9");
    
    switch (nr_gpr) {
#define ARG_GPR(N) \
    case N+1: a##N = gpregs[N];
        ARG_GPR(5);
        ARG_GPR(4);
        ARG_GPR(3);
        ARG_GPR(2);
        ARG_GPR(1);
    case 1: a0 = (PRUint64) that;
    case 0:;
#undef ARG_GPR
    }

    // Ensure that assignments to SSE registers won't be optimized away
    asm("" ::
        "x" (d0), "x" (d1), "x" (d2), "x" (d3),
        "x" (d4), "x" (d5), "x" (d6), "x" (d7));
    
    // Get pointer to method
    PRUint64 methodAddress = *((PRUint64 *)that);
    methodAddress += 8 * methodIndex;
    methodAddress = *((PRUint64 *)methodAddress);
    
    typedef PRUint32 (*Method)(PRUint64, PRUint64, PRUint64, PRUint64, PRUint64, PRUint64);
    PRUint32 result = ((Method)methodAddress)(a0, a1, a2, a3, a4, a5);
    return result;
}
