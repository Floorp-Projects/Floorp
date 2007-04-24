/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 *   Mark Mentovai <mark@moxienet.com>
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
 * ***** END LICENSE BLOCK ***** */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"
#include "xptc_platforms_unixish_x86.h"

extern "C" {

static void
invoke_copy_to_stack(PRUint32 paramCount, nsXPTCVariant* s, PRUint32* d)
{
    for(PRUint32 i = paramCount; i >0; i--, d++, s++)
    {
        if(s->IsPtrData())
        {
            *((void**)d) = s->ptr;
            continue;
        }

/* XXX: the following line is here (rather than as the default clause in
 *      the following switch statement) so that the Sun native compiler
 *      will generate the correct assembly code on the Solaris Intel
 *      platform. See the comments in bug #28817 for more details.
 */

        *((void**)d) = s->val.p;

        switch(s->type)
        {
        case nsXPTType::T_I64    : *((PRInt64*) d) = s->val.i64; d++;    break;
        case nsXPTType::T_U64    : *((PRUint64*)d) = s->val.u64; d++;    break;
        case nsXPTType::T_DOUBLE : *((double*)  d) = s->val.d;   d++;    break;
        }
    }
}

}

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
                 PRUint32 paramCount, nsXPTCVariant* params)
{
#ifdef __GNUC__            /* Gnu compiler. */
  PRUint32 result;
  /* Each param takes at most 2, 4-byte words
     It doesn't matter if we push too many words, and calculating the exact
     amount takes time. */
  PRUint32 n = paramCount << 3;
  void (*fn_copy) (unsigned int, nsXPTCVariant *, PRUint32 *) = invoke_copy_to_stack;
  int temp1, temp2;
 
  /* These are only significant when KEEP_STACK_16_BYTE_ALIGNED is
     defined.  Otherwise, they're just placeholders to keep the parameter
     indices the same for aligned and unaligned users in the inline asm
     block. */
  unsigned int saved_esp;
  
 __asm__ __volatile__(
#ifdef KEEP_STACK_16_BYTE_ALIGNED
    "movl  %%esp, %3\n\t"
#endif
    "subl  %8, %%esp\n\t" /* make room for params */
#ifdef KEEP_STACK_16_BYTE_ALIGNED
    /* For the second CALL, there will be one parameter before the ones
       copied by invoke_copy_to_stack.  Make sure that the stack will be
       aligned for that CALL. */
    "subl  $4, %%esp\n\t"
    "andl  $0xfffffff0, %%esp\n\t"
    /* For the first CALL, there are three parameters.  Leave padding to
       ensure alignment. */
    "subl  $4, %%esp\n\t"
    /* The third parameter to invoke_copy_to_stack is the destination pointer.
       It needs to point into the parameter area prepared for the second CALL,
       leaving room for the |that| parameter.  This reuses |n|, which was
       the stack space to reserve, but that's OK because it's no longer needed
       if the stack is being kept aligned. */
    "leal  8(%%esp), %8\n\t"
    "pushl %8\n\t"
#else
    "pushl %%esp\n\t"
#endif
    "pushl %7\n\t"
    "pushl %6\n\t"
    "call  *%9\n\t"       /* copy params */
#ifdef KEEP_STACK_16_BYTE_ALIGNED
    /* The stack is still aligned from the first CALL.  Keep it aligned for
       the next one by popping past the parameters from the first CALL and
       leaving space for the first (|that|) parameter for the second CALL. */
    "addl  $0x14, %%esp\n\t"
#else
    "addl  $0xc, %%esp\n\t"
#endif
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
#if defined(__GXX_ABI_VERSION) && __GXX_ABI_VERSION >= 100 /* G++ V3 ABI */
    "leal  (%%edx,%%eax,4), %%edx\n\t"
#else /* not G++ V3 ABI  */
    "leal  8(%%edx,%%eax,4), %%edx\n\t"
#endif /* G++ V3 ABI */
#endif
    "call  *(%%edx)\n\t"    /* safe to not cleanup esp */
#ifdef KEEP_STACK_16_BYTE_ALIGNED
    "movl  %3, %%esp\n\t"
#else
    "addl  $4, %%esp\n\t"
    "addl  %8, %%esp"
#endif
    : "=a" (result),        /* %0 */
      "=c" (temp1),         /* %1 */
      "=d" (temp2),         /* %2 */
#ifdef KEEP_STACK_16_BYTE_ALIGNED
      "=&g" (saved_esp)     /* %3 */
#else
      /* Don't waste a register, this isn't used if alignment is unimportant */
      "=m" (saved_esp)      /* %3 */
#endif
    : "g" (that),           /* %4 */
      "g" (methodIndex),    /* %5 */
      "1" (paramCount),     /* %6 */
      "2" (params),         /* %7 */
#ifdef KEEP_STACK_16_BYTE_ALIGNED
      /* Must be in a register, it's the target of an LEA instruction */
      "r" (n),              /* %8 */
#else
      "g" (n),              /* %8 */
#endif
      "0" (fn_copy)         /* %9 */
    : "memory"
    );
    
  return result;

#else
#error "can't find a compiler to use"
#endif /* __GNUC__ */

}    
