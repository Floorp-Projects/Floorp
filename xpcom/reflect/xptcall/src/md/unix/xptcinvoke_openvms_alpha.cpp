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

extern "C" {

/* This is in the ASM file */
XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params);


void
invoke_copy_to_stack(PRUint64* d, PRUint32 paramCount, nsXPTCVariant* s)
{
    const PRUint8 NUM_ARG_REGS = 6-1;        // -1 for "this" pointer

    for(PRUint32 i = 0; i < paramCount; i++, d++, s++)
    {
        if(s->IsPtrData())
        {
            *d = (PRUint64)s->ptr;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     : *d = (PRUint64)s->val.i8;     break;
        case nsXPTType::T_I16    : *d = (PRUint64)s->val.i16;    break;
        case nsXPTType::T_I32    : *d = (PRUint64)s->val.i32;    break;
        case nsXPTType::T_I64    : *d = (PRUint64)s->val.i64;    break;
        case nsXPTType::T_U8     : *d = (PRUint64)s->val.u8;     break;
        case nsXPTType::T_U16    : *d = (PRUint64)s->val.u16;    break;
        case nsXPTType::T_U32    : *d = (PRUint64)s->val.u32;    break;
        case nsXPTType::T_U64    : *d = (PRUint64)s->val.u64;    break;
        case nsXPTType::T_FLOAT  :
            if(i < NUM_ARG_REGS)
            {
                // convert floats to doubles if they are to be passed
                // via registers so we can just deal with doubles later
                union { PRUint64 u64; double d; } t;
                t.d = (double)s->val.f;
                *d = t.u64;
            }
            else
                // otherwise copy to stack normally
                *d = (PRUint64)s->val.u32;
            break;
        case nsXPTType::T_DOUBLE : *d = (PRUint64)s->val.u64;    break;
        case nsXPTType::T_BOOL   : *d = (PRUint64)s->val.b;      break;
        case nsXPTType::T_CHAR   : *d = (PRUint64)s->val.c;      break;
        case nsXPTType::T_WCHAR  : *d = (PRUint64)s->val.wc;     break;
        default:
            // all the others are plain pointer types
            *d = (PRUint64)s->val.p;
            break;
        }
    }
}

}

