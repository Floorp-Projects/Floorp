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

#if !defined(LINUX) || !defined(__arm)
#error "this code is for Linux ARM only"
#endif

// Remember that these 'words' are 32bit DWORDS

static PRUint32
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

static void
invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount, nsXPTCVariant* s)
{
    for(PRUint32 i = 0; i < paramCount; i++, d++, s++)
    {
        if(s->IsPtrData())
        {
            *((void**)d) = s->ptr;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     : *((PRInt8*)  d) = s->val.i8;          break;
        case nsXPTType::T_I16    : *((PRInt16*) d) = s->val.i16;         break;
        case nsXPTType::T_I32    : *((PRInt32*) d) = s->val.i32;         break;
        case nsXPTType::T_I64    : *((PRInt64*) d) = s->val.i64; d++;    break;
        case nsXPTType::T_U8     : *((PRUint8*) d) = s->val.u8;          break;
        case nsXPTType::T_U16    : *((PRUint16*)d) = s->val.u16;         break;
        case nsXPTType::T_U32    : *((PRUint32*)d) = s->val.u32;         break;
        case nsXPTType::T_U64    : *((PRUint64*)d) = s->val.u64; d++;    break;
        case nsXPTType::T_FLOAT  : *((float*)   d) = s->val.f;           break;
        case nsXPTType::T_DOUBLE : *((double*)  d) = s->val.d;   d++;    break;
        case nsXPTType::T_BOOL   : *((PRBool*)  d) = s->val.b;           break;
        case nsXPTType::T_CHAR   : *((char*)    d) = s->val.c;           break;
        case nsXPTType::T_WCHAR  : *((wchar_t*) d) = s->val.wc;          break;
        default:
            // all the others are plain pointer types
            *((void**)d) = s->val.p;
            break;
        }
    }
}

extern "C" 
struct my_params_struct {
    nsISupports* that;      
    PRUint32 Index;         
    PRUint32 Count;         
    nsXPTCVariant* params;  
    PRUint32 fn_count;     
    PRUint32 fn_copy;      
};

XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params)
{
    PRUint32 result;
    struct my_params_struct my_params;
    my_params.that = that;
    my_params.Index = methodIndex;
    my_params.Count = paramCount;
    my_params.params = params;
    my_params.fn_copy = (PRUint32) &invoke_copy_to_stack;
    my_params.fn_count = (PRUint32) &invoke_count_words;

  __asm__ __volatile__(
    "ldr	r1, [%1, #12]	\n\t"
    "ldr	ip, [%1, #16]	\n\t"
    "ldr	r0, [%1,  #8]	\n\t"
    "mov	lr, pc		\n\t"
    "mov	pc, ip		\n\t"
    "mov	r4, r0, lsl #2	\n\t"
    "sub	sp, sp, r4	\n\t"
    "mov	r0, sp		\n\t"
    "ldr	r1, [%1,  #8]	\n\t"
    "ldr	r2, [%1, #12]	\n\t"
    "ldr	ip, [%1, #20]	\n\t"
    "mov	lr, pc		\n\t"
    "mov	pc, ip		\n\t"
    "ldr	r0, [%1]	\n\t"
    "ldr	r1, [r0, #0]	\n\t"
    "ldr	r2, [%1, #4]	\n\t"
    "mov	r2, r2, lsl #2	\n\t"
    "add	r2, r2, #8	\n\t"
    "ldr	ip, [r1, r2]	\n\t"
    "cmp	r4, #12		\n\t"
    "ldmgtia	sp!, {r1, r2, r3}\n\t"
    "subgt	r4, r4, #12	\n\t"
    "ldmleia	sp, {r1, r2, r3}\n\t"
    "addle	sp, sp, r4	\n\t"
    "movle	r4, #0		\n\t"
    "ldr	r0, [%1, #0]	\n\t"
    "mov	lr, pc		\n\t"
    "mov	pc, ip		\n\t"
    "add	sp, sp, r4	\n\t"
    "mov	%0, r0		\n\t"
    : "=r" (result)
    : "r" (&my_params)
    : "r0", "r1", "r2", "r3", "r4", "ip", "lr"
    );
    
  return result;
}    
