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

extern "C" {
void __attribute__ ((__used__)) __attribute__ ((regparm(3)))
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
        case nsXPTType::T_BOOL   : *((bool*)    d) = s->val.b;           break;
        case nsXPTType::T_CHAR   : *((char*)    d) = s->val.c;           break;
        case nsXPTType::T_WCHAR  : *((wchar_t*) d) = s->val.wc;          break;
        default:
            // all the others are plain pointer types
            *((void**)d) = s->val.p;
            break;
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
	".globl _NS_InvokeByIndex_P\n\t"
	"_NS_InvokeByIndex_P:\n\t"
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
	"call  _invoke_copy_to_stack\n\t"
	"movl  0x08(%ebp), %ecx\n\t"	/* 'that' */
	"movl  (%ecx), %edx\n\t"
	"movl  0x0c(%ebp), %eax\n\t"    /* function index */
	"leal  (%edx,%eax,4), %edx\n\t"
	"call  *(%edx)\n\t"
	"movl  %ebp, %esp\n\t"
	"popl  %ebp\n\t"
	"ret\n"
	".section .drectve\n\t"
	".ascii \" -export:NS_InvokeByIndex_P\"\n\t"
	".text\n\t"
);
