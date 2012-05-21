/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"
#include "xptc_gcc_x86_unix.h"

extern "C" {
static void ATTRIBUTE_USED __attribute__ ((regparm(3)))
invoke_copy_to_stack(PRUint32 paramCount, nsXPTCVariant* s, PRUint32* d)
{
    for(PRUint32 i = paramCount; i >0; i--, d++, s++)
    {
        if(s->IsPtrData())
        {
            *((void**)d) = s->ptr;
            continue;
        }

        switch(s->type)
        {
        case nsXPTType::T_I64    : *((PRInt64*) d) = s->val.i64; d++;    break;
        case nsXPTType::T_U64    : *((PRUint64*)d) = s->val.u64; d++;    break;
        case nsXPTType::T_DOUBLE : *((double*)  d) = s->val.d;   d++;    break;
        default                  : *((void**)d)    = s->val.p;           break;
        }
    }
}
} // extern "C"

/*
  EXPORT_XPCOM_API(nsresult)
  NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params);

  Each param takes at most two 4-byte words.
  It doesn't matter if we push too many words, and calculating the exact
  amount takes time.

  that        = ebp + 0x08
  methodIndex = ebp + 0x0c
  paramCount  = ebp + 0x10
  params      = ebp + 0x14

*/

__asm__ (
	".text\n\t"
/* alignment here seems unimportant here; this was 16, now it's 2 which
   is what xptcstubs uses. */
	".align 2\n\t"
	".globl " SYMBOL_UNDERSCORE "NS_InvokeByIndex_P\n\t"
#ifndef XP_MACOSX
	".type  " SYMBOL_UNDERSCORE "NS_InvokeByIndex_P,@function\n"
#endif
	SYMBOL_UNDERSCORE "NS_InvokeByIndex_P:\n\t"
	"pushl %ebp\n\t"
	"movl  %esp, %ebp\n\t"
	"movl  0x10(%ebp), %eax\n\t"
	"leal  0(,%eax,8),%edx\n\t"

        /* set up call frame for method. */
	"subl  %edx, %esp\n\t"       /* make room for params. */
/* Align to maximum x86 data size: 128 bits == 16 bytes == XMM register size.
 * This is to avoid protection faults where SSE+ alignment of stack pointer
 * is assumed and required, e.g. by GCC4's -ftree-vectorize option.
 */
	"andl  $0xfffffff0, %esp\n\t"   /* drop(?) stack ptr to 128-bit align */
/* $esp should be aligned to a 16-byte boundary here (note we include an 
 * additional 4 bytes in a later push instruction). This will ensure $ebp 
 * in the function called below is aligned to a 0x8 boundary. SSE instructions 
 * like movapd/movdqa expect memory operand to be aligned on a 16-byte
 * boundary. The GCC compiler will generate the memory operand using $ebp
 * with an 8-byte offset.
 */
	"subl  $0xc, %esp\n\t"          /* lower again; push/call below will re-align */
	"movl  %esp, %ecx\n\t"          /* ecx = d */
	"movl  8(%ebp), %edx\n\t"       /* edx = this */
	"pushl %edx\n\t"                /* push this. esp % 16 == 0 */

	"movl  0x14(%ebp), %edx\n\t"
	"call  " SYMBOL_UNDERSCORE "invoke_copy_to_stack\n\t"
	"movl  0x08(%ebp), %ecx\n\t"	/* 'that' */
	"movl  (%ecx), %edx\n\t"
	"movl  0x0c(%ebp), %eax\n\t"    /* function index */
	"leal  (%edx,%eax,4), %edx\n\t"
	"call  *(%edx)\n\t"
	"movl  %ebp, %esp\n\t"
	"popl  %ebp\n\t"
	"ret\n"
#ifndef XP_MACOSX
	".size " SYMBOL_UNDERSCORE "NS_InvokeByIndex_P, . -" SYMBOL_UNDERSCORE "NS_InvokeByIndex_P\n\t"
#endif
);
