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
 * Siarhei Siamashka <siarhei.siamashka@nokia.com>
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

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)) \
    && defined(__ARM_EABI__) && !defined(__ARM_PCS_VFP) && !defined(__ARM_PCS)
#error "Can't identify floating point calling conventions.\nPlease ensure that your toolchain defines __ARM_PCS or __ARM_PCS_VFP."
#endif

#ifndef __ARM_PCS_VFP

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

#else /* __ARM_PCS_VFP */

/* "Procedure Call Standard for the ARM Architecture" document, sections
 * "5.5 Parameter Passing" and "6.1.2 Procedure Calling" contain all the
 * needed information.
 *
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0042d/IHI0042D_aapcs.pdf
 */

#if defined(__thumb__) && !defined(__thumb2__)
#error "Thumb1 is not supported"
#endif

#ifndef __ARMEL__
#error "Only little endian compatibility was tested"
#endif

/*
 * Allocation of integer function arguments initially to registers r1-r3
 * and then to stack. Handling of 'this' argument which goes to r0 registers
 * is handled separately and does not belong to these two inline functions.
 *
 * The doubleword arguments are allocated to even:odd
 * register pairs or get aligned at 8-byte boundary on stack. The "holes"
 * which may appear as a result of this realignment remain unused.
 *
 * 'ireg_args'  - pointer to the current position in the buffer,
 *                corresponding to the register arguments
 * 'stack_args' - pointer to the current position in the buffer,
 *                corresponding to the arguments on stack
 * 'end'        - pointer to the end of the registers argument
 *                buffer (it is guaranteed to be 8-bytes aligned)
 */

static inline void copy_word(PRUint32* &ireg_args,
                             PRUint32* &stack_args,
                             PRUint32* end,
                             PRUint32  data)
{
  if (ireg_args < end) {
    *ireg_args = data;
    ireg_args++;
  } else {
    *stack_args = data;
    stack_args++;
  }
}

static inline void copy_dword(PRUint32* &ireg_args,
                              PRUint32* &stack_args,
                              PRUint32* end,
                              PRUint64  data)
{
  if (ireg_args + 1 < end) {
    if ((PRUint32)ireg_args & 4) {
      ireg_args++;
    }
    *(PRUint64 *)ireg_args = data;
    ireg_args += 2;
  } else {
    if ((PRUint32)stack_args & 4) {
      stack_args++;
    }
    *(PRUint64 *)stack_args = data;
    stack_args += 2;
  }
}

/*
 * Allocation of floating point arguments to VFP registers (s0-s15, d0-d7).
 *
 * Unlike integer registers allocation, "back-filling" needs to be
 * supported. For example, the third floating point argument in the
 * following function is going to be allocated to s1 register, back-filling
 * the "hole":
 *    void f(float s0, double d1, float s1)
 *
 * Refer to the "Procedure Call Standard for the ARM Architecture" document
 * for more details.
 *
 * 'vfp_s_args' - pointer to the current position in the buffer with
 *                the next unallocated single precision register
 * 'vfp_d_args' - pointer to the current position in the buffer with
 *                the next unallocated double precision register,
 *                it has the same value as 'vfp_s_args' when back-filling
 *                is not used
 * 'end'        - pointer to the end of the vfp registers argument
 *                buffer (it is guaranteed to be 8-bytes aligned)
 *
 * Mozilla bugtracker has a test program attached which be used for
 * experimenting with VFP registers allocation code and testing its
 * correctness:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=601914#c19
 */

static inline bool copy_vfp_single(float* &vfp_s_args, double* &vfp_d_args,
                                   float* end, float data)
{
  if (vfp_s_args >= end)
    return false;

  *vfp_s_args = data;
  vfp_s_args++;
  if (vfp_s_args < (float *)vfp_d_args) {
    // It was the case of back-filling, now the next free single precision
    // register should overlap with the next free double precision register
    vfp_s_args = (float *)vfp_d_args;
  } else if (vfp_s_args > (float *)vfp_d_args) {
    // also update the pointer to the next free double precision register
    vfp_d_args++;
  }
  return true;
}

static inline bool copy_vfp_double(float* &vfp_s_args, double* &vfp_d_args,
                                   float* end, double data)
{
  if (vfp_d_args >= (double *)end) {
    // The back-filling continues only so long as no VFP CPRC has been
    // allocated to a slot on the stack. Basically no VFP registers can
    // be allocated after this point.
    vfp_s_args = end;
    return false;
  }

  if (vfp_s_args == (float *)vfp_d_args) {
    // also update the pointer to the next free single precision register
    vfp_s_args += 2;
  }
  *vfp_d_args = data;
  vfp_d_args++;
  return true;
}

static void
invoke_copy_to_stack(PRUint32* stk, PRUint32 *end,
                     PRUint32 paramCount, nsXPTCVariant* s)
{
  PRUint32 *ireg_args  = end - 3;
  float    *vfp_s_args = (float *)end;
  double   *vfp_d_args = (double *)end;
  float    *vfp_end    = vfp_s_args + 16;

  for (PRUint32 i = 0; i < paramCount; i++, s++) {
    if (s->IsPtrData()) {
      copy_word(ireg_args, stk, end, (PRUint32)s->ptr);
      continue;
    }
    // According to the ARM EABI, integral types that are smaller than a word
    // are to be sign/zero-extended to a full word and treated as 4-byte values
    switch (s->type)
    {
      case nsXPTType::T_FLOAT:
        if (!copy_vfp_single(vfp_s_args, vfp_d_args, vfp_end, s->val.f)) {
          copy_word(end, stk, end, reinterpret_cast<PRUint32&>(s->val.f));
        }
        break;
      case nsXPTType::T_DOUBLE:
        if (!copy_vfp_double(vfp_s_args, vfp_d_args, vfp_end, s->val.d)) {
          copy_dword(end, stk, end, reinterpret_cast<PRUint64&>(s->val.d));
        }
        break;
      case nsXPTType::T_I8:  copy_word(ireg_args, stk, end, s->val.i8);   break;
      case nsXPTType::T_I16: copy_word(ireg_args, stk, end, s->val.i16);  break;
      case nsXPTType::T_I32: copy_word(ireg_args, stk, end, s->val.i32);  break;
      case nsXPTType::T_I64: copy_dword(ireg_args, stk, end, s->val.i64); break;
      case nsXPTType::T_U8:  copy_word(ireg_args, stk, end, s->val.u8);   break;
      case nsXPTType::T_U16: copy_word(ireg_args, stk, end, s->val.u16);  break;
      case nsXPTType::T_U32: copy_word(ireg_args, stk, end, s->val.u32);  break;
      case nsXPTType::T_U64: copy_dword(ireg_args, stk, end, s->val.u64); break;
      case nsXPTType::T_BOOL: copy_word(ireg_args, stk, end, s->val.b);   break;
      case nsXPTType::T_CHAR: copy_word(ireg_args, stk, end, s->val.c);   break;
      case nsXPTType::T_WCHAR: copy_word(ireg_args, stk, end, s->val.wc); break;
      default:
        // all the others are plain pointer types
        copy_word(ireg_args, stk, end, reinterpret_cast<PRUint32>(s->val.p));
        break;
    }
  }
}

typedef PRUint32 (*vtable_func)(nsISupports *, PRUint32, PRUint32, PRUint32);

EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params)
{
  vtable_func *vtable = *reinterpret_cast<vtable_func **>(that);
#if defined(__GXX_ABI_VERSION) && __GXX_ABI_VERSION >= 100 /* G++ V3 ABI */
  vtable_func func = vtable[methodIndex];
#else /* non G++ V3 ABI */
  vtable_func func = vtable[2 + methodIndex];
#endif
  // 'register PRUint32 result asm("r0")' could be used here, but it does not
  //  seem to be reliable in all cases: http://gcc.gnu.org/PR46164
  PRUint32 result;
  asm (
    "mov    %[stack_space_size], %[param_count_plus_2], lsl #3\n"
    "tst    sp, #4\n" /* check stack alignment */

    "add    %[stack_space_size], #(4 * 16)\n" /* space for VFP registers */
    "mov    r3, %[params]\n"

    "it     ne\n"
    "addne  %[stack_space_size], %[stack_space_size], #4\n"
    "sub    r0, sp, %[stack_space_size]\n" /* allocate space on stack */

    "sub    r2, %[param_count_plus_2], #2\n"
    "mov    sp, r0\n"

    "add    r1, r0, %[param_count_plus_2], lsl #3\n"
    "blx    %[invoke_copy_to_stack]\n"

    "add    ip, sp, %[param_count_plus_2], lsl #3\n"
    "mov    r0, %[that]\n"
    "ldmdb  ip, {r1, r2, r3}\n"
    "vldm   ip, {d0, d1, d2, d3, d4, d5, d6, d7}\n"
    "blx    %[func]\n"

    "add    sp, sp, %[stack_space_size]\n" /* cleanup stack */
    "mov    %[stack_space_size], r0\n" /* it's actually 'result' variable */
    : [stack_space_size]     "=&r" (result)
    : [func]                 "r"   (func),
      [that]                 "r"   (that),
      [params]               "r"   (params),
      [param_count_plus_2]   "r"   (paramCount + 2),
      [invoke_copy_to_stack] "r"   (invoke_copy_to_stack)
    : "cc", "memory",
      // Mark all the scratch registers as clobbered because they may be
      // modified by the functions, called from this inline assembly block
      "r0", "r1", "r2", "r3", "ip", "lr",
      "d0",  "d1",  "d2",  "d3",  "d4",  "d5",  "d6",  "d7",
      // Also unconditionally mark d16-d31 registers as clobbered even though
      // they actually don't exist in vfpv2 and vfpv3-d16 variants. There is
      // no way to identify VFP variant using preprocessor at the momemnt
      // (see http://gcc.gnu.org/PR46128 for more details), but fortunately
      // current versions of gcc do not seem to complain about these registers
      // even when this code is compiled with '-mfpu=vfpv3-d16' option.
      // If gcc becomes more strict in the future and/or provides a way to
      // identify VFP variant, the following d16-d31 registers list needs
      // to be wrapped into some #ifdef
      "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
      "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31"
  );
  return result;
}

#endif
