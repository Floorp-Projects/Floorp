/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Platform specific code to invoke XPCOM methods on native objects

#include "xptcprivate.h"
#include "alloca.h"

// 6 integral parameters are passed in registers
const uint32_t GPR_COUNT = 6;

// 8 floating point parameters are passed in SSE registers
const uint32_t FPR_COUNT = 8;

// Remember that these 'words' are 64-bit long
static inline void
invoke_count_words(uint32_t paramCount, nsXPTCVariant * s,
                   uint32_t & nr_gpr, uint32_t & nr_fpr, uint32_t & nr_stack)
{
    nr_gpr = 1; // skip one GP register for 'that'
    nr_fpr = 0;
    nr_stack = 0;

    /* Compute number of eightbytes of class MEMORY.  */
    for (uint32_t i = 0; i < paramCount; i++, s++) {
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
invoke_copy_to_stack(uint64_t * d, uint32_t paramCount, nsXPTCVariant * s,
                     uint64_t * gpregs, double * fpregs)
{
    uint32_t nr_gpr = 1; // skip one GP register for 'that'
    uint32_t nr_fpr = 0;
    uint64_t value;

    for (uint32_t i = 0; i < paramCount; i++, s++) {
        if (s->IsPtrData())
            value = (uint64_t) s->ptr;
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
            default:                  value = (uint64_t) s->val.p;  break;
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
NS_InvokeByIndex(nsISupports * that, uint32_t methodIndex,
                 uint32_t paramCount, nsXPTCVariant * params)
{
    uint32_t nr_gpr, nr_fpr, nr_stack;
    invoke_count_words(paramCount, params, nr_gpr, nr_fpr, nr_stack);
    
    // Stack, if used, must be 16-bytes aligned
    if (nr_stack)
        nr_stack = (nr_stack + 1) & ~1;

    // Load parameters to stack, if necessary
    uint64_t *stack = (uint64_t *) __builtin_alloca(nr_stack * 8);
    uint64_t gpregs[GPR_COUNT];
    double fpregs[FPR_COUNT];
    invoke_copy_to_stack(stack, paramCount, params, gpregs, fpregs);

    switch (nr_fpr) {
      case 8: asm("movupd %0, %xmm7" : : "xmm7" (fpregs[7]));
      case 7: asm("movupd %0, %xmm6" : : "xmm6" (fpregs[6]));
      case 6: asm("movupd %0, %xmm5" : : "xmm5" (fpregs[5]));
      case 5: asm("movupd %0, %xmm4" : : "xmm4" (fpregs[4]));
      case 4: asm("movupd %0, %xmm3" : : "xmm3" (fpregs[3]));
      case 3: asm("movupd %0, %xmm2" : : "xmm2" (fpregs[2]));
      case 2: asm("movupd %0, %xmm1" : : "xmm1" (fpregs[1]));
      case 1: asm("movupd %0, %xmm0" : : "xmm0" (fpregs[0]));
      case 0:;
    }
    
    // Ensure that assignments to SSE registers won't be optimized away
    asm("" ::: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
    
    // Get pointer to method
    uint64_t methodAddress = *((uint64_t *)that);
    methodAddress += 16 + 8 * methodIndex;
    methodAddress = *((uint64_t *)methodAddress);
    
    typedef uint32_t (*Method)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
    uint32_t result = ((Method)methodAddress)((uint64_t)that, gpregs[1], gpregs[2], gpregs[3], gpregs[4], gpregs[5]);
    return result;
}
