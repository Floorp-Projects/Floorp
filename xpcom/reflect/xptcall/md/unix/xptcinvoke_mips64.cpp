/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

#if (_MIPS_SIM != _ABIN32) && (_MIPS_SIM != _ABI64)
#error "This code is for MIPS n32/n64 only"
#endif

extern "C" uint32_t
invoke_count_words(uint32_t paramCount, nsXPTCVariant* s)
{
    return paramCount;
}

extern "C" void
invoke_copy_to_stack(uint64_t* d, uint32_t paramCount,
                     nsXPTCVariant* s, uint64_t *regs)
{
#define N_ARG_REGS       7       /* 8 regs minus 1 for "this" ptr */

    for (uint32_t i = 0; i < paramCount; i++, s++)
    {
        if (s->IsIndirect()) {
            if (i < N_ARG_REGS)
                regs[i] = (uint64_t) &s->val;
            else
                *d++ = (uint64_t) &s->val;
            continue;
        }
        switch (s->type) {
        //
        // signed types first
        //
        case nsXPTType::T_I8:
            if (i < N_ARG_REGS)
                ((int64_t*)regs)[i] = s->val.i8;
            else
                *d++ = s->val.i8;
            break;
        case nsXPTType::T_I16:
            if (i < N_ARG_REGS)
                ((int64_t*)regs)[i] = s->val.i16;
            else
                *d++ = s->val.i16;
            break;
        case nsXPTType::T_I32:
            if (i < N_ARG_REGS)
                ((int64_t*)regs)[i] = s->val.i32;
            else
                *d++ = s->val.i32;
            break;
        case nsXPTType::T_I64:
            if (i < N_ARG_REGS)
                ((int64_t*)regs)[i] = s->val.i64;
            else
                *d++ = s->val.i64;
            break;
        //
        // unsigned types next
        //
        case nsXPTType::T_U8:
            if (i < N_ARG_REGS)
                regs[i] = s->val.u8;
            else
                *d++ = s->val.u8;
            break;
        case nsXPTType::T_U16:
            if (i < N_ARG_REGS)
                regs[i] = s->val.u16;
            else
                *d++ = s->val.u16;
            break;
        case nsXPTType::T_U32:
            if (i < N_ARG_REGS)
		// 32-bit values need to be sign-extended
		// in register, so use the signed value.
                regs[i] = s->val.i32;
            else
                *d++ = s->val.u32;
            break;
        case nsXPTType::T_U64:
            if (i < N_ARG_REGS)
                regs[i] = s->val.u64;
            else
                *d++ = s->val.u64;
            break;
        case nsXPTType::T_FLOAT:
            // the float data formate must not be converted!
            // Just only copy without conversion.
            if (i < N_ARG_REGS)
                *(float*)&regs[i] = s->val.f;
            else
                *(float*)d++ = s->val.f;
            break;
        case nsXPTType::T_DOUBLE:
            if (i < N_ARG_REGS)
                *(double*)&regs[i] = s->val.d;
            else
                *(double*)d++ = s->val.d;
            break;
        case nsXPTType::T_BOOL:
            if (i < N_ARG_REGS)
                regs[i] = s->val.b;
            else
                *d++ = s->val.b;
            break;
        case nsXPTType::T_CHAR:
            if (i < N_ARG_REGS)
                regs[i] = s->val.c;
            else
                *d++ = s->val.c;
            break;
        case nsXPTType::T_WCHAR:
            if (i < N_ARG_REGS)
                regs[i] = s->val.wc;
            else
                *d++ = s->val.wc;
            break;
        default:
            // all the others are plain pointer types
            if (i < N_ARG_REGS)
                regs[i] = (uint64_t)s->val.p;
            else
               *d++ = (uint64_t)s->val.p;
            break;
        }
    }
}

extern "C" nsresult _NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                                        uint32_t paramCount,
                                        nsXPTCVariant* params);

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                   uint32_t paramCount, nsXPTCVariant* params)
{
    return _NS_InvokeByIndex(that, methodIndex, paramCount, params);
}
