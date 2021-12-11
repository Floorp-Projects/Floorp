/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"


static uint32_t
invoke_count_words(uint32_t paramCount, nsXPTCVariant* s)
{
    uint32_t overflow = 0, gpr = 1 /*this*/, fpr = 0;
    for(uint32_t i = 0; i < paramCount; i++, s++)
    {
        if(s->IsPtrData())
        {
            if (gpr < 5) gpr++; else overflow++;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     :
        case nsXPTType::T_I16    :
        case nsXPTType::T_I32    :
            if (gpr < 5) gpr++; else overflow++;
            break;
        case nsXPTType::T_I64    :
            if (gpr < 4) gpr+=2; else gpr=5, overflow+=2;
            break;
        case nsXPTType::T_U8     :
        case nsXPTType::T_U16    :
        case nsXPTType::T_U32    :
            if (gpr < 5) gpr++; else overflow++;
            break;
        case nsXPTType::T_U64    :
            if (gpr < 4) gpr+=2; else gpr=5, overflow+=2;
            break;
        case nsXPTType::T_FLOAT  :
            if (fpr < 2) fpr++; else overflow++;
            break;
        case nsXPTType::T_DOUBLE :
            if (fpr < 2) fpr++; else overflow+=2;
            break;
        case nsXPTType::T_BOOL   :
        case nsXPTType::T_CHAR   :
        case nsXPTType::T_WCHAR  :
            if (gpr < 5) gpr++; else overflow++;
            break;
        default:
            // all the others are plain pointer types
            if (gpr < 5) gpr++; else overflow++;
            break;
        }
    }
    /* Round up number of overflow words to ensure stack
       stays aligned to 8 bytes.  */
    return (overflow + 1) & ~1;
}

static void
invoke_copy_to_stack(uint32_t paramCount, nsXPTCVariant* s, uint32_t* d_ov, uint32_t overflow)
{
    uint32_t *d_gpr = d_ov + overflow;
    uint64_t *d_fpr = (uint64_t *)(d_gpr + 4);
    uint32_t gpr = 1 /*this*/, fpr = 0;

    for(uint32_t i = 0; i < paramCount; i++, s++)
    {
        if(s->IsPtrData())
        {
            if (gpr < 5)
                *((void**)d_gpr) = s->ptr, d_gpr++, gpr++;
            else
                *((void**)d_ov ) = s->ptr, d_ov++;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     :
            if (gpr < 5)
                *((int32_t*) d_gpr) = s->val.i8, d_gpr++, gpr++;
            else
                *((int32_t*) d_ov ) = s->val.i8, d_ov++;
            break;
        case nsXPTType::T_I16    :
            if (gpr < 5)
                *((int32_t*) d_gpr) = s->val.i16, d_gpr++, gpr++;
            else
                *((int32_t*) d_ov ) = s->val.i16, d_ov++;
            break;
        case nsXPTType::T_I32    :
            if (gpr < 5)
                *((int32_t*) d_gpr) = s->val.i32, d_gpr++, gpr++;
            else
                *((int32_t*) d_ov ) = s->val.i32, d_ov++;
            break;
        case nsXPTType::T_I64    :
            if (gpr < 4)
                *((int64_t*) d_gpr) = s->val.i64, d_gpr+=2, gpr+=2;
            else
                *((int64_t*) d_ov ) = s->val.i64, d_ov+=2, gpr=5;
            break;
        case nsXPTType::T_U8     :
            if (gpr < 5)
                *((uint32_t*) d_gpr) = s->val.u8, d_gpr++, gpr++;
            else
                *((uint32_t*) d_ov ) = s->val.u8, d_ov++;
            break;
        case nsXPTType::T_U16    :
            if (gpr < 5)
                *((uint32_t*)d_gpr) = s->val.u16, d_gpr++, gpr++;
            else
                *((uint32_t*)d_ov ) = s->val.u16, d_ov++;
            break;
        case nsXPTType::T_U32    :
            if (gpr < 5)
                *((uint32_t*)d_gpr) = s->val.u32, d_gpr++, gpr++;
            else
                *((uint32_t*)d_ov ) = s->val.u32, d_ov++;
            break;
        case nsXPTType::T_U64    :
            if (gpr < 4)
                *((uint64_t*)d_gpr) = s->val.u64, d_gpr+=2, gpr+=2;
            else
                *((uint64_t*)d_ov ) = s->val.u64, d_ov+=2, gpr=5;
            break;
        case nsXPTType::T_FLOAT  :
            if (fpr < 2)
                *((float*)   d_fpr) = s->val.f, d_fpr++, fpr++;
            else
                *((float*)   d_ov ) = s->val.f, d_ov++;
            break;
        case nsXPTType::T_DOUBLE :
            if (fpr < 2)
                *((double*)  d_fpr) = s->val.d, d_fpr++, fpr++;
            else
                *((double*)  d_ov ) = s->val.d, d_ov+=2;
            break;
        case nsXPTType::T_BOOL   :
            if (gpr < 5)
                *((uint32_t*)d_gpr) = s->val.b, d_gpr++, gpr++;
            else
                *((uint32_t*)d_ov ) = s->val.b, d_ov++;
            break;
        case nsXPTType::T_CHAR   :
            if (gpr < 5)
                *((uint32_t*)d_gpr) = s->val.c, d_gpr++, gpr++;
            else
                *((uint32_t*)d_ov ) = s->val.c, d_ov++;
            break;
        case nsXPTType::T_WCHAR  :
            if (gpr < 5)
                *((uint32_t*)d_gpr) = s->val.wc, d_gpr++, gpr++;
            else
                *((uint32_t*)d_ov ) = s->val.wc, d_ov++;
            break;
        default:
            // all the others are plain pointer types
            if (gpr < 5)
                *((void**)   d_gpr) = s->val.p, d_gpr++, gpr++;
            else
                *((void**)   d_ov ) = s->val.p, d_ov++;
            break;
        }
    }
}

typedef nsresult (*vtable_func)(nsISupports *, uint32_t, uint32_t, uint32_t, uint32_t, double, double);

// Avoid AddressSanitizer instrumentation for the next function because it
// depends on __builtin_alloca behavior and alignment that cannot be relied on
// once the function is compiled with a version of ASan that has dynamic-alloca
// instrumentation enabled.

MOZ_ASAN_BLACKLIST
EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                 uint32_t paramCount, nsXPTCVariant* params)
{
    vtable_func *vtable = *reinterpret_cast<vtable_func **>(that);
    vtable_func method = vtable[methodIndex];
    uint32_t overflow = invoke_count_words (paramCount, params);
    uint32_t *stack_space = reinterpret_cast<uint32_t *>(__builtin_alloca((overflow + 8 /* 4 32-bits gpr + 2 64-bits fpr */) * 4));

    invoke_copy_to_stack(paramCount, params, stack_space, overflow);

    uint32_t *d_gpr = stack_space + overflow;
    double *d_fpr = reinterpret_cast<double *>(d_gpr + 4);

    return method(that, d_gpr[0], d_gpr[1], d_gpr[2], d_gpr[3],  d_fpr[0], d_fpr[1]);
}
