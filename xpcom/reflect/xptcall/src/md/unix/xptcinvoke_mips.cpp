/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This code is for MIPS using the O32 ABI. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

#include "mozilla/StandardInteger.h"

extern "C" uint32
invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
{
    // Count a word for a0 even though it's never stored or loaded
    // We do this only for alignment of register pairs.
    PRUint32 result = 1;
    for (PRUint32 i = 0; i < paramCount; i++, result++, s++)
    {
        if (s->IsPtrData())
            continue;

        switch(s->type)
        {
        case nsXPTType::T_I64    :
        case nsXPTType::T_U64    :
        case nsXPTType::T_DOUBLE :
	    if (result & 1)
		result++;
	    result++;
	    break;

        default:
            break;
        }
    }
    return (result + 1) & ~(PRUint32)1;
}

extern "C" void
invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount,
                     nsXPTCVariant* s)
{
    // Skip the unused a0 slot, which we keep only for register pair alignment.
    d++;

    for (PRUint32 i = 0; i < paramCount; i++, d++, s++)
    {
        if (s->IsPtrData())
        {
            *((void**)d) = s->ptr;
            continue;
        }

        switch(s->type)
        {
        case nsXPTType::T_I8     : *d = (PRUint32) s->val.i8;   break;
        case nsXPTType::T_I16    : *d = (PRUint32) s->val.i16;  break;
        case nsXPTType::T_I32    : *d = (PRUint32) s->val.i32;  break;
        case nsXPTType::T_I64    :
            if ((intptr_t)d & 4) d++;
            *((PRInt64*) d)  = s->val.i64;    d++;
            break;
        case nsXPTType::T_U8     : *d = (PRUint32) s->val.u8;   break;
        case nsXPTType::T_U16    : *d = (PRUint32) s->val.u16;  break;
        case nsXPTType::T_U32    : *d = (PRUint32) s->val.u32;  break;
        case nsXPTType::T_U64    :
            if ((intptr_t)d & 4) d++;
            *((PRUint64*) d) = s->val.u64;    d++;
            break;
        case nsXPTType::T_FLOAT  : *((float*)   d) = s->val.f;  break;
        case nsXPTType::T_DOUBLE :
            if ((intptr_t)d & 4) d++;
            *((double*)   d) = s->val.d;      d++;
            break;
        case nsXPTType::T_BOOL   : *d = (bool)  s->val.b;     break;
        case nsXPTType::T_CHAR   : *d = (char)    s->val.c;     break;
        case nsXPTType::T_WCHAR  : *d = (wchar_t) s->val.wc;    break;
        default:
            *((void**)d) = s->val.p;
            break;
        }
    }
}

extern "C" nsresult _NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
                                        PRUint32 paramCount,
                                        nsXPTCVariant* params);

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params)
{
    return _NS_InvokeByIndex_P(that, methodIndex, paramCount, params);
}
