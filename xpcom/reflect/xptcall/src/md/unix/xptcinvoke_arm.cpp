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
 * Mike Hommey <mh@glandium.org>
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

#if !defined(__arm__) && !(defined(LINUX) || defined(ANDROID))
#error "This code is for Linux ARM only. Check that it works on your system, too.\nBeware that this code is highly compiler dependent."
#endif

/* This function copies a 64-bits word from dw to the given pointer in
 * a buffer delimited by start and end, possibly wrapping around the
 * buffer boundaries, and/or properly aligning the data at 64-bits word
 * boundaries (for EABI).
 * start and end are both assumed to be 64-bits aligned.
 * Returns a pointer to the second 32-bits word copied (to accomodate
 * the invoke_copy_to_stack loop).
 */
static PRUint32 *
copy_double_word(PRUint32 *start, PRUint32 *current, PRUint32 *end, PRUint64 *dw)
{
#ifdef __ARM_EABI__
    /* Aligning the pointer for EABI */
    current = (PRUint32 *)(((PRUint32)current + 7) & ~7);
    /* Wrap when reaching the end of the buffer */
    if (current == end) current = start;
#else
    /* On non-EABI, 64-bits values are not aligned and when we reach the end
     * of the buffer, we need to write half of the data at the end, and the
     * other half at the beginning. */
    if (current == end - 1) {
        *current = ((PRUint32*)dw)[0];
        *start = ((PRUint32*)dw)[1];
        return start;
    }
#endif

    *((PRUint64*) current) = *dw;
    return current + 1;
}

/* See stack_space comment in NS_InvokeByIndex to see why this needs not to
 * be static on DEBUG builds. */
#ifndef DEBUG
static
#endif
void
invoke_copy_to_stack(PRUint32* stk, PRUint32 *end,
                     PRUint32 paramCount, nsXPTCVariant* s)
{
    /* The stack buffer is 64-bits aligned. The end argument points to its end.
     * The caller is assumed to create a stack buffer of at least four 32-bits
     * words.
     * We use the last three 32-bit words to store the values for r1, r2 and r3
     * for the method call, i.e. the first words for arguments passing.
     */
    PRUint32 *d = end - 3;
    for(PRUint32 i = 0; i < paramCount; i++, d++, s++)
    {
        /* Wrap when reaching the end of the stack buffer */
        if (d == end) d = stk;
        NS_ASSERTION(d >= stk && d < end,
            "invoke_copy_to_stack is copying outside its given buffer");
        if(s->IsPtrData())
        {
            *((void**)d) = s->ptr;
            continue;
        }
        // According to the ARM EABI, integral types that are smaller than a word
        // are to be sign/zero-extended to a full word and treated as 4-byte values.

        switch(s->type)
        {
        case nsXPTType::T_I8     : *((PRInt32*) d) = s->val.i8;          break;
        case nsXPTType::T_I16    : *((PRInt32*) d) = s->val.i16;         break;
        case nsXPTType::T_I32    : *((PRInt32*) d) = s->val.i32;         break;
        case nsXPTType::T_I64    :
            d = copy_double_word(stk, d, end, (PRUint64 *)&s->val.i64);
            break;
        case nsXPTType::T_U8     : *((PRUint32*)d) = s->val.u8;          break;
        case nsXPTType::T_U16    : *((PRUint32*)d) = s->val.u16;         break;
        case nsXPTType::T_U32    : *((PRUint32*)d) = s->val.u32;         break;
        case nsXPTType::T_U64    :
            d = copy_double_word(stk, d, end, (PRUint64 *)&s->val.u64);
            break;
        case nsXPTType::T_FLOAT  : *((float*)   d) = s->val.f;           break;
        case nsXPTType::T_DOUBLE :
            d = copy_double_word(stk, d, end, (PRUint64 *)&s->val.d);
            break;
        case nsXPTType::T_BOOL   : *((PRInt32*) d) = s->val.b;           break;
        case nsXPTType::T_CHAR   : *((PRInt32*) d) = s->val.c;           break;
        case nsXPTType::T_WCHAR  : *((PRInt32*) d) = s->val.wc;          break;
        default:
            // all the others are plain pointer types
            *((void**)d) = s->val.p;
            break;
        }
    }
}

typedef PRUint32 (*vtable_func)(nsISupports *, PRUint32, PRUint32, PRUint32);

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params)
{

/* This is to call a given method of class that.
 * The parameters are in params, the number is in paramCount.
 * The routine will issue calls to count the number of words
 * required for argument passing and to copy the arguments to
 * the stack.
 * ACPS passes the first 3 params in r1-r3 (with exceptions for 64-bits
 * arguments), and the remaining goes onto the stack.
 * We allocate a buffer on the stack for a "worst case" estimate of how much
 * stack might be needed for EABI, i.e. twice the number of parameters.
 * The end of this buffer will be used to store r1 to r3, so that the start
 * of the stack is the remaining parameters.
 * The magic here is to call the method with "that" and three 32-bits
 * arguments corresponding to r1-r3, so that the compiler generates the
 * proper function call. The stack will also contain the remaining arguments.
 *
 * !!! IMPORTANT !!!
 * This routine makes assumptions about the vtable layout of the c++ compiler. It's implemented
 * for arm-linux GNU g++ >= 2.8.1 (including egcs and gcc-2.95.[1-3])!
 *
 */
 
  register vtable_func *vtable, func;
  register int base_size = (paramCount > 1) ? paramCount : 2;

/* !!! IMPORTANT !!!
 * On DEBUG builds, the NS_ASSERTION used in invoke_copy_to_stack needs to use
 * the stack to pass the 5th argument to NS_DebugBreak. When invoke_copy_to_stack
 * is inlined, this can result, depending on the compiler and flags, in the
 * stack pointer not pointing at stack_space when the method is called at the
 * end of this function. More generally, any function call requiring stack
 * allocation of arguments is unsafe to be inlined in this function.
 */
  PRUint32 *stack_space = (PRUint32 *) __builtin_alloca(base_size * 8);

  invoke_copy_to_stack(stack_space, &stack_space[base_size * 2],
                       paramCount, params);

  vtable = *reinterpret_cast<vtable_func **>(that);
#if defined(__GXX_ABI_VERSION) && __GXX_ABI_VERSION >= 100 /* G++ V3 ABI */
  func = vtable[methodIndex];
#else /* non G++ V3 ABI */
  func = vtable[2 + methodIndex];
#endif

  return func(that, stack_space[base_size * 2 - 3],
                    stack_space[base_size * 2 - 2],
                    stack_space[base_size * 2 - 1]);
}    
