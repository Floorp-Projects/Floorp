/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Platform specific code to invoke XPCOM methods on native objects

// The purpose of NS_InvokeByIndex_P() is to map a platform
// independent call to the platform ABI. To do that,
// NS_InvokeByIndex_P() has to determine the method to call via vtable
// access. The parameters for the method are read from the
// nsXPTCVariant* and prepared for the native ABI.

// The PowerPC64 platform ABI can be found here:
// http://www.freestandards.org/spec/ELF/ppc64/
// and in particular:
// http://www.freestandards.org/spec/ELF/ppc64/PPC-elf64abi-1.9.html#FUNC-CALL

#include <stdio.h>
#include "xptcprivate.h"

// 8 integral parameters are passed in registers, not including 'that'
#define GPR_COUNT     7

// 8 floating point parameters are passed in registers, floats are
// promoted to doubles when passed in registers
#define FPR_COUNT     13

extern "C" PRUint32
invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
{
    return PRUint32(((paramCount * 2) + 3) & ~3);
}

extern "C" void
invoke_copy_to_stack(PRUint64* gpregs,
                     double* fpregs,
                     PRUint32 paramCount,
                     nsXPTCVariant* s, 
                     PRUint64* d)
{
    PRUint64 tempu64;

    for(uint32 i = 0; i < paramCount; i++, s++) {
        if(s->IsPtrData())
            tempu64 = (PRUint64) s->ptr;
        else {
            switch(s->type) {
            case nsXPTType::T_FLOAT:                                  break;
            case nsXPTType::T_DOUBLE:                                 break;
            case nsXPTType::T_I8:     tempu64 = s->val.i8;            break;
            case nsXPTType::T_I16:    tempu64 = s->val.i16;           break;
            case nsXPTType::T_I32:    tempu64 = s->val.i32;           break;
            case nsXPTType::T_I64:    tempu64 = s->val.i64;           break;
            case nsXPTType::T_U8:     tempu64 = s->val.u8;            break;
            case nsXPTType::T_U16:    tempu64 = s->val.u16;           break;
            case nsXPTType::T_U32:    tempu64 = s->val.u32;           break;
            case nsXPTType::T_U64:    tempu64 = s->val.u64;           break;
            case nsXPTType::T_BOOL:   tempu64 = s->val.b;             break;
            case nsXPTType::T_CHAR:   tempu64 = s->val.c;             break;
            case nsXPTType::T_WCHAR:  tempu64 = s->val.wc;            break;
            default:                  tempu64 = (PRUint64) s->val.p;  break;
            }
        }

        if (!s->IsPtrData() && s->type == nsXPTType::T_DOUBLE) {
            if (i < FPR_COUNT)
                fpregs[i]    = s->val.d;
            else
                *(double *)d = s->val.d;
        }
        else if (!s->IsPtrData() && s->type == nsXPTType::T_FLOAT) {
            if (i < FPR_COUNT) {
                fpregs[i]   = s->val.f; // if passed in registers, floats are promoted to doubles
            } else {
                float *p = (float *)d;
                p++;
                *p = s->val.f;
            }
        }
        else {
            if (i < GPR_COUNT)
                gpregs[i] = tempu64;
            else
                *d = tempu64;
        }
        if (i >= 7)
            d++;
    }
}

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params);

