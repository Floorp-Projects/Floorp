/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
        result++;
        switch(s->type)
        {
        case nsXPTType::T_I64    :
        case nsXPTType::T_U64    :
        case nsXPTType::T_DOUBLE :
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

XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params)
{
#ifdef __GNUC__            /* Gnu compiler. */
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
#if defined(__GXX_ABI_VERSION) && __GXX_ABI_VERSION >= 100 /* G++ V3 ABI */
    "leal  (%%edx,%%eax,4), %%edx\n\t"
#else /* not G++ V3 ABI  */
    "leal  8(%%edx,%%eax,4), %%edx\n\t"
#endif /* G++ V3 ABI */
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
#elif defined(__SUNPRO_CC)               /* Sun Workshop Compiler. */

asm(
	"\n\t /: PRUint32 n = invoke_count_words (paramCount, params) * 4;"

	"\n\t pushl %ebx / preserve ebx"
	"\n\t pushl %esi / preserve esi"
	"\n\t movl  %esp, %ebx / save address of pushed esi and ebx"

	"\n\t pushl 20(%ebp) / \"params\""
	"\n\t pushl 16(%ebp) / \"paramCount\""
	"\n\t call  invoke_count_words"
	"\n\t mov   %ebx, %esp / restore esp"

	"\n\t sall  $2,%eax"
	"\n\t subl  %eax, %esp / make room for arguments"
	"\n\t movl  %esp, %esi / save new esp"

	"\n\t pushl %esp"
	"\n\t pushl 20(%ebp) / \"params\""
	"\n\t pushl 16(%ebp) / \"paramCount\""
	"\n\t call  invoke_copy_to_stack  /  copy params"
	"\n\t movl  %esi, %esp / restore new esp"

	"\n\t movl  8(%ebp),%ecx / \"that\""
	"\n\t pushl %ecx / \"that\""
	"\n\t movl  (%ecx), %edx" 
	"\n\t movl  12(%ebp), %eax / function index: \"methodIndex\""
	"\n\t movl  8(%edx,%eax,4), %edx"

	"\n\t call  *%edx"
	"\n\t mov   %ebx, %esp"
	"\n\t popl  %esi"
	"\n\t popl  %ebx"
);

/* result == %eax */
  if(0) /* supress "*** is expected to return a value." error */
     return 0;

#else
#error "can't find a compiler to use"
#endif /* __GNUC__ */

}    
