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
 * Contributor(s): John Fairhurst <john_fairhurst@iname.com>
 */

/* Platform specific code to invoke XPCOM methods on native objects */

// This is 80% copied directly from other platforms; the assembler
// stuff is all my fault, though.
//
// In fact it looks pretty much like we could do away with this os2
// directory & use the "unixish-x86" files; it's all gcc after all.
//
// On the other hand, if we ever do decide to support VAC++ then that'll
// require special treatment but should share some code with this easy stuff.
// 
// So let's keep it.
//
// XXX docs to come on how VAC++ _Optlink callers/interpreters could work
//

#include "xptcprivate.h"

// Remember that these 'words' are 32bit DWORDS

#if !defined(__EMX__)
#error "This code is for OS/2 EMX only"
#endif

static uint32
invoke_count_words( PRUint32 paramCount, nsXPTCVariant* s)
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
invoke_copy_to_stack( PRUint32* d, uint32 paramCount, nsXPTCVariant* s)
{
    for( PRUint32 i = 0; i < paramCount; i++, d++, s++)
    {
        if(s->IsPtrData())
        {
            *((void**)d) = s->ptr;
            continue;
        }
        switch(s->type)
        {
        case nsXPTType::T_I8     : *((int8*)   d) = s->val.i8;          break;
        case nsXPTType::T_I16    : *((int16*)  d) = s->val.i16;         break;
        case nsXPTType::T_I32    : *((int32*)  d) = s->val.i32;         break;
        case nsXPTType::T_I64    : *((int64*)  d) = s->val.i64; d++;    break;
        case nsXPTType::T_U8     : *((uint8*)  d) = s->val.u8;          break;
        case nsXPTType::T_U16    : *((uint16*) d) = s->val.u16;         break;
        case nsXPTType::T_U32    : *((uint32*) d) = s->val.u32;         break;
        case nsXPTType::T_U64    : *((uint64*) d) = s->val.u64; d++;    break;
        case nsXPTType::T_FLOAT  : *((float*)  d) = s->val.f;           break;
        case nsXPTType::T_DOUBLE : *((double*) d) = s->val.d;   d++;    break;
        case nsXPTType::T_BOOL   : *((PRBool*) d) = s->val.b;           break;
        case nsXPTType::T_CHAR   : *((char*)   d) = s->val.c;           break;
        case nsXPTType::T_WCHAR  : *((wchar_t*)d) = s->val.wc;          break;
        default:
            // all the others are plain pointer types
            *((void**)d) = s->val.p;
            break;
        }
    }
}

XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex( nsISupports *that, PRUint32 index,
                    PRUint32 paramcount, nsXPTCVariant* params)
{
   int   ibytes;
   void *pStack;
   int   result = NS_OK;

   // Find size in bytes necessary for call
   ibytes = 4 * invoke_count_words( paramcount, params);

   __asm__ __volatile__ (
   "movl %1,    %%eax\n"                  /* load |ibytes| into eax */
   "subl %%eax, %%esp\n"                      /* make room on stack */
   "movl %%esp, %0"                       /* store base in |pStack| */
   : "=g" (pStack)   /* %0 */
   : "g" (ibytes)    /* %1 */
   : "ax", "memory", "sp"
   );

   // Fill in that gap in the stack with the params to the method
   invoke_copy_to_stack( (PRUint32*) pStack, paramcount, params);

   // push the hidden 'this' parameter, traverse the vtable,
   // and then call the method.

   __asm__ __volatile__ (
   "movl  %2, %%eax\n"                         /* |that| ptr -> eax */
   "pushl %%eax\n"                                /* enstack |that| */
   "movl  (%%eax), %%edx\n"                          /* vptr -> edx */
   "movl  %3, %%eax\n"
   "shl   $3, %%eax\n"                      /* 8 bytes per method.. */
   "addl  $12, %%eax\n"              /* ..plus 12 more at the start */
   "addl  %%eax, %%edx\n"                   /* find pointer to code */
   "call  (%%edx)\n"                                 /* call method */
   "movl  %%eax, %0\n"                       /* save rc in |result| */
   "movl  %1, %%ebx\n"                            /* clear up stack */
   "addl  $4, %%ebx\n"
   "addl  %%ebx, %%esp"
   : "=g" (result)     /* %0 */
   : "g"  (ibytes),    /* %1 */
     "g"  (that),      /* %2 */
     "g"  (index)      /* %3 */
   : "ax", "bx", "dx", "memory", "sp"
   );

   return result;
}    
