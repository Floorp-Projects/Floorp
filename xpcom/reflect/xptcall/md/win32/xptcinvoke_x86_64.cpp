/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

extern "C" void
invoke_copy_to_stack(uint64_t* d, uint32_t paramCount, nsXPTCVariant* s)
{
   for(; paramCount > 0; paramCount--, d++, s++)
    {
        if(s->IsIndirect())
        {
            *((void**)d) = &s->val;
            continue;
        }

        /*
         * AMD64 platform uses 8 bytes align.
         */

        switch(s->type)
        {
        case nsXPTType::T_I8     : *((int8_t*)  d) = s->val.i8;          break;
        case nsXPTType::T_I16    : *((int16_t*) d) = s->val.i16;         break;
        case nsXPTType::T_I32    : *((int32_t*) d) = s->val.i32;         break;
        case nsXPTType::T_I64    : *((int64_t*) d) = s->val.i64;         break;
        case nsXPTType::T_U8     : *((uint8_t*) d) = s->val.u8;          break;
        case nsXPTType::T_U16    : *((uint16_t*)d) = s->val.u16;         break;
        case nsXPTType::T_U32    : *((uint32_t*)d) = s->val.u32;         break;
        case nsXPTType::T_U64    : *((uint64_t*)d) = s->val.u64;         break;
        case nsXPTType::T_FLOAT  : *((float*)   d) = s->val.f;           break;
        case nsXPTType::T_DOUBLE : *((double*)  d) = s->val.d;           break;
        case nsXPTType::T_BOOL   : *((bool*)  d) = s->val.b;           break;
        case nsXPTType::T_CHAR   : *((char*)    d) = s->val.c;           break;
        case nsXPTType::T_WCHAR  : *((wchar_t*) d) = s->val.wc;          break;
        default:
            // all the others are plain pointer types
            *((void**)d) = s->val.p;
            break;
        }
    }
}

extern "C" nsresult
XPTC__InvokebyIndex(nsISupports* that, uint32_t methodIndex,
                    uint32_t paramCount, nsXPTCVariant* params);

extern "C"
EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                   uint32_t paramCount, nsXPTCVariant* params)
{
    return XPTC__InvokebyIndex(that, methodIndex, paramCount, params);
}

