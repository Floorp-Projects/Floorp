/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"
#include "xptc_platforms_unixish_x86.h"

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
invoke_copy_to_stack(PRUint32 paramCount, nsXPTCVariant* s, PRUint32* d)
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

XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params)
{
    PRUint32 result;
  PRUint32 n = invoke_count_words (paramCount, params) * 4;
  void (*fn_copy) (unsigned int, nsXPTCVariant *, PRUint32 *) = invoke_copy_to_stack;
  int temp1, temp2, temp3;

 __asm__ __volatile__(
    "subl  %8, %%esp\n\t" /* make room for params */
    "pushl %%esp\n\t"
    "pushl %7\n\t"
    "pushl %6\n\t"
    "call  *%0\n\t"       /* copy params */
    "addl  $0xc, %%esp\n\t"
    "movl  %4, %%ecx\n\t"
#ifdef CFRONT_STYLE_THIS_ADJUST
    "movl  (%%ecx), %%edx\n\t" 
    "movl  %5, %%eax\n\t"   /* function index */
    "shl   $3, %%eax\n\t"   /* *= 8 */
    "addl  $8, %%eax\n\t"   /* += 8 skip first entry */
    "addl  %%eax, %%edx\n\t"
    "movswl (%%edx), %%eax\n\t" /* 'this' offset */
    "addl  %%eax, %%ecx\n\t"
    "pushl %%ecx\n\t"
    "addl  $4, %%edx\n\t"   /* += 4, method pointer */
#else /* THUNK_BASED_THIS_ADJUST */
    "pushl %%ecx\n\t"
    "movl  (%%ecx), %%edx\n\t" 
    "movl  %5, %%eax\n\t"   /* function index */
    "leal  8(%%edx,%%eax,4), %%edx\n\t"
#endif
    "call  *(%%edx)\n\t"    /* safe to not cleanup esp */
    "addl  $4, %%esp\n\t"
    "addl  %8, %%esp"
    : "=a" (result),        /* %0 */
      "=c" (temp1),         /* %1 */
      "=d" (temp2),         /* %2 */
      "=g" (temp3)          /* %3 */
    : "g" (that),           /* %4 */
      "g" (methodIndex),    /* %5 */
      "1" (paramCount),     /* %6 */
      "2" (params),         /* %7 */
      "g" (n),              /* %8 */
      "0" (fn_copy)         /* %3 */
    : "memory"
    );
  
  return result;
}    
