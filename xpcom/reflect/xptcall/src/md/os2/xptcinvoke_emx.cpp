/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Fairhurst <john_fairhurst@iname.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/22/2000  IBM Corp.       Fixed multiple inheritance bug in XPTC_InvokeByIndex to adjust the
 *                                     "that" (this) pointer appropriately.
 */

/* Platform specific code to invoke XPCOM methods on native objects */

// This is 80% copied directly from other platforms; the assembler
// stuff is all my fault, though. (jmf)

#if !defined(__EMX__)
#error "This code is for OS/2 EMX only"
#endif

#include "xptcprivate.h"

// Remember that these 'words' are 32-bit DWORDs
static PRUint32
invoke_count_words( PRUint32 paramCount, nsXPTCVariant* s)
{
    PRUint32 result = 0;

    for(PRUint32 i = 0; i < paramCount; i++, s++)
    {
        if(s->IsPtrData())
            result++;

        else {

            switch(s->type)
            { 
             // 64-bit types
             case nsXPTType::T_I64    :
             case nsXPTType::T_U64    :
             case nsXPTType::T_DOUBLE :
                 result+=2;
                 break;

             // all others are dwords
             default:
                 result++;
                 break;
            }
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
            *((void**)d) = s->ptr;

        else {

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

   __asm__ __volatile__(
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

   __asm__ __volatile__(
   "movl  %2, %%eax\n"                         /* |that| ptr -> eax */
   "movl  (%%eax), %%edx\n"                          /* vptr -> edx */
   "movl  %3, %%ebx\n"
   "shl   $3, %%ebx\n"                      /* 8 bytes per method.. */
   "addl  $8, %%ebx\n"              /* ..plus 8 to skip over 1st 8 bytes of vtbl */

   "addl  %%ebx, %%edx\n"                 /* find appropriate vtbl entry */
   "movswl (%%edx),%%ecx\n"           /* get possible |that| ptr adjustment value */
   "addl  %%ecx, %%eax\n"                 /* adjust the |that| ptr (needed for multiple inheritance) */
   "pushl %%eax\n"                            /* enstack the possibly-adjusted |that| */

   "addl  $4, %%edx\n"              /* ..add 4 more to get to the method's entry point */

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
