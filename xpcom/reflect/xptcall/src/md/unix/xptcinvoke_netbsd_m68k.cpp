/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

// Remember that these 'words' are 32bit DWORDS

#if !defined(__NetBSD__) || !defined(__m68k__)
#error This code is for NetBSD/m68k only
#endif

extern "C" {
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
            // 8 and 16 bit types should be promoted to 32 bits when copying
            // onto the stack.
            case nsXPTType::T_I8     : *((PRUint32*)d) = s->val.i8;          break;
            case nsXPTType::T_I16    : *((PRUint32*)d) = s->val.i16;         break;
            case nsXPTType::T_I32    : *((PRInt32*) d) = s->val.i32;         break;
            case nsXPTType::T_I64    : *((PRInt64*) d) = s->val.i64; d++;    break;
            case nsXPTType::T_U8     : *((PRUint32*)d) = s->val.u8;          break;
            case nsXPTType::T_U16    : *((PRUint32*)d) = s->val.u16;         break;
            case nsXPTType::T_U32    : *((PRUint32*)d) = s->val.u32;         break;
            case nsXPTType::T_U64    : *((PRUint64*)d) = s->val.u64; d++;    break;
            case nsXPTType::T_FLOAT  : *((float*)   d) = s->val.f;           break;
            case nsXPTType::T_DOUBLE : *((double*)  d) = s->val.d;   d++;    break;
            case nsXPTType::T_BOOL   : *((PRUint32*)d) = s->val.b;           break;
            case nsXPTType::T_CHAR   : *((PRUint32*)d) = s->val.c;           break;
            // wchar_t is an int (32 bits) on NetBSD
            case nsXPTType::T_WCHAR  : *((wchar_t*) d) = s->val.wc;          break;
            default:
                // all the others are plain pointer types
                *((void**)d) = s->val.p;
                break;
            }
        }
    }
}

XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params)
{
    PRUint32 result;

 __asm__ __volatile__(
    "movl  %4, sp@-\n\t"
    "movl  %3, sp@-\n\t"
    "jbsr  _invoke_count_words\n\t"     /* count words */
    "addql #8, sp\n\t"
    "lsll  #2, d0\n\t"      /* *= 4 */
    "movl  sp, a2\n\t"	    /* save original sp */
    "subl  d0, sp\n\t"      /* make room for params */
    "movl  sp, a0\n\t"
    "movl  %4, sp@-\n\t"
    "movl  %3, sp@-\n\t"
    "movl  a0, sp@-\n\t"
    "jbsr  _invoke_copy_to_stack\n\t"   /* copy params */
    "addl  #12, sp\n\t"
    "movl  %1, a0\n\t"
    "movl  a0@, a1\n\t"
    "movl  %2, d0\n\t"      /* function index */
    "movl  a0, d1\n\t"
    "movw  a1@(8,d0:l:8), a0\n\t"
    "addl  a0, d1\n\t"
    "movl  a1@(12,d0:l:8), a1\n\t"
    "movl  d1, sp@-\n\t"
    "jbsr  a1@\n\t"
    "movl  a2, sp\n\t"	    /* restore original sp */
    "movl  d0, %0\n\t"
    : "=g" (result)         /* %0 */
    : "g" (that),           /* %1 */
      "g" (methodIndex),    /* %2 */
      "g" (paramCount),     /* %3 */
      "g" (params)          /* %4 */
    : "a0", "a1", "a2", "d0", "d1", "memory"
    );
  
  return result;
}
