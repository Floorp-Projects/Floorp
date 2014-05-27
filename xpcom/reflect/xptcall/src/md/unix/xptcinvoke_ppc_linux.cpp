/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Platform specific code to invoke XPCOM methods on native objects

// The purpose of NS_InvokeByIndex() is to map a platform
// indepenpent call to the platform ABI. To do that,
// NS_InvokeByIndex() has to determine the method to call via vtable
// access. The parameters for the method are read from the
// nsXPTCVariant* and prepared for th native ABI.  For the Linux/PPC
// ABI this means that the first 8 integral and floating point
// parameters are passed in registers.

#include "xptcprivate.h"

// 8 integral parameters are passed in registers
#define GPR_COUNT     8

// With hardfloat support 8 floating point parameters are passed in registers,
// floats are promoted to doubles when passed in registers
// In Softfloat mode, everything is handled via gprs
#ifndef __NO_FPRS__
#define FPR_COUNT     8
#endif
extern "C" uint32_t
invoke_count_words(uint32_t paramCount, nsXPTCVariant* s)
{
  return uint32_t(((paramCount * 2) + 3) & ~3);
}

extern "C" void
invoke_copy_to_stack(uint32_t* d,
                     uint32_t paramCount,
                     nsXPTCVariant* s, 
                     uint32_t* gpregs,
                     double* fpregs)
{
    uint32_t gpr = 1; // skip one GP reg for 'that'
#ifndef __NO_FPRS__
    uint32_t fpr = 0;
#endif
    uint32_t tempu32;
    uint64_t tempu64;
    
    for(uint32_t i = 0; i < paramCount; i++, s++) {
        if(s->IsPtrData()) {
            if(s->type == nsXPTType::T_JSVAL)
                tempu32 = (uint32_t) &s->ptr;
            else
                tempu32 = (uint32_t) s->ptr;
        }
        else {
            switch(s->type) {
            case nsXPTType::T_FLOAT:                                  break;
            case nsXPTType::T_DOUBLE:                                 break;
            case nsXPTType::T_I8:     tempu32 = s->val.i8;            break;
            case nsXPTType::T_I16:    tempu32 = s->val.i16;           break;
            case nsXPTType::T_I32:    tempu32 = s->val.i32;           break;
            case nsXPTType::T_I64:    tempu64 = s->val.i64;           break;
            case nsXPTType::T_U8:     tempu32 = s->val.u8;            break;
            case nsXPTType::T_U16:    tempu32 = s->val.u16;           break;
            case nsXPTType::T_U32:    tempu32 = s->val.u32;           break;
            case nsXPTType::T_U64:    tempu64 = s->val.u64;           break;
            case nsXPTType::T_BOOL:   tempu32 = s->val.b;             break;
            case nsXPTType::T_CHAR:   tempu32 = s->val.c;             break;
            case nsXPTType::T_WCHAR:  tempu32 = s->val.wc;            break;
            default:                  tempu32 = (uint32_t) s->val.p;  break;
            }
        }

        if (!s->IsPtrData() && s->type == nsXPTType::T_DOUBLE) {
#ifndef __NO_FPRS__
            if (fpr < FPR_COUNT)
                fpregs[fpr++]    = s->val.d;
#else
            if (gpr & 1)
                gpr++;
            if ((gpr + 1) < GPR_COUNT) {
                *((double*) &gpregs[gpr]) = s->val.d;
                gpr += 2;
            }
#endif
            else {
                if ((uint32_t) d & 4) d++; // doubles are 8-byte aligned on stack
                *((double*) d) = s->val.d;
                d += 2;
            }
        }
        else if (!s->IsPtrData() && s->type == nsXPTType::T_FLOAT) {
#ifndef __NO_FPRS__
            if (fpr < FPR_COUNT)
                fpregs[fpr++]   = s->val.f; // if passed in registers, floats are promoted to doubles
#else
            if (gpr < GPR_COUNT)
                *((float*) &gpregs[gpr++]) = s->val.f;
#endif
            else
                *((float*) d++) = s->val.f;
        }
        else if (!s->IsPtrData() && (s->type == nsXPTType::T_I64
                                     || s->type == nsXPTType::T_U64)) {
            if (gpr & 1) gpr++; // longlongs are aligned in odd/even register pairs, eg. r5/r6
            if ((gpr + 1) < GPR_COUNT) {
                *((uint64_t*) &gpregs[gpr]) = tempu64;
                gpr += 2;
            }
            else {
                if ((uint32_t) d & 4) d++; // longlongs are 8-byte aligned on stack
                *((uint64_t*) d)            = tempu64;
                d += 2;
            }
        }
        else {
            if (gpr < GPR_COUNT)
                gpregs[gpr++] = tempu32;
            else
                *d++          = tempu32;
        }
        
    }
}

extern "C"
EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                 uint32_t paramCount, nsXPTCVariant* params);
