/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

#ifndef sparc
#error "This code is for Sparc only"
#endif


PRUint32
invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
{
    PRUint32 result = 0;
    for(PRUint32 i = 0; i < paramCount; i++, s++)
    {
        if(s->IsPtrData())
        {
            result++;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     :
        case nsXPTType::T_I16    :
        case nsXPTType::T_I32    :
            result++;
            break;
        case nsXPTType::T_I64    :
            result+=2;
            break;
        case nsXPTType::T_U8     :
        case nsXPTType::T_U16    :
        case nsXPTType::T_U32    :
            result++;
            break;
        case nsXPTType::T_U64    :
            result+=2;
            break;
        case nsXPTType::T_FLOAT  :
            result++;
            break;
        case nsXPTType::T_DOUBLE :
            result+=2;
            break;
        case nsXPTType::T_BOOL   :
        case nsXPTType::T_CHAR   :
        case nsXPTType::T_WCHAR  :
            result++;
            break;
        default:
            // all the others are plain pointer types
            result++;
            break;
        }
    }
    return result;
}

void
invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount, nsXPTCVariant* s)
{
/*
    We need to copy the parameters for this function to locals and use them
    from there since the parameters occupy the same stack space as the stack
    we're trying to populate.
*/
    uint32 *l_d;
    nsXPCVariant *l_s = s;
    uint32 l_paramCount = paramCount;

    for(uint32 i = 0; i < l_paramCount; i++, l_d++, l_s++)
    {
        if(l_s->IsPtrData())
        {
            *((void**)l_d) = l_s->ptr;
            continue;
        }
        switch(l_s->type)
        {
        case nsXPTType::T_I8     : *((int8*)   l_d) = l_s->val.i8;          break;
        case nsXPTType::T_I16    : *((int16*)  l_d) = l_s->val.i16;         break;
        case nsXPTType::T_I32    : *((int32*)  l_d) = l_s->val.i32;         break;
        case nsXPTType::T_I64    : *((int64*)  l_d) = l_s->val.i64; l_d++;  break;
        case nsXPTType::T_U8     : *((uint8*)  l_d) = l_s->val.u8;          break;
        case nsXPTType::T_U16    : *((uint16*) l_d) = l_s->val.u16;         break;
        case nsXPTType::T_U32    : *((uint32*) l_d) = l_s->val.u32;         break;
        case nsXPTType::T_U64    : *((uint64*) l_d) = l_s->val.u64; l_d++;  break;
        case nsXPTType::T_FLOAT  : *((float*)  l_d) = l_s->val.f;           break;
        case nsXPTType::T_DOUBLE : *((double*) l_d) = l_s->val.d;   l_d++;  break;
        case nsXPTType::T_BOOL   : *((PRBool*) l_d) = l_s->val.b;           break;
        case nsXPTType::T_CHAR   : *((char*)   l_d) = l_s->val.c;           break;
        case nsXPTType::T_WCHAR  : *((wchar_t*)l_d) = l_s->val.wc;          break;
        default:
            // all the others are plain pointer types
            *((void**)l_d) = l_s->val.p;
            break;
        }
    }
}

